// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/visitedlink/browser/partitioned_visitedlink_writer.h"

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/metrics/histogram_functions.h"
#include "base/rand_util.h"
#include "base/trace_event/trace_event.h"
#include "components/visitedlink/browser/visitedlink_delegate.h"
#include "components/visitedlink/browser/visitedlink_event_listener.h"
#include "components/visitedlink/core/visited_link.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/schemeful_site.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"
#include "third_party/blink/public/common/features.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. NOTE: Please also keep in line with
// components/visitedlink/browser/visitedlink_writer.cc:AddFingerprint.
//
// LINT.IfChange(AddFingerprint)
enum class AddFingerprint {
  kNewVisit = 0,
  kAlreadyVisited = 1,
  kTableError = 2,
  kMaxValue = kTableError,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/history/enums.xml:AddFingerprint)

}  // namespace

namespace visitedlink {

// This value should also be the same as the smallest size in the lookup
// table in NewTableSizeForCount (prime number).
const unsigned PartitionedVisitedLinkWriter::kDefaultTableSize = 16381;

bool PartitionedVisitedLinkWriter::fail_table_creation_for_testing_ = false;

// TableBuilder ---------------------------------------------------------------

// How building from history works
// ---------------------------------
//
// We mark that we're building our hashtable from history by setting the
// table_builder_ member in PartitionedVisitedLinkWriter to the TableBuilder we
// create. This builder will be called on the history (DB) thread by the history
// system for every partition key in the VisitedLinkDatabase.
//
// The builder will store the fingerprints for those links, as well as the
// origin salts used to calculate those fingerprints, and then marshalls
// back to the main (UI) thread where the PartitionedVisitedLinkWriter will be
// notified. The writer then replaces its empty table with a new table
// containing the computed fingerprints. The map of origin salts is copied into
// `salts_` and the UI thread is allowed to get or add to the map itself.
//
// The builder must remain active while the history system is using it. If the
// WeakPtr to the PartitionedVisitedLinkWriter is severed during table build, no
// callback will be executed once we are marshalled back to the UI thread.
class PartitionedVisitedLinkWriter::TableBuilder
    : public VisitedLinkDelegate::VisitedLinkEnumerator {
 public:
  explicit TableBuilder(base::WeakPtr<PartitionedVisitedLinkWriter> writer);

  TableBuilder(const TableBuilder&) = delete;
  TableBuilder& operator=(const TableBuilder&) = delete;

  // VisitedLinkDelegate::VisitedLinkEnumerator overrides.
  void OnVisitedLink(const GURL& link_url,
                     const net::SchemefulSite& top_level_site,
                     const url::Origin& frame_origin) override;
  void OnVisitedLinkComplete(bool success) override;

 private:
  ~TableBuilder() override = default;

  // When building the partitioned hashtable, we need to get or add <origin,
  // salt> pairs to our salts map, as these salts will be used in generating the
  // fingerprints stored in the hashtable. However, to avoid threading
  // discrepancies, the UI thread should not be able to access or alter the salt
  // map while the table is building on the DB thread.
  //
  // As a result, we keep a local copy of our salt map (`local_salts_`) in
  // TableBuilder, and call this function to get from it or add to it. Once we
  // return to the UI thread, we will copy `local_salts_` to
  // PartitionedVisitedLinkWriter's `salts_` and allow the UI thread access.
  uint64_t GetOrAddLocalOriginSalt(const url::Origin& origin);

  // OnComplete mashals to this function on the main (UI) thread to do the
  // notification.
  void OnCompleteMainThread();

  // Owner of this object. MAY ONLY BE ACCESSED ON THE MAIN (UI) THREAD!
  base::WeakPtr<PartitionedVisitedLinkWriter> writer_;

  // Indicates whether or not table building has failed.
  bool success_ = true;

  // Stores the fingerprints we computed on the background thread.
  VisitedLinkCommon::Fingerprints fingerprints_;

  // Stores the salts we computed on the background thread. See
  // GetOrAddLocalOriginSalt() above for more details.
  std::map<url::Origin, uint64_t> local_salts_;
};

// TableBuilder ----------------------------------------------------

PartitionedVisitedLinkWriter::TableBuilder::TableBuilder(
    base::WeakPtr<PartitionedVisitedLinkWriter> writer)
    : writer_(std::move(writer)) {}

void PartitionedVisitedLinkWriter::TableBuilder::OnVisitedLink(
    const GURL& link_url,
    const net::SchemefulSite& top_level_site,
    const url::Origin& frame_origin) {
  // We only want to store valid visited links in the partitioned hashtable.
  // Otherwise we cannot determine if they are visited in the renderer.
  VisitedLink link = {link_url, top_level_site, frame_origin};
  if (!link.IsValid()) {
    return;
  }
  // Attempt to add this visited link to the partitioned hashtable.
  const uint64_t salt = GetOrAddLocalOriginSalt(frame_origin);
  fingerprints_.push_back(
      VisitedLinkWriter::ComputePartitionedFingerprint(link, salt));

  // Attempt to add the self-link version of this visited links to the
  // partitioned hashtable if the feature is enabled.
  if (base::FeatureList::IsEnabled(
          blink::features::kPartitionVisitedLinkDatabaseWithSelfLinks)) {
    std::optional<VisitedLink> self_link = link.MaybeCreateSelfLink();
    if (self_link.has_value()) {
      const uint64_t self_salt =
          GetOrAddLocalOriginSalt(self_link->frame_origin);
      fingerprints_.push_back(VisitedLinkWriter::ComputePartitionedFingerprint(
          self_link.value(), self_salt));
    }
  }
}

// NOTE: in prod, this function should not be called on the UI thread.
void PartitionedVisitedLinkWriter::TableBuilder::OnVisitedLinkComplete(
    bool success) {
  success_ = success;
  DLOG_IF(WARNING, !success)
      << "Unable to build visited links hashtable from VisitedLinkDatabase";
  // Marshall to the main thread where we can access writer_ and notify it
  // of the results.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &PartitionedVisitedLinkWriter::TableBuilder::OnCompleteMainThread,
          this));
}

