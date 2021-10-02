// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/storage/storage_queue.h"

#include <atomic>
#include <cstdint>
#include <initializer_list>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/reporting/compression/compression_module.h"
#include "components/reporting/compression/decompression.h"
#include "components/reporting/compression/test_compression_module.h"
#include "components/reporting/encryption/test_encryption_module.h"
#include "components/reporting/proto/record.pb.h"
#include "components/reporting/storage/resources/resource_interface.h"
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
using ::testing::WithoutArgs;

namespace reporting {
namespace {

// Test uploader counter - for generation of unique ids.
std::atomic<int64_t> next_uploader_id{0};

constexpr size_t kCompressionThreshold = 2;
const CompressionInformation::CompressionAlgorithm kCompressionType =
    CompressionInformation::COMPRESSION_SNAPPY;

// Metadata file name prefix.
const base::FilePath::CharType METADATA_NAME[] = FILE_PATH_LITERAL("META");

class MockUpload
    : public base::RefCountedDeleteOnSequence<::testing::NiceMock<MockUpload>> {
 public:
  explicit MockUpload(
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner)
      : base::RefCountedDeleteOnSequence<::testing::NiceMock<MockUpload>>(
            sequenced_task_runner) {
    DETACH_FROM_SEQUENCE(mock_uploader_checker_);
    upload_progress_.assign("Start\n");
  }
  MockUpload(const MockUpload& other) = delete;
  MockUpload& operator=(const MockUpload& other) = delete;

  void DoEncounterSeqId(int64_t uploader_id, int64_t sequence_id) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(mock_uploader_checker_);
    upload_progress_.append("SeqId: ")
        .append(base::NumberToString(sequence_id))
        .append("\n");
    EncounterSeqId(uploader_id, sequence_id);
  }
  bool DoUploadRecord(int64_t uploader_id,
                      int64_t sequence_id,
                      base::StringPiece data) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(mock_uploader_checker_);
    upload_progress_.append("Record: ")
        .append(base::NumberToString(sequence_id))
        .append(" '")
        .append(data.data(), data.size())
        .append("'\n");
    return UploadRecord(uploader_id, sequence_id, data);
  }
  bool DoUploadRecordFailure(int64_t uploader_id,
                             int64_t sequence_id,
                             Status status) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(mock_uploader_checker_);
    upload_progress_.append("Failure: ")
        .append(base::NumberToString(sequence_id))
        .append(" '")
        .append(status.ToString())
        .append("'\n");
    return UploadRecordFailure(uploader_id, sequence_id, status);
  }
  bool DoUploadGap(int64_t uploader_id, int64_t sequence_id, uint64_t count) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(mock_uploader_checker_);
    upload_progress_.append("Gap: ")
        .append(base::NumberToString(sequence_id))
        .append("(")
        .append(base::NumberToString(count))
        .append(")\n");
    return UploadGap(uploader_id, sequence_id, count);
  }
  void DoUploadComplete(int64_t uploader_id, Status status) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(mock_uploader_checker_);
    upload_progress_.append("Complete: ")
        .append(status.ToString())
        .append("\n");
    UploadComplete(uploader_id, status);
  }

  MOCK_METHOD(void,
              EncounterSeqId,
              (int64_t /*uploader_id*/, int64_t),
              (const));
  MOCK_METHOD(bool,
              UploadRecord,
              (int64_t /*uploader_id*/, int64_t, base::StringPiece),
              (const));
  MOCK_METHOD(bool,
              UploadRecordFailure,
              (int64_t /*uploader_id*/, int64_t, Status),
              (const));
  MOCK_METHOD(bool,
              UploadGap,
              (int64_t /*uploader_id*/, int64_t, uint64_t),
              (const));
  MOCK_METHOD(void, UploadComplete, (int64_t /*uploader_id*/, Status), (const));

 protected:
  virtual ~MockUpload() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(mock_uploader_checker_);
    LOG(ERROR) << "TestUploader: " << upload_progress_ << "End\n";
  }

 private:
  friend class base::RefCountedDeleteOnSequence<
      ::testing::NiceMock<MockUpload>>;

  SEQUENCE_CHECKER(mock_uploader_checker_);

  // Snapshot of data received in this upload (for debug purposes).
  std::string upload_progress_;
};

class TestUploader : public UploaderInterface {
 public:
  // Mapping of <generation id, sequencing id> to matching record digest.
  // Whenever a record is uploaded and includes last record digest, this map
  // should have that digest already recorded. Only the first record in a
  // generation is uploaded without last record digest. "Optional" is set to
  // no-value if there was a gap record instead of a real one.
  using LastRecordDigestMap = base::flat_map<
      std::pair<int64_t /*generation id */, int64_t /*sequencing id*/>,
      absl::optional<std::string /*digest*/>>;

  TestUploader(LastRecordDigestMap* last_record_digest_map,
               scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner)
      : uploader_id_(next_uploader_id.fetch_add(1)),
        last_record_digest_map_(last_record_digest_map),
        sequenced_task_runner_(sequenced_task_runner),
        mock_upload_(base::MakeRefCounted<::testing::NiceMock<MockUpload>>(
            sequenced_task_runner)) {
    DETACH_FROM_SEQUENCE(test_uploader_checker_);
  }

