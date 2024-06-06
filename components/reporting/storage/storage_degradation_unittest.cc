// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atomic>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback_helpers.h"
#include "base/sequence_checker.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "components/reporting/compression/test_compression_module.h"
#include "components/reporting/encryption/decryption.h"
#include "components/reporting/encryption/encryption_module.h"
#include "components/reporting/encryption/encryption_module_interface.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/proto/synced/record_constants.pb.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/storage/storage.h"
#include "components/reporting/storage/storage_configuration.h"
#include "components/reporting/storage/storage_uploader_interface.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "crypto/sha2.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyOf;
using ::testing::Args;
using ::testing::Between;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::Gt;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::Property;
using ::testing::Return;
using ::testing::Sequence;
using ::testing::StartsWith;
using ::testing::StrEq;
using ::testing::WithArg;
using ::testing::WithoutArgs;

// TODO(b/278734198): Combine common degradation test logic with
// legacy_storage_degradation_unittest.cc
namespace reporting {

namespace {

using TestRecord = std::tuple<Priority, int64_t, std::string>;
using ExpectRecordGroupCallback =
    base::RepeatingCallback<void(std::vector<TestRecord>)>;

std::string ComposeLargeString() {
  static std::string large_string((1024 * 1024 / 3), 'A');
  return large_string;
}

constexpr std::array<const char*, 3> kData = {"Rec1111", "Rec222", "Rec33"};

std::string xBigData() {
  static const std::string large_string = ComposeLargeString();
  return large_string;
}

// Stores an entire upload of records from `SequenceBoundUpload` in the order
// they were received when the upload is declared complete. Intended to be a
// class member of `StorageTest`, so that it outlives
// `TestUploader` and `SequenceBoundUpload` and can be used to perform checks
// that span multiple separate uploads. The user is responsible for resetting
// the state by calling `Reset()`.
class RecordUploadStore {
 public:
  void Store(std::vector<TestRecord> records) {
    // Mark these records as uploaded
    records_.insert(records_.end(), records.begin(), records.end());
    // Add the entire upload as a whole
    uploads_.emplace_back(std::move(records));
  }
  void Reset() {
    uploads_.clear();
    records_.clear();
  }

  std::vector<std::vector<TestRecord>> Uploads() { return uploads_; }
  std::vector<TestRecord> Records() { return records_; }

 private:
  // List of uploads. Each vector is a distinct upload.
  std::vector<std::vector<TestRecord>> uploads_;
  // Concatenation of all records across all uploads in the order they were
  // received.
  std::vector<TestRecord> records_;
};

// Test uploader counter - for generation of unique ids.
std::atomic<int64_t> next_uploader_id{0};

// Maximum length of debug data prints to prevent excessive output.
static constexpr size_t kDebugDataPrintSize = 16uL;

// Storage options to be used in tests.
class TestStorageOptions : public StorageOptions {
 public:
  TestStorageOptions()
      : StorageOptions(
            base::BindRepeating(&TestStorageOptions::ModifyQueueOptions,
                                base::Unretained(this))) {
    // Extend total memory size to accommodate all big records:
    // We do not want the degradation tests to fail because of insufficient
    // memory - only insufficient disk space is expected.
    set_max_total_memory_size(32u * 1024uLL * 1024uLL);  // 32 MiB
  }

  // Prepare options adjustment.
  // Must be called before the options are used by Storage::Create().
  void set_upload_retry_delay(base::TimeDelta upload_retry_delay) {
    upload_retry_delay_ = upload_retry_delay;
  }

 private:
  void ModifyQueueOptions(Priority /*priority*/,
                          QueueOptions& queue_options) const {
    queue_options.set_upload_retry_delay(upload_retry_delay_);
  }