uint64_t PartitionedVisitedLinkWriter::TableBuilder::GetOrAddLocalOriginSalt(
    const url::Origin& origin) {
  // Obtain the salt for this origin if it already exists.
  auto it = local_salts_.find(origin);
  if (it != local_salts_.end()) {
    return it->second;
  }
  // Otherwise, generate a new salt for this origin.
  const uint64_t generated_salt = base::RandUint64();
  local_salts_.insert({origin, generated_salt});
  return generated_salt;
}

void PartitionedVisitedLinkWriter::TableBuilder::OnCompleteMainThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (writer_) {
    // Marshal to the main thread with the resulting fingerprints and salts.
    writer_->OnTableBuildComplete(success_, fingerprints_,
                                  std::move(local_salts_));
  }
}

// PartitionedVisitedLinkWriter
// ----------------------------------------------------------
PartitionedVisitedLinkWriter::PartitionedVisitedLinkWriter(
    content::BrowserContext* browser_context,
    VisitedLinkDelegate* delegate)
    : browser_context_(browser_context),
      delegate_(delegate),
      listener_(
          std::make_unique<VisitedLinkEventListener>(browser_context, this)) {}

PartitionedVisitedLinkWriter::PartitionedVisitedLinkWriter(
    std::unique_ptr<Listener> listener,
    VisitedLinkDelegate* delegate,
    bool suppress_build,
    int32_t default_table_size)
    : delegate_(delegate),
      listener_(std::move(listener)),
      suppress_build_(suppress_build),
      table_size_override_(default_table_size) {
  DCHECK(listener_);
}

PartitionedVisitedLinkWriter::~PartitionedVisitedLinkWriter() = default;

