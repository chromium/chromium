// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITEDLINK_BROWSER_VISITEDLINK_WRITER_H_
#define COMPONENTS_VISITEDLINK_BROWSER_VISITEDLINK_WRITER_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <set>
#include <vector>

#include "base/callback.h"
#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/read_only_shared_memory_region.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "build/build_config.h"
#include "components/visitedlink/common/visitedlink_common.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#if defined(UNIT_TEST) || defined(PERF_TEST) || !defined(NDEBUG)
#include "base/check_op.h"
#endif

class GURL;

namespace content {
class BrowserContext;
}

namespace visitedlink {

class VisitedLinkDelegate;

// Controls the link coloring database. The writer controls all writing to the
// database as well as disk I/O. There should be only one writer.
//
// This class will defer writing operations to the file thread. This means that
// class destruction, the file may still be open since operations are pending on
// another thread.
class VisitedLinkWriter : public VisitedLinkCommon {
 public:
  // Listens to the link coloring database events. The writer is given this
  // event as a constructor argument and dispatches events using it.
  class Listener {
   public:
    virtual ~Listener() {}

    // Called when link coloring database has been created or replaced. The
    // argument is a memory region containing the new table.
    virtual void NewTable(base::ReadOnlySharedMemoryRegion* table_region) = 0;

    // Called when new link has been added. The argument is the fingerprint
    // (hash) of the link.
    virtual void Add(Fingerprint fingerprint) = 0;

    // Called when link coloring state has been reset. This may occur when
    // entire or parts of history were deleted. Also this may occur when
    // the table was rebuilt or loaded. The salt is stored in the database file.
    // As a result the salt will change after loading the table from the
    // database file. In this case we use |invalidate_hashes| to inform that
    // all cached visitedlink hashes need to be recalculated.
    virtual void Reset(bool invalidate_hashes) = 0;
  };

  VisitedLinkWriter(content::BrowserContext* browser_context,
                    VisitedLinkDelegate* delegate,
                    bool persist_to_disk);

  // In unit test mode, we allow the caller to optionally specify the database
  // filename so that it can be run from a unit test. The directory where this
  // file resides must exist in this mode. You can also specify the default
  // table size to test table resizing. If this parameter is 0, we will use the
  // defaults.
  //
  // In the unit test mode, we also allow the caller to provide a history
  // service pointer (the history service can't be fetched from the browser
  // process when we're in unit test mode). This can be NULL to try to access
  // the main version, which will probably fail (which can be good for testing
  // this failure mode).
  //
  // When |suppress_rebuild| is set, we'll not attempt to load data from
  // history if the file can't be loaded. This should generally be set for
  // testing except when you want to test the rebuild process explicitly.
  VisitedLinkWriter(Listener* listener,
                    VisitedLinkDelegate* delegate,
                    bool persist_to_disk,
                    bool suppress_rebuild,
                    const base::FilePath& filename,
                    int32_t default_table_size);

  VisitedLinkWriter(const VisitedLinkWriter&) = delete;
  VisitedLinkWriter& operator=(const VisitedLinkWriter&) = delete;

  ~VisitedLinkWriter() override;

  // Must be called immediately after object creation. Nothing else will work
  // until this is called. Returns true on success, false means that this
  // object won't work.
  bool Init();

  base::MappedReadOnlyRegion& mapped_table_memory() {
    return mapped_table_memory_;
  }

  // Adds a URL to the table.
  void AddURL(const GURL& url);

  // Adds a set of URLs to the table.
  void AddURLs(const std::vector<GURL>& urls);

  // See DeleteURLs.
  class URLIterator {
   public:
    // HasNextURL must return true when this is called. Returns the next URL
    // then advances the iterator. Note that the returned reference is only
    // valid until the next call of NextURL.
    virtual const GURL& NextURL() = 0;

    // Returns true if still has URLs to be iterated.
    virtual bool HasNextURL() const = 0;

   protected:
    virtual ~URLIterator() {}
  };

  // Deletes the specified URLs from |rows| from the table.
  void DeleteURLs(URLIterator* iterator);

  // Clears the visited links table by deleting the file from disk. Used as
  // part of history clearing.
  void DeleteAllURLs();

  // Returns the Delegate of this Writer.
  VisitedLinkDelegate* GetDelegate();

#if defined(UNIT_TEST) || !defined(NDEBUG) || defined(PERF_TEST)
  // This is a debugging function that can be called to double-check internal
  // data structures. It will assert if the check fails.
  void DebugValidate();

