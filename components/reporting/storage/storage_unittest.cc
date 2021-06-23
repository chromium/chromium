// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/storage/storage.h"

#include <atomic>
#include <cstdint>
#include <tuple>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/reporting/encryption/decryption.h"
#include "components/reporting/encryption/encryption.h"
#include "components/reporting/encryption/encryption_module.h"
#include "components/reporting/encryption/encryption_module_interface.h"
#include "components/reporting/encryption/test_encryption_module.h"
#include "components/reporting/encryption/testing_primitives.h"
#include "components/reporting/proto/record.pb.h"
#include "components/reporting/proto/record_constants.pb.h"
#include "components/reporting/storage/resources/resource_interface.h"
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
using ::testing::Between;
using ::testing::Eq;
using ::testing::HasSubstr;
using ::testing::Invoke;
using ::testing::Ne;
using ::testing::NotNull;
using ::testing::Property;
using ::testing::Return;
using ::testing::Sequence;
using ::testing::StrEq;
using ::testing::WithArg;
using ::testing::WithArgs;

namespace reporting {
namespace {

// Context of single decryption. Self-destructs upon completion or failure.
class SingleDecryptionContext {
 public:
  SingleDecryptionContext(
      const EncryptedRecord& encrypted_record,
      scoped_refptr<test::Decryptor> decryptor,
      base::OnceCallback<void(StatusOr<base::StringPiece>)> response)
      : encrypted_record_(encrypted_record),
        decryptor_(decryptor),
        response_(std::move(response)) {}

  SingleDecryptionContext(const SingleDecryptionContext& other) = delete;
  SingleDecryptionContext& operator=(const SingleDecryptionContext& other) =
      delete;

  ~SingleDecryptionContext() {
    DCHECK(!response_) << "Self-destruct without prior response";
  }

  void Start() {
    base::ThreadPool::PostTask(
        FROM_HERE,
        base::BindOnce(&SingleDecryptionContext::RetrieveMatchingPrivateKey,
                       base::Unretained(this)));
  }

 private:
  void Respond(StatusOr<base::StringPiece> result) {
    std::move(response_).Run(result);
    delete this;
  }

  void RetrieveMatchingPrivateKey() {
    // Retrieve private key that matches public key hash.
    decryptor_->RetrieveMatchingPrivateKey(
        encrypted_record_.encryption_info().public_key_id(),
        base::BindOnce(
            [](SingleDecryptionContext* self,
               StatusOr<std::string> private_key_result) {
              if (!private_key_result.ok()) {
                self->Respond(private_key_result.status());
                return;
              }
              base::ThreadPool::PostTask(
                  FROM_HERE,
                  base::BindOnce(&SingleDecryptionContext::DecryptSharedSecret,
                                 base::Unretained(self),
                                 private_key_result.ValueOrDie()));
            },
            base::Unretained(this)));
  }

  void DecryptSharedSecret(base::StringPiece private_key) {
    // Decrypt shared secret from private key and peer public key.
    auto shared_secret_result = decryptor_->DecryptSecret(
        private_key, encrypted_record_.encryption_info().encryption_key());
    if (!shared_secret_result.ok()) {
      Respond(shared_secret_result.status());
      return;
    }
    base::ThreadPool::PostTask(
        FROM_HERE, base::BindOnce(&SingleDecryptionContext::OpenRecord,
                                  base::Unretained(this),
                                  shared_secret_result.ValueOrDie()));
  }

  void OpenRecord(base::StringPiece shared_secret) {
    decryptor_->OpenRecord(
        shared_secret,
        base::BindOnce(
            [](SingleDecryptionContext* self,
               StatusOr<test::Decryptor::Handle*> handle_result) {
              if (!handle_result.ok()) {
                self->Respond(handle_result.status());
                return;
              }
              base::ThreadPool::PostTask(
                  FROM_HERE,
                  base::BindOnce(&SingleDecryptionContext::AddToRecord,
                                 base::Unretained(self),
                                 base::Unretained(handle_result.ValueOrDie())));
            },
            base::Unretained(this)));
  }

  void AddToRecord(test::Decryptor::Handle* handle) {
    handle->AddToRecord(
        encrypted_record_.encrypted_wrapped_record(),
        base::BindOnce(
            [](SingleDecryptionContext* self, test::Decryptor::Handle* handle,
               Status status) {
              if (!status.ok()) {
                self->Respond(status);
                return;
              }
              base::ThreadPool::PostTask(
                  FROM_HERE,
                  base::BindOnce(&SingleDecryptionContext::CloseRecord,
                                 base::Unretained(self),
                                 base::Unretained(handle)));
            },
            base::Unretained(this), base::Unretained(handle)));
  }

  void CloseRecord(test::Decryptor::Handle* handle) {
    handle->CloseRecord(base::BindOnce(
        [](SingleDecryptionContext* self,
           StatusOr<base::StringPiece> decryption_result) {
          self->Respond(decryption_result);
        },
        base::Unretained(this)));
  }

 private:
  const EncryptedRecord encrypted_record_;
  const scoped_refptr<test::Decryptor> decryptor_;
  base::OnceCallback<void(StatusOr<base::StringPiece>)> response_;
};

class MockUploadClient : public ::testing::NiceMock<UploaderInterface> {
 public:
  // Mapping of <generation id, sequencing id> to matching record digest.
  // Whenever a record is uploaded and includes last record digest, this map
  // should have that digest already recorded. Only the first record in a
  // generation is uploaded without last record digest.
  using LastRecordDigestMap = std::map<std::tuple<Priority,
                                                  int64_t /*generation id*/,
                                                  int64_t /*sequencing id*/>,
                                       base::Optional<std::string /*digest*/>>;

  explicit MockUploadClient(
      LastRecordDigestMap* last_record_digest_map,
      scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner,
      scoped_refptr<test::Decryptor> decryptor)
      : last_record_digest_map_(last_record_digest_map),
        sequenced_task_runner_(sequenced_task_runner),
        decryptor_(decryptor) {}

  void ProcessRecord(EncryptedRecord encrypted_record,
                     base::OnceCallback<void(bool)> processed_cb) override {
    const auto& sequencing_information =
        encrypted_record.sequencing_information();
    if (!encrypted_record.has_encryption_info()) {
      // Wrapped record is not encrypted.
      WrappedRecord wrapped_record;
      ASSERT_TRUE(wrapped_record.ParseFromString(
          encrypted_record.encrypted_wrapped_record()));
      ScheduleVerifyRecord(sequencing_information, std::move(wrapped_record),
                           std::move(processed_cb));
      return;
    }
    // Decrypt encrypted_record.
    (new SingleDecryptionContext(
         encrypted_record, decryptor_,
         base::BindOnce(
             [](SequencingInformation sequencing_information,
                base::OnceCallback<void(bool)> processed_cb,
                MockUploadClient* client, StatusOr<base::StringPiece> result) {
               ASSERT_OK(result.status());
               WrappedRecord wrapped_record;
               ASSERT_TRUE(wrapped_record.ParseFromArray(
                   result.ValueOrDie().data(), result.ValueOrDie().size()));
               // Verify wrapped record once decrypted.
               client->ScheduleVerifyRecord(sequencing_information,
                                            std::move(wrapped_record),
                                            std::move(processed_cb));
             },
             sequencing_information, std::move(processed_cb),
             base::Unretained(this))))
        ->Start();
  }