bool PartitionedVisitedLinkWriter::Init() {
  TRACE_EVENT0("browser", "PartitionedVisitedLinkWriter::Init");
  // Create a temporary table in mapped_table_memory_ full of null hashes. While
  // we build the table from history on the DB thread, this temporary
  // table will be available to query on the UI thread.
  if (!CreateVisitedLinkTable(DefaultTableSize())) {
    return false;
  }

  // When enabled in unit tests, prevents building from the VisitedLinkDatabase.
  // Resulting hashtable is of size `DefaultTableSize()` but empty.
  if (suppress_build_) {
    return true;
  }

  // Send the temporary table to the renderer processes via `listener_`
  if (mapped_table_memory_.region.IsValid()) {
    listener_->NewTable(&mapped_table_memory_.region);
  }

#ifndef NDEBUG
  DebugValidate();
#endif

  return BuildTableFromDelegate();
}

// Initializes the shared memory structure.
bool PartitionedVisitedLinkWriter::CreateVisitedLinkTable(int32_t num_entries) {
  base::MappedReadOnlyRegion table_memory;
  if (!PartitionedVisitedLinkWriter::fail_table_creation_for_testing_ &&
      CreateVisitedLinkTableHelper(num_entries, &table_memory)) {
    mapped_table_memory_ = std::move(table_memory);
    hash_table_ = GetHashTableFromMapping(mapped_table_memory_.mapping);
    table_length_ = num_entries;
    used_items_ = 0;
    return true;
  }
  return false;
}

// static
bool PartitionedVisitedLinkWriter::CreateVisitedLinkTableHelper(
    int32_t num_entries,
    base::MappedReadOnlyRegion* memory) {
  DCHECK(memory);

  // The hashtable is a shared header followed by the entries.
  uint32_t alloc_size =
      num_entries * sizeof(Fingerprint) + sizeof(PartitionedSharedHeader);
  base::UmaHistogramCustomCounts(
      "History.VisitedLinks.HashTableSizeOnTableCreate",
      alloc_size / 1024 / 1024, 1, 10000, 100);

  // Create the shared memory object.
  *memory = base::ReadOnlySharedMemoryRegion::Create(alloc_size);
  if (!memory->IsValid()) {
    return false;
  }

  memset(memory->mapping.memory(), 0, alloc_size);

  // Save the header for other processes to read.
  PartitionedSharedHeader* header =
      static_cast<PartitionedSharedHeader*>(memory->mapping.memory());
  header->length = num_entries;
  return true;
}

bool PartitionedVisitedLinkWriter::ResizeTableIfNecessary() {
  DCHECK(table_length_ > 0) << "Must have a table";

  // Load limits for good performance/space. We are pretty conservative about
  // keeping the table not very full. This is because we use linear probing
  // which increases the likelihood of clumps of entries which will reduce
  // performance.
  const float max_table_load = 0.5f;  // Grow when we're > this full.
  const float min_table_load = 0.2f;  // Shrink when we're < this full.

  float load = ComputeTableLoad();
  if (load < max_table_load &&
      (table_length_ <= static_cast<float>(kDefaultTableSize) ||
       load > min_table_load)) {
    return false;
  }

  // Table needs to grow or shrink.
  int new_size = NewTableSizeForCount(used_items_);
  DCHECK(new_size > used_items_);
  DCHECK(load <= min_table_load || new_size > table_length_);
  ResizeTable(new_size);
  return true;
}

void PartitionedVisitedLinkWriter::ResizeTable(int32_t new_size) {
  DCHECK(mapped_table_memory_.region.IsValid() &&
         mapped_table_memory_.mapping.IsValid());

#ifndef NDEBUG
  DebugValidate();
#endif

  auto old_hash_table_mapping = std::move(mapped_table_memory_.mapping);
  int32_t old_table_length = table_length_;
  if (!CreateVisitedLinkTable(new_size)) {
    // Restore modified members.
    mapped_table_memory_.mapping = std::move(old_hash_table_mapping);
    return;
  }

  {
    Fingerprint* old_hash_table =
        GetHashTableFromMapping(old_hash_table_mapping);
    // Now we have two tables, our local copy which is the old one, and the new
    // one loaded into this object where we need to copy the data.
    for (int32_t i = 0; i < old_table_length; i++) {
      Fingerprint cur = old_hash_table[i];
      if (cur) {
        AddFingerprint(cur, false);
      }
    }
  }
  // Send an update notification to all child processes so they read the new
  // table.
  listener_->NewTable(&mapped_table_memory_.region);

#ifndef NDEBUG
  DebugValidate();
#endif
}

