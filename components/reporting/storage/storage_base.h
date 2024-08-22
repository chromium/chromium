// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REPORTING_STORAGE_STORAGE_BASE_H_
#define COMPONENTS_REPORTING_STORAGE_STORAGE_BASE_H_

#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "base/feature_list.h"
#include "base/files/file.h"
#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/reporting/encryption/encryption_module_interface.h"
#include "components/reporting/encryption/verification.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/storage/storage_configuration.h"
#include "components/reporting/storage/storage_queue.h"
#include "components/reporting/storage/storage_uploader_interface.h"
#include "components/reporting/util/status.h"

// This file is for common logic shared in both implementations of the storage
// class: multi-generation and single-generation (legacy).
namespace reporting {

// Helper class keeps all `StorageQueue`s and manages controlled degradation
// if is is enabled. The queues are indexed by priority and generation, even
// though legacy Storage does not actually use generation.
// Note: Destruction of `Storage` will  trigger destruction of all
// `StorageQueues` inside `QueuesContainer`, but may not destroy
// `QueuesContainer` itself since components besides `Storage` may hold the
// references to `QueuesContainer`. Destruction of `QueuesContainer` will happen
// when its reference count reaches zero.
class QueuesContainer
    : public base::RefCountedDeleteOnSequence<QueuesContainer> {
 public:
  // Factory method creates task runner and the container.
  static scoped_refptr<QueuesContainer> Create(
      bool storage_degradation_enabled);
  QueuesContainer(const QueuesContainer&) = delete;
  QueuesContainer& operator=(const QueuesContainer) = delete;

  Status AddQueue(Priority priority, scoped_refptr<StorageQueue> queue);

  // Helper method that selects queue by priority. Returns error
  // if priority does not match any queue.
  StatusOr<scoped_refptr<StorageQueue>> GetQueue(Priority priority) const;

  // Asynchronously constructs references to all storage queues to consider
  // for degradation for the sake of the current `queue` (candidates queue is
  // empty if degradation is disabled). The candidate queues are ordered from
  // lowest priority to the one below the current one. The method is made
  // `static` so that even if weak pointer is null, we still can respond (with
  // an empty result).
  static void GetDegradationCandidates(
      base::WeakPtr<QueuesContainer> container,
      Priority priority,
      const scoped_refptr<StorageQueue> queue,
      base::OnceCallback<void(std::queue<scoped_refptr<StorageQueue>>)>
          result_cb);

  void RegisterCompletionCallback(base::OnceClosure callback);

  base::WeakPtr<QueuesContainer> GetWeakPtr();

  // Accessors.
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner() const;

 private:
  friend base::RefCountedDeleteOnSequence<QueuesContainer>;
  friend class base::DeleteHelper<QueuesContainer>;

  // Map used to retrieve queues for writes, confirms, and flushes.
  struct QueuesMapHash {
    size_t operator()(Priority priority) const noexcept {
      static constexpr std::hash<Priority> priority_hasher;
      return priority_hasher(priority);
    }
  };

  using QueuesMap =
      std::unordered_map<Priority, scoped_refptr<StorageQueue>, QueuesMapHash>;

  QueuesContainer(
      bool storage_degradation_enabled,
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner);

  ~QueuesContainer();

  const scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;
  SEQUENCE_CHECKER(sequence_checker_);

  bool storage_degradation_enabled_ GUARDED_BY_CONTEXT(sequence_checker_);

  QueuesMap queues_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Weak ptr factory.
  base::WeakPtrFactory<QueuesContainer> weak_ptr_factory_{this};
};

// Bridge class for uploading records from a queue to storage.
class QueueUploaderInterface : public UploaderInterface {
 public:
  QueueUploaderInterface(
      Priority priority,
      std::unique_ptr<UploaderInterface> storage_uploader_interface);
  ~QueueUploaderInterface() override;

  // Factory method.
  static void AsyncProvideUploader(
      Priority priority,
      UploaderInterface::AsyncStartUploaderCb async_start_upload_cb,
      scoped_refptr<EncryptionModuleInterface> encryption_module,
      UploaderInterface::UploadReason reason,
      UploaderInterfaceResultCb start_uploader_cb);

  void ProcessRecord(EncryptedRecord encrypted_record,
                     ScopedReservation scoped_reservation,
                     base::OnceCallback<void(bool)> processed_cb) override;

  void ProcessGap(SequenceInformation start,
                  uint64_t count,
                  base::OnceCallback<void(bool)> processed_cb) override;

  void Completed(Status final_status) override;

 private:
  static void WrapInstantiatedUploader(
      Priority priority,
      UploaderInterfaceResultCb start_uploader_cb,
      StatusOr<std::unique_ptr<UploaderInterface>> uploader_result);

  const Priority priority_;
  const std::unique_ptr<UploaderInterface> storage_uploader_interface_;
};

// Class that represents the encryption key in storage.
class KeyInStorage {
 public:
  KeyInStorage(std::string_view signature_verification_public_key,
               const base::FilePath& directory);
  ~KeyInStorage();

  // Uploads signed encryption key to a file with an |index| >=
  // |next_key_file_index_|. Returns status in case of any error. If succeeds,
  // removes all files with lower indexes (if any). Called every time encryption
  // key is updated.
  Status UploadKeyFile(const SignedEncryptionInfo& signed_encryption_key);

  // Locates and downloads the latest valid enumeration keys file.
  // Atomically sets |next_key_file_index_| to the a value larger than any found
  // file. Returns key and key id pair, or error status (NOT_FOUND if no valid
  // file has been found). Called once during initialization only.
  StatusOr<std::pair<std::string, EncryptionModuleInterface::PublicKeyId>>
  DownloadKeyFile();

  Status VerifySignature(const SignedEncryptionInfo& signed_encryption_key);

 private:
  // Writes key into file. Called during key upload.
  Status WriteKeyInfoFile(uint64_t new_file_index,
                          const SignedEncryptionInfo& signed_encryption_key);

  // Enumerates key files and deletes those with index lower than
  // |new_file_index|. Called during key upload.
  void RemoveKeyFilesWithLowerIndexes(uint64_t new_file_index);

  // Enumerates possible key files, collects the ones that have valid name,
  // sets next_key_file_index_ to a value that is definitely not used.
  // Called once, during initialization.
  void EnumerateKeyFiles(
      std::unordered_set<base::FilePath>* all_key_files,
      std::map<uint64_t, base::FilePath, std::greater<>>* found_key_files);

  // Enumerates found key files and locates one with the highest index and
  // valid key. Returns pair of file name and loaded signed key proto.
  // Called once, during initialization.
  std::optional<std::pair<base::FilePath, SignedEncryptionInfo>>
  LocateValidKeyAndParse(
      const std::map<uint64_t, base::FilePath, std::greater<>>&
          found_key_files);

  // Index of the file to serialize the signed key to.
  // Initialized to the next available number or 0, if none present.
  // Every time a new key is received, it is stored in a file with the next
  // index; however, any file found with the matching signature can be used
  // to successfully encrypt records and for the server to then decrypt them.
  std::atomic<uint64_t> next_key_file_index_{0};

  const SignatureVerifier verifier_;

  const base::FilePath directory_;
};

}  // namespace reporting

#endif  // COMPONENTS_REPORTING_STORAGE_STORAGE_BASE_H_