  base::TimeDelta upload_retry_delay_{
      base::TimeDelta()};  // no retry by default
};

class StorageDegradationTest
    : public ::testing::TestWithParam<::testing::tuple<size_t, bool>> {
  // Mapping of <generation id, sequencing id> to matching record digest.
  // Whenever a record is uploaded and includes last record digest, this map
  // should have that digest already recorded. Only the first record in a
  // generation is uploaded without last record digest.
  struct LastRecordDigest {
    struct Hash {
      size_t operator()(
          const std::tuple<Priority,
                           int64_t /*generation id*/,
                           int64_t /*sequencing id*/>& v) const noexcept {
        const auto& [priority, generation_id, sequencing_id] = v;
        static constexpr std::hash<Priority> priority_hasher;
        static constexpr std::hash<int64_t> generation_id_hasher;
        static constexpr std::hash<int64_t> sequencing_id_hasher;
        return priority_hasher(priority) ^ generation_id_hasher(generation_id) ^
               sequencing_id_hasher(sequencing_id);
      }
    };
    using Map = std::unordered_map<std::tuple<Priority,
                                              int64_t /*generation id*/,
                                              int64_t /*sequencing id*/>,
                                   std::optional<std::string /*digest*/>,
                                   Hash>;
  };

  // Track the last uploaded generation id based on priority
  using LastUploadedGenerationIdMap = std::unordered_map<Priority, int64_t>;

 protected:
  void SetUp() override {
    ASSERT_TRUE(location_.CreateUniqueTempDir());
    options_.set_directory(location_.GetPath());

    // Turn uploads to no-ops unless other expectation is set (any later
    // EXPECT_CALL will take precedence over this one).
    EXPECT_CALL(set_mock_uploader_expectations_, Call(_))
        .WillRepeatedly(Invoke([this](UploaderInterface::UploadReason reason) {
          return TestUploader::SetUpDummy(this);
        }));

    // Disable encryption
    scoped_feature_list_.InitWithFeatures({}, {kEncryptedReportingFeature});
    upload_store_.Reset();
  }

  void TearDown() override {
    ResetTestStorage();
    // Log next uploader id for possible verification.
    LOG(ERROR) << "Next uploader id=" << next_uploader_id.load();
  }

  // Mock class used for setting upload expectations on it.
  class MockUpload {
   public:
    MockUpload() = default;
    virtual ~MockUpload() = default;
    MOCK_METHOD(void,
                EncounterSeqId,
                (int64_t /*uploader_id*/, Priority, int64_t),
                (const));
    MOCK_METHOD(bool,
                UploadRecord,
                (int64_t /*uploader_id*/, Priority, int64_t, std::string_view),
                (const));
    MOCK_METHOD(bool,
                UploadRecordFailure,
                (int64_t /*uploader_id*/, Priority, int64_t, Status),
                (const));
    MOCK_METHOD(bool,
                UploadGap,
                (int64_t /*uploader_id*/, Priority, int64_t, uint64_t),
                (const));
    MOCK_METHOD(void,
                UploadComplete,
                (int64_t /*uploader_id*/, Status),
                (const));
  };

  // Helper class to be wrapped in SequenceBound<..>, in order to make sure
  // all its methods are run on a main sequential task wrapper. As a result,
  // collected information and EXPECT_CALLs to MockUpload are safe - executed on
  // the main test thread.
  class SequenceBoundUpload {
   public:
    explicit SequenceBoundUpload(
        std::unique_ptr<const MockUpload> mock_upload,
        LastUploadedGenerationIdMap* const last_upload_generation_id,
        LastRecordDigest::Map* const last_record_digest_map,
        ExpectRecordGroupCallback callback)
        : mock_upload_(std::move(mock_upload)),
          last_upload_generation_id_(last_upload_generation_id),
          last_record_digest_map_(last_record_digest_map),
          expect_record_group_callback_(std::move(callback)) {
      DETACH_FROM_SEQUENCE(scoped_checker_);
      upload_progress_.assign("\nStart\n");
    }
    SequenceBoundUpload(const SequenceBoundUpload& other) = delete;
    SequenceBoundUpload& operator=(const SequenceBoundUpload& other) = delete;
    ~SequenceBoundUpload() { DCHECK_CALLED_ON_VALID_SEQUENCE(scoped_checker_); }

    void ProcessGap(uint64_t uploader_id_,
                    SequenceInformation sequence_information,
                    uint64_t count,
                    base::OnceCallback<void(bool)> processed_cb) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(scoped_checker_);
      // Verify generation match.
      if (generation_id_.has_value() &&
          generation_id_.value() != sequence_information.generation_id()) {
        DoUploadRecordFailure(
            uploader_id_, sequence_information.priority(),
            sequence_information.sequencing_id(),
            sequence_information.generation_id(),
            Status(error::DATA_LOSS,
                   base::StrCat({"Generation id mismatch, expected=",
                                 base::NumberToString(generation_id_.value()),
                                 " actual=",
                                 base::NumberToString(
                                     sequence_information.generation_id())})),
            std::move(processed_cb));
        return;
      }
      if (!generation_id_.has_value()) {
        generation_id_ = sequence_information.generation_id();
        last_upload_generation_id_->emplace(
            sequence_information.priority(),
            sequence_information.generation_id());
      }

      last_record_digest_map_->emplace(
          std::make_tuple(sequence_information.priority(),
                          sequence_information.sequencing_id(),
                          sequence_information.generation_id()),
          std::nullopt);

      DoUploadGap(uploader_id_, sequence_information.priority(),
                  sequence_information.sequencing_id(),
                  sequence_information.generation_id(), count,
                  std::move(processed_cb));
    }

    void VerifyRecord(int64_t uploader_id_,
                      SequenceInformation sequence_information,
                      WrappedRecord wrapped_record,
                      base::OnceCallback<void(bool)> processed_cb) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(scoped_checker_);
      // Verify generation match.
      if (generation_id_.has_value() &&
          generation_id_.value() != sequence_information.generation_id()) {
        DoUploadRecordFailure(
            uploader_id_, sequence_information.priority(),
            sequence_information.sequencing_id(),
            sequence_information.generation_id(),
            Status(error::DATA_LOSS,
                   base::StrCat({"Generation id mismatch, expected=",
                                 base::NumberToString(generation_id_.value()),
                                 " actual=",
                                 base::NumberToString(
                                     sequence_information.generation_id())})),
            std::move(processed_cb));
        return;
      }
      if (!generation_id_.has_value()) {
        generation_id_ = sequence_information.generation_id();
        last_upload_generation_id_->emplace(
            sequence_information.priority(),
            sequence_information.generation_id());
      }

      // Verify digest and its match.
      {
        std::string serialized_record;
        wrapped_record.record().SerializeToString(&serialized_record);
        const auto record_digest = crypto::SHA256HashString(serialized_record);
        CHECK_EQ(record_digest.size(), crypto::kSHA256Length);
        if (record_digest != wrapped_record.record_digest()) {
          DoUploadRecordFailure(
              uploader_id_, sequence_information.priority(),
              sequence_information.sequencing_id(),
              sequence_information.generation_id(),
              Status(error::DATA_LOSS, "Record digest mismatch"),
              std::move(processed_cb));
          return;
        }
        if (wrapped_record.has_last_record_digest()) {
          auto it = last_record_digest_map_->find(
              std::make_tuple(sequence_information.priority(),
                              sequence_information.sequencing_id() - 1,
                              sequence_information.generation_id()));
          // If previous record has been seen, last record digest must match it.
          // Otherwise ignore digest - previous record might have been erased
          // during degradation.
          if (it != last_record_digest_map_->end()) {
            if (it->second != wrapped_record.last_record_digest()) {
              DoUploadRecordFailure(
                  uploader_id_, sequence_information.priority(),
                  sequence_information.sequencing_id(),
                  sequence_information.generation_id(),
                  Status(error::DATA_LOSS, "Last record digest mismatch"),
                  std::move(processed_cb));
              return;
            }
          }
        }
        last_record_digest_map_->emplace(
            std::make_tuple(sequence_information.priority(),
                            sequence_information.sequencing_id(),
                            sequence_information.generation_id()),
            record_digest);
      }

      DoUploadRecord(uploader_id_, sequence_information.priority(),
                     sequence_information.sequencing_id(),
                     sequence_information.generation_id(),
                     wrapped_record.record().data(), std::move(processed_cb));
    }

