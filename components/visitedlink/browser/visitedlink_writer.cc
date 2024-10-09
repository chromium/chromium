// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/visitedlink/browser/visitedlink_writer.h"

#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <memory>
#include <string_view>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_macros.h"
#include "base/rand_util.h"
#include "base/strings/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/visitedlink/browser/visitedlink_delegate.h"
#include "components/visitedlink/browser/visitedlink_event_listener.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <io.h>
#include <shlobj.h>
#endif  // BUILDFLAG(IS_WIN)

using content::BrowserThread;

namespace visitedlink {

const int32_t VisitedLinkWriter::kFileHeaderSignatureOffset = 0;
const int32_t VisitedLinkWriter::kFileHeaderVersionOffset = 4;
const int32_t VisitedLinkWriter::kFileHeaderLengthOffset = 8;
const int32_t VisitedLinkWriter::kFileHeaderUsedOffset = 12;
const int32_t VisitedLinkWriter::kFileHeaderSaltOffset = 16;

const int32_t VisitedLinkWriter::kFileCurrentVersion = 3;

// the signature at the beginning of the URL table = "VLnk" (visited links)
const int32_t VisitedLinkWriter::kFileSignature = 0x6b6e4c56;
const size_t VisitedLinkWriter::kFileHeaderSize =
    kFileHeaderSaltOffset + LINK_SALT_LENGTH;

// This value should also be the same as the smallest size in the lookup
// table in NewTableSizeForCount (prime number).
const unsigned VisitedLinkWriter::kDefaultTableSize = 16381;

bool VisitedLinkWriter::fail_table_creation_for_testing_ = false;

namespace {

// Fills the given salt structure with some quasi-random values
// It is not necessary to generate a cryptographically strong random string,
// only that it be reasonably different for different users.
void GenerateSalt(uint8_t (&salt)[LINK_SALT_LENGTH]) {
  uint64_t randval = base::RandUint64();
  static_assert(sizeof(salt) == sizeof(randval), "Salt size mismatch");
  memcpy(salt, &randval, sizeof(salt));
}

// Opens file on a background thread to not block UI thread.
void AsyncOpen(base::ScopedFILE* file, const base::FilePath& filename) {
  DCHECK(file);
  DCHECK(!*file);
  file->reset(base::OpenFile(filename, "wb+"));
  DLOG_IF(ERROR, !*file) << "Failed to open file " << filename.value();
}

// Returns true if the write was complete.
static bool WriteToFile(FILE* file,
                        off_t offset,
                        const void* data,
                        size_t data_len) {
  if (fseek(file, offset, SEEK_SET) != 0)
    return false;  // Don't write to an invalid part of the file.

  size_t num_written = fwrite(data, 1, data_len, file);

  // The write may not make it to the kernel (stdlib may buffer the write)
  // until the next fseek/fclose call.  If we crash, it's easy for our used
  // item count to be out of sync with the number of hashes we write.
  // Protect against this by calling fflush.
  int ret = fflush(file);
  DCHECK_EQ(0, ret);
  return num_written == data_len;
}

// This task executes on a background thread and executes a write. This
// prevents us from blocking the UI thread doing I/O. This is ignored if the
// previous call to AsyncOpen() failed to open the file.
void AsyncWrite(base::ScopedFILE* file,
                int32_t offset,
                const std::string& data) {
  DCHECK(file);
  if (*file) {
    WriteToFile(file->get(), offset, data.data(), data.size());
  }
}

// Truncates the file to the current position asynchronously on a background
// thread. This is ignored if the previous call to AsyncOpen() failed to open
// the file.
void AsyncTruncate(base::ScopedFILE* file) {
  DCHECK(file);
  if (*file) {
    base::IgnoreResult(base::TruncateFile(file->get()));
  }
}

// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. NOTE: Please also keep in line with
// components/visitedlink/browser/partitioned_visitedlink_writer.cc:
// AddFingerprint.
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

struct VisitedLinkWriter::LoadFromFileResult
    : public base::RefCountedThreadSafe<LoadFromFileResult> {
  LoadFromFileResult(base::ScopedFILE file,
                     base::MappedReadOnlyRegion hash_table_memory,
                     int32_t num_entries,
                     int32_t used_count,
                     uint8_t salt[LINK_SALT_LENGTH]);

  LoadFromFileResult(const LoadFromFileResult&) = delete;
  LoadFromFileResult& operator=(const LoadFromFileResult&) = delete;

  base::ScopedFILE file;
  base::MappedReadOnlyRegion hash_table_memory;
  int32_t num_entries;
  int32_t used_count;
  uint8_t salt[LINK_SALT_LENGTH];

 private:
  friend class base::RefCountedThreadSafe<LoadFromFileResult>;
  virtual ~LoadFromFileResult();
};

VisitedLinkWriter::LoadFromFileResult::LoadFromFileResult(
    base::ScopedFILE file,
    base::MappedReadOnlyRegion hash_table_memory,
    int32_t num_entries,
    int32_t used_count,
    uint8_t salt[LINK_SALT_LENGTH])
    : file(std::move(file)),
      hash_table_memory(std::move(hash_table_memory)),
      num_entries(num_entries),
      used_count(used_count) {
  memcpy(this->salt, salt, LINK_SALT_LENGTH);
}

VisitedLinkWriter::LoadFromFileResult::~LoadFromFileResult() {}

// TableBuilder ---------------------------------------------------------------

// How rebuilding from history works
// ---------------------------------
//
// We mark that we're rebuilding from history by setting the table_builder_
// member in VisitedLinkWriter to the TableBuilder we create. This builder
// will be called on the history thread by the history system for every URL
// in the database.
//
// The builder will store the fingerprints for those URLs, and then marshalls
// back to the main thread where the VisitedLinkWriter will be notified. The
// writer then replaces its table with a new table containing the computed
// fingerprints.
//
// The builder must remain active while the history system is using it.
// Sometimes, the writer will be deleted before the rebuild is complete, in
// which case it notifies the builder via DisownWriter(). The builder will
// delete itself once rebuilding is complete, and not execute any callback.
class VisitedLinkWriter::TableBuilder
    : public VisitedLinkDelegate::URLEnumerator {
 public:
  TableBuilder(VisitedLinkWriter* writer, const uint8_t salt[LINK_SALT_LENGTH]);

  TableBuilder(const TableBuilder&) = delete;
  TableBuilder& operator=(const TableBuilder&) = delete;

  // Called on the main thread when the writer is being destroyed. This will
  // prevent a crash when the query completes and the writer is no longer
  // around. We can not actually do anything but mark this fact, since the
  // table will be being rebuilt simultaneously on the other thread.
  void DisownWriter();

  // VisitedLinkDelegate::URLEnumerator
  void OnURL(const GURL& url) override;
  void OnComplete(bool succeed) override;

 private:
  ~TableBuilder() override = default;

  // OnComplete mashals to this function on the main thread to do the
  // notification.
  void OnCompleteMainThread();

  // Owner of this object. MAY ONLY BE ACCESSED ON THE MAIN THREAD!
  raw_ptr<VisitedLinkWriter, FlakyDanglingUntriaged> writer_;

  // Indicates whether the operation has failed or not.
  bool success_;

  // Salt for this new table.
  uint8_t salt_[LINK_SALT_LENGTH];

  // Stores the fingerprints we computed on the background thread.
  VisitedLinkCommon::Fingerprints fingerprints_;
};

// VisitedLinkWriter ----------------------------------------------------------

VisitedLinkWriter::VisitedLinkWriter(content::BrowserContext* browser_context,
                                     VisitedLinkDelegate* delegate,
                                     bool persist_to_disk)
    : browser_context_(browser_context),
      delegate_(delegate),
      listener_(std::make_unique<VisitedLinkEventListener>(browser_context)),
      persist_to_disk_(persist_to_disk) {}

VisitedLinkWriter::VisitedLinkWriter(Listener* listener,
                                     VisitedLinkDelegate* delegate,
                                     bool persist_to_disk,
                                     bool suppress_rebuild,
                                     const base::FilePath& filename,
                                     int32_t default_table_size)
    : delegate_(delegate),
      listener_(listener),
      persist_to_disk_(persist_to_disk),
      database_name_override_(filename),
      table_size_override_(default_table_size),
      suppress_rebuild_(suppress_rebuild) {
  DCHECK(listener_);
}

VisitedLinkWriter::~VisitedLinkWriter() {
  if (table_builder_) {
    // Prevent the table builder from calling us back now that we're being
    // destroyed. Note that we DON'T delete the object, since the history
    // system is still writing into it. When that is complete, the table
    // builder will destroy itself when it finds we are gone.
    table_builder_->DisownWriter();
  }
  FreeURLTable();
  // FreeURLTable() will schedule closing of the file and deletion of
  // |scoped_file_holder_|. So nothing should be done here.

  if (table_is_loading_from_file_ &&
      (!added_since_load_.empty() || !deleted_since_load_.empty())) {
    // Delete the database file if it exists because we don't have enough time
    // to load the table from the database file and now we have inconsistent
    // state. On the next start table will be rebuilt.
    base::FilePath filename;
    GetDatabaseFileName(&filename);
    PostIOTask(FROM_HERE, base::GetDeleteFileCallback(filename));
  }

  DCHECK(!scoped_file_holder_);  // Must have been moved to the IO thread for
                                 // releasing the file in the correct sequence.
}

bool VisitedLinkWriter::Init() {
  TRACE_EVENT0("browser", "VisitedLinkWriter::Init");
  // Create the temporary table. If the table is rebuilt that temporary table
  // will be became the main table.
  // The salt must be generated before the table so that it can be copied to
  // the shared memory.
  GenerateSalt(salt_);
  if (!CreateURLTable(DefaultTableSize()))
    return false;

  if (mapped_table_memory_.region.IsValid())
    listener_->NewTable(&mapped_table_memory_.region);

#ifndef NDEBUG
  DebugValidate();
#endif

  if (persist_to_disk_) {
    if (InitFromFile())
      return true;
  }
  return InitFromScratch(suppress_rebuild_);
}

void VisitedLinkWriter::AddURL(const GURL& url, bool update_file) {
  TRACE_EVENT0("browser", "VisitedLinkWriter::AddURL");
  UMA_HISTOGRAM_COUNTS_10M("History.VisitedLinks.HashTableUsageOnLinkAdded",
                           used_items_);
  Hash index = TryToAddURL(url);
  if (!table_builder_ && !table_is_loading_from_file_ && index != null_hash_) {
    // Not rebuilding, so we want to keep the file on disk up to date.
    if (update_file && persist_to_disk_) {
      WriteUsedItemCountToFile();
      WriteHashRangeToFile(index, index);
    }
    ResizeTableIfNecessary();
  }
}

VisitedLinkWriter::Hash VisitedLinkWriter::TryToAddURL(const GURL& url) {
  // Extra check that we are not incognito. This should not happen.
  // TODO(boliu): Move this check to HistoryService when IsOffTheRecord is
  // removed from BrowserContext.
  if (browser_context_ && browser_context_->IsOffTheRecord()) {
    NOTREACHED_IN_MIGRATION();
    return null_hash_;
  }

  if (!url.is_valid())
    return null_hash_;  // Don't add invalid URLs.

  Fingerprint fingerprint = ComputeURLFingerprint(url.spec(), salt_);
  // If the table isn't loaded the table will be rebuilt and after
  // that accumulated fingerprints will be applied to the table.
  if (table_builder_.get() || table_is_loading_from_file_) {
    // If we have a pending delete for this fingerprint, cancel it.
    deleted_since_rebuild_.erase(fingerprint);

    // A rebuild or load is in progress, save this addition in the temporary
    // list so it can be added once rebuild is complete.
    added_since_rebuild_.insert(fingerprint);
  }

  if (table_is_loading_from_file_) {
    // If we have a pending delete for this url, cancel it.
    deleted_since_load_.erase(url);

    // The loading is in progress, save this addition in the temporary
    // list so it can be added once the loading is complete.
    added_since_load_.insert(url);
  }

  // If the table is "full", we don't add URLs and just drop them on the floor.
  // This can happen if we get thousands of new URLs and something causes
  // the table resizing to fail. This check prevents a hang in that case. Note
  // that this is *not* the resize limit, this is just a sanity check.
  if (used_items_ / 8 > table_length_ / 10)
    return null_hash_;  // Table is more than 80% full.

  return AddFingerprint(fingerprint, true);
}

void VisitedLinkWriter::PostIOTask(const base::Location& from_here,
                                   base::OnceClosure task) {
  DCHECK(persist_to_disk_);
  file_task_runner_->PostTask(from_here, std::move(task));
}

void VisitedLinkWriter::AddURL(const GURL& url) {
  AddURL(url, /*update_file=*/true);
}

void VisitedLinkWriter::AddURLs(const std::vector<GURL>& urls) {
  TRACE_EVENT0("browser", "VisitedLinkWriter::AddURLs");

  bool bulk_write = (urls.size() > kBulkOperationThreshold);

  for (const GURL& url : urls)
    AddURL(url, !bulk_write);

  // Write the full table if in bulk mode.
  if (bulk_write && persist_to_disk_ && !table_builder_ &&
      !table_is_loading_from_file_) {
    WriteFullTable();
  }
}

void VisitedLinkWriter::DeleteAllURLs() {
  // Any pending modifications are invalid.
  added_since_rebuild_.clear();
  deleted_since_rebuild_.clear();

  added_since_load_.clear();
  deleted_since_load_.clear();
  table_is_loading_from_file_ = false;

  // Clear the hash table.
  used_items_ = 0;
  memset(hash_table_, 0, this->table_length_ * sizeof(Fingerprint));

  // Resize it if it is now too empty. Resize may write the new table out for
  // us, otherwise, schedule writing the new table to disk ourselves.
  if (!ResizeTableIfNecessary() && persist_to_disk_)
    WriteFullTable();

  listener_->Reset(false);
}

VisitedLinkDelegate* VisitedLinkWriter::GetDelegate() {
  return delegate_;
}

std::optional<uint64_t> VisitedLinkWriter::GetOrAddOriginSalt(
    const url::Origin& origin) {
  // To avoid race conditions, we should not get from or add to the salt map
  // while the hashtable is building.
  if (table_builder_ || table_is_loading_from_file_) {
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

void VisitedLinkWriter::DeleteURLs(URLIterator* urls) {
  if (!urls->HasNextURL())
    return;

  listener_->Reset(false);

  if (table_builder_.get() || table_is_loading_from_file_) {
    // A rebuild or load is in progress, save this deletion in the temporary
    // list so it can be added once rebuild is complete.
    while (urls->HasNextURL()) {
      const GURL& url(urls->NextURL());
      if (!url.is_valid())
        continue;

      Fingerprint fingerprint = ComputeURLFingerprint(url.spec(), salt_);
      deleted_since_rebuild_.insert(fingerprint);

      // If the URL was just added and now we're deleting it, it may be in the
      // list of things added since the last rebuild. Delete it from that list.
      added_since_rebuild_.erase(fingerprint);

      if (table_is_loading_from_file_) {
        deleted_since_load_.insert(url);
        added_since_load_.erase(url);
      }

      // Delete the URLs from the in-memory table, but don't bother writing
      // to disk since it will be replaced soon.
      DeleteFingerprint(fingerprint, false);
    }
    return;
  }

  // Compute the deleted URLs' fingerprints and delete them
  std::set<Fingerprint> deleted_fingerprints;
  while (urls->HasNextURL()) {
    const GURL& url(urls->NextURL());
    if (!url.is_valid())
      continue;
    deleted_fingerprints.insert(ComputeURLFingerprint(url.spec(), salt_));
  }
  DeleteFingerprintsFromCurrentTable(deleted_fingerprints);
}

// See VisitedLinkCommon::IsVisited which should be in sync with this algorithm
VisitedLinkWriter::Hash VisitedLinkWriter::AddFingerprint(
    Fingerprint fingerprint,
    bool send_notifications) {
  if (!hash_table_ || table_length_ == 0) {
    UMA_HISTOGRAM_ENUMERATION("History.VisitedLinks.TryToAddFingerprint",
                              AddFingerprint::kTableError);
    NOTREACHED_IN_MIGRATION();  // Not initialized.
    return null_hash_;
  }

  Hash cur_hash = HashFingerprint(fingerprint);
  Hash first_hash = cur_hash;
  while (true) {
    Fingerprint cur_fingerprint = FingerprintAt(cur_hash);
    if (cur_fingerprint == fingerprint) {
      UMA_HISTOGRAM_ENUMERATION("History.VisitedLinks.TryToAddFingerprint",
                                AddFingerprint::kAlreadyVisited);
      return null_hash_;  // This fingerprint is already in there, do nothing.
    }

    if (cur_fingerprint == null_fingerprint_) {
      // End of probe sequence found, insert here.
      hash_table_[cur_hash] = fingerprint;
      used_items_++;
      // If allowed, notify listener that a new visited link was added.
      if (send_notifications)
        listener_->Add(fingerprint);
      UMA_HISTOGRAM_ENUMERATION("History.VisitedLinks.TryToAddFingerprint",
                                AddFingerprint::kNewVisit);
      return cur_hash;
    }

    // Advance in the probe sequence.
    cur_hash = IncrementHash(cur_hash);
    if (cur_hash == first_hash) {
      // This means that we've wrapped around and are about to go into an
      // infinite loop. Something was wrong with the hashtable resizing
      // logic, so stop here.
      UMA_HISTOGRAM_ENUMERATION("History.VisitedLinks.TryToAddFingerprint",
                                AddFingerprint::kTableError);
      NOTREACHED_IN_MIGRATION();
      return null_hash_;
    }
  }
}

void VisitedLinkWriter::DeleteFingerprintsFromCurrentTable(
    const std::set<Fingerprint>& fingerprints) {
  bool bulk_write = (fingerprints.size() > kBulkOperationThreshold);

  // Delete the URLs from the table.
  for (auto i = fingerprints.begin(); i != fingerprints.end(); ++i)
    DeleteFingerprint(*i, !bulk_write);

  // These deleted fingerprints may make us shrink the table.
  if (ResizeTableIfNecessary())
    return;  // The resize function wrote the new table to disk for us.

  // Nobody wrote this out for us, write the full file to disk.
  if (bulk_write && persist_to_disk_)
    WriteFullTable();
}

bool VisitedLinkWriter::DeleteFingerprint(Fingerprint fingerprint,
                                          bool update_file) {
  if (!hash_table_ || table_length_ == 0) {
    NOTREACHED_IN_MIGRATION();  // Not initialized.
    return false;
  }
  if (!IsVisited(fingerprint))
    return false;  // Not in the database to delete.

  // First update the header used count.
  used_items_--;
  if (update_file && persist_to_disk_)
    WriteUsedItemCountToFile();

  Hash deleted_hash = HashFingerprint(fingerprint);

  // Find the range of "stuff" in the hash table that is adjacent to this
  // fingerprint. These are things that could be affected by the change in
  // the hash table. Since we use linear probing, anything after the deleted
  // item up until an empty item could be affected.
  Hash end_range = deleted_hash;
  while (true) {
    Hash next_hash = IncrementHash(end_range);
    if (next_hash == deleted_hash)
      break;  // We wrapped around and the whole table is full.
    if (!hash_table_[next_hash])
      break;  // Found the last spot.
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

  // Write the affected range to disk [deleted_hash, end_range].
  if (update_file && persist_to_disk_)
    WriteHashRangeToFile(deleted_hash, end_range);

  return true;
}

void VisitedLinkWriter::WriteFullTable() {
  // This function can get called when the file is open, for example, when we
  // resize the table. We must handle this case and not try to reopen the file,
  // since there may be write operations pending on the file I/O thread.
  //
  // Note that once we start writing, we do not delete on error. This means
  // there can be a partial file, but the short file will be detected next time
  // we start, and will be replaced.
  //
  // This might possibly get corrupted if we crash in the middle of writing.
  // We should pick up the most common types of these failures when we notice
  // that the file size is different when we load it back in, and then we will
  // regenerate the table.
  DCHECK(persist_to_disk_);

  if (!scoped_file_holder_) {
    scoped_file_holder_ = std::make_unique<base::ScopedFILE>();
    base::FilePath filename;
    GetDatabaseFileName(&filename);
    PostIOTask(FROM_HERE,
               base::BindOnce(&AsyncOpen, scoped_file_holder_.get(), filename));
  }

  // Write the new header.
  int32_t header[4];
  header[0] = kFileSignature;
  header[1] = kFileCurrentVersion;
  header[2] = table_length_;
  header[3] = used_items_;
  WriteToFile(scoped_file_holder_.get(), 0, header, sizeof(header));
  WriteToFile(scoped_file_holder_.get(), sizeof(header), salt_,
              LINK_SALT_LENGTH);

  // Write the hash data.
  WriteToFile(scoped_file_holder_.get(), kFileHeaderSize, hash_table_,
              table_length_ * sizeof(Fingerprint));

  // The hash table may have shrunk, so make sure this is the end.
  PostIOTask(FROM_HERE,
             base::BindOnce(&AsyncTruncate, scoped_file_holder_.get()));
}

bool VisitedLinkWriter::InitFromFile() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DCHECK(!scoped_file_holder_);
  DCHECK(persist_to_disk_);

  base::FilePath filename;
  if (!GetDatabaseFileName(&filename))
    return false;

  table_is_loading_from_file_ = true;

  TableLoadCompleteCallback callback = base::BindOnce(
      &VisitedLinkWriter::OnTableLoadComplete, weak_ptr_factory_.GetWeakPtr());

  PostIOTask(FROM_HERE, base::BindOnce(&VisitedLinkWriter::LoadFromFile,
                                       filename, std::move(callback)));

  return true;
}

// static
void VisitedLinkWriter::LoadFromFile(const base::FilePath& filename,
                                     TableLoadCompleteCallback callback) {
  scoped_refptr<LoadFromFileResult> load_from_file_result;
  bool success = LoadApartFromFile(filename, &load_from_file_result);

  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), success, load_from_file_result));
}

// static
bool VisitedLinkWriter::LoadApartFromFile(
    const base::FilePath& filename,
    scoped_refptr<LoadFromFileResult>* load_from_file_result) {
  DCHECK(load_from_file_result);

  base::ScopedFILE file_closer(base::OpenFile(filename, "rb+"));
  if (!file_closer.get())
    return false;

  int32_t num_entries, used_count;
  uint8_t salt[LINK_SALT_LENGTH];
  if (!ReadFileHeader(file_closer.get(), &num_entries, &used_count, salt))
    return false;  // Header isn't valid.

  // Allocate and read the table.
  base::MappedReadOnlyRegion hash_table_memory;
  if (!CreateApartURLTable(num_entries, salt, &hash_table_memory))
    return false;

  if (!ReadFromFile(file_closer.get(), kFileHeaderSize,
                    GetHashTableFromMapping(hash_table_memory.mapping),
                    num_entries * sizeof(Fingerprint))) {
    return false;
  }

  *load_from_file_result = new LoadFromFileResult(
      std::move(file_closer), std::move(hash_table_memory), num_entries,
      used_count, salt);
  return true;
}

void VisitedLinkWriter::OnTableLoadComplete(
    bool success,
    scoped_refptr<LoadFromFileResult> load_from_file_result) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(persist_to_disk_);
  DCHECK(!table_builder_);