  // Sets a task to execute when the next rebuild from history is complete.
  // This is used by unit tests to wait for the rebuild to complete before
  // they continue. The pointer will be owned by this object after the call.
  void set_rebuild_complete_task(base::OnceClosure task) {
    DCHECK(rebuild_complete_task_.is_null());
    rebuild_complete_task_ = std::move(task);
  }

  // returns the number of items in the table for testing verification
  int32_t GetUsedCount() const { return used_items_; }

  // Returns the listener.
  VisitedLinkWriter::Listener* GetListener() const { return listener_.get(); }

  // Call to cause the entire database file to be re-written from scratch
  // to disk. Used by the performance tester.
  void RewriteFile() { WriteFullTable(); }
#endif

 private:
  FRIEND_TEST_ALL_PREFIXES(VisitedLinkTest, DatabaseIOAddURLs);
  FRIEND_TEST_ALL_PREFIXES(VisitedLinkTest, Delete);
  FRIEND_TEST_ALL_PREFIXES(VisitedLinkTest, BigDelete);
  FRIEND_TEST_ALL_PREFIXES(VisitedLinkTest, BigImport);

  // Keeps the result of loading the table from the database file to the UI
  // thread.
  struct LoadFromFileResult;

  using TableLoadCompleteCallback = base::OnceCallback<void(
      bool success,
      scoped_refptr<LoadFromFileResult> load_from_file_result)>;

  // Object to rebuild the table on the history thread (see the .cc file).
  class TableBuilder;

  // Byte offsets of values in the header.
  static const int32_t kFileHeaderSignatureOffset;
  static const int32_t kFileHeaderVersionOffset;
  static const int32_t kFileHeaderLengthOffset;
  static const int32_t kFileHeaderUsedOffset;
  static const int32_t kFileHeaderSaltOffset;

  // The signature at the beginning of a file.
  static const int32_t kFileSignature;

  // version of the file format this module currently uses
  static const int32_t kFileCurrentVersion;

  // Bytes in the file header, including the salt.
  static const size_t kFileHeaderSize;

  // When creating a fresh new table, we use this many entries.
  static const unsigned kDefaultTableSize;

  // When the user is adding or deleting a boatload of URLs, we don't really
  // want to do individual writes for each of them. When the count exceeds this
  // threshold, we will write the whole table to disk at once instead of
  // individual items.
  static constexpr size_t kBulkOperationThreshold = 64;

  // Adds |url| to the table and updates the file if |update_file| and
  // |persist_to_disk_| are true.
  void AddURL(const GURL& url, bool update_file);

  // If a rebuild is in progress, we save the URL in the temporary list.
  // Otherwise, we add this to the table. Returns the index of the
  // inserted fingerprint or null_hash_ on failure.
  Hash TryToAddURL(const GURL& url);

  // File I/O functions
  // ------------------
  // These functions are only called if |persist_to_disk_| is true.

  // Posts the given task to the blocking worker pool with our options.
  void PostIOTask(const base::Location& from_here, base::OnceClosure task);

  // Writes the entire table to disk. It will leave the table file open and
  // the handle to it will be stored in file_.
  void WriteFullTable();

  // Tries to load asynchronously the table from the database file.
  bool InitFromFile();

  // Load the table from the database file. Calls |callback| when completed. It
  // is called from the background thread. It must be first in the sequence of
  // background operations with the database file.
  static void LoadFromFile(const base::FilePath& filename,
                           TableLoadCompleteCallback callback);

  // Load the table from the database file. Returns true on success.
  // Fills parameter |load_from_file_result| on success. It is called from
  // the background thread.
  static bool LoadApartFromFile(
      const base::FilePath& filename,
      scoped_refptr<LoadFromFileResult>* load_from_file_result);

  // It is called from the background thread and executed on the UI
  // thread.
  void OnTableLoadComplete(
      bool success,
      scoped_refptr<LoadFromFileResult> load_from_file_result);

  // Reads the header of the link coloring database from disk. Assumes the
  // file pointer is at the beginning of the file and that it is the first
  // asynchronous I/O operation on the background thread.
  //
  // Returns true on success and places the size of the table in num_entries
  // and the number of nonzero fingerprints in used_count. This will fail if
  // the version of the file is not the current version of the database.
  static bool ReadFileHeader(FILE* hfile,
                             int32_t* num_entries,
                             int32_t* used_count,
                             uint8_t salt[LINK_SALT_LENGTH]);

  // Fills *filename with the name of the link database filename
  bool GetDatabaseFileName(base::FilePath* filename);

  // Wrapper around Window's WriteFile using asynchronous I/O. This will proxy
  // the write to a background thread.
  void WriteToFile(FILE** hfile, off_t offset, void* data, int32_t data_size);