    void DoEncounterSeqId(int64_t uploader_id,
                          Priority priority,
                          int64_t sequencing_id,
                          int64_t generation_id) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(scoped_checker_);
      upload_progress_.append("SeqId: ")
          .append(base::NumberToString(sequencing_id))
          .append("/")
          .append(base::NumberToString(generation_id))
          .append("\n");
      mock_upload_->EncounterSeqId(uploader_id, priority, sequencing_id);
    }

    void DoUploadRecord(int64_t uploader_id,
                        Priority priority,
                        int64_t sequencing_id,
                        int64_t generation_id,
                        std::string_view data,
                        base::OnceCallback<void(bool)> processed_cb) {
      DoEncounterSeqId(uploader_id, priority, sequencing_id, generation_id);
      DCHECK_CALLED_ON_VALID_SEQUENCE(scoped_checker_);
      upload_progress_.append("Record: ")
          .append(base::NumberToString(sequencing_id))
          .append("/")
          .append(base::NumberToString(generation_id))
          .append(" '")
          .append(data.data(), 0, std::min(data.size(), kDebugDataPrintSize))
          .append("'\n");
      std::move(processed_cb)
          .Run(mock_upload_->UploadRecord(uploader_id, priority, sequencing_id,
                                          data));
      records_.emplace_back(priority, sequencing_id, data);
    }