// See the TableBuilder definition in the header file for how this works.
bool PartitionedVisitedLinkWriter::BuildTableFromDelegate() {
  DCHECK(!table_builder_);

  table_builder_ =
      base::MakeRefCounted<TableBuilder>(weak_ptr_factory_.GetWeakPtr());
  delegate_->BuildVisitedLinkTable(table_builder_);
  return true;
}

// NOTE: Keep VisitedLinkCommon::IsVisited in sync with this algorithm!
VisitedLinkWriter::Hash PartitionedVisitedLinkWriter::AddFingerprint(
    Fingerprint fingerprint,
    bool send_notifications) {
  if (!hash_table_ || table_length_ == 0) {
    base::UmaHistogramEnumeration("History.VisitedLinks.TryToAddFingerprint",
                                  AddFingerprint::kTableError);
    NOTREACHED_IN_MIGRATION();  // Not initialized.
    return null_hash_;
  }

  Hash cur_hash = HashFingerprint(fingerprint);
  Hash first_hash = cur_hash;
  while (true) {
    Fingerprint cur_fingerprint = FingerprintAt(cur_hash);
    if (cur_fingerprint == fingerprint) {
      base::UmaHistogramEnumeration("History.VisitedLinks.TryToAddFingerprint",
                                    AddFingerprint::kAlreadyVisited);
      return null_hash_;  // This fingerprint is already in there, do nothing.
    }

    if (cur_fingerprint == null_fingerprint_) {
      // End of probe sequence found, insert here.
      hash_table_[cur_hash] = fingerprint;
      used_items_++;
      // If allowed, notify listener that a new visited link was added.
      if (send_notifications) {
        base::UmaHistogramEnumeration(
            "History.VisitedLinks.TryToAddFingerprint",
            AddFingerprint::kNewVisit);
        listener_->Add(fingerprint);
      }
      return cur_hash;
    }

    // Advance in the probe sequence.
    cur_hash = IncrementHash(cur_hash);
    if (cur_hash == first_hash) {
      // This means that we've wrapped around and are about to go into an
      // infinite loop. Something was wrong with the hashtable resizing
      // logic, so stop here.
      base::UmaHistogramEnumeration("History.VisitedLinks.TryToAddFingerprint",
                                    AddFingerprint::kTableError);
      NOTREACHED_IN_MIGRATION();
      return null_hash_;
    }
  }
}

void PartitionedVisitedLinkWriter::DeleteFingerprintsFromCurrentTable(
    const std::set<Fingerprint>& fingerprints) {
  // Delete the Fingerprints from the table.
  for (auto i = fingerprints.begin(); i != fingerprints.end(); ++i) {
    DeleteFingerprint(*i);
  }

  // These deleted fingerprints may make us shrink the table.
  ResizeTableIfNecessary();
}

bool PartitionedVisitedLinkWriter::DeleteFingerprint(Fingerprint fingerprint) {
  if (!hash_table_ || table_length_ == 0) {
    NOTREACHED_IN_MIGRATION();  // Not initialized.
    return false;
  }
  if (!IsVisited(fingerprint)) {
    return false;  // Not in the database to delete.
  }

  // First update the header used count.
  used_items_--;
  Hash deleted_hash = HashFingerprint(fingerprint);

  // Find the range of "stuff" in the hash table that is adjacent to this
  // fingerprint. These are things that could be affected by the change in
  // the hash table. Since we use linear probing, anything after the deleted
  // item up until an empty item could be affected.
  Hash end_range = deleted_hash;
  while (true) {
    Hash next_hash = IncrementHash(end_range);
    if (next_hash == deleted_hash) {
      break;  // We wrapped around and the whole table is full.
    }
    if (!hash_table_[next_hash]) {
      break;  // Found the last spot.
    }
    end_range = next_hash;
  }

  // We could get all fancy and move the affected fingerprints around, but
  // instead we just remove them all and re-add them (minus our deleted one).
  // This will mean there's a small window of time where the affected links
  // won't be marked visited.
  absl::InlinedVector<Fingerprint, 32> shuffled_fingerprints;
  Hash stop_loop = IncrementHash(end_range);  // The end range is inclusive.
  for (Hash i = deleted_hash; i != stop_loop; i = IncrementHash(i)) {
    if (hash_table_[i] != fingerprint) {
      // Don't save the one we're deleting!
      shuffled_fingerprints.push_back(hash_table_[i]);

      // This will balance the increment of this value in AddFingerprint below
      // so there is no net change.
      used_items_--;
    }
    hash_table_[i] = null_fingerprint_;
  }

  if (!shuffled_fingerprints.empty()) {
    // Need to add the new items back.
    for (size_t i = 0; i < shuffled_fingerprints.size(); i++) {
      AddFingerprint(shuffled_fingerprints[i], false);
    }
  }
  return true;
}