  void ProcessGap(SequencingInformation sequencing_information,
                  uint64_t count,
                  base::OnceCallback<void(bool)> processed_cb) override {
    // Verify generation match.
    if (generation_id_.has_value() &&
        generation_id_.value() != sequencing_information.generation_id()) {
      std::move(processed_cb)
          .Run(UploadRecordFailure(
              sequencing_information.priority(),
              sequencing_information.sequencing_id(),
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

    last_record_digest_map_->emplace(
        std::make_tuple(sequencing_information.priority(),
                        sequencing_information.sequencing_id(),
                        sequencing_information.generation_id()),
        base::nullopt);

    for (uint64_t c = 0; c < count; ++c) {
      EncounterSeqId(
          sequencing_information.priority(),
          sequencing_information.sequencing_id() + static_cast<int64_t>(c));
    }
    std::move(processed_cb)
        .Run(UploadGap(sequencing_information.priority(),
                       sequencing_information.sequencing_id(), count));
  }

  void Completed(Status status) override { UploadComplete(status); }

  MOCK_METHOD(void, EncounterSeqId, (Priority, int64_t), (const));
  MOCK_METHOD(bool,
              UploadRecord,
              (Priority, int64_t, base::StringPiece),
              (const));
  MOCK_METHOD(bool, UploadRecordFailure, (Priority, int64_t, Status), (const));
  MOCK_METHOD(bool, UploadGap, (Priority, int64_t, uint64_t), (const));
  MOCK_METHOD(void, UploadComplete, (Status), (const));

  // Helper class for setting up mock client expectations of a successful
  // completion.
  class SetUp {
   public:
    explicit SetUp(Priority priority,
                   MockUploadClient* client,
                   test::TestCallbackWaiter* waiter)
        : priority_(priority), client_(client), waiter_(waiter) {}
    ~SetUp() {
      EXPECT_CALL(*client_, UploadRecordFailure(_, _, _))
          .Times(0)
          .InSequence(client_->test_upload_sequence_);
      test::TestCallbackWaiter* const waiter =
          waiter_;  // let pointer outlive SetUp
      EXPECT_CALL(*client_, UploadComplete(Eq(Status::StatusOK())))
          .InSequence(client_->test_upload_sequence_,
                      client_->test_encounter_sequence_)
          .WillOnce(Invoke([waiter] { waiter->Signal(); }));
    }

    SetUp& Required(int64_t sequencing_id, base::StringPiece value) {
      EXPECT_CALL(*client_, UploadRecord(Eq(priority_), Eq(sequencing_id),
                                         StrEq(std::string(value))))
          .InSequence(client_->test_upload_sequence_)
          .WillOnce(Return(true));
      return *this;
    }

    SetUp& Possible(int64_t sequencing_id, base::StringPiece value) {
      EXPECT_CALL(*client_, UploadRecord(Eq(priority_), Eq(sequencing_id),
                                         StrEq(std::string(value))))
          .Times(Between(0, 1))
          .InSequence(client_->test_upload_sequence_)
          .WillRepeatedly(Return(true));
      return *this;
    }

    SetUp& PossibleGap(int64_t sequence_number, uint64_t count) {
      EXPECT_CALL(*client_,
                  UploadGap(Eq(priority_), Eq(sequence_number), Eq(count)))
          .Times(Between(0, 1))
          .InSequence(client_->test_upload_sequence_)
          .WillRepeatedly(Return(true));
      return *this;
    }

    // The following two expectations refer to the fact that specific
    // sequencing ids have been encountered, regardless of whether they
    // belonged to records or gaps. The expectations are set on a separate
    // test sequence.
    SetUp& RequiredSeqId(int64_t sequence_number) {
      EXPECT_CALL(*client_, EncounterSeqId(Eq(priority_), Eq(sequence_number)))
          .Times(1)
          .InSequence(client_->test_encounter_sequence_);
      return *this;
    }

    SetUp& PossibleSeqId(int64_t sequence_number) {
      EXPECT_CALL(*client_, EncounterSeqId(Eq(priority_), Eq(sequence_number)))
          .Times(Between(0, 1))
          .InSequence(client_->test_encounter_sequence_);
      return *this;
    }

   private:
    const Priority priority_;
    MockUploadClient* const client_;
    test::TestCallbackWaiter* const waiter_;
  };

  // Helper class for setting up mock client expectations on empty queue.
  class SetEmpty {
   public:
    explicit SetEmpty(MockUploadClient* client) : client_(client) {}

    ~SetEmpty() {
      EXPECT_CALL(*client_, UploadRecord(_, _, _)).Times(0);
      EXPECT_CALL(*client_, UploadRecordFailure(_, _, _)).Times(0);
      EXPECT_CALL(*client_, UploadComplete(Eq(Status::StatusOK()))).Times(1);
    }

   private:
    MockUploadClient* const client_;
  };

  // Helper class for setting up mock client expectations for key delivery.
  class SetKeyDelivery {
   public:
    explicit SetKeyDelivery(MockUploadClient* client,
                            test::TestCallbackWaiter* waiter)
        : client_(client), waiter_(waiter) {}

    ~SetKeyDelivery() {
      EXPECT_CALL(*client_, UploadRecord(_, _, _)).Times(0);
      EXPECT_CALL(*client_, UploadRecordFailure(_, _, _)).Times(0);
      test::TestCallbackWaiter* const waiter =
          waiter_;  // let pointer outlive SetUp
      EXPECT_CALL(*client_, UploadComplete(Eq(Status::StatusOK())))
          .WillOnce(Invoke([waiter] { waiter->Signal(); }));
    }

   private:
    MockUploadClient* const client_;
    test::TestCallbackWaiter* const waiter_;
  };

 private:
  void ScheduleVerifyRecord(SequencingInformation sequencing_information,
                            WrappedRecord wrapped_record,
                            base::OnceCallback<void(bool)> processed_cb) {
    sequenced_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&MockUploadClient::VerifyRecord, base::Unretained(this),
                       sequencing_information, std::move(wrapped_record),
                       std::move(processed_cb)));
  }

  void VerifyRecord(SequencingInformation sequencing_information,
                    WrappedRecord wrapped_record,
                    base::OnceCallback<void(bool)> processed_cb) {
    // Verify generation match.
    if (generation_id_.has_value() &&
        generation_id_.value() != sequencing_information.generation_id()) {
      std::move(processed_cb)
          .Run(UploadRecordFailure(
              sequencing_information.priority(),
              sequencing_information.sequencing_id(),
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
            .Run(UploadRecordFailure(
                sequencing_information.priority(),
                sequencing_information.sequencing_id(),
                Status(error::DATA_LOSS, "Record digest mismatch")));
        return;
      }
      if (wrapped_record.has_last_record_digest()) {
        auto it = last_record_digest_map_->find(
            std::make_tuple(sequencing_information.priority(),
                            sequencing_information.sequencing_id() - 1,
                            sequencing_information.generation_id()));
        if (it == last_record_digest_map_->end()) {
          // Previous record has not been seen yet, reschedule. This can happen
          // because decryption is done asynchronously and only sets on
          // sequenced_task_runner_ after it. As a result, later record may get
          // decrypted early and be posted to sequenced_task_runner_ for
          // verification before its predecessor. Rescheduling will move it back
          // in the sequence.
          // Rescheduling may happen multiple times, but once the earlier record
          // is decrypted, it will be also posted to sequenced_task_runner_ and
          // get its digest recorded, making it ready for the current one. This
          // is not an efficient method, but is simple and good enough for the
          // test.
          sequenced_task_runner_->PostTask(
              FROM_HERE,
              base::BindOnce(&MockUploadClient::VerifyRecord,
                             base::Unretained(this), sequencing_information,
                             std::move(wrapped_record),
                             std::move(processed_cb)));
          return;
        }
        // Previous record has been seen, last record digest must match it.
        if (it->second != wrapped_record.last_record_digest()) {
          std::move(processed_cb)
              .Run(UploadRecordFailure(
                  sequencing_information.priority(),
                  sequencing_information.sequencing_id(),
                  Status(error::DATA_LOSS, "Last record digest mismatch")));
          return;
        }
      }
      last_record_digest_map_->emplace(
          std::make_tuple(sequencing_information.priority(),
                          sequencing_information.sequencing_id(),
                          sequencing_information.generation_id()),
          record_digest);
    }

    EncounterSeqId(sequencing_information.priority(),
                   sequencing_information.sequencing_id());
    std::move(processed_cb)
        .Run(UploadRecord(sequencing_information.priority(),
                          sequencing_information.sequencing_id(),
                          wrapped_record.record().data()));
  }

  base::Optional<int64_t> generation_id_;
  LastRecordDigestMap* const last_record_digest_map_;
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  const scoped_refptr<test::Decryptor> decryptor_;

  Sequence test_encounter_sequence_;
  Sequence test_upload_sequence_;
};

class StorageTest
    : public ::testing::TestWithParam<::testing::tuple<bool, size_t>> {
 protected:
  void SetUp() override {
    ASSERT_TRUE(location_.CreateUniqueTempDir());
    // Encryption is disabled by default.
    ASSERT_FALSE(EncryptionModuleInterface::is_enabled());
    if (is_encryption_enabled()) {
      // Enable encryption.
      scoped_feature_list_.InitFromCommandLine(
          {EncryptionModuleInterface::kEncryptedReporting}, {});
      // Generate signing key pair.
      test::GenerateSigningKeyPair(signing_private_key_,
                                   signature_verification_public_key_);
      // Create decryption module.
      auto decryptor_result = test::Decryptor::Create();
      ASSERT_OK(decryptor_result.status()) << decryptor_result.status();
      decryptor_ = std::move(decryptor_result.ValueOrDie());
      // First creation of Storage would need key delivered.
      expect_to_need_key_ = true;
    }
  }

  void TearDown() override {
    ResetTestStorage();
    task_environment_.RunUntilIdle();
    // Make sure all memory is deallocated.
    ASSERT_THAT(GetMemoryResource()->GetUsed(), Eq(0u));
    // Make sure all disk is not reserved (files remain, but Storage is not
    // responsible for them anymore).
    ASSERT_THAT(GetDiskResource()->GetUsed(), Eq(0u));
  }

  StatusOr<scoped_refptr<Storage>> CreateTestStorage(
      const StorageOptions& options,
      scoped_refptr<EncryptionModuleInterface> encryption_module) {
    test::TestCallbackWaiter waiter;
    if (expect_to_need_key_) {
      // Set uploader expectations for any queue; expect no records and need
      // key. Make sure no uploads happen, and key is requested.
      waiter.Attach();
      EXPECT_CALL(set_mock_uploader_expectations_,
                  Call(_, /*need_encryption_key=*/Eq(true), NotNull()))
          .WillOnce(WithArg<2>(
              Invoke([&waiter](MockUploadClient* mock_upload_client) {
                MockUploadClient::SetKeyDelivery client(mock_upload_client,
                                                        &waiter);
              })))
          .RetiresOnSaturation();
    }
    // Initialize Storage with no key.
    test::TestEvent<StatusOr<scoped_refptr<Storage>>> e;
    Storage::Create(options,
                    base::BindRepeating(&StorageTest::AsyncStartMockUploader,
                                        base::Unretained(this)),
                    encryption_module, e.cb());
    ASSIGN_OR_RETURN(auto storage, e.result());
    waiter.Wait();
    if (expect_to_need_key_) {
      // Provision the storage with a key.
      // Key delivery must have been requested above.
      GenerateAndDeliverKey(storage.get());
    }
    return storage;
  }

  void CreateTestStorageOrDie(
      const StorageOptions& options,
      scoped_refptr<EncryptionModuleInterface> encryption_module =
          EncryptionModule::Create(
              /*renew_encryption_key_period=*/base::TimeDelta::FromMinutes(
                  30))) {
    ASSERT_FALSE(storage_) << "StorageTest already assigned";
    StatusOr<scoped_refptr<Storage>> storage_result =
        CreateTestStorage(options, encryption_module);
    ASSERT_OK(storage_result)
        << "Failed to create StorageTest, error=" << storage_result.status();
    storage_ = std::move(storage_result.ValueOrDie());
  }

  void ResetTestStorage() {
    task_environment_.RunUntilIdle();
    storage_.reset();
    expect_to_need_key_ = false;
  }

  StatusOr<scoped_refptr<Storage>> CreateTestStorageWithFailedKeyDelivery(
      const StorageOptions& options,
      size_t failures_count,
      scoped_refptr<EncryptionModuleInterface> encryption_module =
          EncryptionModule::Create(
              /*renew_encryption_key_period=*/base::TimeDelta::FromMinutes(
                  30))) {
    // Initialize Storage with no key.
    test::TestEvent<StatusOr<scoped_refptr<Storage>>> e;
    Storage::Create(
        options,
        base::BindRepeating(&StorageTest::AsyncStartMockUploaderFailing,
                            base::Unretained(this), failures_count),
        encryption_module, e.cb());
    ASSIGN_OR_RETURN(auto storage, e.result());
    return storage;
  }

  StorageOptions BuildTestStorageOptions() const {
    auto options = StorageOptions()
                       .set_directory(base::FilePath(location_.GetPath()))
                       .set_single_file_size(is_encryption_enabled());
    if (is_encryption_enabled()) {
      // Encryption enabled.
      options.set_signature_verification_public_key(std::string(
          reinterpret_cast<const char*>(signature_verification_public_key_),
          kKeySize));
    }
    return options;
  }

  void AsyncStartMockUploader(
      Priority priority,
      bool need_encryption_key,
      UploaderInterface::UploaderInterfaceResultCb start_uploader_cb) {
    auto uploader = std::make_unique<MockUploadClient>(
        &last_record_digest_map_, sequenced_task_runner_, decryptor_);
    set_mock_uploader_expectations_.Call(priority, need_encryption_key,
                                         uploader.get());
    std::move(start_uploader_cb).Run(std::move(uploader));
  }

  void AsyncStartMockUploaderFailing(
      size_t failures_count,
      Priority priority,
      bool need_encryption_key,
      UploaderInterface::UploaderInterfaceResultCb start_uploader_cb) {
    if (key_delivery_failure_count_.fetch_add(1) < failures_count) {
      std::move(start_uploader_cb)
          .Run(Status(error::FAILED_PRECONDITION, "Test cannot start upload"));
      return;
    }
    AsyncStartMockUploader(priority, need_encryption_key,
                           std::move(start_uploader_cb));
  }

  Status WriteString(Priority priority, base::StringPiece data) {
    EXPECT_TRUE(storage_) << "Storage not created yet";
    test::TestEvent<Status> w;
    Record record;
    record.set_data(std::string(data));
    record.set_destination(UPLOAD_EVENTS);
    record.set_dm_token("DM TOKEN");
    storage_->Write(priority, std::move(record), w.cb());
    return w.result();
  }

  void WriteStringOrDie(Priority priority, base::StringPiece data) {
    const Status write_result = WriteString(priority, data);
    ASSERT_OK(write_result) << write_result;
  }

  void ConfirmOrDie(Priority priority,
                    base::Optional<std::int64_t> sequencing_id,
                    bool force = false) {
    test::TestEvent<Status> c;
    storage_->Confirm(priority, sequencing_id, force, c.cb());
    const Status c_result = c.result();
    ASSERT_OK(c_result) << c_result;
  }

  void GenerateAndDeliverKey(Storage* storage) {
    ASSERT_TRUE(decryptor_) << "Decryptor not created";
    // Generate new pair of private key and public value.
    uint8_t private_key[kKeySize];
    Encryptor::PublicKeyId public_key_id;
    uint8_t public_value[kKeySize];
    test::GenerateEncryptionKeyPair(private_key, public_value);
    test::TestEvent<StatusOr<Encryptor::PublicKeyId>> prepare_key_pair;
    decryptor_->RecordKeyPair(
        std::string(reinterpret_cast<const char*>(private_key), kKeySize),
        std::string(reinterpret_cast<const char*>(public_value), kKeySize),
        prepare_key_pair.cb());
    auto prepare_key_result = prepare_key_pair.result();
    ASSERT_OK(prepare_key_result.status());
    public_key_id = prepare_key_result.ValueOrDie();
    // Deliver public key to storage.
    SignedEncryptionInfo signed_encryption_key;
    signed_encryption_key.set_public_asymmetric_key(
        std::string(reinterpret_cast<const char*>(public_value), kKeySize));
    signed_encryption_key.set_public_key_id(public_key_id);
    // Sign public key.
    uint8_t value_to_sign[sizeof(Encryptor::PublicKeyId) + kKeySize];
    memcpy(value_to_sign, &public_key_id, sizeof(Encryptor::PublicKeyId));
    memcpy(value_to_sign + sizeof(Encryptor::PublicKeyId), public_value,
           kKeySize);
    uint8_t signature[kSignatureSize];
    test::SignMessage(
        signing_private_key_,
        base::StringPiece(reinterpret_cast<const char*>(value_to_sign),
                          sizeof(value_to_sign)),
        signature);
    signed_encryption_key.set_signature(
        std::string(reinterpret_cast<const char*>(signature), kSignatureSize));
    // Double check signature.
    ASSERT_TRUE(VerifySignature(
        signature_verification_public_key_,
        base::StringPiece(reinterpret_cast<const char*>(value_to_sign),
                          sizeof(value_to_sign)),
        signature));
    storage->UpdateEncryptionKey(signed_encryption_key);
  }

  bool is_encryption_enabled() const { return ::testing::get<0>(GetParam()); }
  size_t single_file_size_limit() const {
    return ::testing::get<1>(GetParam());
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  base::test::ScopedFeatureList scoped_feature_list_;

  uint8_t signature_verification_public_key_[kKeySize];
  uint8_t signing_private_key_[kSignKeySize];

  base::ScopedTempDir location_;
  scoped_refptr<test::Decryptor> decryptor_;
  scoped_refptr<Storage> storage_;
  bool expect_to_need_key_{false};
  std::atomic<size_t> key_delivery_failure_count_{0};

  // Test-wide global mapping of <generation id, sequencing id> to record
  // digest. Serves all MockUploadClients created by test fixture.
  MockUploadClient::LastRecordDigestMap last_record_digest_map_;
  // Guard Access to last_record_digest_map_
  scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_{
      base::ThreadPool::CreateSequencedTaskRunner(base::TaskTraits())};

  ::testing::MockFunction<
      void(Priority, bool /*need_encryption_key*/, MockUploadClient*)>
      set_mock_uploader_expectations_;
};

constexpr std::array<const char*, 3> kData = {"Rec1111", "Rec222", "Rec33"};
constexpr std::array<const char*, 3> kMoreData = {"More1111", "More222",
                                                  "More33"};

TEST_P(StorageTest, WriteIntoNewStorageAndReopen) {
  CreateTestStorageOrDie(BuildTestStorageOptions());
  EXPECT_CALL(set_mock_uploader_expectations_, Call(_, _, NotNull())).Times(0);
  WriteStringOrDie(FAST_BATCH, kData[0]);
  WriteStringOrDie(FAST_BATCH, kData[1]);
  WriteStringOrDie(FAST_BATCH, kData[2]);

  ResetTestStorage();

  CreateTestStorageOrDie(BuildTestStorageOptions());
}

TEST_P(StorageTest, WriteIntoNewStorageReopenAndWriteMore) {
  CreateTestStorageOrDie(BuildTestStorageOptions());
  EXPECT_CALL(set_mock_uploader_expectations_, Call(_, _, NotNull())).Times(0);
  WriteStringOrDie(FAST_BATCH, kData[0]);
  WriteStringOrDie(FAST_BATCH, kData[1]);
  WriteStringOrDie(FAST_BATCH, kData[2]);

  ResetTestStorage();

  CreateTestStorageOrDie(BuildTestStorageOptions());
  WriteStringOrDie(FAST_BATCH, kMoreData[0]);
  WriteStringOrDie(FAST_BATCH, kMoreData[1]);
  WriteStringOrDie(FAST_BATCH, kMoreData[2]);
}

TEST_P(StorageTest, WriteIntoNewStorageAndUpload) {
  CreateTestStorageOrDie(BuildTestStorageOptions());
  WriteStringOrDie(FAST_BATCH, kData[0]);
  WriteStringOrDie(FAST_BATCH, kData[1]);
  WriteStringOrDie(FAST_BATCH, kData[2]);

  // Set uploader expectations.
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(
      set_mock_uploader_expectations_,
      Call(Eq(FAST_BATCH), /*need_encryption_key=*/Eq(false), NotNull()))
      .WillOnce(WithArgs<0, 2>(Invoke(
          [&waiter](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client, &waiter)
                .Required(0, kData[0])
                .Required(1, kData[1])
                .Required(2, kData[2]);
          })));

  // Trigger upload.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
}

TEST_P(StorageTest, WriteIntoNewStorageAndUploadWithKeyUpdate) {
  // Run the test only when encryption is enabled.
  if (!is_encryption_enabled()) {
    return;
  }

  static constexpr auto kKeyRenewalTime = base::TimeDelta::FromSeconds(5);
  CreateTestStorageOrDie(BuildTestStorageOptions(),
                         EncryptionModule::Create(kKeyRenewalTime));
  WriteStringOrDie(MANUAL_BATCH, kData[0]);
  WriteStringOrDie(MANUAL_BATCH, kData[1]);
  WriteStringOrDie(MANUAL_BATCH, kData[2]);

  // Set uploader expectations.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(Ne(MANUAL_BATCH), /*need_encryption_key=*/_, NotNull()))
        .WillRepeatedly(WithArgs<0, 2>(
            Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
              MockUploadClient::SetEmpty client(mock_upload_client);
            })));
    EXPECT_CALL(
        set_mock_uploader_expectations_,
        Call(Eq(MANUAL_BATCH), /*need_encryption_key=*/Eq(false), NotNull()))
        .WillOnce(WithArgs<0, 2>(Invoke(
            [&waiter](Priority priority, MockUploadClient* mock_upload_client) {
              MockUploadClient::SetUp(priority, mock_upload_client, &waiter)
                  .Required(0, kData[0])
                  .Required(1, kData[1])
                  .Required(2, kData[2]);
            })));