    void DoUploadRecordFailure(int64_t uploader_id,
                               Priority priority,
                               int64_t sequencing_id,
                               int64_t generation_id,
                               Status status,
                               base::OnceCallback<void(bool)> processed_cb) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(scoped_checker_);
      upload_progress_.append("Failure: ")
          .append(base::NumberToString(sequencing_id))
          .append("/")
          .append(base::NumberToString(generation_id))
          .append(" '")
          .append(status.ToString())
          .append("'\n");
      std::move(processed_cb)
          .Run(mock_upload_->UploadRecordFailure(uploader_id, priority,
                                                 sequencing_id, status));
    }

    void DoUploadGap(int64_t uploader_id,
                     Priority priority,
                     int64_t sequencing_id,
                     int64_t generation_id,
                     uint64_t count,
                     base::OnceCallback<void(bool)> processed_cb) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(scoped_checker_);
      for (uint64_t c = 0; c < count; ++c) {
        DoEncounterSeqId(uploader_id, priority,
                         sequencing_id + static_cast<int64_t>(c),
                         generation_id);
      }
      upload_progress_.append("Gap: ")
          .append(base::NumberToString(sequencing_id))
          .append("/")
          .append(base::NumberToString(generation_id))
          .append(" (")
          .append(base::NumberToString(count))
          .append(")\n");
      std::move(processed_cb)
          .Run(mock_upload_->UploadGap(uploader_id, priority, sequencing_id,
                                       count));
    }

    void DoUploadComplete(int64_t uploader_id, Status status) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(scoped_checker_);
      upload_progress_.append("Complete: ")
          .append(status.ToString())
          .append("\n");
      LOG(ERROR) << "TestUploader: " << upload_progress_ << "End\n";
      mock_upload_->UploadComplete(uploader_id, status);
      expect_record_group_callback_.Run(std::move(records_));
    }

   private:
    const std::unique_ptr<const MockUpload> mock_upload_;
    std::optional<int64_t> generation_id_;
    const raw_ptr<LastUploadedGenerationIdMap> last_upload_generation_id_;
    const raw_ptr<LastRecordDigest::Map> last_record_digest_map_;
    ExpectRecordGroupCallback expect_record_group_callback_;
    std::vector<TestRecord> records_;
    SEQUENCE_CHECKER(scoped_checker_);

    // Snapshot of data received in this upload (for debug purposes).
    std::string upload_progress_;
  };

  // Uploader interface implementation to be assigned to tests.
  // Note that Storage guarantees that all APIs are executed on the same
  // sequenced task runner (not the main test thread!).
  class TestUploader : public UploaderInterface {
   public:
    // Helper class for setting up mock uploader expectations of a successful
    // completion.
    class SetUp {
     public:
      SetUp(Priority priority,
            test::TestCallbackWaiter* waiter,
            StorageDegradationTest* self)
          : priority_(priority),
            uploader_(std::make_unique<TestUploader>(self)),
            uploader_id_(uploader_->uploader_id_),
            waiter_(waiter) {}
      SetUp(const SetUp& other) = delete;
      SetUp& operator=(const SetUp& other) = delete;
      ~SetUp() { CHECK(!uploader_) << "Missed 'Complete' call"; }

      std::unique_ptr<TestUploader> Complete(
          Status status = Status::StatusOK()) {
        CHECK(uploader_) << "'Complete' already called";
        EXPECT_CALL(*uploader_->mock_upload_,
                    UploadRecordFailure(Eq(uploader_id_), _, _, _))
            .Times(0)
            .InSequence(uploader_->test_upload_sequence_);
        EXPECT_CALL(*uploader_->mock_upload_,
                    UploadComplete(Eq(uploader_id_), Eq(status)))
            .InSequence(uploader_->test_upload_sequence_,
                        uploader_->test_encounter_sequence_)
            .WillOnce(DoAll(
                WithoutArgs(
                    Invoke(waiter_.get(), &test::TestCallbackWaiter::Signal)),
                WithoutArgs(
                    Invoke([]() { LOG(ERROR) << "Completion signaled"; }))));
        return std::move(uploader_);
      }

      SetUp& Required(int64_t sequencing_id, std::string_view value) {
        CHECK(uploader_) << "'Complete' already called";
        EXPECT_CALL(*uploader_->mock_upload_,
                    UploadRecord(Eq(uploader_id_), Eq(priority_),
                                 Eq(sequencing_id), StrEq(std::string(value))))
            .InSequence(uploader_->test_upload_sequence_)
            .WillOnce(Return(true));
        return *this;
      }

      SetUp& RequireEither(int64_t seq_id,
                           std::string_view value,
                           int64_t seq_id_other,
                           std::string_view value_other) {
        CHECK(uploader_) << "'Complete' already called";
        EXPECT_CALL(*uploader_->mock_upload_,
                    UploadRecord(uploader_id_, priority_, _, _))
            .With(AnyOf(
                Args<2, 3>(Eq(std::make_tuple(seq_id, value))),
                Args<2, 3>(Eq(std::make_tuple(seq_id_other, value_other)))))
            .InSequence(uploader_->test_upload_sequence_)
            .WillOnce(Return(true));
        return *this;
      }

      SetUp& Possible(int64_t sequencing_id, std::string_view value) {
        CHECK(uploader_) << "'Complete' already called";
        EXPECT_CALL(*uploader_->mock_upload_,
                    UploadRecord(Eq(uploader_id_), Eq(priority_),
                                 Eq(sequencing_id), StrEq(std::string(value))))
            .Times(Between(0, 1))
            .InSequence(uploader_->test_upload_sequence_)
            .WillRepeatedly(Return(true));
        return *this;
      }

      SetUp& PossibleGap(int64_t sequencing_id, uint64_t count) {
        CHECK(uploader_) << "'Complete' already called";
        EXPECT_CALL(*uploader_->mock_upload_,
                    UploadGap(Eq(uploader_id_), Eq(priority_),
                              Eq(sequencing_id), Eq(count)))
            .Times(Between(0, 1))
            .InSequence(uploader_->test_upload_sequence_)
            .WillRepeatedly(Return(true));
        return *this;
      }

      // The following two expectations refer to the fact that specific
      // sequencing ids have been encountered, regardless of whether they
      // belonged to records or gaps. The expectations are set on a separate
      // test sequence.
      SetUp& RequiredSeqId(int64_t sequencing_id) {
        CHECK(uploader_) << "'Complete' already called";
        EXPECT_CALL(
            *uploader_->mock_upload_,
            EncounterSeqId(Eq(uploader_id_), Eq(priority_), Eq(sequencing_id)))
            .Times(1)
            .InSequence(uploader_->test_encounter_sequence_);
        return *this;
      }

      SetUp& PossibleSeqId(int64_t sequencing_id) {
        CHECK(uploader_) << "'Complete' already called";
        EXPECT_CALL(
            *uploader_->mock_upload_,
            EncounterSeqId(Eq(uploader_id_), Eq(priority_), Eq(sequencing_id)))
            .Times(Between(0, 1))
            .InSequence(uploader_->test_encounter_sequence_);
        return *this;
      }

     private:
      const Priority priority_;
      std::unique_ptr<TestUploader> uploader_;
      const int64_t uploader_id_;
      const raw_ptr<test::TestCallbackWaiter> waiter_;
    };

    explicit TestUploader(StorageDegradationTest* self)
        : uploader_id_(next_uploader_id.fetch_add(1)),
          // Allocate MockUpload as raw pointer and immediately wrap it in
          // unique_ptr and pass to SequenceBoundUpload to own.
          // MockUpload outlives TestUploader and is destructed together with
          // SequenceBoundUpload (on a sequenced task runner).
          mock_upload_(new ::testing::NiceMock<const MockUpload>()),
          sequence_bound_upload_(
              self->main_task_runner_,
              base::WrapUnique(mock_upload_.get()),
              &self->last_upload_generation_id_,
              &self->last_record_digest_map_,
              base::BindRepeating(&RecordUploadStore::Store,
                                  base::Unretained(&self->upload_store_))),

          decryptor_(self->decryptor_) {
      DETACH_FROM_SEQUENCE(test_uploader_checker_);
    }

    ~TestUploader() override {
      DCHECK_CALLED_ON_VALID_SEQUENCE(test_uploader_checker_);
    }

    void ProcessRecord(EncryptedRecord encrypted_record,
                       ScopedReservation scoped_reservation,
                       base::OnceCallback<void(bool)> processed_cb) override {
      DCHECK_CALLED_ON_VALID_SEQUENCE(test_uploader_checker_);
      auto sequence_information = encrypted_record.sequence_information();
      // Wrapped record is not encrypted.
      WrappedRecord wrapped_record;
      ASSERT_TRUE(wrapped_record.ParseFromString(
          encrypted_record.encrypted_wrapped_record()));
      VerifyRecord(std::move(sequence_information), std::move(wrapped_record),
                   std::move(processed_cb));
      return;
    }

    void ProcessGap(SequenceInformation sequence_information,
                    uint64_t count,
                    base::OnceCallback<void(bool)> processed_cb) override {
      DCHECK_CALLED_ON_VALID_SEQUENCE(test_uploader_checker_);
      sequence_bound_upload_.AsyncCall(&SequenceBoundUpload::ProcessGap)
          .WithArgs(uploader_id_, std::move(sequence_information), count,
                    std::move(processed_cb));
    }

    void Completed(Status status) override {
      DCHECK_CALLED_ON_VALID_SEQUENCE(test_uploader_checker_);
      sequence_bound_upload_.AsyncCall(&SequenceBoundUpload::DoUploadComplete)
          .WithArgs(uploader_id_, status);
    }

    // Helper method for setting up dummy mock uploader expectations.
    // To be used only for uploads that we want to just ignore and do not care
    // about their outcome.
    static std::unique_ptr<TestUploader> SetUpDummy(
        StorageDegradationTest* self) {
      auto uploader = std::make_unique<TestUploader>(self);
      // Any Record, RecordFailure of Gap could be encountered, and
      // returning false will cut the upload short.
      EXPECT_CALL(*uploader->mock_upload_,
                  UploadRecord(Eq(uploader->uploader_id_), _, _, _))
          .InSequence(uploader->test_upload_sequence_)
          .WillRepeatedly(Return(false));
      EXPECT_CALL(*uploader->mock_upload_,
                  UploadRecordFailure(Eq(uploader->uploader_id_), _, _, _))
          .InSequence(uploader->test_upload_sequence_)
          .WillRepeatedly(Return(false));
      EXPECT_CALL(*uploader->mock_upload_,
                  UploadGap(Eq(uploader->uploader_id_), _, _, _))
          .InSequence(uploader->test_upload_sequence_)
          .WillRepeatedly(Return(false));
      // Complete will always happen last (whether records/gaps were
      // encountered or not).
      EXPECT_CALL(*uploader->mock_upload_,
                  UploadComplete(Eq(uploader->uploader_id_), _))
          .Times(1)
          .InSequence(uploader->test_upload_sequence_);
      return uploader;
    }

   private:
    void VerifyRecord(SequenceInformation sequence_information,
                      WrappedRecord wrapped_record,
                      base::OnceCallback<void(bool)> processed_cb) {
      DCHECK_CALLED_ON_VALID_SEQUENCE(test_uploader_checker_);
      sequence_bound_upload_.AsyncCall(&SequenceBoundUpload::VerifyRecord)
          .WithArgs(uploader_id_, sequence_information, wrapped_record,
                    std::move(processed_cb));
    }

    SEQUENCE_CHECKER(test_uploader_checker_);

    // Unique ID of the uploader - even if the uploader is allocated
    // on the same address as an earlier one (already released),
    // it will get a new id and thus will ensure the expectations
    // match the expected uploader.
    const int64_t uploader_id_;
    const raw_ptr<const MockUpload> mock_upload_;
    const base::SequenceBound<SequenceBoundUpload> sequence_bound_upload_;

    const scoped_refptr<test::Decryptor> decryptor_;

    Sequence test_encounter_sequence_;
    Sequence test_upload_sequence_;
  };

  StatusOr<scoped_refptr<Storage>> CreateTestStorage(
      const StorageOptions& options,
      scoped_refptr<EncryptionModuleInterface> encryption_module) {
    // Initialize Storage with no key.
    test::TestEvent<StatusOr<scoped_refptr<Storage>>> e;
    Storage::Create(
        options,
        QueuesContainer::Create(
            /*storage_degradation_enabled=*/is_degradation_enabled()),
        encryption_module, base::MakeRefCounted<test::TestCompressionModule>(),
        base::BindRepeating(&StorageDegradationTest::AsyncStartMockUploader,
                            base::Unretained(this)),
        e.cb());
    ASSIGN_OR_RETURN(auto storage, e.result());
    return storage;
  }

  void CreateTestStorageOrDie(
      const StorageOptions& options,
      scoped_refptr<EncryptionModuleInterface> encryption_module =
          EncryptionModule::Create(
              /*renew_encryption_key_period=*/base::Minutes(30))) {
    // No attempts to deliver key.
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(UploaderInterface::UploadReason::KEY_DELIVERY))
        .Times(0);

    ASSERT_FALSE(storage_) << "TestStorage already assigned";
    StatusOr<scoped_refptr<Storage>> storage_result =
        CreateTestStorage(options, encryption_module);
    ASSERT_OK(storage_result)
        << "Failed to create TestStorage, error=" << storage_result.error();
    storage_ = std::move(storage_result.value());
  }

  void ResetTestStorage() {
    if (storage_) {
      // StorageQueue comprising Storage are destructed on threads, wait
      // for them to finish.
      test::TestCallbackAutoWaiter waiter;
      storage_->RegisterCompletionCallback(base::BindOnce(
          &test::TestCallbackAutoWaiter::Signal, base::Unretained(&waiter)));
      storage_.reset();
    }
    // Let remaining asynchronous activity finish.
    // TODO(b/254418902): The next line is not logically necessary, but for
    // unknown reason the tests becomes flaky without it, keeping it for now.
    task_environment_.RunUntilIdle();
    // Make sure all memory is deallocated.
    EXPECT_THAT(options_.memory_resource()->GetUsed(), Eq(0u));
    // Make sure all disk is not reserved (files remain, but Storage is
    // not responsible for them anymore).
    EXPECT_THAT(options_.disk_space_resource()->GetUsed(), Eq(0u));
  }

  const StorageOptions& BuildTestStorageOptions() const { return options_; }

  void AsyncStartMockUploader(
      UploaderInterface::UploadReason reason,
      UploaderInterface::UploaderInterfaceResultCb start_uploader_cb) {
    main_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](UploaderInterface::UploadReason reason,
               UploaderInterface::UploaderInterfaceResultCb start_uploader_cb,
               StorageDegradationTest* self) {
              LOG(ERROR) << "Attempt upload, reason="
                         << UploaderInterface::ReasonToString(reason);
              auto result = self->set_mock_uploader_expectations_.Call(reason);
              if (!result.has_value()) {
                LOG(ERROR) << "Upload not allowed, reason="
                           << UploaderInterface::ReasonToString(reason) << " "
                           << result.error();
                std::move(start_uploader_cb)
                    .Run(base::unexpected(result.error()));
                return;
              }
              auto uploader = std::move(result.value());
              std::move(start_uploader_cb).Run(std::move(uploader));
            },
            reason, std::move(start_uploader_cb), base::Unretained(this)));
  }

  Status WriteString(Priority priority, std::string_view data) {
    EXPECT_TRUE(storage_) << "Storage not created yet";
    test::TestEvent<Status> w;
    Record record;
    record.set_data(std::string(data));
    record.set_destination(UPLOAD_EVENTS);
    record.set_dm_token("DM TOKEN");
    LOG(ERROR) << "Write priority=" << priority << " data='"
               << record.data().substr(0, kDebugDataPrintSize) << "'";
    storage_->Write(priority, std::move(record), w.cb());
    return w.result();
  }

  void WriteStringOrDie(Priority priority, std::string_view data) {
    const Status write_result = WriteString(priority, data);
    ASSERT_OK(write_result) << write_result;
  }

  void ConfirmOrDie(Priority priority,
                    int64_t sequencing_id,
                    bool force = false) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    auto generation_it = last_upload_generation_id_.find(priority);
    ASSERT_NE(generation_it, last_upload_generation_id_.end()) << priority;
    LOG(ERROR) << "Confirm priority=" << priority << " force=" << force
               << " seq=" << sequencing_id << " gen=" << generation_it->second;
    SequenceInformation seq_info;
    seq_info.set_sequencing_id(sequencing_id);
    seq_info.set_generation_id(generation_it->second);
    seq_info.set_priority(priority);
    test::TestEvent<Status> c;
    storage_->Confirm(std::move(seq_info), force, c.cb());
    const Status c_result = c.result();
    ASSERT_OK(c_result) << c_result;
  }

  void FlushOrDie(Priority priority) {
    test::TestEvent<Status> c;
    storage_->Flush(priority, c.cb());
    const Status c_result = c.result();
    ASSERT_OK(c_result) << c_result;
  }

  bool is_degradation_enabled() const { return ::testing::get<1>(GetParam()); }
  size_t single_file_size_limit() const {
    return ::testing::get<1>(GetParam());
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  // Track records that are uploaded across multiple uploads
  RecordUploadStore upload_store_;

  // Sequenced task runner where all EXPECTs will happen - main thread.
  const scoped_refptr<base::SequencedTaskRunner> main_task_runner_{
      base::SequencedTaskRunner::GetCurrentDefault()};
  SEQUENCE_CHECKER(sequence_checker_);

  base::test::ScopedFeatureList scoped_feature_list_;

  base::ScopedTempDir location_;
  TestStorageOptions options_;
  scoped_refptr<test::Decryptor> decryptor_;
  scoped_refptr<Storage> storage_;
  LastUploadedGenerationIdMap last_upload_generation_id_
      GUARDED_BY_CONTEXT(sequence_checker_);
  bool expect_to_need_key_{false};

  // Test-wide global mapping of <generation id, sequencing id> to record
  // digest. Serves all TestUploaders created by test fixture.
  LastRecordDigest::Map last_record_digest_map_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Mock to be called for setting up the uploader.
  ::testing::MockFunction<StatusOr<std::unique_ptr<TestUploader>>(
      UploaderInterface::UploadReason /*reason*/)>
      set_mock_uploader_expectations_;
};