  ~TestUploader() override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(test_uploader_checker_);
  }

  void ProcessRecord(EncryptedRecord encrypted_record,
                     base::OnceCallback<void(bool)> processed_cb) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(test_uploader_checker_);
    auto sequencing_information = encrypted_record.sequencing_information();
    // Decompress encrypted_wrapped_record if is was compressed.
    WrappedRecord wrapped_record;
    if (encrypted_record.has_compression_information()) {
      std::string decompressed_record = Decompression::DecompressRecord(
          encrypted_record.encrypted_wrapped_record(),
          encrypted_record.compression_information());
      encrypted_record.set_encrypted_wrapped_record(decompressed_record);
    }
    ASSERT_TRUE(wrapped_record.ParseFromString(
        encrypted_record.encrypted_wrapped_record()));

    // Verify compression information is enabled or disabled.
    if (CompressionModule::is_enabled()) {
      EXPECT_TRUE(encrypted_record.has_compression_information());
    } else {
      EXPECT_FALSE(encrypted_record.has_compression_information());
    }

    ScheduleVerifyRecord(std::move(sequencing_information),
                         std::move(wrapped_record), std::move(processed_cb));
  }

  void ProcessGap(SequencingInformation sequencing_information,
                  uint64_t count,
                  base::OnceCallback<void(bool)> processed_cb) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(test_uploader_checker_);
    // Verify generation match.
    if (generation_id_.has_value() &&
        generation_id_.value() != sequencing_information.generation_id()) {
      sequenced_task_runner_->PostTask(
          FROM_HERE,
          base::BindOnce(
              [](SequencingInformation sequencing_information,
                 int64_t uploader_id, int64_t generation_id,
                 scoped_refptr<MockUpload> mock_upload,
                 base::OnceCallback<void(bool)> processed_cb) {
                std::move(processed_cb)
                    .Run(mock_upload->DoUploadRecordFailure(
                        uploader_id, sequencing_information.sequencing_id(),
                        Status(
                            error::DATA_LOSS,
                            base::StrCat({"Generation id mismatch, expected=",
                                          base::NumberToString(generation_id),
                                          " actual=",
                                          base::NumberToString(
                                              sequencing_information
                                                  .generation_id())}))));
                return;
              },
              std::move(sequencing_information), uploader_id_,
              generation_id_.value(), mock_upload_, std::move(processed_cb)));
      return;
    }
    if (!generation_id_.has_value()) {
      generation_id_ = sequencing_information.generation_id();
    }

    last_record_digest_map_->emplace(
        std::make_pair(sequencing_information.sequencing_id(),
                       sequencing_information.generation_id()),
        absl::nullopt);

    sequenced_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](uint64_t count, SequencingInformation sequencing_information,
               int64_t uploader_id, scoped_refptr<MockUpload> mock_upload,
               base::OnceCallback<void(bool)> processed_cb) {
              for (uint64_t c = 0; c < count; ++c) {
                mock_upload->DoEncounterSeqId(
                    uploader_id, sequencing_information.sequencing_id() +
                                     static_cast<int64_t>(c));
              }
              std::move(processed_cb)
                  .Run(mock_upload->DoUploadGap(
                      uploader_id, sequencing_information.sequencing_id(),
                      count));
            },
            count, std::move(sequencing_information), uploader_id_,
            mock_upload_, std::move(processed_cb)));
  }

  void Completed(Status status) override {
    DCHECK_CALLED_ON_VALID_SEQUENCE(test_uploader_checker_);
    sequenced_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&MockUpload::DoUploadComplete, mock_upload_,
                                  uploader_id_, status));
  }

  // Helper class for setting up mock uploader expectations of a successful
  // completion.
  class SetUp {
   public:
    SetUp(TestUploader* uploader, test::TestCallbackWaiter* waiter)
        : uploader_(uploader), waiter_(waiter) {}
    ~SetUp() {
      EXPECT_CALL(
          *uploader_->mock_upload_,
          UploadComplete(Eq(uploader_->uploader_id_), Eq(Status::StatusOK())))
          .InSequence(uploader_->test_upload_sequence_,
                      uploader_->test_encounter_sequence_)
          .WillOnce(
              WithoutArgs(Invoke(waiter_, &test::TestCallbackWaiter::Signal)));
    }

    SetUp& Required(int64_t sequence_number, base::StringPiece value) {
      EXPECT_CALL(*uploader_->mock_upload_,
                  UploadRecord(Eq(uploader_->uploader_id_), Eq(sequence_number),
                               StrEq(std::string(value))))
          .InSequence(uploader_->test_upload_sequence_)
          .WillOnce(Return(true));
      return *this;
    }

    SetUp& Possible(int64_t sequence_number, base::StringPiece value) {
      EXPECT_CALL(*uploader_->mock_upload_,
                  UploadRecord(Eq(uploader_->uploader_id_), Eq(sequence_number),
                               StrEq(std::string(value))))
          .Times(Between(0, 1))
          .InSequence(uploader_->test_upload_sequence_)
          .WillRepeatedly(Return(true));
      return *this;
    }

    SetUp& RequiredGap(int64_t sequence_number, uint64_t count) {
      EXPECT_CALL(*uploader_->mock_upload_,
                  UploadGap(Eq(uploader_->uploader_id_), Eq(sequence_number),
                            Eq(count)))
          .InSequence(uploader_->test_upload_sequence_)
          .WillOnce(Return(true));
      return *this;
    }

    SetUp& PossibleGap(int64_t sequence_number, uint64_t count) {
      EXPECT_CALL(*uploader_->mock_upload_,
                  UploadGap(Eq(uploader_->uploader_id_), Eq(sequence_number),
                            Eq(count)))
          .Times(Between(0, 1))
          .InSequence(uploader_->test_upload_sequence_)
          .WillRepeatedly(Return(true));
      return *this;
    }

    SetUp& Failure(int64_t sequence_number, Status error) {
      EXPECT_CALL(*uploader_->mock_upload_,
                  UploadRecordFailure(Eq(uploader_->uploader_id_),
                                      Eq(sequence_number), Eq(error)))
          .InSequence(uploader_->test_upload_sequence_)
          .WillOnce(Return(true));
      return *this;
    }

    // The following two expectations refer to the fact that specific
    // sequencing ids have been encountered, regardless of whether they
    // belonged to records or gaps. The expectations are set on a separate
    // test sequence.
    SetUp& RequiredSeqId(int64_t sequence_number) {
      EXPECT_CALL(
          *uploader_->mock_upload_,
          EncounterSeqId(Eq(uploader_->uploader_id_), Eq(sequence_number)))
          .Times(1)
          .InSequence(uploader_->test_encounter_sequence_);
      return *this;
    }

    SetUp& PossibleSeqId(int64_t sequence_number) {
      EXPECT_CALL(
          *uploader_->mock_upload_,
          EncounterSeqId(Eq(uploader_->uploader_id_), Eq(sequence_number)))
          .Times(Between(0, 1))
          .InSequence(uploader_->test_encounter_sequence_);
      return *this;
    }

   private:
    TestUploader* const uploader_;
    test::TestCallbackWaiter* const waiter_;
  };

 private:
  void ScheduleVerifyRecord(SequencingInformation sequencing_information,
                            WrappedRecord wrapped_record,
                            base::OnceCallback<void(bool)> processed_cb) {
    sequenced_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&TestUploader::VerifyRecord, base::Unretained(this),
                       std::move(sequencing_information),
                       std::move(wrapped_record), std::move(processed_cb)));
  }

  void VerifyRecord(SequencingInformation sequencing_information,
                    WrappedRecord wrapped_record,
                    base::OnceCallback<void(bool)> processed_cb) {
    // Verify generation match.
    if (generation_id_.has_value() &&
        generation_id_.value() != sequencing_information.generation_id()) {
      std::move(processed_cb)
          .Run(mock_upload_->DoUploadRecordFailure(
              uploader_id_, sequencing_information.sequencing_id(),
              Status(
                  error::DATA_LOSS,
                  base::StrCat(
                      {"Generation id mismatch, expected=",
                       base::NumberToString(generation_id_.value()), " actual=",
                       base::NumberToString(
                           sequencing_information.generation_id())}))));
      return;
    }
    if (!generation_id_.has_value()) {
      generation_id_ = sequencing_information.generation_id();
    }

    // Verify digest and its match.
    {
      std::string serialized_record;
      wrapped_record.record().SerializeToString(&serialized_record);
      const auto record_digest = crypto::SHA256HashString(serialized_record);
      DCHECK_EQ(record_digest.size(), crypto::kSHA256Length);
      if (record_digest != wrapped_record.record_digest()) {
        std::move(processed_cb)
            .Run(mock_upload_->DoUploadRecordFailure(
                uploader_id_, sequencing_information.sequencing_id(),
                Status(error::DATA_LOSS, "Record digest mismatch")));
        return;
      }
      // Store record digest for the next record in sequence to
      // verify.
      last_record_digest_map_->emplace(
          std::make_pair(sequencing_information.sequencing_id(),
                         sequencing_information.generation_id()),
          record_digest);
      // If last record digest is present, match it and validate,
      // unless previous record was a gap.
      if (wrapped_record.has_last_record_digest()) {
        auto it = last_record_digest_map_->find(
            std::make_pair(sequencing_information.sequencing_id() - 1,
                           sequencing_information.generation_id()));
        if (it == last_record_digest_map_->end() ||
            (it->second.has_value() &&
             it->second.value() != wrapped_record.last_record_digest())) {
          std::move(processed_cb)
              .Run(mock_upload_->DoUploadRecordFailure(
                  uploader_id_, sequencing_information.sequencing_id(),
                  Status(error::DATA_LOSS, "Last record digest mismatch")));
          return;
        }
      }
    }

    mock_upload_->DoEncounterSeqId(uploader_id_,
                                   sequencing_information.sequencing_id());
    std::move(processed_cb)
        .Run(mock_upload_->DoUploadRecord(
            uploader_id_, sequencing_information.sequencing_id(),
            wrapped_record.record().data()));
  }

  SEQUENCE_CHECKER(test_uploader_checker_);

  // Unique ID of the uploader - even if the uploader is allocated
  // on the same address as an earlier one (already released),
  // it will get a new id and thus will ensure the expectations
  // match the expected uploader.
  const int64_t uploader_id_;

  absl::optional<int64_t> generation_id_;
  LastRecordDigestMap* const last_record_digest_map_;

  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  scoped_refptr<::testing::NiceMock<MockUpload>> mock_upload_;

  Sequence test_encounter_sequence_;
  Sequence test_upload_sequence_;
};

