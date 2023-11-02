// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DRIVE_RESOURCE_METADATA_STORAGE_H_
#define COMPONENTS_DRIVE_RESOURCE_METADATA_STORAGE_H_

#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "components/drive/drive.pb.h"
#include "components/drive/file_errors.h"

namespace base {
class SequencedTaskRunner;
}

namespace leveldb {
class DB;
class Iterator;
}

namespace drive {

class ResourceEntry;
class ResourceMetadataHeader;

namespace internal {

// Storage for ResourceMetadata which is responsible to manage resource
// entries and child-parent relationships between entries.
class ResourceMetadataStorage {
 public:
  // This should be incremented when incompatibility change is made to DB
  // format.
  static constexpr int kDBVersion = 19;

  // Object to iterate over entries stored in this storage.
  class Iterator {
   public:
    explicit Iterator(std::unique_ptr<leveldb::Iterator> it);

    Iterator(const Iterator&) = delete;
    Iterator& operator=(const Iterator&) = delete;

    ~Iterator();

    // Returns true if this iterator cannot advance any more and does not point
    // to a valid entry. Get() and Advance() should not be called in such cases.
    bool IsAtEnd() const;

    // Returns the ID of the entry currently pointed by this object.
    std::string GetID() const;

    // Returns the entry currently pointed by this object.
    const ResourceEntry& GetValue() const;

    // Advances to the next entry.
    void Advance();

    // Returns true if this object has encountered any error.
    bool HasError() const;

   private:
    ResourceEntry entry_;
    std::unique_ptr<leveldb::Iterator> it_;
  };

  // Cache information recovered from trashed DB.
  struct RecoveredCacheInfo {
    RecoveredCacheInfo();
    ~RecoveredCacheInfo();

    bool is_dirty;
    std::string md5;
    std::string title;
  };
  using RecoveredCacheInfoMap = std::map<std::string, RecoveredCacheInfo>;

  // Returns true if the DB was successfully upgraded to the newest version.
  static bool UpgradeOldDB(const base::FilePath& directory_path);

  ResourceMetadataStorage(const base::FilePath& directory_path,
                          base::SequencedTaskRunner* blocking_task_runner);

  ResourceMetadataStorage(const ResourceMetadataStorage&) = delete;
  ResourceMetadataStorage& operator=(const ResourceMetadataStorage&) = delete;

  const base::FilePath& directory_path() const { return directory_path_; }

  // Returns true when cache entries were not loaded to the DB during
  // initialization.
  bool cache_file_scan_is_needed() const { return cache_file_scan_is_needed_; }

  // Initializes this object.
  bool Initialize();

  // Destroys this object.
  void Destroy();

  // Collects cache info from trashed resource map DB.
  void RecoverCacheInfoFromTrashedResourceMap(RecoveredCacheInfoMap* out_info);

  // Sets the largest changestamp.
  FileError SetLargestChangestamp(int64_t largest_changestamp);

  // Gets the largest changestamp.
  FileError GetLargestChangestamp(int64_t* largest_changestamp);

  FileError GetStartPageToken(std::string* out_value);

  FileError SetStartPageToken(const std::string& value);

  // Puts the entry to this storage.
  FileError PutEntry(const ResourceEntry& entry);

  // Gets an entry stored in this storage.
  FileError GetEntry(const std::string& id, ResourceEntry* out_entry);

  // Removes an entry from this storage.
  FileError RemoveEntry(const std::string& id);

  // Returns an object to iterate over entries stored in this storage.
  std::unique_ptr<Iterator> GetIterator();

  // Returns the ID of the parent's child.
  FileError GetChild(const std::string& parent_id,
                     const std::string& child_name,
                     std::string* child_id) const;

  // Returns the IDs of the parent's children.
  FileError GetChildren(const std::string& parent_id,
                        std::vector<std::string>* children) const;

  // Returns the local ID associated with the given resource ID.
  FileError GetIdByResourceId(const std::string& resource_id,
                              std::string* out_id) const;

 private:
  friend class ResourceMetadataStorageTest;

  // To destruct this object, use Destroy().
  ~ResourceMetadataStorage();

  // Used to implement Destroy().
  void DestroyOnBlockingPool();

  // Returns a string to be used as a key for child entry.
  // Exposed in the header for unit testing.
  static std::string GetChildEntryKey(const std::string& parent_id,
                                      const std::string& child_name);

  // Puts header.
  FileError PutHeader(const ResourceMetadataHeader& header);

  // Gets header.
  FileError GetHeader(ResourceMetadataHeader* out_header) const;

  // Checks validity of the data.
  bool CheckValidity();

  // Path to the directory where the data is stored.
  const base::FilePath directory_path_;

  bool cache_file_scan_is_needed_;

  // Entries stored in this storage.
  std::unique_ptr<leveldb::DB> resource_map_;

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
};

}  // namespace internal
}  // namespace drive

#endif  // COMPONENTS_DRIVE_RESOURCE_METADATA_STORAGE_H_