  // When the apart table was loading from the database file the current table
  // have been cleared.
  if (!table_is_loading_from_file_)
    return;

  table_is_loading_from_file_ = false;

  if (!success) {
    // This temporary sets are used only when table was loaded.
    added_since_load_.clear();
    deleted_since_load_.clear();

    // If the table isn't loaded the table will be rebuilt.
    if (!suppress_rebuild_) {
      RebuildTableFromDelegate();
    } else {
      // When we disallow rebuilds (normally just unit tests), just use the
      // current empty table.
      WriteFullTable();
    }
    return;
  }

  // This temporary sets are needed only to rebuild table.
  added_since_rebuild_.clear();
  deleted_since_rebuild_.clear();

  DCHECK(load_from_file_result);

  // Delete the previous table.
  DCHECK(mapped_table_memory_.region.IsValid());
  mapped_table_memory_ = base::MappedReadOnlyRegion();

  // Assign the open file.
  DCHECK(!scoped_file_holder_);
  DCHECK(load_from_file_result->file.get());
  scoped_file_holder_ = std::make_unique<base::ScopedFILE>();
  *scoped_file_holder_ = std::move(load_from_file_result->file);

  // Assign the loaded table.
  DCHECK(load_from_file_result->hash_table_memory.region.IsValid() &&
         load_from_file_result->hash_table_memory.mapping.IsValid());
  memcpy(salt_, load_from_file_result->salt, LINK_SALT_LENGTH);
  mapped_table_memory_ = std::move(load_from_file_result->hash_table_memory);
  hash_table_ = GetHashTableFromMapping(mapped_table_memory_.mapping);
  table_length_ = load_from_file_result->num_entries;
  used_items_ = load_from_file_result->used_count;

#ifndef NDEBUG
  DebugValidate();
#endif