class StorageQueueTest : public ::testing::TestWithParam<size_t> {
 protected:
  void SetUp() override {
    ASSERT_TRUE(location_.CreateUniqueTempDir());
    options_.set_directory(base::FilePath(location_.GetPath()))
        .set_single_file_size(GetParam());
    EXPECT_CALL(set_mock_uploader_expectations_, Call(_, NotNull()))
        .WillRepeatedly(Invoke([](UploaderInterface::UploadReason reason,
                                  TestUploader* test_uploader) {
          return Status(
              error::UNAVAILABLE,
              base::StrCat({"Test uploader not provided by the test, reason=",
                            base::NumberToString(reason)}));
        }));
  }

  void TearDown() override {
    ResetTestStorageQueue();
    // Make sure all memory is deallocated.
    ASSERT_THAT(GetMemoryResource()->GetUsed(), Eq(0u));
    // Make sure all disk is not reserved (files remain, but Storage is not
    // responsible for them anymore).
    ASSERT_THAT(GetDiskResource()->GetUsed(), Eq(0u));
    // Log next uploader id for possible verification.
    LOG(ERROR) << "Next uploader id=" << next_uploader_id.load();
  }

  void CreateTestStorageQueueOrDie(const QueueOptions& options) {
    ASSERT_FALSE(storage_queue_) << "TestStorageQueue already assigned";
    test_encryption_module_ =
        base::MakeRefCounted<test::TestEncryptionModule>();
    test::TestEvent<Status> key_update_event;
    test_encryption_module_->UpdateAsymmetricKey("DUMMY KEY", 0,
                                                 key_update_event.cb());
    ASSERT_OK(key_update_event.result());
    test::TestEvent<StatusOr<scoped_refptr<StorageQueue>>>
        storage_queue_create_event;
    StorageQueue::Create(
        options,
        base::BindRepeating(&StorageQueueTest::AsyncStartMockUploader,
                            base::Unretained(this)),
        test_encryption_module_,
        CompressionModule::Create(kCompressionThreshold, kCompressionType),
        storage_queue_create_event.cb());
    StatusOr<scoped_refptr<StorageQueue>> storage_queue_result =
        storage_queue_create_event.result();
    ASSERT_OK(storage_queue_result)
        << "Failed to create TestStorageQueue, error="
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

  void InjectFailures(std::initializer_list<int64_t> sequencing_ids) {
    storage_queue_->TestInjectBlockReadErrors(sequencing_ids);
  }

  QueueOptions BuildStorageQueueOptionsImmediate(
      base::TimeDelta upload_retry_delay = base::Seconds(1)) const {
    return QueueOptions(options_)
        .set_subdirectory(FILE_PATH_LITERAL("D1"))
        .set_file_prefix(FILE_PATH_LITERAL("F0001"))
        .set_upload_retry_delay(upload_retry_delay);
  }

  QueueOptions BuildStorageQueueOptionsPeriodic(
      base::TimeDelta upload_period = base::Seconds(1)) const {
    return BuildStorageQueueOptionsImmediate().set_upload_period(upload_period);
  }

  QueueOptions BuildStorageQueueOptionsOnlyManual() const {
    return BuildStorageQueueOptionsPeriodic(base::TimeDelta::Max());
  }

  void AsyncStartMockUploader(
      UploaderInterface::UploadReason reason,
      UploaderInterface::UploaderInterfaceResultCb start_uploader_cb) {
    auto uploader = std::make_unique<TestUploader>(&last_record_digest_map_,
                                                   sequenced_task_runner_);
    sequenced_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](UploaderInterface::UploadReason reason,
               std::unique_ptr<TestUploader> uploader,
               UploaderInterface::UploaderInterfaceResultCb start_uploader_cb,
               StorageQueueTest* self) {
              const auto status = self->set_mock_uploader_expectations_.Call(
                  reason, uploader.get());
              LOG(ERROR) << "Upload reason=" << reason << " " << status;
              if (!status.ok()) {
                std::move(start_uploader_cb).Run(status);
                return;
              }
              std::move(start_uploader_cb).Run(std::move(uploader));
            },
            reason, std::move(uploader), std::move(start_uploader_cb),
            base::Unretained(this)));
  }

  Status WriteString(base::StringPiece data) {
    EXPECT_TRUE(storage_queue_) << "StorageQueue not created yet";
    test::TestEvent<Status> w;
    Record record;
    record.set_data(std::string(data));
    record.set_destination(UPLOAD_EVENTS);
    record.set_dm_token("DM TOKEN");
    storage_queue_->Write(std::move(record), w.cb());
    return w.result();
  }

  void WriteStringOrDie(base::StringPiece data) {
    const Status write_result = WriteString(data);
    ASSERT_OK(write_result) << write_result;
  }

  void ConfirmOrDie(absl::optional<std::int64_t> sequencing_id,
                    bool force = false) {
    test::TestEvent<Status> c;
    storage_queue_->Confirm(sequencing_id, force, c.cb());
    const Status c_result = c.result();
    ASSERT_OK(c_result) << c_result;
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  const scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_{
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::BLOCK_SHUTDOWN, base::MayBlock()})};

  base::test::ScopedFeatureList scoped_feature_list_;
  base::ScopedTempDir location_;
  StorageOptions options_;
  scoped_refptr<test::TestEncryptionModule> test_encryption_module_;
  scoped_refptr<StorageQueue> storage_queue_;

  // Test-wide global mapping of <generation id, sequencing id> to record
  // digest. Serves all TestUploaders created by test fixture.
  TestUploader::LastRecordDigestMap last_record_digest_map_;

  ::testing::MockFunction<Status(UploaderInterface::UploadReason,
                                 TestUploader*)>
      set_mock_uploader_expectations_;
};