// Test no available files to delete
TEST_P(StorageDegradationTest, WriteAttemptWithRecordsSheddingFailure) {
  // TO-DO cleanup this test, build a test that actually deletes files.
  CreateTestStorageOrDie(BuildTestStorageOptions());

  // Reserve the remaining space to have none available and trigger Records
  // Shedding
  const uint64_t temp_used = options_.disk_space_resource()->GetUsed();
  const uint64_t temp_total = options_.disk_space_resource()->GetTotal();
  const uint64_t to_reserve = temp_total - temp_used;
  options_.disk_space_resource()->Reserve(to_reserve);

  // Write records on a higher priority queue to see if records shedding has any
  // effect.
  const Status write_result = WriteString(IMMEDIATE, kData[2]);
  ASSERT_FALSE(write_result.ok());

  // Discard the space reserved
  options_.disk_space_resource()->Discard(to_reserve);
}

// Test even single writable file allows degradation.
TEST_P(StorageDegradationTest, WriteAttemptWithSingleFileShedding) {
  CreateTestStorageOrDie(BuildTestStorageOptions());

  // Write records on a certain priority StorageQueue
  WriteStringOrDie(MANUAL_BATCH, kData[0]);
  WriteStringOrDie(MANUAL_BATCH, kData[1]);

  // Reserve the remaining space to have none available and trigger Records
  // Shedding
  const uint64_t temp_used = options_.disk_space_resource()->GetUsed();
  const uint64_t temp_total = options_.disk_space_resource()->GetTotal();
  const uint64_t to_reserve = temp_total - temp_used;
  options_.disk_space_resource()->Reserve(to_reserve);

  // Write records on a higher priority queue to see if records shedding has any
  // effect.
  if (is_degradation_enabled()) {
    LOG(ERROR) << "Feature Enabled >> RecordSheddingSuccessTest";
    // Write and expect immediate upload.
    {
      test::TestCallbackAutoWaiter waiter;
      EXPECT_CALL(set_mock_uploader_expectations_,
                  Call(Eq(UploaderInterface::UploadReason::IMMEDIATE_FLUSH)))
          .WillOnce(
              Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
                return TestUploader::SetUp(IMMEDIATE, &waiter, this)
                    .Required(0, kData[2])
                    .Complete();
              }))
          .RetiresOnSaturation();
      WriteStringOrDie(IMMEDIATE, kData[2]);
    }

    // Discard the space reserved
    options_.disk_space_resource()->Discard(to_reserve);

    // Check that MANUAL_BATCH is partially lost.
    // Add one more record, otherwise upload could be skipped.
    WriteStringOrDie(MANUAL_BATCH, kData[2]);
    {
      test::TestCallbackAutoWaiter waiter;
      EXPECT_CALL(set_mock_uploader_expectations_,
                  Call(Eq(UploaderInterface::UploadReason::MANUAL)))
          .WillOnce(
              Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
                return TestUploader::SetUp(MANUAL_BATCH, &waiter, this)
                    .PossibleGap(0, 1)  // Always lost.
                    .PossibleGap(1, 1)  // May be lost.
                    .Possible(1, kData[1])
                    .Required(2, kData[2])
                    .Complete();
              }))
          .RetiresOnSaturation();
      FlushOrDie(MANUAL_BATCH);
    }

  } else {
    LOG(ERROR) << "Feature Disabled >> RecordSheddingSuccessTest";
    const Status write_result = WriteString(IMMEDIATE, kData[2]);
    ASSERT_FALSE(write_result.ok());

    // Discard the space reserved
    options_.disk_space_resource()->Discard(to_reserve);

    // Check that MANUAL_BATCH is intact.
    {
      test::TestCallbackAutoWaiter waiter;
      EXPECT_CALL(set_mock_uploader_expectations_,
                  Call(Eq(UploaderInterface::UploadReason::MANUAL)))
          .WillOnce(
              Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
                return TestUploader::SetUp(MANUAL_BATCH, &waiter, this)
                    .Required(0, kData[0])
                    .Required(1, kData[1])
                    .Complete();
              }))
          .RetiresOnSaturation();
      FlushOrDie(MANUAL_BATCH);
    }
  }
}

