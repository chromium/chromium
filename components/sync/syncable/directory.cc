// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/syncable/directory.h"

#include <inttypes.h>

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "base/files/file_enumerator.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_usage_estimator.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "components/sync/base/unique_position.h"
#include "components/sync/base/unrecoverable_error_handler.h"
#include "components/sync/protocol/proto_memory_estimations.h"
#include "components/sync/syncable/in_memory_directory_backing_store.h"
#include "components/sync/syncable/model_neutral_mutable_entry.h"
#include "components/sync/syncable/nigori_handler.h"
#include "components/sync/syncable/on_disk_directory_backing_store.h"
#include "components/sync/syncable/scoped_kernel_lock.h"
#include "components/sync/syncable/scoped_parent_child_index_updater.h"
#include "components/sync/syncable/syncable_base_transaction.h"
#include "components/sync/syncable/syncable_changes_version.h"
#include "components/sync/syncable/syncable_read_transaction.h"
#include "components/sync/syncable/syncable_util.h"
#include "components/sync/syncable/syncable_write_transaction.h"

using std::string;

namespace syncer {
namespace syncable {

// static
const base::FilePath::CharType Directory::kSyncDatabaseFilename[] =
    FILE_PATH_LITERAL("SyncData.sqlite3");

Directory::PersistedKernelInfo::PersistedKernelInfo() {
  ModelTypeSet protocol_types = ProtocolTypes();
  for (ModelType type : protocol_types) {
    ResetDownloadProgress(type);
    transaction_version[type] = 0;
  }
}

Directory::PersistedKernelInfo::~PersistedKernelInfo() {}

void Directory::PersistedKernelInfo::ResetDownloadProgress(
    ModelType model_type) {
  // Clear everything except the data type id field.
  download_progress[model_type].Clear();
  download_progress[model_type].set_data_type_id(
      GetSpecificsFieldNumberFromModelType(model_type));

  // Explicitly set an empty token field to denote no progress.
  download_progress[model_type].set_token("");
}

bool Directory::PersistedKernelInfo::HasEmptyDownloadProgress(
    ModelType model_type) {
  const sync_pb::DataTypeProgressMarker& progress_marker =
      download_progress[model_type];
  return progress_marker.token().empty();
}

size_t Directory::PersistedKernelInfo::EstimateMemoryUsage() const {
  using base::trace_event::EstimateMemoryUsage;
  return EstimateMemoryUsage(legacy_store_birthday) +
         EstimateMemoryUsage(legacy_bag_of_chips) +
         EstimateMemoryUsage(datatype_context);
}

Directory::SaveChangesSnapshot::SaveChangesSnapshot()
    : kernel_info_status(KERNEL_SHARE_INFO_INVALID) {}

Directory::SaveChangesSnapshot::~SaveChangesSnapshot() {}

bool Directory::SaveChangesSnapshot::HasUnsavedMetahandleChanges() const {
  return !dirty_metas.empty() || !metahandles_to_purge.empty();
}

Directory::Kernel::Kernel(
    const std::string& name,
    const KernelLoadInfo& info,
    DirectoryChangeDelegate* delegate,
    const WeakHandle<TransactionObserver>& transaction_observer)
    : next_write_transaction_id(0),
      name(name),
      info_status(Directory::KERNEL_SHARE_INFO_VALID),
      persisted_info(info.kernel_info),
      legacy_cache_guid(info.legacy_cache_guid),
      next_metahandle(info.max_metahandle + 1),
      delegate(delegate),
      transaction_observer(transaction_observer) {
  DCHECK(delegate);
  DCHECK(transaction_observer.IsInitialized());
}

Directory::Kernel::~Kernel() {}

Directory::Directory(
    std::unique_ptr<DirectoryBackingStore> store,
    const WeakHandle<UnrecoverableErrorHandler>& unrecoverable_error_handler,
    const base::Closure& report_unrecoverable_error_function,
    NigoriHandler* nigori_handler)
    : store_(std::move(store)),
      unrecoverable_error_handler_(unrecoverable_error_handler),
      report_unrecoverable_error_function_(report_unrecoverable_error_function),
      unrecoverable_error_set_(false),
      nigori_handler_(nigori_handler),
      invariant_check_level_(VERIFY_CHANGES) {}

Directory::~Directory() {
  Close();
}

DirOpenResult Directory::Open(
    const string& name,
    DirectoryChangeDelegate* delegate,
    const WeakHandle<TransactionObserver>& transaction_observer) {
  TRACE_EVENT0("sync", "SyncDatabaseOpen");

  const DirOpenResult result = OpenImpl(name, delegate, transaction_observer);

  if (OPENED_NEW != result && OPENED_EXISTING != result)
    Close();
  return result;
}

void Directory::InitializeIndices(MetahandlesMap* handles_map) {
  ScopedKernelLock lock(this);
  kernel_->metahandles_map.swap(*handles_map);
  for (auto it = kernel_->metahandles_map.begin();
       it != kernel_->metahandles_map.end(); ++it) {
    EntryKernel* entry = it->second.get();
    if (ParentChildIndex::ShouldInclude(entry))
      kernel_->parent_child_index.Insert(entry);
    const int64_t metahandle = entry->ref(META_HANDLE);
    if (entry->ref(IS_UNSYNCED))
      kernel_->unsynced_metahandles.insert(metahandle);
    if (entry->ref(IS_UNAPPLIED_UPDATE)) {
      const ModelType type = entry->GetServerModelType();
      kernel_->unapplied_update_metahandles[type].insert(metahandle);
    }
    if (!entry->ref(UNIQUE_SERVER_TAG).empty()) {
      DCHECK(kernel_->server_tags_map.find(entry->ref(UNIQUE_SERVER_TAG)) ==
             kernel_->server_tags_map.end())
          << "Unexpected duplicate use of server tag";
      kernel_->server_tags_map[entry->ref(UNIQUE_SERVER_TAG)] = entry;
    }
    if (!entry->ref(UNIQUE_CLIENT_TAG).empty()) {
      DCHECK(kernel_->client_tags_map.find(entry->ref(UNIQUE_CLIENT_TAG)) ==
             kernel_->client_tags_map.end())
          << "Unexpected duplicate use of client tag";
      kernel_->client_tags_map[entry->ref(UNIQUE_CLIENT_TAG)] = entry;
    }
    DCHECK(kernel_->ids_map.find(entry->ref(ID).value()) ==
           kernel_->ids_map.end())
        << "Unexpected duplicate use of ID";
    kernel_->ids_map[entry->ref(ID).value()] = entry;
    DCHECK(!entry->is_dirty());
  }
}

DirOpenResult Directory::OpenImpl(
    const string& name,
    DirectoryChangeDelegate* delegate,
    const WeakHandle<TransactionObserver>& transaction_observer) {
  KernelLoadInfo info;
  // Temporary indices before kernel_ initialized in case Load fails. We 0(1)
  // swap these later.
  Directory::MetahandlesMap tmp_handles_map;

  MetahandleSet metahandles_to_purge;

  DirOpenResult result =
      store_->Load(&tmp_handles_map, &metahandles_to_purge, &info);
  if (OPENED_NEW != result && OPENED_EXISTING != result)
    return result;

  DCHECK(!kernel_);
  kernel_ =
      std::make_unique<Kernel>(name, info, delegate, transaction_observer);
  kernel_->metahandles_to_purge.swap(metahandles_to_purge);
  InitializeIndices(&tmp_handles_map);

  // Save changes back in case there are any metahandles to purge.
  if (!SaveChanges())
    return FAILED_INITIAL_WRITE;

  // Now that we've successfully opened the store, install an error handler to
  // deal with catastrophic errors that may occur later on. Use a weak pointer
  // because we cannot guarantee that this Directory will outlive the Closure.
  store_->SetCatastrophicErrorHandler(base::Bind(
      &Directory::OnCatastrophicError, weak_ptr_factory_.GetWeakPtr()));

  return result;
}

void Directory::Close() {
  store_.reset();
  kernel_.reset();
}

void Directory::OnUnrecoverableError(const BaseTransaction* trans,
                                     const base::Location& location,
                                     const std::string& message) {
  DCHECK(trans != nullptr);
  unrecoverable_error_set_ = true;
  unrecoverable_error_handler_.Call(
      FROM_HERE, &UnrecoverableErrorHandler::OnUnrecoverableError, location,
      message);
}

EntryKernel* Directory::GetEntryById(const Id& id) {
  ScopedKernelLock lock(this);
  return GetEntryById(lock, id);
}

EntryKernel* Directory::GetEntryById(const ScopedKernelLock& lock,
                                     const Id& id) {
  DCHECK(kernel_);
  // Find it in the in memory ID index.
  auto id_found = kernel_->ids_map.find(id.value());
  if (id_found != kernel_->ids_map.end()) {
    return id_found->second;
  }
  return nullptr;
}

EntryKernel* Directory::GetEntryByClientTag(const string& tag) {
  ScopedKernelLock lock(this);
  DCHECK(kernel_);

  auto it = kernel_->client_tags_map.find(tag);
  if (it != kernel_->client_tags_map.end()) {
    return it->second;
  }
  return nullptr;
}

EntryKernel* Directory::GetEntryByServerTag(const string& tag) {
  ScopedKernelLock lock(this);
  DCHECK(kernel_);
  auto it = kernel_->server_tags_map.find(tag);
  if (it != kernel_->server_tags_map.end()) {
    return it->second;
  }
  return nullptr;
}

EntryKernel* Directory::GetEntryByHandle(int64_t metahandle) {
  ScopedKernelLock lock(this);
  return GetEntryByHandle(lock, metahandle);
}

EntryKernel* Directory::GetEntryByHandle(const ScopedKernelLock& lock,
                                         int64_t metahandle) {
  // Look up in memory
  auto found = kernel_->metahandles_map.find(metahandle);
  if (found != kernel_->metahandles_map.end()) {
    // Found it in memory.  Easy.
    return found->second.get();
  }
  return nullptr;
}

bool Directory::GetChildHandlesById(BaseTransaction* trans,
                                    const Id& parent_id,
                                    Directory::Metahandles* result) {
  if (!SyncAssert(this == trans->directory(), FROM_HERE,
                  "Directories don't match", trans))
    return false;
  result->clear();

  ScopedKernelLock lock(this);
  AppendChildHandles(lock, parent_id, result);
  return true;
}

int Directory::GetTotalNodeCount(BaseTransaction* trans,
                                 EntryKernel* kernel) const {
  if (!SyncAssert(this == trans->directory(), FROM_HERE,
                  "Directories don't match", trans))
    return false;

  int count = 1;
  base::circular_deque<const OrderedChildSet*> child_sets;

  GetChildSetForKernel(trans, kernel, &child_sets);
  while (!child_sets.empty()) {
    const OrderedChildSet* set = child_sets.front();
    child_sets.pop_front();
    for (auto it = set->begin(); it != set->end(); ++it) {
      count++;
      GetChildSetForKernel(trans, *it, &child_sets);
    }
  }

  return count;
}

void Directory::GetChildSetForKernel(
    BaseTransaction* trans,
    EntryKernel* kernel,
    base::circular_deque<const OrderedChildSet*>* child_sets) const {
  if (!kernel->ref(IS_DIR))
    return;  // Not a directory => no children.

  const OrderedChildSet* descendants =
      kernel_->parent_child_index.GetChildren(kernel->ref(ID));
  if (!descendants)
    return;  // This directory has no children.

  // Add our children to the list of items to be traversed.
  child_sets->push_back(descendants);
}

int Directory::GetPositionIndex(BaseTransaction* trans,
                                EntryKernel* kernel) const {
  const OrderedChildSet* siblings =
      kernel_->parent_child_index.GetSiblings(kernel);

  auto it = siblings->find(kernel);
  return std::distance(siblings->begin(), it);
}

bool Directory::InsertEntry(BaseWriteTransaction* trans,
                            std::unique_ptr<EntryKernel> entry) {
  ScopedKernelLock lock(this);
  return InsertEntry(lock, trans, std::move(entry));
}

bool Directory::InsertEntry(const ScopedKernelLock& lock,
                            BaseWriteTransaction* trans,
                            std::unique_ptr<EntryKernel> entry) {
  if (!SyncAssert(nullptr != entry, FROM_HERE, "Entry is null", trans))
    return false;
  EntryKernel* entry_ptr = entry.get();

  static const char error[] = "Entry already in memory index.";

  if (!SyncAssert(kernel_->metahandles_map
                      .insert(std::make_pair(entry_ptr->ref(META_HANDLE),
                                             std::move(entry)))
                      .second,
                  FROM_HERE, error, trans)) {
    return false;
  }
  if (!SyncAssert(
          kernel_->ids_map
              .insert(std::make_pair(entry_ptr->ref(ID).value(), entry_ptr))
              .second,
          FROM_HERE, error, trans)) {
    return false;
  }
  if (ParentChildIndex::ShouldInclude(entry_ptr)) {
    if (!SyncAssert(kernel_->parent_child_index.Insert(entry_ptr), FROM_HERE,
                    error, trans)) {
      return false;
    }
  }

  // Should NEVER be created with a client tag or server tag.
  if (!SyncAssert(entry_ptr->ref(UNIQUE_SERVER_TAG).empty(), FROM_HERE,
                  "Server tag should be empty", trans)) {
    return false;
  }
  if (!SyncAssert(entry_ptr->ref(UNIQUE_CLIENT_TAG).empty(), FROM_HERE,
                  "Client tag should be empty", trans))
    return false;

  return true;
}

bool Directory::ReindexId(BaseWriteTransaction* trans,
                          EntryKernel* const entry,
                          const Id& new_id) {
  ScopedKernelLock lock(this);
  if (nullptr != GetEntryById(lock, new_id))
    return false;

  {
    // Update the indices that depend on the ID field.
    ScopedParentChildIndexUpdater updater_b(lock, entry,
                                            &kernel_->parent_child_index);
    size_t num_erased = kernel_->ids_map.erase(entry->ref(ID).value());
    DCHECK_EQ(1U, num_erased);
    entry->put(ID, new_id);
    kernel_->ids_map[entry->ref(ID).value()] = entry;
  }
  return true;
}

bool Directory::ReindexParentId(BaseWriteTransaction* trans,
                                EntryKernel* const entry,
                                const Id& new_parent_id) {
  ScopedKernelLock lock(this);

  {
    // Update the indices that depend on the PARENT_ID field.
    ScopedParentChildIndexUpdater index_updater(lock, entry,
                                                &kernel_->parent_child_index);
    entry->put(PARENT_ID, new_parent_id);
  }
  return true;
}

// static
void Directory::DeleteDirectoryFiles(const base::FilePath& directory_path) {
  // We assume that the directory database files are all top level files, and
  // use no folders. We also assume that there might be child folders under
  // |directory_path| that are used for non-directory things, like storing
  // ModelTypeStore/LevelDB data, and we expressly do not want to delete those.
  if (base::DirectoryExists(directory_path)) {
    base::FileEnumerator fe(directory_path, false, base::FileEnumerator::FILES);
    for (base::FilePath current = fe.Next(); !current.empty();
         current = fe.Next()) {
      if (!base::DeleteFile(current, false)) {
        LOG(DFATAL) << "Could not delete all sync directory files.";
      }
    }
  }
}

bool Directory::unrecoverable_error_set(const BaseTransaction* trans) const {
  DCHECK(trans != nullptr);
  return unrecoverable_error_set_;
}

void Directory::ClearDirtyMetahandles(const ScopedKernelLock& lock) {
  kernel_->transaction_mutex.AssertAcquired();
  kernel_->dirty_metahandles.clear();
}

bool Directory::SafeToPurgeFromMemory(const EntryKernel& entry) const {
  return entry.ref(IS_DEL) && !entry.is_dirty() && !entry.ref(SYNCING) &&
         !entry.ref(IS_UNAPPLIED_UPDATE) && !entry.ref(IS_UNSYNCED);
}

bool Directory::SafeToPurgeFromMemoryForTransaction(
    WriteTransaction* trans,
    const EntryKernel* const entry) const {
  bool safe = SafeToPurgeFromMemory(*entry);

  if (safe) {
    int64_t handle = entry->ref(META_HANDLE);
    const ModelType type = entry->GetServerModelType();
    if (!SyncAssert(kernel_->dirty_metahandles.count(handle) == 0U, FROM_HERE,
                    "Dirty metahandles should be empty", trans))
      return false;
    // TODO(tim): Bug 49278.
    if (!SyncAssert(!kernel_->unsynced_metahandles.count(handle), FROM_HERE,
                    "Unsynced handles should be empty", trans))
      return false;
    if (!SyncAssert(!kernel_->unapplied_update_metahandles[type].count(handle),
                    FROM_HERE, "Unapplied metahandles should be empty", trans))
      return false;
  }

  return safe;
}

void Directory::TakeSnapshotForSaveChanges(SaveChangesSnapshot* snapshot) {
  ReadTransaction trans(FROM_HERE, this);
  ScopedKernelLock lock(this);

  // If there is an unrecoverable error then just bail out.
  if (unrecoverable_error_set(&trans))
    return;

  // Deep copy dirty entries from kernel_->metahandles_index into snapshot and
  // clear dirty flags.
  for (auto i = kernel_->dirty_metahandles.begin();
       i != kernel_->dirty_metahandles.end(); ++i) {
    EntryKernel* entry = GetEntryByHandle(lock, *i);
    if (!entry)
      continue;
    // Skip over false positives; it happens relatively infrequently.
    if (!entry->is_dirty())
      continue;
    snapshot->dirty_metas.insert(snapshot->dirty_metas.end(),
                                 std::make_unique<EntryKernel>(*entry));
    DCHECK_EQ(1U, kernel_->dirty_metahandles.count(*i));
    // We don't bother removing from the index here as we blow the entire thing
    // in a moment, and it unnecessarily complicates iteration.
    entry->clear_dirty(nullptr);
  }
  ClearDirtyMetahandles(lock);

  // Set purged handles.
  DCHECK(snapshot->metahandles_to_purge.empty());
  snapshot->metahandles_to_purge.swap(kernel_->metahandles_to_purge);

  // Fill kernel_info_status and kernel_info.
  snapshot->kernel_info = kernel_->persisted_info;
  snapshot->kernel_info_status = kernel_->info_status;
  // This one we reset on failure.
  kernel_->info_status = KERNEL_SHARE_INFO_VALID;
}

bool Directory::SaveChanges() {
  bool success = false;

  base::AutoLock scoped_lock(kernel_->save_changes_mutex);

  // Snapshot and save.
  SaveChangesSnapshot snapshot;
  TakeSnapshotForSaveChanges(&snapshot);
  success = store_->SaveChanges(snapshot);

  // Handle success or failure.
  if (success)
    success = VacuumAfterSaveChanges(snapshot);
  else
    HandleSaveChangesFailure(snapshot);
  return success;
}

bool Directory::VacuumAfterSaveChanges(const SaveChangesSnapshot& snapshot) {
  if (snapshot.dirty_metas.empty())
    return true;

  // Need a write transaction as we are about to permanently purge entries.
  WriteTransaction trans(FROM_HERE, VACUUM_AFTER_SAVE, this);
  ScopedKernelLock lock(this);
  // Now drop everything we can out of memory.
  for (auto i = snapshot.dirty_metas.begin(); i != snapshot.dirty_metas.end();
       ++i) {
    auto found = kernel_->metahandles_map.find((*i)->ref(META_HANDLE));
    if (found != kernel_->metahandles_map.end() &&
        SafeToPurgeFromMemoryForTransaction(&trans, found->second.get())) {
      // We now drop deleted metahandles that are up to date on both the client
      // and the server.
      std::unique_ptr<EntryKernel> entry = std::move(found->second);

      size_t num_erased = 0;
      kernel_->metahandles_map.erase(found);
      num_erased = kernel_->ids_map.erase(entry->ref(ID).value());
      DCHECK_EQ(1u, num_erased);
      if (!entry->ref(UNIQUE_SERVER_TAG).empty()) {
        num_erased =
            kernel_->server_tags_map.erase(entry->ref(UNIQUE_SERVER_TAG));
        DCHECK_EQ(1u, num_erased);
      }
      if (!entry->ref(UNIQUE_CLIENT_TAG).empty()) {
        num_erased =
            kernel_->client_tags_map.erase(entry->ref(UNIQUE_CLIENT_TAG));
        DCHECK_EQ(1u, num_erased);
      }
      if (!SyncAssert(!kernel_->parent_child_index.Contains(entry.get()),
                      FROM_HERE, "Deleted entry still present", (&trans)))
        return false;
    }
    if (trans.unrecoverable_error_set())
      return false;
  }
  return true;
}

void Directory::UnapplyEntry(EntryKernel* entry) {
  int64_t handle = entry->ref(META_HANDLE);
  ModelType server_type =
      GetModelTypeFromSpecifics(entry->ref(SERVER_SPECIFICS));

  // Clear enough so that on the next sync cycle all local data will
  // be overwritten.
  // Note: do not modify the root node in order to preserve the
  // initial sync ended bit for this type (else on the next restart
  // this type will be treated as disabled and therefore fully purged).
  if (entry->ref(PARENT_ID).IsRoot()) {
    ModelType root_type = server_type;
    // Support both server created and client created type root folders.
    if (!IsRealDataType(root_type)) {
      root_type = GetModelTypeFromSpecifics(entry->ref(SPECIFICS));
    }
    if (IsRealDataType(root_type) &&
        ModelTypeToRootTag(root_type) == entry->ref(UNIQUE_SERVER_TAG)) {
      return;
    }
  }

  // Set the unapplied bit if this item has server data.
  if (IsRealDataType(server_type) && !entry->ref(IS_UNAPPLIED_UPDATE)) {
    entry->put(IS_UNAPPLIED_UPDATE, true);
    kernel_->unapplied_update_metahandles[server_type].insert(handle);
    entry->mark_dirty(&kernel_->dirty_metahandles);
  }

  // Unset the unsynced bit.
  if (entry->ref(IS_UNSYNCED)) {
    kernel_->unsynced_metahandles.erase(handle);
    entry->put(IS_UNSYNCED, false);
    entry->mark_dirty(&kernel_->dirty_metahandles);
  }

  // Mark the item as locally deleted. No deleted items are allowed in the
  // parent child index.
  if (!entry->ref(IS_DEL)) {
    kernel_->parent_child_index.Remove(entry);
    entry->put(IS_DEL, true);
    entry->mark_dirty(&kernel_->dirty_metahandles);
  }

  // Set the version to the "newly created" version.
  if (entry->ref(BASE_VERSION) != CHANGES_VERSION) {
    entry->put(BASE_VERSION, CHANGES_VERSION);
    entry->mark_dirty(&kernel_->dirty_metahandles);
  }

  // At this point locally created items that aren't synced will become locally
  // deleted items, and purged on the next snapshot. All other items will match
  // the state they would have had if they were just created via a server
  // update. See MutableEntry::MutableEntry(.., CreateNewUpdateItem, ..).
}

void Directory::DeleteEntry(const ScopedKernelLock& lock,
                            EntryKernel* entry_ptr) {
  int64_t handle = entry_ptr->ref(META_HANDLE);

  kernel_->metahandles_to_purge.insert(handle);

  std::unique_ptr<EntryKernel> entry =
      std::move(kernel_->metahandles_map[handle]);

  ModelType server_type =
      GetModelTypeFromSpecifics(entry->ref(SERVER_SPECIFICS));

  size_t num_erased = 0;
  num_erased = kernel_->metahandles_map.erase(handle);
  DCHECK_EQ(1u, num_erased);
  num_erased = kernel_->ids_map.erase(entry->ref(ID).value());
  DCHECK_EQ(1u, num_erased);
  num_erased = kernel_->unsynced_metahandles.erase(handle);
  DCHECK_EQ(entry->ref(IS_UNSYNCED), num_erased > 0);
  num_erased = kernel_->unapplied_update_metahandles[server_type].erase(handle);
  DCHECK_EQ(entry->ref(IS_UNAPPLIED_UPDATE), num_erased > 0);
  if (kernel_->parent_child_index.Contains(entry.get()))
    kernel_->parent_child_index.Remove(entry.get());

  if (!entry->ref(UNIQUE_CLIENT_TAG).empty()) {
    num_erased = kernel_->client_tags_map.erase(entry->ref(UNIQUE_CLIENT_TAG));
    DCHECK_EQ(1u, num_erased);
  }
  if (!entry->ref(UNIQUE_SERVER_TAG).empty()) {
    num_erased = kernel_->server_tags_map.erase(entry->ref(UNIQUE_SERVER_TAG));
    DCHECK_EQ(1u, num_erased);
  }
}

void Directory::PurgeEntriesWithTypeIn(ModelTypeSet disabled_types,
                                       ModelTypeSet types_to_journal,
                                       ModelTypeSet types_to_unapply) {
  disabled_types.RemoveAll(ProxyTypes());
  if (disabled_types.Empty())
    return;

  WriteTransaction trans(FROM_HERE, PURGE_ENTRIES, this);

  {
    ScopedKernelLock lock(this);

    bool found_progress = false;
    for (ModelType type : disabled_types) {
      if (!kernel_->persisted_info.HasEmptyDownloadProgress(type))
        found_progress = true;
    }

    // If none of the disabled types have progress markers, there's nothing to
    // purge.
    if (!found_progress)
      return;

    for (auto it = kernel_->metahandles_map.begin();
         it != kernel_->metahandles_map.end();) {
      EntryKernel* entry = it->second.get();
      const sync_pb::EntitySpecifics& local_specifics = entry->ref(SPECIFICS);
      const sync_pb::EntitySpecifics& server_specifics =
          entry->ref(SERVER_SPECIFICS);
      ModelType local_type = GetModelTypeFromSpecifics(local_specifics);
      ModelType server_type = GetModelTypeFromSpecifics(server_specifics);

      // Increment the iterator before (potentially) calling DeleteEntry,
      // otherwise our iterator may be invalidated.
      ++it;

      if ((IsRealDataType(local_type) && disabled_types.Has(local_type)) ||
          (IsRealDataType(server_type) && disabled_types.Has(server_type))) {
        if (types_to_unapply.Has(local_type) ||
            types_to_unapply.Has(server_type)) {
          UnapplyEntry(entry);
        } else {
          DeleteEntry(lock, entry);
        }
      }
    }

    // Ensure meta tracking for these data types reflects the purged state.
    for (ModelType type : disabled_types) {
      kernel_->persisted_info.transaction_version[type] = 0;

      // Don't discard progress markers or context for unapplied types.
      if (!types_to_unapply.Has(type)) {
        kernel_->persisted_info.ResetDownloadProgress(type);
        kernel_->persisted_info.datatype_context[type].Clear();
      }
    }

    kernel_->info_status = KERNEL_SHARE_INFO_DIRTY;
  }
}

bool Directory::ResetVersionsForType(BaseWriteTransaction* trans,
                                     ModelType type) {
  if (!ProtocolTypes().Has(type))
    return false;
  DCHECK_NE(type, BOOKMARKS) << "Only non-hierarchical types are supported";

  EntryKernel* type_root = GetEntryByServerTag(ModelTypeToRootTag(type));
  if (!type_root)
    return false;

  ScopedKernelLock lock(this);
  const Id& type_root_id = type_root->ref(ID);
  Directory::Metahandles children;
  AppendChildHandles(lock, type_root_id, &children);

  for (auto it = children.begin(); it != children.end(); ++it) {
    EntryKernel* entry = GetEntryByHandle(lock, *it);
    if (!entry)
      continue;
    if (entry->ref(BASE_VERSION) > 1)
      entry->put(BASE_VERSION, 1);
    if (entry->ref(SERVER_VERSION) > 1)
      entry->put(SERVER_VERSION, 1);

    // Note that we do not unset IS_UNSYNCED or IS_UNAPPLIED_UPDATE in order
    // to ensure no in-transit data is lost.

    entry->mark_dirty(&kernel_->dirty_metahandles);
  }

  return true;
}

void Directory::HandleSaveChangesFailure(const SaveChangesSnapshot& snapshot) {
  WriteTransaction trans(FROM_HERE, HANDLE_SAVE_FAILURE, this);
  ScopedKernelLock lock(this);
  kernel_->info_status = KERNEL_SHARE_INFO_DIRTY;

  // Because we optimistically cleared the dirty bit on the real entries when
  // taking the snapshot, we must restore it on failure.  Not doing this could
  // cause lost data, if no other changes are made to the in-memory entries
  // that would cause the dirty bit to get set again. Setting the bit ensures
  // that SaveChanges will at least try again later.
  for (auto i = snapshot.dirty_metas.begin(); i != snapshot.dirty_metas.end();
       ++i) {
    auto found = kernel_->metahandles_map.find((*i)->ref(META_HANDLE));
    if (found != kernel_->metahandles_map.end()) {
      found->second->mark_dirty(&kernel_->dirty_metahandles);
    }
  }

  kernel_->metahandles_to_purge.insert(snapshot.metahandles_to_purge.begin(),
                                       snapshot.metahandles_to_purge.end());
}

void Directory::GetDownloadProgress(
    ModelType model_type,
    sync_pb::DataTypeProgressMarker* value_out) const {
  ScopedKernelLock lock(this);
  return value_out->CopyFrom(
      kernel_->persisted_info.download_progress[model_type]);
}

size_t Directory::GetEntriesCount() const {
  ScopedKernelLock lock(this);
  return kernel_->metahandles_map.size();
}

void Directory::OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd) {
  std::string dump_name_base =
      base::StringPrintf("sync/0x%" PRIXPTR, reinterpret_cast<uintptr_t>(this));

  size_t kernel_memory_usage;
  size_t model_type_entry_count[ModelType::NUM_ENTRIES] = {0};
  {
    using base::trace_event::EstimateMemoryUsage;

    ReadTransaction trans(FROM_HERE, this);
    ScopedKernelLock lock(this);
    kernel_memory_usage =
        EstimateMemoryUsage(kernel_->name) +
        EstimateMemoryUsage(kernel_->metahandles_map) +
        EstimateMemoryUsage(kernel_->ids_map) +
        EstimateMemoryUsage(kernel_->server_tags_map) +
        EstimateMemoryUsage(kernel_->client_tags_map) +
        EstimateMemoryUsage(kernel_->parent_child_index) +
        EstimateMemoryUsage(kernel_->unapplied_update_metahandles) +
        EstimateMemoryUsage(kernel_->unsynced_metahandles) +
        EstimateMemoryUsage(kernel_->dirty_metahandles) +
        EstimateMemoryUsage(kernel_->metahandles_to_purge) +
        EstimateMemoryUsage(kernel_->persisted_info) +
        EstimateMemoryUsage(kernel_->legacy_cache_guid);

    for (const auto& handle_and_kernel : kernel_->metahandles_map) {
      const EntryKernel* kernel = handle_and_kernel.second.get();
      // Counting logic from DirectoryBackingStore::LoadEntries()
      ModelType model_type = kernel->GetModelType();
      if (!IsRealDataType(model_type)) {
        model_type = kernel->GetServerModelType();
      }
      ++model_type_entry_count[model_type];
    }
  }

  // Similar to UploadModelTypeEntryCount()
  for (size_t i = FIRST_REAL_MODEL_TYPE; i != ModelType::NUM_ENTRIES; ++i) {
    ModelType model_type = static_cast<ModelType>(i);
    std::string notification_type;
    if (RealModelTypeToNotificationType(model_type, &notification_type)) {
      std::string dump_name =
          base::StringPrintf("%s/model_type/%s", dump_name_base.c_str(),
                             notification_type.c_str());
      pmd->CreateAllocatorDump(dump_name)->AddScalar(
          base::trace_event::MemoryAllocatorDump::kNameObjectCount,
          base::trace_event::MemoryAllocatorDump::kUnitsObjects,
          model_type_entry_count[i]);
    }
  }

  {
    std::string dump_name =
        base::StringPrintf("%s/kernel", dump_name_base.c_str());

    auto* dump = pmd->CreateAllocatorDump(dump_name);
    dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                    base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                    kernel_memory_usage);
    const char* system_allocator_name =
        base::trace_event::MemoryDumpManager::GetInstance()
            ->system_allocator_pool_name();
    if (system_allocator_name) {
      pmd->AddSuballocation(dump->guid(), system_allocator_name);
    }
  }

