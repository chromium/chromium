// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/visitedlink/browser/partitioned_visitedlink_writer.h"

#include "base/logging.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/rand_util.h"
#include "components/visitedlink/browser/visitedlink_delegate.h"
#include "components/visitedlink/browser/visitedlink_event_listener.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/schemeful_site.h"
#include "url/gurl.h"
#include "url/origin.h"

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
  if (!link_url.is_valid() || top_level_site.opaque() ||
      frame_origin.opaque()) {
    return;
  }
  // Attempt to add this visited link to the partitioned hashtable.
  const uint64_t salt = GetOrAddLocalOriginSalt(frame_origin);
  fingerprints_.push_back(VisitedLinkWriter::ComputePartitionedFingerprint(
      link_url, top_level_site, frame_origin, salt));
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
    : browser_context_(browser_context), delegate_(delegate) {}

PartitionedVisitedLinkWriter::PartitionedVisitedLinkWriter(
    VisitedLinkDelegate* delegate,
    int32_t default_table_size)
    : delegate_(delegate), table_size_override_(default_table_size) {}

PartitionedVisitedLinkWriter::~PartitionedVisitedLinkWriter() = default;

bool PartitionedVisitedLinkWriter::Init() {
  // Create a temporary table in mapped_table_memory_ full of null hashes. While
  // we build the table from history on the DB thread, this temporary
  // table will be available to query on the UI thread.
  if (!CreateVisitedLinkTable(DefaultTableSize())) {
    return false;
  }

  // TODO(crbug.com/332364003): Notify the listener instance of the new
  // `mapped_table_memory_` region.

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
    NOTREACHED();  // Not initialized.
    return null_hash_;
  }

  Hash cur_hash = HashFingerprint(fingerprint);
  Hash first_hash = cur_hash;
  while (true) {
    Fingerprint cur_fingerprint = FingerprintAt(cur_hash);
    if (cur_fingerprint == fingerprint) {
      return null_hash_;  // This fingerprint is already in there, do nothing.
    }

    if (cur_fingerprint == null_fingerprint_) {
      // End of probe sequence found, insert here.
      hash_table_[cur_hash] = fingerprint;
      used_items_++;
      // TODO(crbug.com/332364003): if `send_notifications` is true, we would
      // alert the listener about the added fingerprint here.
      return cur_hash;
    }

    // Advance in the probe sequence.
    cur_hash = IncrementHash(cur_hash);
    if (cur_hash == first_hash) {
      // This means that we've wrapped around and are about to go into an
      // infinite loop. Something was wrong with the hashtable resizing
      // logic, so stop here.
      NOTREACHED();
      return null_hash_;
    }
  }
}

bool PartitionedVisitedLinkWriter::DeleteFingerprint(Fingerprint fingerprint) {
  if (!hash_table_ || table_length_ == 0) {
    NOTREACHED();  // Not initialized.
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

      // TODO(crbug.com/41483930): Implement support for adding and deleting
      // visited links from the partitioned hashtable; specifically populate
      // `added_during_build_` and `deleted_during_build`.
      //
      // Also add anything that was added while we were asynchronously
      // generating the new table.
      for (const auto& fingerprint : added_during_build_) {
        AddFingerprint(fingerprint, false);
      }
      added_during_build_.clear();

      // Now handle deletions. Do not shrink the table now, we'll shrink it when
      // adding or deleting an url the next time.
      for (const auto& fingerprint : deleted_during_build_) {
        DeleteFingerprint(fingerprint);
      }
      deleted_during_build_.clear();

      // TODO(crbug.com/332364003): Notify the listener of the new hashtable
      // and ask the VisitedLinkReaders to reset their links.
    }
  }
  table_builder_ = nullptr;  // Will release our reference to the builder.

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