    // Trigger upload with no key update.
    EXPECT_OK(storage_->Flush(MANUAL_BATCH));
  }

  // Write more data.
  WriteStringOrDie(MANUAL_BATCH, kMoreData[0]);
  WriteStringOrDie(MANUAL_BATCH, kMoreData[1]);
  WriteStringOrDie(MANUAL_BATCH, kMoreData[2]);

  // Wait to trigger encryption key request on the next upload
  task_environment_.FastForwardBy(kKeyRenewalTime +
                                  base::TimeDelta::FromSeconds(1));

  // Set uploader expectations with encryption key request.
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(
      set_mock_uploader_expectations_,
      Call(Eq(MANUAL_BATCH), /*need_encryption_key=*/Eq(true), NotNull()))
      .WillOnce(WithArgs<0, 2>(Invoke(
          [&waiter](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client, &waiter)
                .Required(0, kData[0])
                .Required(1, kData[1])
                .Required(2, kData[2])
                .Required(3, kMoreData[0])
                .Required(4, kMoreData[1])
                .Required(5, kMoreData[2]);
          })));

  // Trigger upload with key update after a long wait.
  EXPECT_OK(storage_->Flush(MANUAL_BATCH));
}

TEST_P(StorageTest, WriteIntoNewStorageReopenWriteMoreAndUpload) {
  CreateTestStorageOrDie(BuildTestStorageOptions());
  WriteStringOrDie(FAST_BATCH, kData[0]);
  WriteStringOrDie(FAST_BATCH, kData[1]);
  WriteStringOrDie(FAST_BATCH, kData[2]);

  ResetTestStorage();

  CreateTestStorageOrDie(BuildTestStorageOptions());
  WriteStringOrDie(FAST_BATCH, kMoreData[0]);
  WriteStringOrDie(FAST_BATCH, kMoreData[1]);
  WriteStringOrDie(FAST_BATCH, kMoreData[2]);

  // Set uploader expectations.
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(
      set_mock_uploader_expectations_,
      Call(Eq(FAST_BATCH), /*need_encryption_key=*/Eq(false), NotNull()))
      .WillOnce(WithArgs<0, 2>(Invoke(
          [&waiter](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client, &waiter)
                .Required(0, kData[0])
                .Required(1, kData[1])
                .Required(2, kData[2])
                .Required(3, kMoreData[0])
                .Required(4, kMoreData[1])
                .Required(5, kMoreData[2]);
          })));

  // Trigger upload.
  task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
}