  if (store_) {
    std::string dump_name =
        base::StringPrintf("%s/store", dump_name_base.c_str());
    store_->ReportMemoryUsage(pmd, dump_name);
  }
}

// Iterates over entries of |map|, sums memory usage estimate of entries whose
// entry type is |model_type|. Passing owning container will also include memory
// estimate of EntryKernel.
template <typename Container>
size_t EstimateFiteredMapMemoryUsage(const Container& map,
                                     ModelType model_type) {
  using base::trace_event::EstimateMemoryUsage;
  size_t memory_usage = 0;
  for (const auto& kv : map) {
    const ModelType entry_type =
        GetModelTypeFromSpecifics(kv.second->ref(SPECIFICS));
    if (entry_type == model_type) {
      memory_usage += EstimateMemoryUsage(kv);
    }
  }
  return memory_usage;
}

size_t Directory::EstimateMemoryUsageByType(ModelType model_type) {
  using base::trace_event::EstimateMemoryUsage;
  ReadTransaction trans(FROM_HERE, this);
  ScopedKernelLock lock(this);

  size_t memory_usage = 0;
  memory_usage +=
      EstimateFiteredMapMemoryUsage(kernel_->metahandles_map, model_type);
  memory_usage += EstimateFiteredMapMemoryUsage(kernel_->ids_map, model_type);
  memory_usage +=
      EstimateFiteredMapMemoryUsage(kernel_->server_tags_map, model_type);
  memory_usage +=
      EstimateFiteredMapMemoryUsage(kernel_->client_tags_map, model_type);
  memory_usage += EstimateMemoryUsage(
      kernel_->persisted_info.download_progress[model_type]);
  return memory_usage;
}

