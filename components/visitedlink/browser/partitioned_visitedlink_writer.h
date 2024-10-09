// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VISITEDLINK_BROWSER_PARTITIONED_VISITEDLINK_WRITER_H_
#define COMPONENTS_VISITEDLINK_BROWSER_PARTITIONED_VISITEDLINK_WRITER_H_

#include <map>
#include <set>

#include "base/memory/read_only_shared_memory_region.h"
#include "components/visitedlink/common/visitedlink_common.h"
#include "components/visitedlink/core/visited_link.h"
#include "content/public/browser/browser_context.h"

namespace base {
class WritableSharedMemoryMapping;
}

namespace url {
class Origin;
}

namespace visitedlink {

class VisitedLinkDelegate;

// PartitionedVisitedLinkWriter constructs and writes to the partitioned
// :visited links hashtable. There should only be one instance of
// PartitionedVisitedLinkWriter, and it must be initialized before use.
//
// Much of this code is similar to or identical to the (unpartitioned)
// VisitedLinkWriter class. PartitionedVisitedLinkWriter does not persist to
// disk, the code has been "forked" into a separate class that relies on the
// HistoryService's VisitedLinkDatabase to persist partitioned :visited link
// browsing history across sessions. Once constructed from the
// VisitedLinkDatabase, the partitioned hashtable is stored in a shared memory
// instance.
class PartitionedVisitedLinkWriter : public VisitedLinkCommon {
 public:
  // Listens to the link coloring database events. The writer is given this
  // event as a constructor argument and dispatches events using it.
  class Listener {
   public:
    virtual ~Listener() = default;

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

    // This function determines the per-origin salts required for
    // any navigations that took place during the hashtable build (and as a
    // result did not send a per-origin salt in the navigation params - see
    // PartitionedVisitedLinkWriter::salts_ for more information). The
    // per-origin salts are sent via IPC to their respective VisitedLinkReader
    // instances.
    // NOTE: this is called on the main thread once the hashtable has
    // completed building on the DB thread.
    virtual void UpdateOriginSalts() = 0;
  };

  PartitionedVisitedLinkWriter(content::BrowserContext* browser_context,
                               VisitedLinkDelegate* delegate);

  // This constructor is used by unit tests.
  PartitionedVisitedLinkWriter(std::unique_ptr<Listener> listener,
                               VisitedLinkDelegate* delegate,
                               bool suppress_build,
                               int32_t default_table_size);

  ~PartitionedVisitedLinkWriter() override;

  // Must be called immediately after object creation. Nothing else will work
  // until this is called. Returns true on success, false means that this
  // object won't work.
  bool Init();

  // Adds a VisitedLink to the hashtable.
  void AddVisitedLink(const VisitedLink& link);

  // See DeleteURLs.
  class VisitedLinkIterator {
   public:
    // HasNextVisitedLink must return true when this is called. Returns the next
    // VisitedLink then advances the iterator. Note that the returned reference
    // is only valid until the next call of NextVisitedLink.
    virtual const VisitedLink& NextVisitedLink() = 0;

    // Returns true if still has VisitedLinks to be iterated.
    virtual bool HasNextVisitedLink() const = 0;

   protected:
    virtual ~VisitedLinkIterator() = default;
  };

  // Deletes the specified VisitedLinks from the hashtable.
  void DeleteVisitedLinks(VisitedLinkIterator* iterator);

  // Clears all VisitedLinks from the hashtable.
  void DeleteAllVisitedLinks();

  // Return the salt used to hash visited links from this origin. If we have not
  // visited this origin before, a new <origin, salt> pair will be added to the
  // map, and that new salt value will be retuned. Will return
  // std::optional if the table is currently being built or rebuilt.
  //
  // NOTE: THIS FUNCTION MAY ONLY BE CALLED ON THE MAIN (UI) THREAD.
  std::optional<uint64_t> GetOrAddOriginSalt(const url::Origin& origin);

#if defined(UNIT_TEST) || !defined(NDEBUG) || defined(PERF_TEST)
  // This is a debugging function that can be called to double-check internal
  // data structures. It will assert if the check fails.
  void DebugValidate();