// Test Available files to delete in multiple queues when one is insufficient.
TEST_P(StorageDegradationTest, WriteAttemptWithRecordsSheddingMultipleQueues) {
  // The test will try to write this amount of records.
  static constexpr size_t kAmountOfBigRecords = 10;

  CreateTestStorageOrDie(BuildTestStorageOptions());

  // This writes enough records to create `kAmountOfBigRecords` files in each
  // queue: FAST_BATCH and MANUAL_BATCH
  for (size_t i = 0; i < 2 * kAmountOfBigRecords; i++) {
    WriteStringOrDie(FAST_BATCH, xBigData());
    WriteStringOrDie(MANUAL_BATCH, kData[0]);
  }

  // Flush MANUAL queue so that the write file is closed and new one opened,
  // even thought the records are small.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::MANUAL)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              TestUploader::SetUp uploader(MANUAL_BATCH, &waiter, this);
              for (size_t i = 0; i < 2 * kAmountOfBigRecords; i++) {
                uploader.Required(i, kData[0]);
              }
              return uploader.Complete();
            }))
        .RetiresOnSaturation();
    // Trigger upload on MANUAL.
    FlushOrDie(Priority::MANUAL_BATCH);
  }

  // Reserve the remaining space to have none available and trigger Records
  // Shedding
  const uint64_t temp_used = options_.disk_space_resource()->GetUsed();
  const uint64_t temp_total = options_.disk_space_resource()->GetTotal();
  const uint64_t to_reserve = temp_total - temp_used;
  options_.disk_space_resource()->Reserve(to_reserve);

  // Write records on a higher priority queue to see if records shedding has any
  // effect.
  if (is_degradation_enabled()) {
    LOG(ERROR) << "Feature Enabled >> RecordSheddingSuccessTest";
    // Write and expect immediate upload.
    {
      test::TestCallbackAutoWaiter waiter;
      EXPECT_CALL(set_mock_uploader_expectations_,
                  Call(Eq(UploaderInterface::UploadReason::IMMEDIATE_FLUSH)))
          .WillOnce(
              Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
                return TestUploader::SetUp(IMMEDIATE, &waiter, this)
                    .Required(0, xBigData())
                    .Complete();
              }))
          .RetiresOnSaturation();
      WriteStringOrDie(IMMEDIATE, xBigData());
    }

    // Make sure the other queues partially kept their data and can still
    // upload.
    {
      test::TestCallbackAutoWaiter waiter;
      EXPECT_CALL(set_mock_uploader_expectations_,
                  Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
          .WillOnce(
              Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
                TestUploader::SetUp uploader(FAST_BATCH, &waiter, this);
                // In the higher priority queue at least one record should be
                // lost.
                for (size_t i = 1; i < 2 * kAmountOfBigRecords; i++) {
                  uploader.Possible(i, xBigData());
                }
                return uploader.Complete();
              }))
          .RetiresOnSaturation();
      // Trigger upload on FAST_BATCH.
      task_environment_.FastForwardBy(base::Seconds(1));
    }

    // Add one more record, so that the last file is not empty (otherwise upload
    // may be skipped).
    WriteStringOrDie(MANUAL_BATCH, kData[0]);

    {
      test::TestCallbackAutoWaiter waiter;
      EXPECT_CALL(set_mock_uploader_expectations_,
                  Call(Eq(UploaderInterface::UploadReason::MANUAL)))
          .WillOnce(
              Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
                TestUploader::SetUp uploader(MANUAL_BATCH, &waiter, this);
                // In the lower priority queue all initial records should be
                // lost. Expect the last added record only.
                uploader.Required(2 * kAmountOfBigRecords, kData[0]);
                return uploader.Complete();
              }))
          .RetiresOnSaturation();
      // Trigger upload on MANUAL.
      FlushOrDie(Priority::MANUAL_BATCH);
    }
  } else {
    LOG(ERROR) << "Feature Disabled >> RecordSheddingSuccessTest";
    const Status write_result_immediate = WriteString(IMMEDIATE, kData[2]);
    ASSERT_FALSE(write_result_immediate.ok());

    // Make sure the other queues kept their data.
    {
      test::TestCallbackAutoWaiter waiter;
      EXPECT_CALL(set_mock_uploader_expectations_,
                  Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
          .WillOnce(
              Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
                TestUploader::SetUp uploader(FAST_BATCH, &waiter, this);
                for (size_t i = 0; i < 2 * kAmountOfBigRecords; i++) {
                  uploader.Required(i, xBigData());
                }
                return uploader.Complete();
              }))
          .RetiresOnSaturation();
      // Trigger upload on FAST_BATCH.
      task_environment_.FastForwardBy(base::Seconds(1));
    }
    {
      test::TestCallbackAutoWaiter waiter;
      EXPECT_CALL(set_mock_uploader_expectations_,
                  Call(Eq(UploaderInterface::UploadReason::MANUAL)))
          .WillOnce(
              Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
                TestUploader::SetUp uploader(MANUAL_BATCH, &waiter, this);
                for (size_t i = 0; i < 2 * kAmountOfBigRecords; i++) {
                  uploader.Required(i, kData[0]);
                }
                return uploader.Complete();
              }))
          .RetiresOnSaturation();
      // Trigger upload on MANUAL.
      FlushOrDie(Priority::MANUAL_BATCH);
    }
  }

  // Discard the space reserved
  options_.disk_space_resource()->Discard(to_reserve);
}