  // Helper function to schedule and asynchronous write of the used count to
  // disk (this is a common operation).
  void WriteUsedItemCountToFile();

  // Helper function to schedule an asynchronous write of the given range of
  // hash functions to disk. The range is inclusive on both ends. The range can
  // wrap around at 0 and this function will handle it.
  void WriteHashRangeToFile(Hash first_hash, Hash last_hash);

  // Synchronous read from the file. Assumes that it is the first asynchronous
  // I/O operation in the background thread. Returns true if the entire buffer
  // was successfully filled.
  static bool ReadFromFile(FILE* hfile,
                           off_t offset,
                           void* data,
                           size_t data_size);

  // General table handling
  // ----------------------

  // Called to add a fingerprint to the table. If |send_notifications| is true
  // and the item is added successfully, Listener::Add will be invoked.
  // Returns the index of the inserted fingerprint or null_hash_ if there was a
  // duplicate and this item was skippped.
  Hash AddFingerprint(Fingerprint fingerprint, bool send_notifications);

  // Deletes all fingerprints from the given vector from the current hash table
  // and syncs it to disk if there are changes. This does not update the
  // deleted_since_rebuild_ list, the caller must update this itself if there
  // is an update pending.
  void DeleteFingerprintsFromCurrentTable(
      const std::set<Fingerprint>& fingerprints);

  // Removes the indicated fingerprint from the table. If the update_file flag
  // is set, the changes will also be written to disk. Returns true if the
  // fingerprint was deleted, false if it was not in the table to delete.
  bool DeleteFingerprint(Fingerprint fingerprint, bool update_file);

  // Creates a new empty table, call if InitFromFile() fails. Normally, when
  // |suppress_rebuild| is false, the table will be rebuilt from history,
  // keeping us in sync. When |suppress_rebuild| is true, the new table will be
  // empty and we will not consult history. This is used when clearing the
  // database and for unit tests.
  bool InitFromScratch(bool suppress_rebuild);

  // Allocates the Fingerprint structure and length. Structure is filled with 0s
  // and shared header with salt and used_items_ is set to 0.
  bool CreateURLTable(int32_t num_entries);

  // Allocates the Fingerprint structure and length. Returns true on success.
  // Structure is filled with 0s and shared header with salt. The result of
  // allocation is saved into |mapped_region|.
  static bool CreateApartURLTable(int32_t num_entries,
                                  const uint8_t salt[LINK_SALT_LENGTH],
                                  base::MappedReadOnlyRegion* memory);

  // A wrapper for CreateURLTable, this will allocate a new table, initialized
  // to empty. The caller is responsible for saving the shared memory pointer
  // and handles before this call (they will be replaced with new ones) and
  // releasing them later. This is designed for callers that make a new table
  // and then copy values from the old table to the new one, then release the
  // old table.
  //
  // Returns true on success. On failure, the old table will be restored. The
  // caller should not attemp to release the pointer/handle in this case.
  bool BeginReplaceURLTable(int32_t num_entries);

  // unallocates the Fingerprint table
  void FreeURLTable();

  // For growing the table. ResizeTableIfNecessary will check to see if the
  // table should be resized and calls ResizeTable if needed. Returns true if
  // we decided to resize the table.
  bool ResizeTableIfNecessary();

  // Resizes the table (growing or shrinking) as necessary to accomodate the
  // current count.
  void ResizeTable(int32_t new_size);

  // Returns the default table size. It can be overrided in unit tests.
  uint32_t DefaultTableSize() const;

  // Returns the desired table size for |item_count| URLs.
  uint32_t NewTableSizeForCount(int32_t item_count) const;

  // Computes the table load as fraction. For example, if 1/4 of the entries are
  // full, this value will be 0.25
  float ComputeTableLoad() const {
    return static_cast<float>(used_items_) / static_cast<float>(table_length_);
  }

  // Initializes a rebuild of the visited link database based on the browser
  // history. This will set table_builder_ while working, and there should not
  // already be a rebuild in place when called. See the definition for more
  // details on how this works.
  //
  // Returns true on success. Failure means we're not attempting to rebuild
  // the database because something failed.
  bool RebuildTableFromDelegate();

  // Callback that the table rebuilder uses when the rebuild is complete.
  // |success| is true if the fingerprint generation succeeded, in which case
  // |fingerprints| will contain the computed fingerprints. On failure, there
  // will be no fingerprints.
  void OnTableRebuildComplete(bool success,
                              const std::vector<Fingerprint>& fingerprints);

