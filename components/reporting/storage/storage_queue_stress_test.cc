// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "components/reporting/storage/storage_queue.h"

#include <cstdint>
#include <initializer_list>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/reporting/compression/compression_module.h"
#include "components/reporting/compression/test_compression_module.h"
#include "components/reporting/encryption/test_encryption_module.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/resources/resource_interface.h"
#include "components/reporting/storage/storage_configuration.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "crypto/sha2.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using ::testing::_;
using ::testing::Between;
using ::testing::Eq;
using ::testing::Invoke;
using ::testing::NotNull;
using ::testing::Return;
using ::testing::Sequence;
using ::testing::StrEq;
using ::testing::WithArg;

namespace reporting {
namespace {

constexpr size_t kTotalQueueStarts = 4;
constexpr size_t kTotalWritesPerStart = 16;
constexpr char kDataPrefix[] = "Rec";

class TestUploadClient : public UploaderInterface {
 public:
  // Mapping of <generation id, sequencing id> to matching record digest.
  // Whenever a record is uploaded and includes last record digest, this map
  // should have that digest already recorded. Only the first record in a
  // generation is uploaded without last record digest.
  using LastRecordDigestMap = base::flat_map<
      std::pair<int64_t /*generation id */, int64_t /*sequencing id*/>,
      absl::optional<std::string /*digest*/>>;

  explicit TestUploadClient(LastRecordDigestMap* last_record_digest_map)
      : last_record_digest_map_(last_record_digest_map) {}

  void ProcessRecord(EncryptedRecord encrypted_record,
                     base::OnceCallback<void(bool)> processed_cb) override {
    WrappedRecord wrapped_record;
    ASSERT_TRUE(wrapped_record.ParseFromString(
        encrypted_record.encrypted_wrapped_record()));
    // Verify generation match.
    const auto& sequence_information = encrypted_record.sequence_information();
    if (!generation_id_.has_value()) {
      generation_id_ = sequence_information.generation_id();
    } else {
      ASSERT_THAT(generation_id_.value(),
                  Eq(sequence_information.generation_id()));
    }

    // Verify digest and its match.
    // Last record digest is not verified yet, since duplicate records are
    // accepted in this test.
    {
      std::string serialized_record;
      wrapped_record.record().SerializeToString(&serialized_record);
      const auto record_digest = crypto::SHA256HashString(serialized_record);
      DCHECK_EQ(record_digest.size(), crypto::kSHA256Length);
      ASSERT_THAT(record_digest, Eq(wrapped_record.record_digest()));
      // Store record digest for the next record in sequence to verify.
      last_record_digest_map_->emplace(
          std::make_pair(sequence_information.sequencing_id(),
                         sequence_information.generation_id()),
          record_digest);
      // If last record digest is present, match it and validate.
      if (wrapped_record.has_last_record_digest()) {
        auto it = last_record_digest_map_->find(
            std::make_pair(sequence_information.sequencing_id() - 1,
                           sequence_information.generation_id()));
        if (it != last_record_digest_map_->end() && it->second.has_value()) {
          ASSERT_THAT(it->second.value(),
                      Eq(wrapped_record.last_record_digest()))
              << "seq_id=" << sequence_information.sequencing_id();
        }
      }
    }

    std::move(processed_cb).Run(true);
  }

  void ProcessGap(SequenceInformation sequence_information,
                  uint64_t count,
                  base::OnceCallback<void(bool)> processed_cb) override {
    ASSERT_TRUE(false) << "There should be no gaps";
  }

  void Completed(Status status) override { ASSERT_OK(status); }

 private:
  absl::optional<int64_t> generation_id_;
  const raw_ptr<LastRecordDigestMap> last_record_digest_map_;

  Sequence test_upload_sequence_;
};

class StorageQueueStressTest : public ::testing::TestWithParam<size_t> {
 public:
  void SetUp() override {
    // Enable compression.
    scoped_feature_list_.InitFromCommandLine(
        {CompressionModule::kCompressReportingFeature}, {});

    ASSERT_TRUE(location_.CreateUniqueTempDir());
    options_.set_directory(base::FilePath(location_.GetPath()));
  }

  void TearDown() override {
    ResetTestStorageQueue();
    // Make sure all memory is deallocated.
    ASSERT_THAT(GetMemoryResource()->GetUsed(), Eq(0u));
    // Make sure all disk is not reserved (files remain, but Storage is not
    // responsible for them anymore).
    ASSERT_THAT(GetDiskResource()->GetUsed(), Eq(0u));
  }

  void CreateTestStorageQueueOrDie(const QueueOptions& options) {
    ASSERT_FALSE(storage_queue_) << "StorageQueue already assigned";
    test_encryption_module_ =
        base::MakeRefCounted<test::TestEncryptionModule>();
    test_compression_module_ =
        base::MakeRefCounted<test::TestCompressionModule>();
    test::TestEvent<Status> key_update_event;
    test_encryption_module_->UpdateAsymmetricKey("DUMMY KEY", 0,
                                                 key_update_event.cb());
    ASSERT_OK(key_update_event.result());
    test::TestEvent<StatusOr<scoped_refptr<StorageQueue>>>
        storage_queue_create_event;
    StorageQueue::Create(
        options,
        base::BindRepeating(&StorageQueueStressTest::AsyncStartTestUploader,
                            base::Unretained(this)),
        test_encryption_module_, test_compression_module_,
        storage_queue_create_event.cb());
    StatusOr<scoped_refptr<StorageQueue>> storage_queue_result =
        storage_queue_create_event.result();
    ASSERT_OK(storage_queue_result) << "Failed to create StorageQueue, error="
                                    << storage_queue_result.status();
    storage_queue_ = std::move(storage_queue_result.ValueOrDie());
  }