// Test Available files to delete in the lowest priority queue out of multiple.
TEST_P(StorageDegradationTest, WriteAttemptWithRecordsSheddingLowestQueue) {
  // The test will try to write this amount of records.
  static constexpr size_t kAmountOfBigRecords = 10;

  CreateTestStorageOrDie(BuildTestStorageOptions());

  // This writes enough records to create `kAmountOfBigRecords` files in each
  // queue: FAST_BATCH and MANUAL_BATCH
  for (size_t i = 0; i < kAmountOfBigRecords; i++) {
    WriteStringOrDie(FAST_BATCH, xBigData());
    WriteStringOrDie(MANUAL_BATCH, xBigData());
  }

  // Reserve the remaining space to have none available and trigger Records
  // Shedding
  const uint64_t temp_used = options_.disk_space_resource()->GetUsed();
  const uint64_t temp_total = options_.disk_space_resource()->GetTotal();
  const uint64_t to_reserve = temp_total - temp_used;
  options_.disk_space_resource()->Reserve(to_reserve);

  // Write records on a higher priority queue to see if records shedding has any
  // effect.
  if (is_degradation_enabled()) {
    LOG(ERROR) << "Feature Enabled >> RecordSheddingSuccessTest";
    // Write and expect immediate upload.
    {
      test::TestCallbackAutoWaiter waiter;
      EXPECT_CALL(set_mock_uploader_expectations_,
                  Call(Eq(UploaderInterface::UploadReason::IMMEDIATE_FLUSH)))
          .WillOnce(
              Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
                return TestUploader::SetUp(IMMEDIATE, &waiter, this)
                    .Required(0, kData[2])
                    .Complete();
              }))
          .RetiresOnSaturation();
      WriteStringOrDie(IMMEDIATE, kData[2]);
    }

    // Make sure the other queues partially kept their data and can still
    // upload.
    {
      test::TestCallbackAutoWaiter waiter;
      EXPECT_CALL(set_mock_uploader_expectations_,
                  Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
          .WillOnce(
              Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
                TestUploader::SetUp uploader(FAST_BATCH, &waiter, this);
                for (size_t i = 0; i < kAmountOfBigRecords; i++) {
                  uploader.Possible(i, xBigData());
                }
                return uploader.Complete();
              }))
          .RetiresOnSaturation();
      // Trigger upload on FAST_BATCH.
      task_environment_.FastForwardBy(base::Seconds(1));
    }
    {
      test::TestCallbackAutoWaiter waiter;
      EXPECT_CALL(set_mock_uploader_expectations_,
                  Call(Eq(UploaderInterface::UploadReason::MANUAL)))
          .WillOnce(
              Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
                TestUploader::SetUp uploader(MANUAL_BATCH, &waiter, this);
                // In the lower priority queue at least one record should be
                // lost.
                for (size_t i = 1; i < kAmountOfBigRecords; i++) {
                  uploader.Possible(i, xBigData());
                }
                return uploader.Complete();
              }))
          .RetiresOnSaturation();
      // Trigger upload on MANUAL.
      FlushOrDie(Priority::MANUAL_BATCH);
    }
  } else {
    LOG(ERROR) << "Feature Disabled >> RecordSheddingSuccessTest";
    const Status write_result_immediate = WriteString(IMMEDIATE, kData[2]);
    ASSERT_FALSE(write_result_immediate.ok());

    // Make sure the other queues kept their data.
    {
      test::TestCallbackAutoWaiter waiter;
      EXPECT_CALL(set_mock_uploader_expectations_,
                  Call(Eq(UploaderInterface::UploadReason::PERIODIC)))
          .WillOnce(
              Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
                TestUploader::SetUp uploader(FAST_BATCH, &waiter, this);
                for (size_t i = 0; i < kAmountOfBigRecords; i++) {
                  uploader.Required(i, xBigData());
                }
                return uploader.Complete();
              }))
          .RetiresOnSaturation();
      // Trigger upload on FAST_BATCH.
      task_environment_.FastForwardBy(base::Seconds(1));
    }
    {
      test::TestCallbackAutoWaiter waiter;
      EXPECT_CALL(set_mock_uploader_expectations_,
                  Call(Eq(UploaderInterface::UploadReason::MANUAL)))
          .WillOnce(
              Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
                TestUploader::SetUp uploader(MANUAL_BATCH, &waiter, this);
                for (size_t i = 0; i < kAmountOfBigRecords; i++) {
                  uploader.Required(i, xBigData());
                }
                return uploader.Complete();
              }))
          .RetiresOnSaturation();
      // Trigger upload on MANUAL.
      FlushOrDie(Priority::MANUAL_BATCH);
    }
  }

  // Discard the space reserved
  options_.disk_space_resource()->Discard(to_reserve);
}