TEST_P(StorageTest, WriteIntoNewStorageAndFlush) {
  CreateTestStorageOrDie(BuildTestStorageOptions());
  WriteStringOrDie(MANUAL_BATCH, kData[0]);
  WriteStringOrDie(MANUAL_BATCH, kData[1]);
  WriteStringOrDie(MANUAL_BATCH, kData[2]);

  // Set uploader expectations.
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(
      set_mock_uploader_expectations_,
      Call(Eq(MANUAL_BATCH), /*need_encryption_key=*/Eq(false), NotNull()))
      .WillOnce(WithArgs<0, 2>(Invoke(
          [&waiter](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client, &waiter)
                .Required(0, kData[0])
                .Required(1, kData[1])
                .Required(2, kData[2]);
          })));

  // Trigger upload.
  EXPECT_OK(storage_->Flush(MANUAL_BATCH));
}

TEST_P(StorageTest, WriteIntoNewStorageReopenWriteMoreAndFlush) {
  CreateTestStorageOrDie(BuildTestStorageOptions());
  WriteStringOrDie(MANUAL_BATCH, kData[0]);
  WriteStringOrDie(MANUAL_BATCH, kData[1]);
  WriteStringOrDie(MANUAL_BATCH, kData[2]);

  ResetTestStorage();

  CreateTestStorageOrDie(BuildTestStorageOptions());
  WriteStringOrDie(MANUAL_BATCH, kMoreData[0]);
  WriteStringOrDie(MANUAL_BATCH, kMoreData[1]);
  WriteStringOrDie(MANUAL_BATCH, kMoreData[2]);

  // Set uploader expectations.
  test::TestCallbackAutoWaiter waiter;
  EXPECT_CALL(
      set_mock_uploader_expectations_,
      Call(Eq(MANUAL_BATCH), /*need_encryption_key=*/Eq(false), NotNull()))
      .WillOnce(WithArgs<0, 2>(Invoke(
          [&waiter](Priority priority, MockUploadClient* mock_upload_client) {
            MockUploadClient::SetUp(priority, mock_upload_client, &waiter)
                .Required(0, kData[0])
                .Required(1, kData[1])
                .Required(2, kData[2])
                .Required(3, kMoreData[0])
                .Required(4, kMoreData[1])
                .Required(5, kMoreData[2]);
          })));

  // Trigger upload.
  EXPECT_OK(storage_->Flush(MANUAL_BATCH));
}