  // Sets a task to execute when we've completed building the table from
  // history. This is ONLY used by unit tests to wait for the build to complete
  // before they continue. The pointer will be owned by this object after the
  // call.
  void set_build_complete_task(base::OnceClosure task) {
    DCHECK(build_complete_task_.is_null());
    build_complete_task_ = std::move(task);
  }

  // Returns the number non-null hashes in the table for testing verification.
  int32_t GetUsedCount() const { return used_items_; }

  // Returns the listener.
  PartitionedVisitedLinkWriter::Listener* GetListener() const {
    return listener_.get();
  }

  // Returns the hashtable stored in memory.
  const base::MappedReadOnlyRegion& GetMappedTableMemoryForTesting() {
    return mapped_table_memory_;
  }
#endif

 private:
  FRIEND_TEST_ALL_PREFIXES(PartitionedVisitedLinkTest, DeleteWithCollisions);
  FRIEND_TEST_ALL_PREFIXES(PartitionedVisitedLinkTest, HashRangeWraparound);

  // When creating an empty table, we use this many entries (see the .cc file).
  static const unsigned kDefaultTableSize;
  // Object to build the table on the history thread (see the .cc file).
  class TableBuilder;

  // General Table Handling ----------------------------------------------------

  // Creates an empty partitioned hashtable. The table is populated with the
  // partitioned shared header and filled with null hashes. The result of
  // allocation is saved into `mapped_table_memory`.
  bool CreateVisitedLinkTable(int32_t num_entries);

  // Allocates the Fingerprint structure and length. Returns true on success.
  static bool CreateVisitedLinkTableHelper(int32_t num_entries,
                                           base::MappedReadOnlyRegion* memory);

  // ResizeTableIfNecessary will check to see if the table should be resized and
  // calls ResizeTable if needed. Returns true if we decided to resize the
  // table.
  bool ResizeTableIfNecessary();

  // Resizes the table (growing or shrinking) as necessary to accommodate the
  // current count.
  void ResizeTable(int32_t new_size);

  // Computes the table load as fraction. For example, if 1/4 of the entries are
  // full, this value will be 0.25
  float ComputeTableLoad() const {
    return static_cast<float>(used_items_) / static_cast<float>(table_length_);
  }

  // Populates the partitioned hashtable based on the browser history stored
  // in the VisitedLinkDatabase. This will set table_builder_ while working, and
  // there should not already be a build occurring when called. See the
  // definition for more details on how this works.
  //
  // Returns true on success. Failure means we're not attempting to build
  // from the VisitedLinkDatabase because something failed.
  bool BuildTableFromDelegate();

  // Callback that the table builder uses when the build is complete.
  // `success` is true if the fingerprint generation succeeded, in which case
  // `fingerprints` will contain the computed fingerprints. On failure, there
  // will be no fingerprints. `salts` will contain the origin salts used to
  // generate the fingerprints. On failure, there will be no salts.
  void OnTableBuildComplete(bool success,
                            const std::vector<Fingerprint>& fingerprints,
                            std::map<url::Origin, uint64_t> salts);

  // If a build is in progress, we save `link` in `added_during_build`.
  // Otherwise, we add to the hashtable. Returns the index of the inserted
  // fingerprint or `null_hash_` on failure.
  Hash TryToAddVisitedLink(const VisitedLink& link);

  // Increases or decreases the given hash value by one, wrapping around as
  // necessary. Used for probing.
  inline Hash IncrementHash(Hash hash) {
    if (hash >= table_length_ - 1) {
      return 0;  // Wrap around.
    }
    return hash + 1;
  }
  inline Hash DecrementHash(Hash hash) {
    if (hash <= 0) {
      return table_length_ - 1;  // Wrap around.
    }
    return hash - 1;
  }

  // Called to add a fingerprint to the table. Returns the index of the inserted
  // fingerprint or null_hash_ if there was a duplicate and this item was
  // skipped.
  //
  // TODO(crbug.com/332364003): If `send_notifications` is true
  // and the item is added successfully, Listener::Add will be invoked.
  Hash AddFingerprint(Fingerprint fingerprint, bool send_notifications);

  // Deletes all fingerprints from the given vector from the current hashtable.
  // This does not update the `deleted_during_build_` list, the caller must
  // update this itself if there is a build pending.
  void DeleteFingerprintsFromCurrentTable(
      const std::set<Fingerprint>& fingerprints);