constexpr std::array<const char*, 3> kData = {"Rec1111", "Rec222", "Rec33"};
constexpr std::array<const char*, 3> kMoreData = {"More1111", "More222",
                                                  "More33"};

TEST_P(StorageQueueTest, WriteIntoNewStorageQueueAndReopen) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(kData[0]);
  WriteStringOrDie(kData[1]);
  WriteStringOrDie(kData[2]);

  ResetTestStorageQueue();

  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
}

TEST_P(StorageQueueTest, WriteIntoNewStorageQueueReopenAndWriteMore) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(kData[0]);
  WriteStringOrDie(kData[1]);
  WriteStringOrDie(kData[2]);

  ResetTestStorageQueue();

  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(kMoreData[0]);
  WriteStringOrDie(kMoreData[1]);
  WriteStringOrDie(kMoreData[2]);
}

TEST_P(StorageQueueTest, WriteIntoNewStorageQueueAndUpload) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(kData[0]);
  WriteStringOrDie(kData[1]);
  WriteStringOrDie(kData[2]);

  // Set uploader expectations.
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(set_mock_uploader_expectations_,
              Call(Eq(UploaderInterface::PERIODIC), NotNull()))
      .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                 TestUploader* test_uploader) {
        TestUploader::SetUp(test_uploader, &waiter)
            .Required(0, kData[0])
            .Required(1, kData[1])
            .Required(2, kData[2]);
        return Status::StatusOK();
      }))
      .RetiresOnSaturation();

  // Trigger upload.
  task_environment_.FastForwardBy(base::Seconds(1));
}

TEST_P(StorageQueueTest, WriteIntoNewStorageQueueAndUploadWithFailures) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(kData[0]);
  WriteStringOrDie(kData[1]);
  WriteStringOrDie(kData[2]);

  // Inject simulated failures.
  InjectFailures({1});

  // Set uploader expectations.
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(set_mock_uploader_expectations_,
              Call(Eq(UploaderInterface::PERIODIC), NotNull()))
      .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                 TestUploader* test_uploader) {
        TestUploader::SetUp(test_uploader, &waiter)
            .Required(0, kData[0])
            .RequiredGap(1, 1)
            .Possible(2, kData[2]);  // Depending on records binpacking
        return Status::StatusOK();
      }))
      .RetiresOnSaturation();

  // Trigger upload.
  task_environment_.FastForwardBy(base::Seconds(1));
}

TEST_P(StorageQueueTest, WriteIntoNewStorageQueueReopenWriteMoreAndUpload) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(kData[0]);
  WriteStringOrDie(kData[1]);
  WriteStringOrDie(kData[2]);

  ResetTestStorageQueue();

  // Set uploader expectations.
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(kMoreData[0]);
  WriteStringOrDie(kMoreData[1]);
  WriteStringOrDie(kMoreData[2]);

  // Set uploader expectations.
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(set_mock_uploader_expectations_,
              Call(Eq(UploaderInterface::PERIODIC), NotNull()))
      .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                 TestUploader* test_uploader) {
        TestUploader::SetUp(test_uploader, &waiter)
            .Required(0, kData[0])
            .Required(1, kData[1])
            .Required(2, kData[2])
            .Required(3, kMoreData[0])
            .Required(4, kMoreData[1])
            .Required(5, kMoreData[2]);
        return Status::StatusOK();
      }))
      .RetiresOnSaturation();

  // Trigger upload.
  task_environment_.FastForwardBy(base::Seconds(1));
}

TEST_P(StorageQueueTest,
       WriteIntoNewStorageQueueReopenWithMissingMetadataWriteMoreAndUpload) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(kData[0]);
  WriteStringOrDie(kData[1]);
  WriteStringOrDie(kData[2]);

  // Save copy of options.
  const QueueOptions options = storage_queue_->options();

  ResetTestStorageQueue();

  // Delete all metadata files.
  base::FileEnumerator dir_enum(
      options.directory(),
      /*recursive=*/false, base::FileEnumerator::FILES,
      base::StrCat({METADATA_NAME, FILE_PATH_LITERAL(".*")}));
  base::FilePath full_name;
  while (full_name = dir_enum.Next(), !full_name.empty()) {
    base::DeleteFile(full_name);
  }

  // Reopen, starting a new generation.
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(kMoreData[0]);
  WriteStringOrDie(kMoreData[1]);
  WriteStringOrDie(kMoreData[2]);

  // Set uploader expectations. Previous data is all lost.
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(set_mock_uploader_expectations_,
              Call(Eq(UploaderInterface::PERIODIC), NotNull()))
      .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                 TestUploader* test_uploader) {
        TestUploader::SetUp(test_uploader, &waiter)
            .Required(0, kData[0])
            .Required(1, kData[1])
            .Required(2, kData[2])
            .Required(3, kMoreData[0])
            .Required(4, kMoreData[1])
            .Required(5, kMoreData[2]);
        return Status::StatusOK();
      }))
      .RetiresOnSaturation();

  // Trigger upload.
  task_environment_.FastForwardBy(base::Seconds(1));
}