// See the TableBuilder declaration above for how this works.
void PartitionedVisitedLinkWriter::OnTableBuildComplete(
    bool success,
    const std::vector<Fingerprint>& fingerprints,
    std::map<url::Origin, uint64_t> salts) {
  table_builder_ = nullptr;  // Will release our reference to the builder.

  if (success) {
    // Replace salts_ with the map created when we built the hashtable on the DB
    // thread.
    salts_ = std::move(salts);

    // Generate space for the new table in shared memory.
    int new_table_size = NewTableSizeForCount(
        static_cast<int>(fingerprints.size() + added_during_build_.size()));
    if (CreateVisitedLinkTable(new_table_size)) {
      // Add the stored fingerprints to the hash table.
      for (const auto& fingerprint : fingerprints) {
        AddFingerprint(fingerprint, false);
      }

      // Also add anything that was added while we were asynchronously
      // generating the new table.
      for (const auto& link : added_during_build_) {
        CHECK(link.IsValid());
        const std::optional<uint64_t> salt =
            GetOrAddOriginSalt(link.frame_origin);
        CHECK(salt.has_value());
        AddFingerprint(ComputePartitionedFingerprint(link, salt.value()),
                       false);
      }
      added_during_build_.clear();

      // Now handle deletions. Do not shrink the table now, we'll shrink it when
      // adding or deleting a visited link the next time.
      for (const auto& link : deleted_during_build_) {
        CHECK(link.IsValid());
        const std::optional<uint64_t> salt =
            GetOrAddOriginSalt(link.frame_origin);
        CHECK(salt.has_value());
        DeleteFingerprint(ComputePartitionedFingerprint(link, salt.value()));
      }
      deleted_during_build_.clear();

      // Send an update notification to all child processes.
      listener_->NewTable(&mapped_table_memory_.region);
      // All tabs which was loaded when table was being rebuilt
      // invalidate their links again.
      listener_->Reset(false);
    }
  }

  // Now that we have completed our build on the DB thread, we can recover the
  // per-origin salts of navigations that took place during the build.
  listener_->UpdateOriginSalts();

  // Notify the unit test that the build is complete (will be NULL in prod.)
  if (!build_complete_task_.is_null()) {
    std::move(build_complete_task_).Run();
  }
}

uint32_t PartitionedVisitedLinkWriter::DefaultTableSize() const {
  if (table_size_override_) {
    return table_size_override_;
  }
  return kDefaultTableSize;
}

uint32_t PartitionedVisitedLinkWriter::NewTableSizeForCount(
    int32_t item_count) const {
  // These table sizes are selected to be the maximum prime number less than
  // a "convenient" multiple of 1K.
  static const int table_sizes[] = {
      16381,      // 16K  = 16384   <- don't shrink below this table size
                  //                   (should be == default_table_size)
      32767,      // 32K  = 32768
      65521,      // 64K  = 65536
      130051,     // 128K = 131072
      262127,     // 256K = 262144
      524269,     // 512K = 524288
      1048549,    // 1M   = 1048576
      2097143,    // 2M   = 2097152
      4194301,    // 4M   = 4194304
      8388571,    // 8M   = 8388608
      16777199,   // 16M  = 16777216
      33554347};  // 32M  = 33554432

  // Try to leave the table 33% full.
  int desired = item_count * 3;

  // Find the closest prime.
  for (size_t i = 0; i < std::size(table_sizes); i++) {
    if (table_sizes[i] > desired) {
      return table_sizes[i];
    }
  }

  // Growing very big, just approximate a "good" number, not growing as much
  // as normal.
  return item_count * 2 - 1;
}