TEST_P(StorageTest, WriteAndRepeatedlyUploadWithConfirmations) {
  CreateTestStorageOrDie(BuildTestStorageOptions());

  WriteStringOrDie(FAST_BATCH, kData[0]);
  WriteStringOrDie(FAST_BATCH, kData[1]);
  WriteStringOrDie(FAST_BATCH, kData[2]);

  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(
        set_mock_uploader_expectations_,
        Call(Eq(FAST_BATCH), /*need_encryption_key=*/Eq(false), NotNull()))
        .WillOnce(WithArgs<0, 2>(Invoke(
            [&waiter](Priority priority, MockUploadClient* mock_upload_client) {
              MockUploadClient::SetUp(priority, mock_upload_client, &waiter)
                  .Required(0, kData[0])
                  .Required(1, kData[1])
                  .Required(2, kData[2]);
            })));

    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  }

  // Confirm #0 and forward time again, removing data #0
  ConfirmOrDie(FAST_BATCH, /*sequencing_id=*/0);
  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(
        set_mock_uploader_expectations_,
        Call(Eq(FAST_BATCH), /*need_encryption_key=*/Eq(false), NotNull()))
        .WillOnce(WithArgs<0, 2>(Invoke(
            [&waiter](Priority priority, MockUploadClient* mock_upload_client) {
              MockUploadClient::SetUp(priority, mock_upload_client, &waiter)
                  .Required(1, kData[1])
                  .Required(2, kData[2]);
            })));
    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  }

  // Confirm #1 and forward time again, removing data #1
  ConfirmOrDie(FAST_BATCH, /*sequencing_id=*/1);
  {
    test::TestCallbackAutoWaiter waiter;
    // Set uploader expectations.
    EXPECT_CALL(
        set_mock_uploader_expectations_,
        Call(Eq(FAST_BATCH), /*need_encryption_key=*/Eq(false), NotNull()))
        .WillOnce(WithArgs<0, 2>(Invoke(
            [&waiter](Priority priority, MockUploadClient* mock_upload_client) {
              MockUploadClient::SetUp(priority, mock_upload_client, &waiter)
                  .Required(2, kData[2]);
            })));
    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  }

  // Add more records and verify that #2 and new records are returned.
  WriteStringOrDie(FAST_BATCH, kMoreData[0]);
  WriteStringOrDie(FAST_BATCH, kMoreData[1]);
  WriteStringOrDie(FAST_BATCH, kMoreData[2]);

  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(
        set_mock_uploader_expectations_,
        Call(Eq(FAST_BATCH), /*need_encryption_key=*/Eq(false), NotNull()))
        .WillOnce(WithArgs<0, 2>(Invoke(
            [&waiter](Priority priority, MockUploadClient* mock_upload_client) {
              MockUploadClient::SetUp(priority, mock_upload_client, &waiter)
                  .Required(2, kData[2])
                  .Required(3, kMoreData[0])
                  .Required(4, kMoreData[1])
                  .Required(5, kMoreData[2]);
            })));
    task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  }

  // Confirm #2 and forward time again, removing data #2
  ConfirmOrDie(FAST_BATCH, /*sequencing_id=*/2);
  {
    // Set uploader expectations.
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(
        set_mock_uploader_expectations_,
        Call(Eq(FAST_BATCH), /*need_encryption_key=*/Eq(false), NotNull()))
        .WillOnce(WithArgs<0, 2>(Invoke(
            [&waiter](Priority priority, MockUploadClient* mock_upload_client) {
              MockUploadClient::SetUp(priority, mock_upload_client, &waiter)
                  .Required(3, kMoreData[0])
                  .Required(4, kMoreData[1])
                  .Required(5, kMoreData[2]);
            })));
    task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  }
}