// Test Security queue cant_shed_records option
TEST_P(StorageDegradationTest, RecordsSheddingSecurityCantShedRecords) {
  // The test will try to write this amount of records.
  static constexpr size_t kAmountOfBigRecords = 3u;

  CreateTestStorageOrDie(BuildTestStorageOptions());

  // This writes enough records to create `kAmountOfBigRecords` files in
  // SECURITY queue that does not permit shedding.
  for (size_t i = 0; i < kAmountOfBigRecords; i++) {
    // Write and expect immediate uploads.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::IMMEDIATE_FLUSH)))
        .WillOnce(
            Invoke([&waiter, i, this](UploaderInterface::UploadReason reason) {
              auto uploader = TestUploader::SetUp(SECURITY, &waiter, this);
              for (size_t j = 0; j <= i; j++) {
                uploader.Required(j, xBigData());
              }
              return uploader.Complete();
            }))
        .RetiresOnSaturation();
    WriteStringOrDie(SECURITY, xBigData());
  }

  // Reserve the remaining space to have none available and trigger Records
  // Shedding.
  const uint64_t temp_used = options_.disk_space_resource()->GetUsed();
  const uint64_t temp_total = options_.disk_space_resource()->GetTotal();
  const uint64_t to_reserve = temp_total - temp_used;
  options_.disk_space_resource()->Reserve(to_reserve);

  // Write records on a higher priority queue to see if records shedding has no
  // effect. Expect upload even with failure, since there are other records in
  // the queue.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Eq(UploaderInterface::UploadReason::IMMEDIATE_FLUSH)))
        .WillOnce(
            Invoke([&waiter, this](UploaderInterface::UploadReason reason) {
              auto uploader = TestUploader::SetUp(SECURITY, &waiter, this);
              for (size_t j = 0; j < kAmountOfBigRecords; j++) {
                uploader.Required(j, xBigData());
              }
              return uploader.Complete();
            }))
        .RetiresOnSaturation();
    const Status write_result = WriteString(SECURITY, xBigData());
    ASSERT_FALSE(write_result.ok());
  }

  // Discard the space reserved
  options_.disk_space_resource()->Discard(to_reserve);
}

INSTANTIATE_TEST_SUITE_P(
    VaryingFileSize,
    StorageDegradationTest,
    ::testing::Combine(::testing::Values(128u * 1024uLL * 1024uLL,
                                         256u /* two records in file */,
                                         1u /* single record in file */),
                       ::testing::Bool() /* true - degradation enabled */));

}  // namespace
}  // namespace reporting