TEST_P(
    StorageQueueTest,
    WriteIntoNewStorageQueueReopenWithMissingLastMetadataWriteMoreAndUpload) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(kData[0]);
  WriteStringOrDie(kData[1]);
  WriteStringOrDie(kData[2]);

  // Save copy of options.
  const QueueOptions options = storage_queue_->options();

  ResetTestStorageQueue();

  // Delete all metadata files.
  base::FileEnumerator dir_enum(
      options.directory(),
      /*recursive=*/false, base::FileEnumerator::FILES,
      base::StrCat({METADATA_NAME, FILE_PATH_LITERAL(".2")}));
  base::FilePath full_name = dir_enum.Next();
  ASSERT_FALSE(full_name.empty());
  base::DeleteFile(full_name);
  ASSERT_TRUE(dir_enum.Next().empty());

  // Reopen, starting a new generation.
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(kMoreData[0]);
  WriteStringOrDie(kMoreData[1]);
  WriteStringOrDie(kMoreData[2]);

  // Set uploader expectations. Previous data is all lost.
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(set_mock_uploader_expectations_,
              Call(Eq(UploaderInterface::PERIODIC), NotNull()))
      .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                 TestUploader* test_uploader) {
        TestUploader::SetUp(test_uploader, &waiter)
            .Required(0, kData[0])
            .Required(1, kData[1])
            .Required(2, kData[2])
            .Required(3, kMoreData[0])
            .Required(4, kMoreData[1])
            .Required(5, kMoreData[2]);
        return Status::StatusOK();
      }))
      .RetiresOnSaturation();

  // Trigger upload.
  task_environment_.FastForwardBy(base::Seconds(1));
}

TEST_P(StorageQueueTest,
       WriteIntoNewStorageQueueReopenWithMissingDataWriteMoreAndUpload) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  WriteStringOrDie(kData[0]);
  WriteStringOrDie(kData[1]);
  WriteStringOrDie(kData[2]);

  // Save copy of options.
  const QueueOptions options = storage_queue_->options();

  ResetTestStorageQueue();

  // Reopen with the same generation and sequencing information.
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());

  // Delete the data file *.generation.0
  {
    base::FileEnumerator dir_enum(
        options.directory(),
        /*recursive=*/false, base::FileEnumerator::FILES,
        base::StrCat({options.file_prefix(), FILE_PATH_LITERAL(".*.0")}));
    base::FilePath full_name;
    while (full_name = dir_enum.Next(), !full_name.empty()) {
      base::DeleteFile(full_name);
    }
  }

  // Write more data.
  WriteStringOrDie(kMoreData[0]);
  WriteStringOrDie(kMoreData[1]);
  WriteStringOrDie(kMoreData[2]);

  // Set uploader expectations. Previous data is all lost.
  // The expected results depend on the test configuration.
  test::TestCallbackAutoWaiter waiter;
  switch (options.single_file_size()) {
    case 1:  // single record in file - deletion killed the first record
      EXPECT_CALL(set_mock_uploader_expectations_,
                  Call(Eq(UploaderInterface::PERIODIC), NotNull()))
          .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                     TestUploader* test_uploader) {
            TestUploader::SetUp(test_uploader, &waiter)
                .PossibleGap(0, 1)
                .Required(1, kData[1])
                .Required(2, kData[2])
                .Required(3, kMoreData[0])
                .Required(4, kMoreData[1])
                .Required(5, kMoreData[2]);
            return Status::StatusOK();
          }))
          .RetiresOnSaturation();
      break;
    case 256:  // two records in file - deletion killed the first two records.
               // Can bring gap of 2 records or 2 gaps 1 record each.
      EXPECT_CALL(set_mock_uploader_expectations_,
                  Call(Eq(UploaderInterface::PERIODIC), NotNull()))
          .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                     TestUploader* test_uploader) {
            TestUploader::SetUp(test_uploader, &waiter)
                .PossibleGap(0, 1)
                .PossibleGap(1, 1)
                .PossibleGap(0, 2)
                .Failure(
                    2, Status(error::DATA_LOSS, "Last record digest mismatch"))
                .Required(3, kMoreData[0])
                .Required(4, kMoreData[1])
                .Required(5, kMoreData[2]);
            return Status::StatusOK();
          }))
          .RetiresOnSaturation();
      break;
    default:  // Unlimited file size - deletion above killed all the data. Can
              // bring gap of 1-6 records.
      EXPECT_CALL(set_mock_uploader_expectations_,
                  Call(Eq(UploaderInterface::PERIODIC), NotNull()))
          .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                     TestUploader* test_uploader) {
            TestUploader::SetUp(test_uploader, &waiter)
                .PossibleGap(0, 1)
                .PossibleGap(0, 2)
                .PossibleGap(0, 3)
                .PossibleGap(0, 4)
                .PossibleGap(0, 5)
                .PossibleGap(0, 6);
            return Status::StatusOK();
          }))
          .RetiresOnSaturation();
  }

  // Trigger upload.
  task_environment_.FastForwardBy(base::Seconds(1));
}

TEST_P(StorageQueueTest, WriteIntoNewStorageQueueAndFlush) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsOnlyManual());
  WriteStringOrDie(kData[0]);
  WriteStringOrDie(kData[1]);
  WriteStringOrDie(kData[2]);

  // Set uploader expectations.
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(set_mock_uploader_expectations_,
              Call(Eq(UploaderInterface::MANUAL), NotNull()))
      .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                 TestUploader* test_uploader) {
        TestUploader::SetUp(test_uploader, &waiter)
            .Required(0, kData[0])
            .Required(1, kData[1])
            .Required(2, kData[2]);
        return Status::StatusOK();
      }))
      .RetiresOnSaturation();

  // Flush manually.
  storage_queue_->Flush();
}