TEST_P(StorageTest, WriteAndRepeatedlyImmediateUpload) {
  CreateTestStorageOrDie(BuildTestStorageOptions());

  // Upload is initiated asynchronously, so it may happen after the next
  // record is also written. Because of that we set expectations for the
  // records after the current one as |Possible|.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(
        set_mock_uploader_expectations_,
        Call(Eq(IMMEDIATE), /*need_encryption_key=*/Eq(false), NotNull()))
        .WillOnce(WithArgs<0, 2>(Invoke(
            [&waiter](Priority priority, MockUploadClient* mock_upload_client) {
              MockUploadClient::SetUp(priority, mock_upload_client, &waiter)
                  .Required(0, kData[0]);
            })));
    WriteStringOrDie(IMMEDIATE,
                     kData[0]);  // Immediately uploads and verifies.
  }

  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(
        set_mock_uploader_expectations_,
        Call(Eq(IMMEDIATE), /*need_encryption_key=*/Eq(false), NotNull()))
        .WillOnce(WithArgs<0, 2>(Invoke(
            [&waiter](Priority priority, MockUploadClient* mock_upload_client) {
              MockUploadClient::SetUp(priority, mock_upload_client, &waiter)
                  .Required(0, kData[0])
                  .Required(1, kData[1]);
            })));
    WriteStringOrDie(IMMEDIATE,
                     kData[1]);  // Immediately uploads and verifies.
  }

  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(
        set_mock_uploader_expectations_,
        Call(Eq(IMMEDIATE), /*need_encryption_key=*/Eq(false), NotNull()))
        .WillOnce(WithArgs<0, 2>(Invoke(
            [&waiter](Priority priority, MockUploadClient* mock_upload_client) {
              MockUploadClient::SetUp(priority, mock_upload_client, &waiter)
                  .Required(0, kData[0])
                  .Required(1, kData[1])
                  .Required(2, kData[2]);
            })));
    WriteStringOrDie(IMMEDIATE,
                     kData[2]);  // Immediately uploads and verifies.
  }
}

TEST_P(StorageTest, WriteAndRepeatedlyImmediateUploadWithConfirmations) {
  CreateTestStorageOrDie(BuildTestStorageOptions());

  // Upload is initiated asynchronously, so it may happen after the next
  // record is also written. Because of the Confirmation below, we set
  // expectations for the records that may be eliminated by Confirmation as
  // |Possible|.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(
        set_mock_uploader_expectations_,
        Call(Eq(IMMEDIATE), /*need_encryption_key=*/Eq(false), NotNull()))
        .WillOnce(WithArgs<0, 2>(Invoke(
            [&waiter](Priority priority, MockUploadClient* mock_upload_client) {
              MockUploadClient::SetUp(priority, mock_upload_client, &waiter)
                  .Required(0, kData[0]);
            })));
    WriteStringOrDie(IMMEDIATE, kData[0]);
  }

  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(
        set_mock_uploader_expectations_,
        Call(Eq(IMMEDIATE), /*need_encryption_key=*/Eq(false), NotNull()))
        .WillOnce(WithArgs<0, 2>(Invoke(
            [&waiter](Priority priority, MockUploadClient* mock_upload_client) {
              MockUploadClient::SetUp(priority, mock_upload_client, &waiter)
                  .Required(0, kData[0])
                  .Required(1, kData[1]);
            })));
    WriteStringOrDie(IMMEDIATE, kData[1]);
  }

  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(
        set_mock_uploader_expectations_,
        Call(Eq(IMMEDIATE), /*need_encryption_key=*/Eq(false), NotNull()))
        .WillOnce(WithArgs<0, 2>(Invoke(
            [&waiter](Priority priority, MockUploadClient* mock_upload_client) {
              MockUploadClient::SetUp(priority, mock_upload_client, &waiter)
                  .Required(0, kData[0])
                  .Required(1, kData[1])
                  .Required(2, kData[2]);
            })));
    WriteStringOrDie(IMMEDIATE, kData[2]);
  }

  // Confirm #1, removing data #0 and #1
  ConfirmOrDie(IMMEDIATE, /*sequencing_id=*/1);

  // Add more data to verify that #2 and new data are returned.
  // Upload is initiated asynchronously, so it may happen after the next
  // record is also written. Because of that we set expectations for the
  // data after the current one as |Possible|.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(
        set_mock_uploader_expectations_,
        Call(Eq(IMMEDIATE), /*need_encryption_key=*/Eq(false), NotNull()))
        .WillOnce(WithArgs<0, 2>(Invoke(
            [&waiter](Priority priority, MockUploadClient* mock_upload_client) {
              MockUploadClient::SetUp(priority, mock_upload_client, &waiter)
                  .Required(2, kData[2])
                  .Required(3, kMoreData[0]);
            })));
    WriteStringOrDie(IMMEDIATE, kMoreData[0]);
  }

  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(
        set_mock_uploader_expectations_,
        Call(Eq(IMMEDIATE), /*need_encryption_key=*/Eq(false), NotNull()))
        .WillOnce(WithArgs<0, 2>(Invoke(
            [&waiter](Priority priority, MockUploadClient* mock_upload_client) {
              MockUploadClient::SetUp(priority, mock_upload_client, &waiter)
                  .Required(2, kData[2])
                  .Required(3, kMoreData[0])
                  .Required(4, kMoreData[1]);
            })));
    WriteStringOrDie(IMMEDIATE, kMoreData[1]);
  }

  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(
        set_mock_uploader_expectations_,
        Call(Eq(IMMEDIATE), /*need_encryption_key=*/Eq(false), NotNull()))
        .WillOnce(WithArgs<0, 2>(Invoke(
            [&waiter](Priority priority, MockUploadClient* mock_upload_client) {
              MockUploadClient::SetUp(priority, mock_upload_client, &waiter)
                  .Required(2, kData[2])
                  .Required(3, kMoreData[0])
                  .Required(4, kMoreData[1])
                  .Required(5, kMoreData[2]);
            })));
    WriteStringOrDie(IMMEDIATE, kMoreData[2]);
  }
}