  // Send an update notification to all child processes.
  listener_->NewTable(&mapped_table_memory_.region);

  if (!added_since_load_.empty() || !deleted_since_load_.empty()) {
    // Resize the table if the table doesn't have enough capacity.
    int new_used_items =
        used_items_ + static_cast<int>(added_since_load_.size());
    if (new_used_items >= table_length_)
      ResizeTable(NewTableSizeForCount(new_used_items));

    // Also add anything that was added while we were asynchronously
    // loading the table.
    for (const GURL& url : added_since_load_) {
      Fingerprint fingerprint = ComputeURLFingerprint(url.spec(), salt_);
      AddFingerprint(fingerprint, false);
    }
    added_since_load_.clear();

    // Now handle deletions.
    for (const GURL& url : deleted_since_load_) {
      Fingerprint fingerprint = ComputeURLFingerprint(url.spec(), salt_);
      DeleteFingerprint(fingerprint, false);
    }
    deleted_since_load_.clear();

    if (persist_to_disk_)
      WriteFullTable();
  }

  // All tabs which was loaded when table was being loaded drop their cached
  // visited link hashes and invalidate their links again.
  listener_->Reset(true);
}

bool VisitedLinkWriter::InitFromScratch(bool suppress_rebuild) {
  if (suppress_rebuild && persist_to_disk_) {
    // When we disallow rebuilds (normally just unit tests), just use the
    // current empty table.
    WriteFullTable();
    return true;
  }

  // This will build the table from history. On the first run, history will
  // be empty, so this will be correct. This will also write the new table
  // to disk. We don't want to save explicitly here, since the rebuild may
  // not complete, leaving us with an empty but valid visited link database.
  // In the future, we won't know we need to try rebuilding again.
  return RebuildTableFromDelegate();
}