  void ResetTestStorageQueue() {
    // Let everything ongoing to finish.
    task_environment_.RunUntilIdle();
    storage_queue_.reset();
    // StorageQueue is destructed on a thread,
    // so we need to wait for it to destruct.
    task_environment_.RunUntilIdle();
  }

  QueueOptions BuildStorageQueueOptionsImmediate() const {
    return QueueOptions(options_)
        .set_subdirectory(FILE_PATH_LITERAL("D1"))
        .set_file_prefix(FILE_PATH_LITERAL("F0001"))
        .set_max_single_file_size(GetParam());
  }

  QueueOptions BuildStorageQueueOptionsPeriodic(
      base::TimeDelta upload_period = base::Seconds(1)) const {
    return BuildStorageQueueOptionsImmediate().set_upload_period(upload_period);
  }

  QueueOptions BuildStorageQueueOptionsOnlyManual() const {
    return BuildStorageQueueOptionsPeriodic(base::TimeDelta::Max());
  }

  void AsyncStartTestUploader(
      UploaderInterface::UploadReason reason,
      UploaderInterface::UploaderInterfaceResultCb start_uploader_cb) {
    // Ignore reason for stress test.
    std::move(start_uploader_cb)
        .Run(std::make_unique<TestUploadClient>(&last_record_digest_map_));
  }

  void WriteStringAsync(base::StringPiece data,
                        base::OnceCallback<void(Status)> cb) {
    EXPECT_TRUE(storage_queue_) << "StorageQueue not created yet";
    Record record;
    record.mutable_data()->assign(data.data(), data.size());
    record.set_destination(UPLOAD_EVENTS);
    record.set_dm_token("DM TOKEN");
    storage_queue_->Write(std::move(record), std::move(cb));
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir location_;
  StorageOptions options_;
  scoped_refptr<test::TestEncryptionModule> test_encryption_module_;
  scoped_refptr<test::TestCompressionModule> test_compression_module_;
  scoped_refptr<StorageQueue> storage_queue_;

  // Test-wide global mapping of <generation id, sequencing id> to record
  // digest. Serves all TestUploadClients created by test fixture.
  TestUploadClient::LastRecordDigestMap last_record_digest_map_;
};

TEST_P(StorageQueueStressTest,
       WriteIntoNewStorageQueueReopenWriteMoreAndUpload) {
  for (size_t iStart = 0; iStart < kTotalQueueStarts; ++iStart) {
    test::TestCallbackWaiter write_waiter;
    base::RepeatingCallback<void(Status)> cb = base::BindRepeating(
        [](test::TestCallbackWaiter* waiter, Status status) {
          EXPECT_OK(status);
          waiter->Signal();
        },
        &write_waiter);

    SCOPED_TRACE(base::StrCat({"Create ", base::NumberToString(iStart)}));
    CreateTestStorageQueueOrDie(BuildStorageQueueOptionsOnlyManual());

    // Write into the queue at random order (simultaneously).
    SCOPED_TRACE(base::StrCat({"Write ", base::NumberToString(iStart)}));
    const std::string rec_prefix =
        base::StrCat({kDataPrefix, base::NumberToString(iStart), "_"});
    for (size_t iRec = 0; iRec < kTotalWritesPerStart; ++iRec) {
      write_waiter.Attach();
      base::ThreadPool::PostTask(
          FROM_HERE, {base::TaskPriority::BEST_EFFORT},
          base::BindOnce(
              [](base::StringPiece rec_prefix, size_t iRec,
                 StorageQueueStressTest* test,
                 base::RepeatingCallback<void(Status)> cb) {
                test->WriteStringAsync(
                    base::StrCat({rec_prefix, base::NumberToString(iRec)}), cb);
              },
              rec_prefix, iRec, this, cb));
    }
    write_waiter.Wait();

    SCOPED_TRACE(base::StrCat({"Upload ", base::NumberToString(iStart)}));
    storage_queue_->Flush();

    SCOPED_TRACE(base::StrCat({"Reset ", base::NumberToString(iStart)}));
    ResetTestStorageQueue();
    EXPECT_THAT(last_record_digest_map_.size(),
                Eq((iStart + 1) * kTotalWritesPerStart));

    SCOPED_TRACE(base::StrCat({"Done ", base::NumberToString(iStart)}));
  }
}

INSTANTIATE_TEST_SUITE_P(
    VaryingFileSize,
    StorageQueueStressTest,
    testing::Values(1 * 1024LL, 2 * 1024LL, 3 * 1024LL, 4 * 1024LL));

}  // namespace
}  // namespace reporting