void PartitionedVisitedLinkWriter::AddVisitedLink(const VisitedLink& link) {
  TRACE_EVENT0("browser", "PartitionedVisitedLinkWriter::AddVisitedLink");
  base::UmaHistogramCounts10M("History.VisitedLinks.HashTableUsageOnLinkAdded",
                              used_items_);
  // Attempt to add the visited link to the in-memory partitioned hashtable and
  // record whether we returned a valid hash index.
  bool did_add_link = (TryToAddVisitedLink(link) != null_hash_);

  // When kPartitionVisitedLinkDatabaseWithSelfLinks is enabled, we attempt to
  // add <link_url, link_url, link_url> to the in-memory partitioned hashtable
  // as well.
  bool did_add_self_link = false;
  if (base::FeatureList::IsEnabled(
          blink::features::kPartitionVisitedLinkDatabaseWithSelfLinks)) {
    std::optional<VisitedLink> self_link = link.MaybeCreateSelfLink();
    if (self_link.has_value()) {
      // Attempt to add the self-link and record whether we returned a valid
      // hash index.
      did_add_self_link =
          (TryToAddVisitedLink(self_link.value()) != null_hash_);
    }
  }

  // If we have added a link and/or a self-link, we need to call
  // ResizeTableIfNecessary() to determine whether we need to increase the
  // available space in the partitioned hashtable. Before doing so, we also
  // check that the table isn't currently building, which would make this
  // operation redundant.
  if (!table_builder_ && (did_add_link || did_add_self_link)) {
    ResizeTableIfNecessary();
  }
}

VisitedLinkWriter::Hash PartitionedVisitedLinkWriter::TryToAddVisitedLink(
    const VisitedLink& link) {
  // Extra check that we are not incognito. This should not happen.
  // TODO(boliu): Move this check to HistoryService when IsOffTheRecord is
  // removed from BrowserContext.
  if (browser_context_ && browser_context_->IsOffTheRecord()) {
    NOTREACHED_IN_MIGRATION();
    return null_hash_;
  }

  // We don't want to add any invalid VisitedLinks to the hashtable.
  if (!link.IsValid()) {
    return null_hash_;
  }

  // If the table isn't finished building, accumulated links will be
  // applied to the table.
  if (table_builder_.get()) {
    // If we have a pending delete for this link, cancel it.
    deleted_during_build_.erase(link);

    // A build is in progress, save this addition in the temporary
    // list so it can be added once build is complete.
    added_during_build_.insert(link);
  }

  const std::optional<uint64_t> salt = GetOrAddOriginSalt(link.frame_origin);
  if (!salt.has_value()) {
    return null_hash_;
  }
  Fingerprint fingerprint = ComputePartitionedFingerprint(link, salt.value());

  // If the table is "full", we don't add URLs and just drop them on the floor.
  // This can happen if we get thousands of new URLs and something causes
  // the table resizing to fail. This check prevents a hang in that case. Note
  // that this is *not* the resize limit, this is just a sanity check.
  if (used_items_ / 8 > table_length_ / 10) {
    return null_hash_;  // Table is more than 80% full.
  }

  return AddFingerprint(fingerprint, true);
}

void PartitionedVisitedLinkWriter::DeleteAllVisitedLinks() {
  // Any pending modifications are invalid.
  added_during_build_.clear();
  deleted_during_build_.clear();

  // Clear the hash table.
  used_items_ = 0;
  memset(hash_table_, 0, this->table_length_ * sizeof(Fingerprint));

  // Resize it if it is now too empty. Resize may write the new table out for
  // us, otherwise, schedule writing the new table to disk ourselves.
  ResizeTableIfNecessary();

  // Notify reader instances that hashtable state has changed.
  listener_->Reset(false);
}