TEST_P(StorageTest, WriteAndRepeatedlyUploadMultipleQueues) {
  CreateTestStorageOrDie(BuildTestStorageOptions());

  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(
        set_mock_uploader_expectations_,
        Call(Eq(IMMEDIATE), /*need_encryption_key=*/Eq(false), NotNull()))
        .WillOnce(WithArgs<0, 2>(Invoke(
            [&waiter](Priority priority, MockUploadClient* mock_upload_client) {
              MockUploadClient::SetUp(priority, mock_upload_client, &waiter)
                  .Required(0, kData[0]);
            })));
    WriteStringOrDie(IMMEDIATE, kData[0]);
  }

  WriteStringOrDie(SLOW_BATCH, kMoreData[0]);

  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(
        set_mock_uploader_expectations_,
        Call(Eq(IMMEDIATE), /*need_encryption_key=*/Eq(false), NotNull()))
        .WillOnce(WithArgs<0, 2>(Invoke(
            [&waiter](Priority priority, MockUploadClient* mock_upload_client) {
              MockUploadClient::SetUp(priority, mock_upload_client, &waiter)
                  .Required(0, kData[0])
                  .Required(1, kData[1]);
            })));
    WriteStringOrDie(IMMEDIATE, kData[1]);
  }

  WriteStringOrDie(SLOW_BATCH, kMoreData[1]);

  // Set uploader expectations for FAST_BATCH and SLOW_BATCH.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(
        set_mock_uploader_expectations_,
        Call(Eq(FAST_BATCH), /*need_encryption_key=*/Eq(false), NotNull()))
        .WillRepeatedly(WithArgs<0, 2>(
            Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
              MockUploadClient::SetEmpty client(mock_upload_client);
            })));
    EXPECT_CALL(
        set_mock_uploader_expectations_,
        Call(Eq(SLOW_BATCH), /*need_encryption_key=*/Eq(false), NotNull()))
        .WillOnce(WithArgs<0, 2>(Invoke(
            [&waiter](Priority priority, MockUploadClient* mock_upload_client) {
              MockUploadClient::SetUp(priority, mock_upload_client, &waiter)
                  .Required(0, kMoreData[0])
                  .Required(1, kMoreData[1]);
            })));
    task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(20));
  }

  // Confirm #0 SLOW_BATCH, removing data #0
  ConfirmOrDie(SLOW_BATCH, /*sequencing_id=*/0);

  // Confirm #1 IMMEDIATE, removing data #0 and #1
  ConfirmOrDie(IMMEDIATE, /*sequencing_id=*/1);

  // Add more data
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(
        set_mock_uploader_expectations_,
        Call(Eq(IMMEDIATE), /*need_encryption_key=*/Eq(false), NotNull()))
        .WillOnce(WithArgs<0, 2>(Invoke(
            [&waiter](Priority priority, MockUploadClient* mock_upload_client) {
              MockUploadClient::SetUp(priority, mock_upload_client, &waiter)
                  .Possible(1, kData[1])
                  .Required(2, kData[2]);
            })));
    WriteStringOrDie(IMMEDIATE, kData[2]);
  }
  WriteStringOrDie(SLOW_BATCH, kMoreData[2]);

  // Set uploader expectations for FAST_BATCH and SLOW_BATCH.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(
        set_mock_uploader_expectations_,
        Call(Eq(FAST_BATCH), /*need_encryption_key=*/Eq(false), NotNull()))
        .WillRepeatedly(WithArgs<0, 2>(
            Invoke([](Priority priority, MockUploadClient* mock_upload_client) {
              MockUploadClient::SetEmpty client(mock_upload_client);
            })));
    EXPECT_CALL(
        set_mock_uploader_expectations_,
        Call(Eq(SLOW_BATCH), /*need_encryption_key=*/Eq(false), NotNull()))
        .WillOnce(WithArgs<0, 2>(Invoke(
            [&waiter](Priority priority, MockUploadClient* mock_upload_client) {
              MockUploadClient::SetUp(SLOW_BATCH, mock_upload_client, &waiter)
                  .Required(1, kMoreData[1])
                  .Required(2, kMoreData[2]);
            })));
    task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(20));
  }
}

TEST_P(StorageTest, WriteEncryptFailure) {
  if (!is_encryption_enabled()) {
    return;  // No need to test when encryption is disabled.
  }
  auto test_encryption_module =
      base::MakeRefCounted<test::TestEncryptionModule>();
  test::TestEvent<Status> key_update_event;
  test_encryption_module->UpdateAsymmetricKey("DUMMY KEY", 0,
                                              key_update_event.cb());
  ASSERT_OK(key_update_event.result());
  CreateTestStorageOrDie(BuildTestStorageOptions(), test_encryption_module);
  EXPECT_CALL(*test_encryption_module, EncryptRecordImpl(_, _))
      .WillOnce(WithArg<1>(
          Invoke([](base::OnceCallback<void(StatusOr<EncryptedRecord>)> cb) {
            std::move(cb).Run(Status(error::UNKNOWN, "Failing for tests"));
          })));
  const Status result = WriteString(FAST_BATCH, "TEST_MESSAGE");
  EXPECT_FALSE(result.ok());
  EXPECT_EQ(result.error_code(), error::UNKNOWN);
}