size_t Directory::CountEntriesByType(ModelType model_type) const {
  ScopedKernelLock lock(this);
  int count = 0;
  for (const auto& handle_and_kernel : kernel_->metahandles_map) {
    EntryKernel* entry = handle_and_kernel.second.get();
    if (GetModelTypeFromSpecifics(entry->ref(SPECIFICS)) != model_type) {
      continue;
    }
    if (SafeToPurgeFromMemory(*entry)) {
      continue;
    }
    ++count;
  }
  return count;
}

void Directory::SetDownloadProgress(
    ModelType model_type,
    const sync_pb::DataTypeProgressMarker& new_progress) {
  ScopedKernelLock lock(this);
  kernel_->persisted_info.download_progress[model_type].CopyFrom(new_progress);
  kernel_->info_status = KERNEL_SHARE_INFO_DIRTY;
}

bool Directory::HasEmptyDownloadProgress(ModelType type) const {
  ScopedKernelLock lock(this);
  return kernel_->persisted_info.HasEmptyDownloadProgress(type);
}

int64_t Directory::GetTransactionVersion(ModelType type) const {
  kernel_->transaction_mutex.AssertAcquired();
  return kernel_->persisted_info.transaction_version[type];
}

void Directory::IncrementTransactionVersion(ModelType type) {
  kernel_->transaction_mutex.AssertAcquired();
  kernel_->persisted_info.transaction_version[type]++;
  kernel_->info_status = KERNEL_SHARE_INFO_DIRTY;
}