  // Increases or decreases the given hash value by one, wrapping around as
  // necessary. Used for probing.
  inline Hash IncrementHash(Hash hash) {
    if (hash >= table_length_ - 1)
      return 0;  // Wrap around.
    return hash + 1;
  }
  inline Hash DecrementHash(Hash hash) {
    if (hash <= 0)
      return table_length_ - 1;  // Wrap around.
    return hash - 1;
  }

  // Returns a pointer to the start of the hash table, given the mapping
  // containing the hash table.
  static Fingerprint* GetHashTableFromMapping(
      const base::WritableSharedMemoryMapping& hash_table_mapping);

  // Reference to the browser context that this object belongs to
  // (it knows the path to where the data is stored)
  raw_ptr<content::BrowserContext> browser_context_ = nullptr;

  // Client owns the delegate and is responsible for it being valid through
  // the life time this VisitedLinkWriter.
  raw_ptr<VisitedLinkDelegate> delegate_;

  // VisitedLinkEventListener to handle incoming events.
  std::unique_ptr<Listener> listener_;

  // Task runner for posting file tasks.
  scoped_refptr<base::SequencedTaskRunner> file_task_runner_ =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN});

  // When non-NULL, indicates we are in database rebuild mode and points to
  // the class collecting fingerprint information from the history system.
  // The pointer is owned by this class, but it must remain valid while the
  // history query is running. We must only delete it when the query is done.
  scoped_refptr<TableBuilder> table_builder_;

  // Indicates URLs added and deleted since we started rebuilding the table.
  std::set<Fingerprint> added_since_rebuild_;
  std::set<Fingerprint> deleted_since_rebuild_;

  // Indicates URLs added and deleted since we started loading the table.
  // It can be only url because after loading table the salt will be changed.
  std::set<GURL> added_since_load_;
  std::set<GURL> deleted_since_load_;

  // The currently open file with the table in it. This may be NULL if we're
  // rebuilding and haven't written a new version yet or if |persist_to_disk_|
  // is false. Writing to the file may be safely ignored in this case. Also
  // |file_| may be non-NULL but point to a NULL pointer. That would mean that
  // opening of the file is already scheduled in a background thread and any
  // writing to the file can also be scheduled to the background thread as it's
  // guaranteed to be executed after the opening.
  // The class owns both the |file_| pointer and the pointer pointed
  // by |*file_|.
  raw_ptr<FILE*, DanglingUntriaged> file_ = nullptr;

  // If true, will try to persist the hash table to disk. Will rebuild from
  // VisitedLinkDelegate::RebuildTable if there are disk corruptions.
  bool persist_to_disk_;

  // Shared memory consists of a SharedHeader followed by the table.
  base::MappedReadOnlyRegion mapped_table_memory_;

  // When we generate new tables, we increment the serial number of the
  // shared memory object.
  int32_t shared_memory_serial_ = 0;

  // Number of non-empty items in the table, used to compute fullness.
  int32_t used_items_ = 0;

  // We set this to true to avoid writing to the database file.
  bool table_is_loading_from_file_ = false;

  // Testing values -----------------------------------------------------------
  //
  // The following fields exist for testing purposes. They are not used in
  // release builds. It'd be nice to eliminate them in release builds, but we
  // don't want to change the signature of the object between the unit test and
  // regular builds. Instead, we just have "default" values that these take
  // in release builds that give "regular" behavior.

  // Overridden database file name for testing
  base::FilePath database_name_override_;

  // When nonzero, overrides the table size for new databases for testing
  int32_t table_size_override_ = 0;

  // When set, indicates the task that should be run after the next rebuild from
  // history is complete.
  base::OnceClosure rebuild_complete_task_;

  // Set to prevent us from attempting to rebuild the database from global
  // history if we have an error opening the file. This is used for testing,
  // will be false in production.
  bool suppress_rebuild_ = false;

  base::WeakPtrFactory<VisitedLinkWriter> weak_ptr_factory_{this};
};

// NOTE: These methods are defined inline here, so we can share the compilation
//       of visitedlink_writer.cc between the browser and the unit/perf tests.

#if defined(UNIT_TEST) || defined(PERF_TEST) || !defined(NDEBUG)
inline void VisitedLinkWriter::DebugValidate() {
  int32_t used_count = 0;
  for (int32_t i = 0; i < table_length_; i++) {
    if (hash_table_[i])
      used_count++;
  }
  DCHECK_EQ(used_count, used_items_);
}
#endif

}  // namespace visitedlink

#endif  // COMPONENTS_VISITEDLINK_BROWSER_VISITEDLINK_WRITER_H_