TEST_P(StorageQueueTest, WriteIntoNewStorageQueueReopenWriteMoreAndFlush) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsOnlyManual());
  WriteStringOrDie(kData[0]);
  WriteStringOrDie(kData[1]);
  WriteStringOrDie(kData[2]);

  ResetTestStorageQueue();

  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsOnlyManual());
  WriteStringOrDie(kMoreData[0]);
  WriteStringOrDie(kMoreData[1]);
  WriteStringOrDie(kMoreData[2]);

  // Set uploader expectations.
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(set_mock_uploader_expectations_,
              Call(Eq(UploaderInterface::MANUAL), NotNull()))
      .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                 TestUploader* test_uploader) {
        TestUploader::SetUp(test_uploader, &waiter)
            .Required(0, kData[0])
            .Required(1, kData[1])
            .Required(2, kData[2])
            .Required(3, kMoreData[0])
            .Required(4, kMoreData[1])
            .Required(5, kMoreData[2]);
        return Status::StatusOK();
      }))
      .RetiresOnSaturation();

  // Flush manually.
  storage_queue_->Flush();
}

TEST_P(StorageQueueTest, ValidateVariousRecordSizes) {
  std::vector<std::string> data;
  for (size_t i = 16; i < 16 + 16; ++i) {
    data.emplace_back(i, 'R');
  }
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsOnlyManual());
  for (const auto& record : data) {
    WriteStringOrDie(record);
  }

  // Set uploader expectations.
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(set_mock_uploader_expectations_,
              Call(Eq(UploaderInterface::MANUAL), NotNull()))
      .WillOnce(Invoke([data, &waiter](UploaderInterface::UploadReason reason,
                                       TestUploader* test_uploader) {
        TestUploader::SetUp uploader_setup(test_uploader, &waiter);
        for (size_t i = 0; i < data.size(); ++i) {
          uploader_setup.Required(i, data[i]);
        }
        return Status::StatusOK();
      }))
      .RetiresOnSaturation();

  // Flush manually.
  storage_queue_->Flush();
}

TEST_P(StorageQueueTest, WriteAndRepeatedlyUploadWithConfirmations) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());

  WriteStringOrDie(kData[0]);
  WriteStringOrDie(kData[1]);
  WriteStringOrDie(kData[2]);

  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::PERIODIC), NotNull()))
        .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                   TestUploader* test_uploader) {
          TestUploader::SetUp(test_uploader, &waiter)
              .Required(0, kData[0])
              .Required(1, kData[1])
              .Required(2, kData[2]);
          return Status::StatusOK();
        }))
        .RetiresOnSaturation();

    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::Seconds(1));
  }
  // Confirm #0 and forward time again, removing record #0
  ConfirmOrDie(/*sequencing_id=*/0);
  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::PERIODIC), NotNull()))
        .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                   TestUploader* test_uploader) {
          TestUploader::SetUp(test_uploader, &waiter)
              .Required(1, kData[1])
              .Required(2, kData[2]);
          return Status::StatusOK();
        }))
        .RetiresOnSaturation();
    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Confirm #1 and forward time again, removing record #1
  ConfirmOrDie(/*sequencing_id=*/1);
  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::PERIODIC), NotNull()))
        .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                   TestUploader* test_uploader) {
          TestUploader::SetUp(test_uploader, &waiter).Required(2, kData[2]);
          return Status::StatusOK();
        }))
        .RetiresOnSaturation();
    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Add more data and verify that #2 and new data are returned.
  WriteStringOrDie(kMoreData[0]);
  WriteStringOrDie(kMoreData[1]);
  WriteStringOrDie(kMoreData[2]);

  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::PERIODIC), NotNull()))
        .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                   TestUploader* test_uploader) {
          TestUploader::SetUp(test_uploader, &waiter)
              .Required(2, kData[2])
              .Required(3, kMoreData[0])
              .Required(4, kMoreData[1])
              .Required(5, kMoreData[2]);
          return Status::StatusOK();
        }))
        .RetiresOnSaturation();
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Confirm #2 and forward time again, removing record #2
  ConfirmOrDie(/*sequencing_id=*/2);

  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::PERIODIC), NotNull()))
        .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                   TestUploader* test_uploader) {
          TestUploader::SetUp(test_uploader, &waiter)
              .Required(3, kMoreData[0])
              .Required(4, kMoreData[1])
              .Required(5, kMoreData[2]);
          return Status::StatusOK();
        }))
        .RetiresOnSaturation();
    task_environment_.FastForwardBy(base::Seconds(1));
  }
}

TEST_P(StorageQueueTest, WriteAndRepeatedlyUploadWithConfirmationsAndReopen) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());

  WriteStringOrDie(kData[0]);
  WriteStringOrDie(kData[1]);
  WriteStringOrDie(kData[2]);

  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::PERIODIC), NotNull()))
        .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                   TestUploader* test_uploader) {
          TestUploader::SetUp(test_uploader, &waiter)
              .Required(0, kData[0])
              .Required(1, kData[1])
              .Required(2, kData[2]);
          return Status::StatusOK();
        }))
        .RetiresOnSaturation();

    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Confirm #0 and forward time again, removing record #0
  ConfirmOrDie(/*sequencing_id=*/0);
  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::PERIODIC), NotNull()))
        .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                   TestUploader* test_uploader) {
          TestUploader::SetUp(test_uploader, &waiter)
              .Required(1, kData[1])
              .Required(2, kData[2]);
          return Status::StatusOK();
        }))
        .RetiresOnSaturation();
    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Confirm #1 and forward time again, removing record #1
  ConfirmOrDie(/*sequencing_id=*/1);
  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::PERIODIC), NotNull()))
        .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                   TestUploader* test_uploader) {
          TestUploader::SetUp(test_uploader, &waiter).Required(2, kData[2]);
          return Status::StatusOK();
        }))
        .RetiresOnSaturation();
    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  ResetTestStorageQueue();
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());

  // Add more data and verify that #2 and new data are returned.
  WriteStringOrDie(kMoreData[0]);
  WriteStringOrDie(kMoreData[1]);
  WriteStringOrDie(kMoreData[2]);

  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::PERIODIC), NotNull()))
        .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                   TestUploader* test_uploader) {
          TestUploader::SetUp(test_uploader, &waiter)
              .Possible(0, kData[0])
              .Possible(1, kData[1])
              .Required(2, kData[2])
              .Required(3, kMoreData[0])
              .Required(4, kMoreData[1])
              .Required(5, kMoreData[2]);
          return Status::StatusOK();
        }))
        .RetiresOnSaturation();
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Confirm #2 and forward time again, removing record #2
  ConfirmOrDie(/*sequencing_id=*/2);

  {
    test::TestCallbackAutoWaiter waiter;
    // Set uploader expectations.
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::PERIODIC), NotNull()))
        .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                   TestUploader* test_uploader) {
          TestUploader::SetUp(test_uploader, &waiter)
              .Required(3, kMoreData[0])
              .Required(4, kMoreData[1])
              .Required(5, kMoreData[2]);
          return Status::StatusOK();
        }))
        .RetiresOnSaturation();
    task_environment_.FastForwardBy(base::Seconds(1));
  }
}