void Directory::GetDataTypeContext(BaseTransaction* trans,
                                   ModelType type,
                                   sync_pb::DataTypeContext* context) const {
  ScopedKernelLock lock(this);
  context->CopyFrom(kernel_->persisted_info.datatype_context[type]);
}

void Directory::SetDataTypeContext(BaseWriteTransaction* trans,
                                   ModelType type,
                                   const sync_pb::DataTypeContext& context) {
  ScopedKernelLock lock(this);
  kernel_->persisted_info.datatype_context[type].CopyFrom(context);
  kernel_->info_status = KERNEL_SHARE_INFO_DIRTY;
}

// TODO(stanisc): crbug.com/438313: change these to not rely on the folders.
ModelTypeSet Directory::InitialSyncEndedTypes() {
  syncable::ReadTransaction trans(FROM_HERE, this);
  ModelTypeSet protocol_types = ProtocolTypes();
  ModelTypeSet initial_sync_ended_types;
  for (ModelType type : protocol_types) {
    if (InitialSyncEndedForType(&trans, type)) {
      initial_sync_ended_types.Put(type);
    }
  }
  return initial_sync_ended_types;
}

bool Directory::InitialSyncEndedForType(ModelType type) {
  syncable::ReadTransaction trans(FROM_HERE, this);
  return InitialSyncEndedForType(&trans, type);
}