// static
bool VisitedLinkWriter::ReadFileHeader(FILE* file,
                                       int32_t* num_entries,
                                       int32_t* used_count,
                                       uint8_t salt[LINK_SALT_LENGTH]) {
  // Get file size.
  // Note that there is no need to seek back to the original location in the
  // file since ReadFromFile() [which is the next call accessing the file]
  // seeks before reading.
  if (fseek(file, 0, SEEK_END) == -1)
    return false;
  size_t file_size = ftell(file);

  if (file_size <= kFileHeaderSize)
    return false;

  uint8_t header[kFileHeaderSize];
  if (!ReadFromFile(file, 0, &header, kFileHeaderSize))
    return false;

  // Verify the signature.
  int32_t signature;
  memcpy(&signature, &header[kFileHeaderSignatureOffset], sizeof(signature));
  if (signature != kFileSignature)
    return false;

  // Verify the version is up to date. As with other read errors, a version
  // mistmatch will trigger a rebuild of the database from history, which will
  // have the effect of migrating the database.
  int32_t version;
  memcpy(&version, &header[kFileHeaderVersionOffset], sizeof(version));
  if (version != kFileCurrentVersion)
    return false;  // Bad version.

  // Read the table size and make sure it matches the file size.
  memcpy(num_entries, &header[kFileHeaderLengthOffset], sizeof(*num_entries));
  if (*num_entries * sizeof(Fingerprint) + kFileHeaderSize != file_size)
    return false;  // Bad size.

  // Read the used item count.
  memcpy(used_count, &header[kFileHeaderUsedOffset], sizeof(*used_count));
  if (*used_count > *num_entries)
    return false;  // Bad used item count;

  // Read the salt.
  memcpy(salt, &header[kFileHeaderSaltOffset], LINK_SALT_LENGTH);

  // This file looks OK from the header's perspective.
  return true;
}