TEST_P(StorageTest, ForceConfirm) {
  CreateTestStorageOrDie(BuildTestStorageOptions());

  WriteStringOrDie(FAST_BATCH, kData[0]);
  WriteStringOrDie(FAST_BATCH, kData[1]);
  WriteStringOrDie(FAST_BATCH, kData[2]);

  // Set uploader expectations.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(
        set_mock_uploader_expectations_,
        Call(Eq(FAST_BATCH), /*need_encryption_key=*/Eq(false), NotNull()))
        .WillOnce(WithArgs<0, 2>(Invoke(
            [&waiter](Priority priority, MockUploadClient* mock_upload_client) {
              MockUploadClient::SetUp(FAST_BATCH, mock_upload_client, &waiter)
                  .Required(0, kData[0])
                  .Required(1, kData[1])
                  .Required(2, kData[2]);
            })));
    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  }

  // Confirm #1 and forward time again, possibly removing records #0 and #1
  ConfirmOrDie(FAST_BATCH, /*sequencing_id=*/1);
  // Set uploader expectations.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(
        set_mock_uploader_expectations_,
        Call(Eq(FAST_BATCH), /*need_encryption_key=*/Eq(false), NotNull()))
        .WillOnce(WithArgs<0, 2>(Invoke(
            [&waiter](Priority priority, MockUploadClient* mock_upload_client) {
              MockUploadClient::SetUp(FAST_BATCH, mock_upload_client, &waiter)
                  .Required(2, kData[2]);
            })));
    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  }

  // Now force confirm #0 and forward time again.
  ConfirmOrDie(FAST_BATCH, /*sequencing_id=*/base::nullopt, /*force=*/true);
  // Set uploader expectations: #0 and #1 could be returned as Gaps
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(
        set_mock_uploader_expectations_,
        Call(Eq(FAST_BATCH), /*need_encryption_key=*/Eq(false), NotNull()))
        .WillOnce(WithArgs<0, 2>(Invoke(
            [&waiter](Priority priority, MockUploadClient* mock_upload_client) {
              MockUploadClient::SetUp(FAST_BATCH, mock_upload_client, &waiter)
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
            })));
    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  }

  // Force confirm #0 and forward time again.
  ConfirmOrDie(FAST_BATCH, /*sequencing_id=*/0, /*force=*/true);
  // Set uploader expectations: #0 and #1 could be returned as Gaps
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(
        set_mock_uploader_expectations_,
        Call(Eq(FAST_BATCH), /*need_encryption_key=*/Eq(false), NotNull()))
        .WillOnce(WithArgs<0, 2>(Invoke(
            [&waiter](Priority priority, MockUploadClient* mock_upload_client) {
              MockUploadClient::SetUp(FAST_BATCH, mock_upload_client, &waiter)
                  .RequiredSeqId(1)
                  .RequiredSeqId(2)
                  // 0-2 must have been encountered, but actual contents
                  // can be different:
                  .PossibleGap(1, 1)
                  .Possible(1, kData[1])
                  .Required(2, kData[2]);
            })));
    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  }
}

TEST_P(StorageTest, KayDeliveryFailureOnNewStorage) {
  static constexpr size_t kFailuresCount = 3;

  if (!is_encryption_enabled()) {
    return;  // Test only makes sense with encryption enabled.
  }

  // Initialize Storage with failure to deliver key.
  ASSERT_FALSE(storage_) << "StorageTest already assigned";
  StatusOr<scoped_refptr<Storage>> storage_result =
      CreateTestStorageWithFailedKeyDelivery(BuildTestStorageOptions(),
                                             kFailuresCount);
  ASSERT_OK(storage_result)
      << "Failed to create StorageTest, error=" << storage_result.status();
  storage_ = std::move(storage_result.ValueOrDie());

  for (size_t failure = 1; failure < kFailuresCount; ++failure) {
    // Failing attempt to write
    const Status write_result = WriteString(FAST_BATCH, kData[0]);
    EXPECT_FALSE(write_result.ok());
    EXPECT_THAT(write_result.error_code(), Eq(error::NOT_FOUND));
    EXPECT_THAT(write_result.message(),
                HasSubstr("Cannot encrypt record - no key"));

    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  }

  // This time key delivery is to succeed.
  // Set uploader expectations for any queue; expect no records and need
  // key. Make sure no uploads happen, and key is requested.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(set_mock_uploader_expectations_,
                Call(_, /*need_encryption_key=*/Eq(true), NotNull()))
        .WillOnce(
            WithArg<2>(Invoke([&waiter](MockUploadClient* mock_upload_client) {
              MockUploadClient::SetKeyDelivery client(mock_upload_client,
                                                      &waiter);
            })))
        .RetiresOnSaturation();

    // Forward time to trigger upload
    task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  }

  // Provision the storage with a key.
  // Key delivery must have been requested above.
  GenerateAndDeliverKey(storage_.get());

  // Successfully write data
  WriteStringOrDie(FAST_BATCH, kData[0]);
  WriteStringOrDie(FAST_BATCH, kData[1]);
  WriteStringOrDie(FAST_BATCH, kData[2]);

  // Set uploader expectations.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(
        set_mock_uploader_expectations_,
        Call(Eq(FAST_BATCH), /*need_encryption_key=*/Eq(false), NotNull()))
        .WillOnce(WithArgs<0, 2>(Invoke(
            [&waiter](Priority priority, MockUploadClient* mock_upload_client) {
              MockUploadClient::SetUp(priority, mock_upload_client, &waiter)
                  .Required(0, kData[0])
                  .Required(1, kData[1])
                  .Required(2, kData[2]);
            })))
        .RetiresOnSaturation();

    // Trigger successful upload.
    task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  }

  ResetTestStorage();

  // Reopen and write more data.
  CreateTestStorageOrDie(BuildTestStorageOptions());
  WriteStringOrDie(FAST_BATCH, kMoreData[0]);
  WriteStringOrDie(FAST_BATCH, kMoreData[1]);
  WriteStringOrDie(FAST_BATCH, kMoreData[2]);

  // Set uploader expectations.
  {
    test::TestCallbackAutoWaiter waiter;
    EXPECT_CALL(
        set_mock_uploader_expectations_,
        Call(Eq(FAST_BATCH), /*need_encryption_key=*/Eq(false), NotNull()))
        .WillOnce(WithArgs<0, 2>(Invoke(
            [&waiter](Priority priority, MockUploadClient* mock_upload_client) {
              MockUploadClient::SetUp(priority, mock_upload_client, &waiter)
                  .Required(0, kData[0])
                  .Required(1, kData[1])
                  .Required(2, kData[2])
                  .Required(3, kMoreData[0])
                  .Required(4, kMoreData[1])
                  .Required(5, kMoreData[2]);
            })));

    // Trigger upload.
    task_environment_.FastForwardBy(base::TimeDelta::FromSeconds(1));
  }
}

INSTANTIATE_TEST_SUITE_P(
    VaryingFileSize,
    StorageTest,
    ::testing::Combine(::testing::Bool() /* true - encryption enabled */,
                       ::testing::Values(128 * 1024LL * 1024LL,
                                         256 /* two records in file */,
                                         1 /* single record in file */)));

}  // namespace
}  // namespace reporting