bool Directory::InitialSyncEndedForType(BaseTransaction* trans,
                                        ModelType type) {
  // True iff the type's root node has been created and changes
  // for the type have been applied at least once.
  Entry root(trans, GET_TYPE_ROOT, type);
  return root.good() && root.GetBaseVersion() != CHANGES_VERSION;
}

void Directory::MarkInitialSyncEndedForType(BaseWriteTransaction* trans,
                                            ModelType type) {
  // If the root folder is downloaded for the server, the root's base version
  // get updated automatically at the end of update cycle when the update gets
  // applied. However if this is a type with client generated root, the root
  // node gets created locally and never goes through the update cycle. In that
  // case its base version has to be explictly changed from CHANGES_VERSION
  // at the end of the initial update cycle to mark the type as downloaded.
  // See Directory::InitialSyncEndedForType
  DCHECK(IsTypeWithClientGeneratedRoot(type));
  ModelNeutralMutableEntry root(trans, GET_TYPE_ROOT, type);

  // Some tests don't bother creating type root. Need to check if the root
  // exists before clearing its base version.
  if (root.good()) {
    DCHECK(!root.GetIsDel());
    if (root.GetBaseVersion() == CHANGES_VERSION)
      root.PutBaseVersion(0);
  }
}

std::string Directory::legacy_store_birthday() const {
  ScopedKernelLock lock(this);
  return kernel_->persisted_info.legacy_store_birthday;
}