TEST_P(StorageQueueTest,
       WriteAndRepeatedlyUploadWithConfirmationsAndReopenWithFailures) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());

  WriteStringOrDie(kData[0]);
  WriteStringOrDie(kData[1]);
  WriteStringOrDie(kData[2]);

  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::PERIODIC), NotNull()))
        .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                   TestUploader* test_uploader) {
          TestUploader::SetUp(test_uploader, &waiter)
              .Required(0, kData[0])
              .Required(1, kData[1])
              .Required(2, kData[2]);
          return Status::StatusOK();
        }))
        .RetiresOnSaturation();

    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Confirm #0 and forward time again, removing record #0
  ConfirmOrDie(/*sequencing_id=*/0);
  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::PERIODIC), NotNull()))
        .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                   TestUploader* test_uploader) {
          TestUploader::SetUp(test_uploader, &waiter)
              .Required(1, kData[1])
              .Required(2, kData[2]);
          return Status::StatusOK();
        }))
        .RetiresOnSaturation();
    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Confirm #1 and forward time again, removing record #1
  ConfirmOrDie(/*sequencing_id=*/1);
  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::PERIODIC), NotNull()))
        .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                   TestUploader* test_uploader) {
          TestUploader::SetUp(test_uploader, &waiter).Required(2, kData[2]);
          return Status::StatusOK();
        }))
        .RetiresOnSaturation();
    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  ResetTestStorageQueue();
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());

  // Add more data and verify that #2 and new data are returned.
  WriteStringOrDie(kMoreData[0]);
  WriteStringOrDie(kMoreData[1]);
  WriteStringOrDie(kMoreData[2]);

  // Inject simulated failures.
  InjectFailures({4, 5});

  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::PERIODIC), NotNull()))
        .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                   TestUploader* test_uploader) {
          TestUploader::SetUp(test_uploader, &waiter)
              .Possible(0, kData[0])
              .Possible(1, kData[1])
              .Required(2, kData[2])
              .Required(3, kMoreData[0])
              // Gap may be 2 records at once or 2 gaps 1 record each.
              .PossibleGap(4, 2)
              .PossibleGap(4, 1)
              .PossibleGap(5, 1);
          return Status::StatusOK();
        }))
        .RetiresOnSaturation();
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Confirm #2 and forward time again, removing record #2
  ConfirmOrDie(/*sequencing_id=*/2);

  // Reset simulated failures.
  InjectFailures({});

  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::PERIODIC), NotNull()))
        .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                   TestUploader* test_uploader) {
          TestUploader::SetUp(test_uploader, &waiter)
              .Required(3, kMoreData[0])
              .Required(4, kMoreData[1])
              .Required(5, kMoreData[2]);
          return Status::StatusOK();
        }))
        .RetiresOnSaturation();
    task_environment_.FastForwardBy(base::Seconds(1));
  }
}

TEST_P(StorageQueueTest, WriteAndRepeatedlyImmediateUpload) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsImmediate());

  // Upload is initiated asynchronously, so it may happen after the next
  // record is also written. Because of that we set expectations for the
  // data after the current one as |Possible|.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::IMMEDIATE_FLUSH), NotNull()))
        .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                   TestUploader* test_uploader) {
          TestUploader::SetUp(test_uploader, &waiter)
              .Required(0, kData[0])
              .Possible(1, kData[1])
              .Possible(2, kData[2]);
          return Status::StatusOK();
        }))
        .RetiresOnSaturation();
    WriteStringOrDie(kData[0]);
  }

  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::IMMEDIATE_FLUSH), NotNull()))
        .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                   TestUploader* test_uploader) {
          TestUploader::SetUp(test_uploader, &waiter)
              .Required(0, kData[0])
              .Required(1, kData[1])
              .Possible(2, kData[2]);
          return Status::StatusOK();
        }))
        .RetiresOnSaturation();
    WriteStringOrDie(kData[1]);
  }

  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::IMMEDIATE_FLUSH), NotNull()))
        .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                   TestUploader* test_uploader) {
          TestUploader::SetUp(test_uploader, &waiter)
              .Required(0, kData[0])
              .Required(1, kData[1])
              .Required(2, kData[2]);
          return Status::StatusOK();
        }))
        .RetiresOnSaturation();
    WriteStringOrDie(kData[2]);
  }
}

TEST_P(StorageQueueTest, WriteAndRepeatedlyImmediateUploadWithConfirmations) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsImmediate());

  // Upload is initiated asynchronously, so it may happen after the next
  // record is also written. Because of the Confirmation below, we set
  // expectations for the data that may be eliminated by Confirmation as
  // |Possible|.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::IMMEDIATE_FLUSH), NotNull()))
        .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                   TestUploader* test_uploader) {
          TestUploader::SetUp(test_uploader, &waiter).Required(0, kData[0]);
          return Status::StatusOK();
        }))
        .RetiresOnSaturation();
    WriteStringOrDie(kData[0]);
  }

  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::IMMEDIATE_FLUSH), NotNull()))
        .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                   TestUploader* test_uploader) {
          TestUploader::SetUp(test_uploader, &waiter)
              .Required(0, kData[0])
              .Required(1, kData[1]);
          return Status::StatusOK();
        }))
        .RetiresOnSaturation();
    WriteStringOrDie(kData[1]);
  }

  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::IMMEDIATE_FLUSH), NotNull()))
        .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                   TestUploader* test_uploader) {
          TestUploader::SetUp(test_uploader, &waiter)
              .Required(0, kData[0])
              .Required(1, kData[1])
              .Required(2, kData[2]);
          return Status::StatusOK();
        }))
        .RetiresOnSaturation();
    WriteStringOrDie(kData[2]);
  }

  // Confirm #1, removing data #0 and #1
  ConfirmOrDie(/*sequencing_id=*/1);

  // Add more data to verify that #2 and new data are returned.
  // Upload is initiated asynchronously, so it may happen after the next
  // record is also written. Because of that we set expectations for the
  // data after the current one as |Possible|.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::IMMEDIATE_FLUSH), NotNull()))
        .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                   TestUploader* test_uploader) {
          TestUploader::SetUp(test_uploader, &waiter)
              .Required(2, kData[2])
              .Required(3, kMoreData[0]);
          return Status::StatusOK();
        }))
        .RetiresOnSaturation();
    WriteStringOrDie(kMoreData[0]);
  }

  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::IMMEDIATE_FLUSH), NotNull()))
        .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                   TestUploader* test_uploader) {
          TestUploader::SetUp(test_uploader, &waiter)
              .Required(2, kData[2])
              .Required(3, kMoreData[0])
              .Required(4, kMoreData[1]);
          return Status::StatusOK();
        }))
        .RetiresOnSaturation();
    WriteStringOrDie(kMoreData[1]);
  }

  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::IMMEDIATE_FLUSH), NotNull()))
        .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                   TestUploader* test_uploader) {
          TestUploader::SetUp(test_uploader, &waiter)
              .Required(2, kData[2])
              .Required(3, kMoreData[0])
              .Required(4, kMoreData[1])
              .Required(5, kMoreData[2]);
          return Status::StatusOK();
        }))
        .RetiresOnSaturation();
    WriteStringOrDie(kMoreData[2]);
  }
}