void PartitionedVisitedLinkWriter::DeleteVisitedLinks(
    VisitedLinkIterator* links) {
  if (!links->HasNextVisitedLink()) {
    return;
  }

  // Notify reader instances that hashtable state has changed.
  listener_->Reset(false);

  if (table_builder_.get()) {
    // A build is in progress, save this deletion in the temporary
    // list so it can be deleted once the build is complete.
    while (links->HasNextVisitedLink()) {
      // Obtain the next link we want to delete from the hashtable.
      const VisitedLink& link(links->NextVisitedLink());
      if (!link.IsValid()) {
        continue;
      }
      deleted_during_build_.insert(link);
      // If the VisitedLink  was just added and now we're deleting it, it may be
      // in the list of things added since the last build. Delete it from that
      // list.
      added_during_build_.erase(link);

      // If self-links are enabled, we have added links to the in-memory
      // partitioned hashtable that do not exist in the VisitedLinkDatabase. As
      // a result, we must construct the self-link counterpart to each of these
      // VisitedLinks deleted from the VisitedLinkDatabase, so that both the
      // link and self-link are removed from the partitioned hashtable.
      if (base::FeatureList::IsEnabled(
              blink::features::kPartitionVisitedLinkDatabaseWithSelfLinks)) {
        std::optional<VisitedLink> self_link = link.MaybeCreateSelfLink();
        if (self_link.has_value()) {
          deleted_during_build_.insert(self_link.value());
          added_during_build_.erase(self_link.value());
        }
      }
    }
    return;
  }

  // Compute the deleted URLs' fingerprints and delete them.
  std::set<Fingerprint> deleted_fingerprints;
  while (links->HasNextVisitedLink()) {
    const VisitedLink& link(links->NextVisitedLink());
    if (!link.IsValid()) {
      continue;
    }
    const std::optional<uint64_t> salt = GetOrAddOriginSalt(link.frame_origin);
    if (!salt.has_value()) {
      continue;
    }
    deleted_fingerprints.insert(
        ComputePartitionedFingerprint(link, salt.value()));

    // If self-links are enabled, we have added links to the in-memory
    // partitioned hashtable that do not exist in the VisitedLinkDatabase. As
    // a result, we must construct the self-link counterpart to each of these
    // VisitedLinks deleted from the VisitedLinkDatabase, so that both the
    // link and self-link are removed from the partitioned hashtable.
    if (base::FeatureList::IsEnabled(
            blink::features::kPartitionVisitedLinkDatabaseWithSelfLinks)) {
      std::optional<VisitedLink> self_link = link.MaybeCreateSelfLink();
      if (self_link.has_value()) {
        const std::optional<uint64_t> self_salt =
            GetOrAddOriginSalt(self_link->frame_origin);
        if (!self_salt.has_value()) {
          continue;
        }
        deleted_fingerprints.insert(ComputePartitionedFingerprint(
            self_link.value(), self_salt.value()));
      }
    }
  }
  DeleteFingerprintsFromCurrentTable(deleted_fingerprints);
}

std::optional<uint64_t> PartitionedVisitedLinkWriter::GetOrAddOriginSalt(
    const url::Origin& origin) {
  // To avoid race conditions, we should not get from or add to the salt map
  // while the hashtable is building.
  // TODO(crbug.com/332364003): implement a new VisitedLinkNotificationSink
  // interface to determine and send salts for navigations that take place while
  // the hashtable is building.
  if (table_builder_) {
    return std::nullopt;
  }
  // Obtain the salt for this origin if it already exists.
  auto it = salts_.find(origin);
  if (it != salts_.end()) {
    return it->second;
  }
  // Otherwise, generate a new salt for this origin.
  const uint64_t generated_salt = base::RandUint64();
  salts_.insert({origin, generated_salt});
  return generated_salt;
}

// static
VisitedLinkCommon::Fingerprint*
PartitionedVisitedLinkWriter::GetHashTableFromMapping(
    const base::WritableSharedMemoryMapping& hash_table_mapping) {
  DCHECK(hash_table_mapping.IsValid());
  // Our table pointer is just the data immediately following the header.
  return reinterpret_cast<Fingerprint*>(
      static_cast<char*>(hash_table_mapping.memory()) +
      sizeof(PartitionedSharedHeader));
}

}  // namespace visitedlink