void Directory::set_legacy_store_birthday(const string& store_birthday) {
  ScopedKernelLock lock(this);
  if (kernel_->persisted_info.legacy_store_birthday == store_birthday)
    return;
  kernel_->persisted_info.legacy_store_birthday = store_birthday;
  kernel_->info_status = KERNEL_SHARE_INFO_DIRTY;
}

void Directory::set_legacy_bag_of_chips(const string& bag_of_chips) {
  ScopedKernelLock lock(this);
  if (kernel_->persisted_info.legacy_bag_of_chips == bag_of_chips)
    return;
  kernel_->persisted_info.legacy_bag_of_chips = bag_of_chips;
  kernel_->info_status = KERNEL_SHARE_INFO_DIRTY;
}

const string& Directory::cache_guid() const {
  DCHECK(!cache_guid_.empty()) << this;
  return cache_guid_;
}

void Directory::set_cache_guid(const std::string& cache_guid) {
  DCHECK(!cache_guid.empty());
  cache_guid_ = cache_guid;
}

string Directory::legacy_cache_guid() const {
  // No need to lock since nothing ever writes to it after load.
  return kernel_->legacy_cache_guid;
}

NigoriHandler* Directory::GetNigoriHandler() {
  DCHECK(nigori_handler_);
  return nigori_handler_;
}

const Cryptographer* Directory::GetCryptographer(const BaseTransaction* trans) {
  if (!nigori_handler_) {
    // It's possible in some tests, that we have no |nigori_handler_|.
    return nullptr;
  }
  return nigori_handler_->GetCryptographer(trans);
}

void Directory::ReportUnrecoverableError() {
  if (!report_unrecoverable_error_function_.is_null()) {
    report_unrecoverable_error_function_.Run();
  }
}

void Directory::GetAllMetaHandles(BaseTransaction* trans,
                                  MetahandleSet* result) {
  result->clear();
  ScopedKernelLock lock(this);
  for (auto i = kernel_->metahandles_map.begin();
       i != kernel_->metahandles_map.end(); ++i) {
    result->insert(i->first);
  }
}

void Directory::GetUnsyncedMetaHandles(BaseTransaction* trans,
                                       Metahandles* result) {
  result->clear();
  ScopedKernelLock lock(this);
  copy(kernel_->unsynced_metahandles.begin(),
       kernel_->unsynced_metahandles.end(), back_inserter(*result));
}

int64_t Directory::unsynced_entity_count() const {
  ScopedKernelLock lock(this);
  return kernel_->unsynced_metahandles.size();
}

bool Directory::TypeHasUnappliedUpdates(ModelType type) {
  ScopedKernelLock lock(this);
  return !kernel_->unapplied_update_metahandles[type].empty();
}

void Directory::GetUnappliedUpdateMetaHandles(BaseTransaction* trans,
                                              FullModelTypeSet server_types,
                                              std::vector<int64_t>* result) {
  result->clear();
  ScopedKernelLock lock(this);
  for (int i = UNSPECIFIED; i < ModelType::NUM_ENTRIES; ++i) {
    const ModelType type = ModelTypeFromInt(i);
    if (server_types.Has(type)) {
      std::copy(kernel_->unapplied_update_metahandles[type].begin(),
                kernel_->unapplied_update_metahandles[type].end(),
                back_inserter(*result));
    }
  }
}

void Directory::GetMetaHandlesOfType(BaseTransaction* trans,
                                     ModelType type,
                                     std::vector<int64_t>* result) {
  ScopedKernelLock lock(this);
  GetMetaHandlesOfType(lock, trans, type, result);
}

void Directory::GetMetaHandlesOfType(const ScopedKernelLock& lock,
                                     BaseTransaction* trans,
                                     ModelType type,
                                     std::vector<int64_t>* result) {
  result->clear();
  for (const auto& handle_and_kernel : kernel_->metahandles_map) {
    EntryKernel* entry = handle_and_kernel.second.get();
    const ModelType entry_type =
        GetModelTypeFromSpecifics(entry->ref(SPECIFICS));
    if (entry_type == type)
      result->push_back(handle_and_kernel.first);
  }
}

void Directory::CollectMetaHandleCounts(
    std::vector<int>* num_entries_by_type,
    std::vector<int>* num_to_delete_entries_by_type) {
  syncable::ReadTransaction trans(FROM_HERE, this);
  ScopedKernelLock lock(this);

  for (auto it = kernel_->metahandles_map.begin();
       it != kernel_->metahandles_map.end(); ++it) {
    EntryKernel* entry = it->second.get();
    const ModelType type = GetModelTypeFromSpecifics(entry->ref(SPECIFICS));
    (*num_entries_by_type)[type]++;
    if (entry->ref(IS_DEL))
      (*num_to_delete_entries_by_type)[type]++;
  }
}