TEST_P(StorageQueueTest, WriteAndImmediateUploadWithFailure) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsImmediate());

  // Write a record as Immediate, initiating an upload which fails
  // and then restarts.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::IMMEDIATE_FLUSH), NotNull()))
        .WillOnce(Invoke([](UploaderInterface::UploadReason reason,
                            TestUploader* test_uploader) {
          return Status(error::UNAVAILABLE, "Test uploader unavailable");
        }))
        .RetiresOnSaturation();
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::FAILURE_RETRY), NotNull()))
        .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                   TestUploader* test_uploader) {
          TestUploader::SetUp(test_uploader, &waiter).Required(0, kData[0]);
          return Status::StatusOK();
        }))
        .RetiresOnSaturation();
    WriteStringOrDie(kData[0]);  // Immediately uploads and fails.

    // Let it retry upload and verify.
    task_environment_.FastForwardBy(base::Seconds(1));
  }
}

TEST_P(StorageQueueTest, WriteAndImmediateUploadWithoutConfirmation) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsImmediate());

  // Write a record as Immediate, initiating an upload which fails
  // and then restarts.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::IMMEDIATE_FLUSH), NotNull()))
        .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                   TestUploader* test_uploader) {
          TestUploader::SetUp(test_uploader, &waiter).Required(0, kData[0]);
          return Status::StatusOK();
        }))
        .RetiresOnSaturation();
    WriteStringOrDie(kData[0]);  // Immediately uploads and does not confirm.
  }

  // Let it retry upload and verify.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::INCOMPLETE_RETRY), NotNull()))
        .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                   TestUploader* test_uploader) {
          TestUploader::SetUp(test_uploader, &waiter).Required(0, kData[0]);
          return Status::StatusOK();
        }))
        .RetiresOnSaturation();
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Confirm 0 and make sure no retry happens (since everything is confirmed).
  EXPECT_CALL(set_mock_uploader_expectations_,
              Call(Eq(UploaderInterface::INCOMPLETE_RETRY), NotNull()))
      .Times(0);

  ConfirmOrDie(/*sequencing_id=*/0);
  task_environment_.FastForwardBy(base::Seconds(1));
}

TEST_P(StorageQueueTest, WriteEncryptFailure) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());
  DCHECK(test_encryption_module_);
  EXPECT_CALL(*test_encryption_module_, EncryptRecordImpl(_, _))
      .WillOnce(WithArg<1>(
          Invoke([](base::OnceCallback<void(StatusOr<EncryptedRecord>)> cb) {
            std::move(cb).Run(Status(error::UNKNOWN, "Failing for tests"));
          })));
  const Status result = WriteString("TEST_MESSAGE");
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error_code(), error::UNKNOWN);
}

TEST_P(StorageQueueTest, ForceConfirm) {
  CreateTestStorageQueueOrDie(BuildStorageQueueOptionsPeriodic());

  WriteStringOrDie(kData[0]);
  WriteStringOrDie(kData[1]);
  WriteStringOrDie(kData[2]);

  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::PERIODIC), NotNull()))
        .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                   TestUploader* test_uploader) {
          TestUploader::SetUp(test_uploader, &waiter)
              .Required(0, kData[0])
              .Required(1, kData[1])
              .Required(2, kData[2]);
          return Status::StatusOK();
        }))
        .RetiresOnSaturation();

    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Confirm #1 and forward time again, possibly removing records #0 and #1
  ConfirmOrDie(/*sequencing_id=*/1);

  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::PERIODIC), NotNull()))
        .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                   TestUploader* test_uploader) {
          TestUploader::SetUp(test_uploader, &waiter).Required(2, kData[2]);
          return Status::StatusOK();
        }))
        .RetiresOnSaturation();
    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Now force confirm the very beginning and forward time again.
  ConfirmOrDie(/*sequencing_id=*/absl::nullopt, /*force=*/true);

  {
    // Set uploader expectations.
    // #0 and #1 could be returned as Gaps
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::PERIODIC), NotNull()))
        .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                   TestUploader* test_uploader) {
          TestUploader::SetUp(test_uploader, &waiter)
              .RequiredSeqId(0)
              .RequiredSeqId(1)
              .RequiredSeqId(2)
              // 0-2 must have been encountered, but actual contents
              // can be different:
              .Possible(0, kData[0])
              .PossibleGap(0, 1)
              .PossibleGap(0, 2)
              .Possible(1, kData[1])
              .Required(2, kData[2]);
          return Status::StatusOK();
        }))
        .RetiresOnSaturation();
    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::Seconds(1));
  }

  // Force confirm #0 and forward time again.
  ConfirmOrDie(/*sequencing_id=*/0, /*force=*/true);

  {
    // Set uploader expectations.
    // #0 and #1 could be returned as Gaps
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::PERIODIC), NotNull()))
        .WillOnce(Invoke([&waiter](UploaderInterface::UploadReason reason,
                                   TestUploader* test_uploader) {
          TestUploader::SetUp(test_uploader, &waiter)
              .RequiredSeqId(1)
              .RequiredSeqId(2)
              // 0-2 must have been encountered, but actual contents
              // can be different:
              .PossibleGap(1, 1)
              .Possible(1, kData[1])
              .Required(2, kData[2]);
          return Status::StatusOK();
        }))
        .RetiresOnSaturation();
    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::Seconds(1));
  }
}

INSTANTIATE_TEST_SUITE_P(VaryingFileSize,
                         StorageQueueTest,
                         testing::Values(128 * 1024LL * 1024LL,
                                         256 /* two records in file */,
                                         1 /* single record in file */));

// TODO(b/157943006): Additional tests:
// 1) Options object with a bad path.
// 2) Have bad prefix files in the directory.
// 3) Attempt to create file with duplicated file extension.
// 4) Disk and memory limit exceeded.
// 5) Other negative tests.

}  // namespace
}  // namespace reporting
