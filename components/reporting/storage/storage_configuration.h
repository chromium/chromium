// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_STORAGE_STORAGE_CONFIGURATION_H_
#define COMPONENTS_REPORTING_STORAGE_STORAGE_CONFIGURATION_H_

#include <atomic>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/time/time.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/resources/resource_manager.h"

namespace reporting {

using GenerationGuid = std::string;
using DMtoken = std::string;

inline constexpr char kDeviceDMToken[] = "";

// Forward declaration.
class QueueOptions;

// Storage options class allowing to set parameters individually, e.g.:
// Storage::Create(Options()
//                     .set_directory("/var/cache/reporting")
//                     .set_max_record_size(4 * 1024u)
//                     .set_max_total_files_size(64 * 1024u * 1024u)
//                     .set_max_total_memory_size(256 * 1024u),
//                  ...}
//                 callback);
class StorageOptions {
 public:
  // Default period for Storage to check for encryption key.
  static constexpr base::TimeDelta kDefaultKeyCheckPeriod = base::Seconds(1);

  // Default delay until unused queue is garbage collected.
  static constexpr base::TimeDelta kDefaultQueueGarbageCollectionPeriod =
      base::Days(5);

  using QueuesOptionsList = std::vector<std::pair<Priority, QueueOptions>>;

  // Multi-generation state of priorities.
  // Declared as refcounted object in order to kep single instance over all
  // cases of Options copying.
  class MultiGenerational
      : public base::RefCountedThreadSafe<MultiGenerational> {
   public:
    MultiGenerational();
    MultiGenerational(const MultiGenerational&) = delete;
    MultiGenerational& operator=(const MultiGenerational&) = delete;

    // Retrieve the flag.
    bool get(Priority priority) const;

    // Atomically set the flag.
    void set(Priority priority, bool state);

   private:
    friend base::RefCountedThreadSafe<MultiGenerational>;

    ~MultiGenerational() = default;

    // At counstruction all Priorities settings are set to 'false'.
    std::array<std::atomic<bool>, Priority_ARRAYSIZE> is_multi_generational_;
  };

  // Constructor. `modify_queue_options_for_tests` callback allows to adjust
  // queue options (used in tests only, Set to DoNothing in prod).
  explicit StorageOptions(base::RepeatingCallback<void(Priority, QueueOptions&)>
                              modify_queue_options_for_tests =
                                  base::DoNothing());
  StorageOptions(const StorageOptions& options);
  StorageOptions& operator=(const StorageOptions& options) = delete;
  virtual ~StorageOptions();
  StorageOptions& set_directory(const base::FilePath& directory) {
    directory_ = directory;
    return *this;
  }

  // Generates queue options based on a given priority.
  // Calls `modify_queue_options_for_tests_` before returning (for tests only).
  QueueOptions ProduceQueueOptions(Priority priority) const;

  // Generates list queue options. One QueueOption for each priority, in order
  // of priorities. Used when enumerating storage queue directories only.
  QueuesOptionsList ProduceQueuesOptionsList() const;

  // Exposes priorities in order.
  static base::span<const Priority> GetPrioritiesOrder();

  StorageOptions& set_signature_verification_public_key(
      std::string_view signature_verification_public_key) {
    signature_verification_public_key_ =
        std::string(signature_verification_public_key);
    return *this;
  }
  StorageOptions& set_max_record_size(size_t max_record_size) {
    max_record_size_ = max_record_size;
    return *this;
  }
  StorageOptions& set_max_total_files_size(uint64_t max_total_files_size) {
    disk_space_resource_ =
        base::MakeRefCounted<ResourceManager>(max_total_files_size);
    return *this;
  }
  StorageOptions& set_max_total_memory_size(uint64_t max_total_memory_size) {
    memory_resource_ =
        base::MakeRefCounted<ResourceManager>(max_total_memory_size);
    return *this;
  }
  StorageOptions& set_key_check_period(base::TimeDelta key_check_period) {
    key_check_period_ = key_check_period;
    return *this;
  }
  StorageOptions& set_inactive_queue_self_destruct_delay(
      base::TimeDelta inactive_queue_self_destruct_delay) {
    inactive_queue_self_destruct_delay_ = inactive_queue_self_destruct_delay;
    return *this;
  }

  const base::FilePath& directory() const { return directory_; }
  std::string_view signature_verification_public_key() const {
    return signature_verification_public_key_;
  }
  size_t max_record_size() const { return max_record_size_; }

  bool is_multi_generational(Priority priority) const {
    return is_multi_generational_->get(priority);
  }

  void set_multi_generational(Priority priority, bool state) const {
    is_multi_generational_->set(priority, state);
  }

  uint64_t max_total_files_size() const {
    return disk_space_resource_->GetTotal();
  }
  uint64_t max_total_memory_size() const {
    return memory_resource_->GetTotal();
  }

  scoped_refptr<ResourceManager> disk_space_resource() const {
    return disk_space_resource_.get();
  }
  scoped_refptr<ResourceManager> memory_resource() const {
    return memory_resource_;
  }

  base::TimeDelta key_check_period() const { return key_check_period_; }