bool VisitedLinkWriter::GetDatabaseFileName(base::FilePath* filename) {
  if (!database_name_override_.empty()) {
    // use this filename, the directory must exist
    *filename = database_name_override_;
    return true;
  }

  if (!browser_context_ || browser_context_->GetPath().empty())
    return false;

  base::FilePath profile_dir = browser_context_->GetPath();
  *filename = profile_dir.Append(FILE_PATH_LITERAL("Visited Links"));
  return true;
}

// Initializes the shared memory structure. The salt should already be filled
// in so that it can be written to the shared memory
bool VisitedLinkWriter::CreateURLTable(int32_t num_entries) {
  base::MappedReadOnlyRegion table_memory;
  if (!VisitedLinkWriter::fail_table_creation_for_testing_ &&
      CreateApartURLTable(num_entries, salt_, &table_memory)) {
    mapped_table_memory_ = std::move(table_memory);
    hash_table_ = GetHashTableFromMapping(mapped_table_memory_.mapping);
    table_length_ = num_entries;
    used_items_ = 0;
    return true;
  }

  return false;
}

// static
bool VisitedLinkWriter::CreateApartURLTable(
    int32_t num_entries,
    const uint8_t salt[LINK_SALT_LENGTH],
    base::MappedReadOnlyRegion* memory) {
  DCHECK(salt);
  DCHECK(memory);

  // The table is the size of the table followed by the entries.
  uint32_t alloc_size =
      num_entries * sizeof(Fingerprint) + sizeof(SharedHeader);
  UMA_HISTOGRAM_CUSTOM_COUNTS("History.VisitedLinks.HashTableSizeOnTableCreate",
                              alloc_size / 1024 / 1024, 1, 10000, 100);

  // Create the shared memory object.
  *memory = base::ReadOnlySharedMemoryRegion::Create(alloc_size);
  if (!memory->IsValid())
    return false;

  memset(memory->mapping.memory(), 0, alloc_size);

  // Save the header for other processes to read.
  SharedHeader* header = static_cast<SharedHeader*>(memory->mapping.memory());
  header->length = num_entries;
  memcpy(header->salt, salt, LINK_SALT_LENGTH);

  return true;
}