std::unique_ptr<base::ListValue> Directory::GetNodeDetailsForType(
    BaseTransaction* trans,
    ModelType type) {
  std::unique_ptr<base::ListValue> nodes(new base::ListValue());

  ScopedKernelLock lock(this);
  for (auto it = kernel_->metahandles_map.begin();
       it != kernel_->metahandles_map.end(); ++it) {
    if (GetModelTypeFromSpecifics(it->second->ref(SPECIFICS)) != type) {
      continue;
    }

    EntryKernel* kernel = it->second.get();
    std::unique_ptr<base::DictionaryValue> node(
        kernel->ToValue(GetCryptographer(trans)));

    // Add the position index if appropriate.  This must be done here (and not
    // in EntryKernel) because the EntryKernel does not have access to its
    // siblings.
    if (kernel->ShouldMaintainPosition() && !kernel->ref(IS_DEL)) {
      node->SetInteger("positionIndex", GetPositionIndex(trans, kernel));
    }

    nodes->Append(std::move(node));
  }

  return nodes;
}

bool Directory::CheckInvariantsOnTransactionClose(
    syncable::BaseTransaction* trans,
    const MetahandleSet& modified_handles) {
  // NOTE: The trans may be in the process of being destructed.  Be careful if
  // you wish to call any of its virtual methods.
  switch (invariant_check_level_) {
    case FULL_DB_VERIFICATION: {
      MetahandleSet all_handles;
      GetAllMetaHandles(trans, &all_handles);
      return CheckTreeInvariants(trans, all_handles);
    }
    case VERIFY_CHANGES: {
      return CheckTreeInvariants(trans, modified_handles);
    }
    case OFF: {
      return true;
    }
  }
  NOTREACHED();
  return false;
}

bool Directory::FullyCheckTreeInvariants(syncable::BaseTransaction* trans) {
  MetahandleSet handles;
  GetAllMetaHandles(trans, &handles);
  return CheckTreeInvariants(trans, handles);
}

bool Directory::CheckTreeInvariants(syncable::BaseTransaction* trans,
                                    const MetahandleSet& handles) {
  MetahandleSet::const_iterator i;
  for (i = handles.begin(); i != handles.end(); ++i) {
    int64_t metahandle = *i;
    Entry e(trans, GET_BY_HANDLE, metahandle);
    if (!SyncAssert(e.good(), FROM_HERE, "Entry is bad", trans))
      return false;
    syncable::Id id = e.GetId();
    syncable::Id parentid = e.GetParentId();

    if (id.IsRoot()) {
      if (!SyncAssert(e.GetIsDir(), FROM_HERE, "Entry should be a directory",
                      trans))
        return false;
      if (!SyncAssert(parentid.IsRoot(), FROM_HERE, "Entry should be root",
                      trans))
        return false;
      if (!SyncAssert(!e.GetIsUnsynced(), FROM_HERE, "Entry should be synced",
                      trans))
        return false;
      continue;
    }

    if (!e.GetIsDel()) {
      if (!SyncAssert(id != parentid, FROM_HERE,
                      "Id should be different from parent id.", trans))
        return false;
      if (!SyncAssert(!e.GetNonUniqueName().empty(), FROM_HERE,
                      "Non unique name should not be empty.", trans))
        return false;

      if (!parentid.IsNull()) {
        int safety_count = handles.size() + 1;
        while (!parentid.IsRoot()) {
          Entry parent(trans, GET_BY_ID, parentid);
          if (!SyncAssert(parent.good(), FROM_HERE,
                          "Parent entry is not valid.", trans))
            return false;
          if (handles.end() == handles.find(parent.GetMetahandle()))
            break;  // Skip further checking if parent was unmodified.
          if (!SyncAssert(parent.GetIsDir(), FROM_HERE,
                          "Parent should be a directory", trans))
            return false;
          if (!SyncAssert(!parent.GetIsDel(), FROM_HERE,
                          "Parent should not have been marked for deletion.",
                          trans))
            return false;
          if (!SyncAssert(handles.end() != handles.find(parent.GetMetahandle()),
                          FROM_HERE, "Parent should be in the index.", trans))
            return false;
          parentid = parent.GetParentId();
          if (!SyncAssert(--safety_count > 0, FROM_HERE,
                          "Count should be greater than zero.", trans))
            return false;
        }
      }
    }
    int64_t base_version = e.GetBaseVersion();
    int64_t server_version = e.GetServerVersion();
    bool using_unique_client_tag = !e.GetUniqueClientTag().empty();
    if (CHANGES_VERSION == base_version || 0 == base_version) {
      ModelType model_type = e.GetModelType();
      bool is_client_creatable_type_root_folder =
          parentid.IsRoot() && IsTypeWithClientGeneratedRoot(model_type) &&
          e.GetUniqueServerTag() == ModelTypeToRootTag(model_type);
      if (e.GetIsUnappliedUpdate()) {
        // Must be a new item, or a de-duplicated unique client tag
        // that was created both locally and remotely, or a type root folder
        // that was created both locally and remotely.
        if (!(using_unique_client_tag ||
              is_client_creatable_type_root_folder)) {
          if (!SyncAssert(e.GetIsDel(), FROM_HERE,
                          "The entry should have been deleted.", trans))
            return false;
        }
        // It came from the server, so it must have a server ID.
        if (!SyncAssert(id.ServerKnows(), FROM_HERE,
                        "The id should be from a server.", trans))
          return false;
      } else {
        if (e.GetIsDir()) {
          // TODO(chron): Implement this mode if clients ever need it.
          // For now, you can't combine a client tag and a directory.
          if (!SyncAssert(!using_unique_client_tag, FROM_HERE,
                          "Directory cannot have a client tag.", trans))
            return false;
        }
        if (is_client_creatable_type_root_folder) {
          // This must be a locally created type root folder.
          if (!SyncAssert(
                  !e.GetIsUnsynced(), FROM_HERE,
                  "Locally created type root folders should not be unsynced.",
                  trans))
            return false;

          if (!SyncAssert(
                  !e.GetIsDel(), FROM_HERE,
                  "Locally created type root folders should not be deleted.",
                  trans))
            return false;
        } else {
          // Should be an uncomitted item, or a successfully deleted one.
          if (!e.GetIsDel()) {
            if (!SyncAssert(e.GetIsUnsynced(), FROM_HERE,
                            "The item should be unsynced.", trans))
              return false;
          }
        }
        // If the next check failed, it would imply that an item exists
        // on the server, isn't waiting for application locally, but either
        // is an unsynced create or a successful delete in the local copy.
        // Either way, that's a mismatch.
        if (!SyncAssert(0 == server_version, FROM_HERE,
                        "Server version should be zero.", trans))
          return false;
        // Items that aren't using the unique client tag should have a zero
        // base version only if they have a local ID.  Items with unique client
        // tags are allowed to use the zero base version for undeletion and
        // de-duplication; the unique client tag trumps the server ID.
        if (!using_unique_client_tag) {
          if (!SyncAssert(!id.ServerKnows(), FROM_HERE,
                          "Should be a client only id.", trans))
            return false;
        }
      }
    } else {
      if (!SyncAssert(id.ServerKnows(), FROM_HERE, "Should be a server id.",
                      trans))
        return false;
    }

    // Previously we would assert that locally deleted items that have never
    // been synced must not be sent to the server (IS_UNSYNCED must be false).
    // This is not always true in the case that an item is deleted while the
    // initial commit is in flight. See crbug.com/426865.
  }
  return true;
}