  base::TimeDelta inactive_queue_self_destruct_delay() const {
    return inactive_queue_self_destruct_delay_;
  }

 private:
  // Populates queue options for the given priority.
  QueueOptions PopulateQueueOptions(Priority priority) const;

  // Subdirectory of the location assigned for this Storage.
  base::FilePath directory_;

  // Public key for signature verification when encryption key
  // is delivered to Storage.
  std::string signature_verification_public_key_;

  // Frequency with which Storage will check to see if a new encryption key
  // should be requested.
  base::TimeDelta key_check_period_;

  // Delay until inactive queue self-destruct.
  base::TimeDelta inactive_queue_self_destruct_delay_ =
      StorageOptions::kDefaultQueueGarbageCollectionPeriod;

  // Maximum record size.
  size_t max_record_size_ = 1U * 1024UL * 1024UL;  // 1 MiB

  // Map of atomic flags indicating whether each priority is set to
  // multi-generation or single-generation (legacy) action. The map is
  // constructed once and then only the values of the flags can change.
  const scoped_refptr<MultiGenerational> is_multi_generational_;

  // Resources managements.
  scoped_refptr<ResourceManager> memory_resource_;
  scoped_refptr<ResourceManager> disk_space_resource_;

  // Callback that can adjust queue options (used in tests only).
  const base::RepeatingCallback<void(Priority, QueueOptions&)>
      modify_queue_options_for_tests_;
};

// Single queue options class allowing to set parameters individually, e.g.:
// StorageQueue::Create(QueueOptions(storage_options)
//                  .set_subdirectory("reporting")
//                  .set_file_prefix(FILE_PATH_LITERAL("p00000001")),
//                 callback);
// storage_options must outlive QueueOptions.
class QueueOptions {
 public:
  explicit QueueOptions(const StorageOptions& storage_options);
  QueueOptions(const QueueOptions& options);
  QueueOptions& operator=(const QueueOptions& options) = delete;
  QueueOptions& set_subdirectory(
      const base::FilePath::StringType& subdirectory) {
    directory_ = storage_options_.directory().Append(subdirectory);
    return *this;
  }
  QueueOptions& set_subdirectory_extension(std::string_view extension) {
    directory_ = directory_.AddExtensionASCII(extension);
    return *this;
  }
  QueueOptions& set_file_prefix(const base::FilePath::StringType& file_prefix) {
    file_prefix_ = file_prefix;
    return *this;
  }
  QueueOptions& set_upload_period(base::TimeDelta upload_period) {
    upload_period_ = upload_period;
    return *this;
  }
  QueueOptions& set_upload_retry_delay(base::TimeDelta upload_retry_delay) {
    upload_retry_delay_ = upload_retry_delay;
    return *this;
  }
  QueueOptions& set_max_single_file_size(uint64_t max_single_file_size) {
    max_single_file_size_ = max_single_file_size;
    return *this;
  }
  QueueOptions& set_can_shed_records(bool can_shed_records) {
    can_shed_records_ = can_shed_records;
    return *this;
  }
  const base::FilePath& directory() const { return directory_; }
  const base::FilePath::StringType& file_prefix() const { return file_prefix_; }
  size_t max_record_size() const { return storage_options_.max_record_size(); }
  size_t max_total_files_size() const {
    return storage_options_.max_total_files_size();
  }
  size_t max_total_memory_size() const {
    return storage_options_.max_total_memory_size();
  }
  uint64_t max_single_file_size() const { return max_single_file_size_; }
  base::TimeDelta upload_period() const { return upload_period_; }
  base::TimeDelta upload_retry_delay() const { return upload_retry_delay_; }
  base::TimeDelta inactive_queue_self_destruct_delay() const {
    return storage_options_.inactive_queue_self_destruct_delay();
  }
  bool can_shed_records() const { return can_shed_records_; }
  scoped_refptr<ResourceManager> disk_space_resource() const {
    return storage_options_.disk_space_resource();
  }
  scoped_refptr<ResourceManager> memory_resource() const {
    return storage_options_.memory_resource();
  }

 private:
  // Whole storage options, which this queue options are based on.
  const StorageOptions storage_options_;

  // Subdirectory of the Storage location assigned for this StorageQueue.
  base::FilePath directory_;
  // Prefix of data files assigned for this StorageQueue.
  base::FilePath::StringType file_prefix_;
  // Time period the data is uploaded with.
  // If 0, uploaded immediately after a new record is stored
  // (this setting is intended for the immediate priority).
  // Can be set to infinity - in that case Flush() is expected to be
  // called from time to time.
  base::TimeDelta upload_period_;
  // Retry delay for a failed upload. If 0, not retried at all
  // (should only be set to 0 in periodic queues).
  base::TimeDelta upload_retry_delay_;
  // Does the queue have the ability to perform a record shedding process on
  // itself. Only SECURITY can't shed.
  bool can_shed_records_ = true;
  // Cut-off file size of an individual queue
  // When file exceeds this size, the new file is created
  // for further records. Note that each file must have at least
  // one record before it is closed, regardless of that record size.
  uint64_t max_single_file_size_ = 2UL * 1024UL * 1024UL;
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_STORAGE_STORAGE_CONFIGURATION_H_