void VisitedLinkWriter::FreeURLTable() {
  mapped_table_memory_ = base::MappedReadOnlyRegion();
  if (scoped_file_holder_) {
    DCHECK(persist_to_disk_);

    // Release the file on the IO thread:
    PostIOTask(FROM_HERE,
               base::DoNothingWithBoundArgs(std::move(scoped_file_holder_)));
  }
}

bool VisitedLinkWriter::ResizeTableIfNecessary() {
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
       load > min_table_load))
    return false;

  // Table needs to grow or shrink.
  int new_size = NewTableSizeForCount(used_items_);
  DCHECK(new_size > used_items_);
  DCHECK(load <= min_table_load || new_size > table_length_);
  ResizeTable(new_size);
  return true;
}

void VisitedLinkWriter::ResizeTable(int32_t new_size) {
  DCHECK(mapped_table_memory_.region.IsValid() &&
         mapped_table_memory_.mapping.IsValid());

#ifndef NDEBUG
  DebugValidate();
#endif

  auto old_hash_table_mapping = std::move(mapped_table_memory_.mapping);
  int32_t old_table_length = table_length_;
  if (!CreateURLTable(new_size)) {
    // Restore modified members.
    mapped_table_memory_.mapping = std::move(old_hash_table_mapping);
    return;
  }

  shared_memory_serial_++;

  {
    Fingerprint* old_hash_table =
        GetHashTableFromMapping(old_hash_table_mapping);
    // Now we have two tables, our local copy which is the old one, and the new
    // one loaded into this object where we need to copy the data.
    for (int32_t i = 0; i < old_table_length; i++) {
      Fingerprint cur = old_hash_table[i];
      if (cur)
        AddFingerprint(cur, false);
    }
  }

  // Send an update notification to all child processes so they read the new
  // table.
  listener_->NewTable(&mapped_table_memory_.region);

#ifndef NDEBUG
  DebugValidate();
#endif

  // The new table needs to be written to disk.
  if (persist_to_disk_)
    WriteFullTable();
}

