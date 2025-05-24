// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_PERSISTENT_SYSTEM_PROFILE_H_
#define COMPONENTS_METRICS_PERSISTENT_SYSTEM_PROFILE_H_

#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/threading/thread_checker.h"
#include "third_party/metrics_proto/system_profile.pb.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
class PersistentMemoryAllocator;
}  // namespace base

namespace metrics {

// Manages a copy of the system profile inside persistent memory segments.
class PersistentSystemProfile {
 public:
  PersistentSystemProfile();

  PersistentSystemProfile(const PersistentSystemProfile&) = delete;
  PersistentSystemProfile& operator=(const PersistentSystemProfile&) = delete;

  ~PersistentSystemProfile();

  // This object can store records in multiple memory allocators.
  void RegisterPersistentAllocator(
      base::PersistentMemoryAllocator* memory_allocator);
  void DeregisterPersistentAllocator(
      base::PersistentMemoryAllocator* memory_allocator);

  // Stores a complete system profile. Use the version taking the serialized
  // version if available to avoid multiple serialization actions. The
  // |complete| flag indicates that this profile contains all known information
  // and can replace whatever exists. If the flag is false, the existing profile
  // will only be replaced if it is also incomplete. This method should not be
  // called too many times with incomplete profiles before setting a complete
  // profile to prevent impact on startup.
  void SetSystemProfile(const std::string& serialized_profile, bool complete);
  void SetSystemProfile(const SystemProfileProto& profile, bool complete);

  // Records the existence of a field trial.
  void AddFieldTrial(std::string_view trial, std::string_view group);

  // Removes the field trial from the system profile.
  void RemoveFieldTrial(std::string_view trial);

  // Tests if a persistent memory allocator contains an system profile.
  static bool HasSystemProfile(
      const base::PersistentMemoryAllocator& memory_allocator);

  // Retrieves the system profile from a persistent memory allocator. Returns
  // true if a profile was successfully retrieved. If null is passed for the
  // |system_profile|, only a basic check for the existence of one will be
  // done.
  static bool GetSystemProfile(
      const base::PersistentMemoryAllocator& memory_allocator,
      SystemProfileProto* system_profile);

 private:
  friend class PersistentSystemProfileTest;

  // Defines record types that can be stored inside our local Allocators.
  enum RecordType : uint8_t {
    kUnusedSpace = 0,  // The default value for empty memory.
    kSystemProfileProto,
    kFieldTrialInfo,
  };

  // A class for managing record allocations inside a persistent memory segment.
  class RecordAllocator {
   public:
    // Construct an allocator for writing.
    RecordAllocator(base::PersistentMemoryAllocator* memory_allocator,
                    size_t min_size);

    // Construct an allocator for reading.
    RecordAllocator(const base::PersistentMemoryAllocator* memory_allocator);

    // These methods manage writing records to the allocator. Do not mix these
    // with "read" calls; it's one or the other.
    void Reset();
    bool Write(RecordType type, std::string_view record);

    // Read a record from the allocator. Do not mix this with "write" calls;
    // it's one or the other.
    bool HasMoreData() const;
    bool Read(RecordType* type, std::string* record) const;

    base::PersistentMemoryAllocator* allocator() { return allocator_; }

    bool has_complete_profile() { return has_complete_profile_; }
    void set_complete_profile() { has_complete_profile_ = true; }

   private:
    // Advance to the next record segment in the memory allocator.
    bool NextSegment() const;

    // Advance to the next record segment, creating a new one if necessary with
    // sufficent |min_size| space.
    bool AddSegment(size_t min_size);

    // Writes data to the current position, updating the passed values past
    // the amount written. Returns false in case of an error.
    bool WriteData(RecordType type, const char** data, size_t* data_size);

    // Reads data from the current position, updating the passed string
    // in-place. |type| must be initialized to kUnusedSpace and |record| must
    // be an empty string before the first call but unchanged thereafter.
    // Returns true when record is complete.
    bool ReadData(RecordType* type, std::string* record) const;

    // This never changes but can't be "const" because vector calls operator=().
    raw_ptr<base::PersistentMemoryAllocator> allocator_;  // Storage location.

    // Indicates if a complete profile has been stored.
    bool has_complete_profile_;

    // These change even though the underlying data may be "const".
    mutable uint32_t alloc_reference_;  // Last storage block.
    mutable size_t alloc_size_;         // Size of the block.
    mutable size_t end_offset_;         // End of data in block.

    // Copy and assign are allowed for easy use with STL containers.
  };

  // Write a record to all registered allocators.
  void WriteToAll(RecordType type, std::string_view record);

  // Merges all "update" records into a system profile.
  static void MergeUpdateRecords(
      const base::PersistentMemoryAllocator& memory_allocator,
      SystemProfileProto* system_profile);

  // The list of registered persistent allocators, described by RecordAllocator
  // instances.
  std::vector<RecordAllocator> allocators_;

  // Indicates if a complete profile has been stored to all allocators.
  bool all_have_complete_profile_ = false;

  THREAD_CHECKER(thread_checker_);
};

// A singleton instance of the above.
class GlobalPersistentSystemProfile : public PersistentSystemProfile {
 public:
  static GlobalPersistentSystemProfile* GetInstance();

  GlobalPersistentSystemProfile(const GlobalPersistentSystemProfile&) = delete;
  GlobalPersistentSystemProfile& operator=(
      const GlobalPersistentSystemProfile&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<GlobalPersistentSystemProfile>;

  GlobalPersistentSystemProfile() = default;
  ~GlobalPersistentSystemProfile() = default;
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_PERSISTENT_SYSTEM_PROFILE_H_