  // Removes the indicated fingerprint from the table. Returns true if the
  // fingerprint was deleted, false if it was not in the table to delete.
  bool DeleteFingerprint(Fingerprint fingerprint);

  // Returns a pointer to the start of the hash table, given the mapping
  // containing the hash table.
  static Fingerprint* GetHashTableFromMapping(
      const base::WritableSharedMemoryMapping& hash_table_mapping);

  // Returns the default table size. It can be overridden in unit tests.
  uint32_t DefaultTableSize() const;

  // Returns the desired table size for storing `item_count` visited links.
  uint32_t NewTableSizeForCount(int32_t item_count) const;

  // Member variables ----------------------------------------------------------

  // TODO(crbug.com/332364003): We need to create an instance of
  // VisitedLinkEventListener to handle incoming events and define the Listener
  // class in the .h file.

  // When non-NULL, indicates we are building the hashtable from the
  // VisitedLinkDatabase.
  scoped_refptr<TableBuilder> table_builder_;

  // Contains the fingerprints of visited links that have been added or deleted
  // on the main thread since we started building the hashtable on the DB
  // thread.
  std::set<VisitedLink> added_during_build_;
  std::set<VisitedLink> deleted_during_build_;

  // Shared memory consisting of a PartitionedSharedHeader followed by the
  // table.
  base::MappedReadOnlyRegion mapped_table_memory_;

  // Number of non-empty items in the table, used to compute fullness.
  int32_t used_items_ = 0;

  // Reference to the browser context that this object belongs to
  // (it knows the path to where the data is stored).
  raw_ptr<content::BrowserContext> browser_context_ = nullptr;

  // Client owns the delegate and is responsible for it being valid through
  // the lifetime this PartitionedVisitedLinkWriter.
  raw_ptr<VisitedLinkDelegate> delegate_;

  // VisitedLinkEventListener to handle incoming events.
  std::unique_ptr<Listener> listener_;

  // Contains every per-origin salt used in creating the hashtable. Callers
  // should only access on the main (UI) thread.
  //
  // NOTE: When VisitedLinkWriter is created, `salts_` is initially empty.
  // The <origin, salt> pairs populating the map are calculated on a background
  // thread and assigned on the main thread. When this is happening,
  // `table_builder_` is non-null, and `salts_` CANNOT be added to or accessed
  // by the UI thread.
  //
  // Once initialization is complete and `table_builder_` is set to null again,
  // `salts_` can be added to and accessed by the UI thread, whether we are
  // adding new visits via the History Service or sending salt values via the
  // VisitedLinksNavigationThrottle.
  //
  // TODO(crbug.com/330548738): Currently we store all salts relevant to this
  // profile in this one map, but there can be many StoragePartitions per
  // profile. We should revisit in a future phase to take into account which
  // StoragePartition each origin is being committed to.
  std::map<url::Origin, uint64_t> salts_;

  // Testing values ----------------------------------------------------------

  // Set to fail CreateVisitedLinkTable(), to simulate shared memory allocation
  // failure. This is used for testing, will be false in production.
  static bool fail_table_creation_for_testing_;

  // Set to prevent us from building from the VisitedLinkDatabase. This is used
  // for testing and will be false in production.
  const bool suppress_build_ = false;

  // When nonzero, overrides the table size for testing.
  const int32_t table_size_override_ = 0;

  // When set, indicates the task that should be run after the next build from
  // history is complete.
  base::OnceClosure build_complete_task_;

  base::WeakPtrFactory<PartitionedVisitedLinkWriter> weak_ptr_factory_{this};
};

// NOTE: These methods are defined inline here, so we can share the compilation
// of partitioned_visitedlink_writer.cc between the browser and the unit/perf
// tests.

#if defined(UNIT_TEST) || defined(PERF_TEST) || !defined(NDEBUG)
inline void PartitionedVisitedLinkWriter::DebugValidate() {
  int32_t used_count = 0;
  for (int32_t i = 0; i < table_length_; i++) {
    if (hash_table_[i]) {
      used_count++;
    }
  }
  DCHECK_EQ(used_count, used_items_);
}
#endif

}  // namespace visitedlink

#endif  // COMPONENTS_VISITEDLINK_BROWSER_PARTITIONED_VISITEDLINK_WRITER_H_