uint32_t VisitedLinkWriter::DefaultTableSize() const {
  if (table_size_override_)
    return table_size_override_;

  return kDefaultTableSize;
}

uint32_t VisitedLinkWriter::NewTableSizeForCount(int32_t item_count) const {
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
    if (table_sizes[i] > desired)
      return table_sizes[i];
  }

  // Growing very big, just approximate a "good" number, not growing as much
  // as normal.
  return item_count * 2 - 1;
}

// See the TableBuilder definition in the header file for how this works.
bool VisitedLinkWriter::RebuildTableFromDelegate() {
  DCHECK(!table_builder_);

  // TODO(brettw) make sure we have reasonable salt!
  table_builder_ = new TableBuilder(this, salt_);
  delegate_->RebuildTable(table_builder_);
  return true;
}

// See the TableBuilder declaration above for how this works.
void VisitedLinkWriter::OnTableRebuildComplete(
    bool success,
    const std::vector<Fingerprint>& fingerprints) {
  if (success) {
    // Replace the old table with a new blank one.
    shared_memory_serial_++;

    int new_table_size = NewTableSizeForCount(
        static_cast<int>(fingerprints.size() + added_since_rebuild_.size()));
    if (CreateURLTable(new_table_size)) {
      // Add the stored fingerprints to the hash table.
      for (const auto& fingerprint : fingerprints)
        AddFingerprint(fingerprint, false);

      // Also add anything that was added while we were asynchronously
      // generating the new table.
      for (const auto& fingerprint : added_since_rebuild_)
        AddFingerprint(fingerprint, false);
      added_since_rebuild_.clear();

      // Now handle deletions. Do not shrink the table now, we'll shrink it when
      // adding or deleting an url the next time.
      for (const auto& fingerprint : deleted_since_rebuild_)
        DeleteFingerprint(fingerprint, false);
      deleted_since_rebuild_.clear();

      // Send an update notification to all child processes.
      listener_->NewTable(&mapped_table_memory_.region);
      // All tabs which was loaded when table was being rebuilt
      // invalidate their links again.
      listener_->Reset(false);

      if (persist_to_disk_)
        WriteFullTable();
    }
  }
  table_builder_ = nullptr;  // Will release our reference to the builder.

  // Notify the unit test that the rebuild is complete (will be NULL in prod.)
  if (!rebuild_complete_task_.is_null())
    std::move(rebuild_complete_task_).Run();
}