void Directory::SetInvariantCheckLevel(InvariantCheckLevel check_level) {
  invariant_check_level_ = check_level;
}

int64_t Directory::NextMetahandle() {
  ScopedKernelLock lock(this);
  int64_t metahandle = (kernel_->next_metahandle)++;
  return metahandle;
}

// Generates next client ID based on a randomly generated GUID.
Id Directory::NextId() {
  return Id::CreateFromClientString(base::GenerateGUID());
}

bool Directory::HasChildren(BaseTransaction* trans, const Id& id) {
  ScopedKernelLock lock(this);
  return kernel_->parent_child_index.GetChildren(id) != nullptr;
}

Id Directory::GetFirstChildId(BaseTransaction* trans,
                              const EntryKernel* parent) {
  DCHECK(parent);
  DCHECK(parent->ref(IS_DIR));

  ScopedKernelLock lock(this);
  const OrderedChildSet* children =
      kernel_->parent_child_index.GetChildren(parent->ref(ID));

  // We're expected to return root if there are no children.
  if (!children)
    return Id();

  return (*children->begin())->ref(ID);
}

syncable::Id Directory::GetPredecessorId(EntryKernel* e) {
  ScopedKernelLock lock(this);

  DCHECK(ParentChildIndex::ShouldInclude(e));
  const OrderedChildSet* siblings = kernel_->parent_child_index.GetSiblings(e);
  auto i = siblings->find(e);
  DCHECK(i != siblings->end());

  if (i == siblings->begin()) {
    return Id();
  } else {
    i--;
    return (*i)->ref(ID);
  }
}

syncable::Id Directory::GetSuccessorId(EntryKernel* e) {
  ScopedKernelLock lock(this);

  DCHECK(ParentChildIndex::ShouldInclude(e));
  const OrderedChildSet* siblings = kernel_->parent_child_index.GetSiblings(e);
  auto i = siblings->find(e);
  DCHECK(i != siblings->end());

  i++;
  if (i == siblings->end()) {
    return Id();
  } else {
    return (*i)->ref(ID);
  }
}

// TODO(rlarocque): Remove all support for placing ShouldMaintainPosition()
// items as siblings of items that do not maintain postions.  It is required
// only for tests.  See crbug.com/178282.
void Directory::PutPredecessor(EntryKernel* e, EntryKernel* predecessor) {
  DCHECK(!e->ref(IS_DEL));
  if (!e->ShouldMaintainPosition()) {
    DCHECK(!e->ref(UNIQUE_POSITION).IsValid());
    return;
  }
  std::string suffix = e->ref(UNIQUE_BOOKMARK_TAG);
  DCHECK(!suffix.empty());

  // Remove our item from the ParentChildIndex and remember to re-add it later.
  ScopedKernelLock lock(this);
  ScopedParentChildIndexUpdater updater(lock, e, &kernel_->parent_child_index);

  // Note: The ScopedParentChildIndexUpdater will update this set for us as we
  // leave this function.
  const OrderedChildSet* siblings =
      kernel_->parent_child_index.GetChildren(e->ref(PARENT_ID));

  if (!siblings) {
    // This parent currently has no other children.
    DCHECK(predecessor == nullptr);
    UniquePosition pos = UniquePosition::InitialPosition(suffix);
    e->put(UNIQUE_POSITION, pos);
    return;
  }

  if (predecessor == nullptr) {
    // We have at least one sibling, and we're inserting to the left of them.
    UniquePosition successor_pos = (*siblings->begin())->ref(UNIQUE_POSITION);

    UniquePosition pos;
    if (!successor_pos.IsValid()) {
      // If all our successors are of non-positionable types, just create an
      // initial position.  We arbitrarily choose to sort invalid positions to
      // the right of the valid positions.
      //
      // We really shouldn't need to support this.  See TODO above.
      pos = UniquePosition::InitialPosition(suffix);
    } else {
      DCHECK(!siblings->empty());
      pos = UniquePosition::Before(successor_pos, suffix);
    }

    e->put(UNIQUE_POSITION, pos);
    return;
  }

  // We can't support placing an item after an invalid position.  Fortunately,
  // the tests don't exercise this particular case.  We should not support
  // siblings with invalid positions at all.  See TODO above.
  DCHECK(predecessor->ref(UNIQUE_POSITION).IsValid());

  auto neighbour = siblings->find(predecessor);
  DCHECK(neighbour != siblings->end());

  ++neighbour;
  if (neighbour == siblings->end()) {
    // Inserting at the end of the list.
    UniquePosition pos =
        UniquePosition::After(predecessor->ref(UNIQUE_POSITION), suffix);
    e->put(UNIQUE_POSITION, pos);
    return;
  }

  EntryKernel* successor = *neighbour;

  // Another mixed valid and invalid position case.  This one could be supported
  // in theory, but we're trying to deprecate support for siblings with and
  // without valid positions.  See TODO above.
  // Using a release CHECK here because the following UniquePosition::Between
  // call crashes anyway when the position string is empty (see crbug/332371).
  CHECK(successor->ref(UNIQUE_POSITION).IsValid()) << *successor;

  // Finally, the normal case: inserting between two elements.
  UniquePosition pos =
      UniquePosition::Between(predecessor->ref(UNIQUE_POSITION),
                              successor->ref(UNIQUE_POSITION), suffix);
  e->put(UNIQUE_POSITION, pos);
  return;
}

// TODO(rlarocque): Avoid this indirection.  Just return the set.
void Directory::AppendChildHandles(const ScopedKernelLock& lock,
                                   const Id& parent_id,
                                   Directory::Metahandles* result) {
  const OrderedChildSet* children =
      kernel_->parent_child_index.GetChildren(parent_id);
  if (!children)
    return;

  result->reserve(result->size() + children->size());
  for (const EntryKernel* entry : *children) {
    result->push_back(entry->ref(META_HANDLE));
  }
}

void Directory::UnmarkDirtyEntry(WriteTransaction* trans, Entry* entry) {
  DCHECK(trans);
  entry->kernel_->clear_dirty(&kernel_->dirty_metahandles);
}

void Directory::OnCatastrophicError() {
  UMA_HISTOGRAM_BOOLEAN("Sync.DirectoryCatastrophicError", true);
  ReadTransaction trans(FROM_HERE, this);
  OnUnrecoverableError(&trans, FROM_HERE,
                       "Catastrophic error detected, Sync DB is unrecoverable");
}

Directory::Kernel* Directory::kernel() {
  return kernel_.get();
}

const Directory::Kernel* Directory::kernel() const {
  return kernel_.get();
}

}  // namespace syncable
}  // namespace syncer