void VisitedLinkWriter::WriteToFile(base::ScopedFILE* file,
                                    off_t offset,
                                    void* data,
                                    int32_t data_size) {
  DCHECK(file);
  DCHECK(persist_to_disk_);
  DCHECK(!table_is_loading_from_file_);
  PostIOTask(
      FROM_HERE,
      base::BindOnce(&AsyncWrite, file, offset,
                     std::string(static_cast<const char*>(data), data_size)));
}

void VisitedLinkWriter::WriteUsedItemCountToFile() {
  DCHECK(persist_to_disk_);
  if (!scoped_file_holder_) {
    return;  // See comment on the scoped_file_holder_ variable for why this
             // might happen.
  }
  WriteToFile(scoped_file_holder_.get(), kFileHeaderUsedOffset, &used_items_,
              sizeof(used_items_));
}

void VisitedLinkWriter::WriteHashRangeToFile(Hash first_hash, Hash last_hash) {
  DCHECK(persist_to_disk_);

  if (!scoped_file_holder_) {
    return;  // See comment on the scoped_file_holder_ variable for why this
             // might happen.
  }
  if (last_hash < first_hash) {
    // Handle wraparound at 0. This first write is first_hash->EOF
    WriteToFile(scoped_file_holder_.get(),
                first_hash * sizeof(Fingerprint) + kFileHeaderSize,
                &hash_table_[first_hash],
                (table_length_ - first_hash) * sizeof(Fingerprint));

    // Now do 0->last_lash.
    WriteToFile(scoped_file_holder_.get(), kFileHeaderSize, hash_table_,
                (last_hash + 1) * sizeof(Fingerprint));
  } else {
    // Normal case, just write the range.
    WriteToFile(scoped_file_holder_.get(),
                first_hash * sizeof(Fingerprint) + kFileHeaderSize,
                &hash_table_[first_hash],
                (last_hash - first_hash + 1) * sizeof(Fingerprint));
  }
}

// static
bool VisitedLinkWriter::ReadFromFile(FILE* file,
                                     off_t offset,
                                     void* data,
                                     size_t data_size) {
  if (fseek(file, offset, SEEK_SET) != 0)
    return false;

  size_t num_read = fread(data, 1, data_size, file);
  return num_read == data_size;
}

// VisitedLinkTableBuilder ----------------------------------------------------

VisitedLinkWriter::TableBuilder::TableBuilder(
    VisitedLinkWriter* writer,
    const uint8_t salt[LINK_SALT_LENGTH])
    : writer_(writer), success_(true) {
  fingerprints_.reserve(4096);
  memcpy(salt_, salt, LINK_SALT_LENGTH * sizeof(uint8_t));
}

// TODO(brettw): Do we want to try to cancel the request if this happens? It
// could delay shutdown if there are a lot of URLs.
void VisitedLinkWriter::TableBuilder::DisownWriter() {
  writer_ = nullptr;
}

void VisitedLinkWriter::TableBuilder::OnURL(const GURL& url) {
  if (!url.is_empty()) {
    fingerprints_.push_back(
        VisitedLinkWriter::ComputeURLFingerprint(url.spec(), salt_));
  }
}

void VisitedLinkWriter::TableBuilder::OnComplete(bool success) {
  success_ = success;
  DLOG_IF(WARNING, !success) << "Unable to rebuild visited links";

  // Marshal to the main thread to notify the VisitedLinkWriter that the
  // rebuild is complete.
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&TableBuilder::OnCompleteMainThread, this));
}

void VisitedLinkWriter::TableBuilder::OnCompleteMainThread() {
  if (writer_)
    writer_->OnTableRebuildComplete(success_, fingerprints_);
}

// static
VisitedLinkCommon::Fingerprint* VisitedLinkWriter::GetHashTableFromMapping(
    const base::WritableSharedMemoryMapping& hash_table_mapping) {
  DCHECK(hash_table_mapping.IsValid());
  // Our table pointer is just the data immediately following the header.
  return reinterpret_cast<Fingerprint*>(
      static_cast<char*>(hash_table_mapping.memory()) + sizeof(SharedHeader));
}

}  // namespace visitedlink
