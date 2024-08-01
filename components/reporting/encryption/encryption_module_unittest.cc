// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/reporting/encryption/encryption_module.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/bind.h"
#include "base/hash/hash.h"
#include "base/rand_util.h"
#include "base/strings/strcat.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/thread_pool.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "components/reporting/encryption/decryption.h"
#include "components/reporting/encryption/encryption.h"
#include "components/reporting/encryption/encryption_module_interface.h"
#include "components/reporting/encryption/primitives.h"
#include "components/reporting/encryption/testing_primitives.h"
#include "components/reporting/proto/synced/record.pb.h"
#include "components/reporting/resources/resource_manager.h"
#include "components/reporting/util/status.h"
#include "components/reporting/util/status_macros.h"
#include "components/reporting/util/statusor.h"
#include "components/reporting/util/test_support_callbacks.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::StrEq;

namespace reporting {
namespace {

class EncryptionModuleTest : public ::testing::Test {
 protected:
  EncryptionModuleTest() = default;

  void SetUp() override {
    encryption_module_ = EncryptionModule::Create();

    auto decryptor_result = test::Decryptor::Create();
    ASSERT_TRUE(decryptor_result.has_value()) << decryptor_result.error();
    decryptor_ = std::move(decryptor_result.value());
  }

  StatusOr<EncryptedRecord> EncryptSync(std::string_view data) {
    test::TestEvent<StatusOr<EncryptedRecord>> encrypt_record;
    encryption_module_->EncryptRecord(data, encrypt_record.cb());
    return encrypt_record.result();
  }

  StatusOr<std::string> DecryptSync(
      std::pair<std::string /*shared_secret*/, std::string /*encrypted_data*/>
          encrypted) {
    test::TestEvent<StatusOr<test::Decryptor::Handle*>> open_decrypt;
    decryptor_->OpenRecord(encrypted.first, open_decrypt.cb());
    ASSIGN_OR_RETURN(test::Decryptor::Handle* const dec_handle,
                     open_decrypt.result());

    test::TestEvent<Status> add_decrypt;
    dec_handle->AddToRecord(encrypted.second, add_decrypt.cb());
    RETURN_IF_ERROR_STATUS(base::unexpected(add_decrypt.result()));

    std::string decrypted_string;
    test::TestEvent<Status> close_decrypt;
    dec_handle->CloseRecord(base::BindOnce(
        [](std::string* decrypted_string,
           base::OnceCallback<void(Status)> close_cb,
           StatusOr<std::string_view> result) {
          if (!result.has_value()) {
            std::move(close_cb).Run(result.error());
            return;
          }
          *decrypted_string = std::string(result.value());
          std::move(close_cb).Run(Status::StatusOK());
        },
        base::Unretained(&decrypted_string), close_decrypt.cb()));
    RETURN_IF_ERROR_STATUS(base::unexpected(close_decrypt.result()));
    return decrypted_string;
  }

  StatusOr<std::string> DecryptMatchingSecret(
      Encryptor::PublicKeyId public_key_id,
      std::string_view encrypted_key) {
    // Retrieve private key that matches public key hash.
    test::TestEvent<StatusOr<std::string>> retrieve_private_key;
    decryptor_->RetrieveMatchingPrivateKey(public_key_id,
                                           retrieve_private_key.cb());
    ASSIGN_OR_RETURN(std::string private_key, retrieve_private_key.result());
    // Decrypt symmetric key with that private key and peer public key.
    ASSIGN_OR_RETURN(std::string shared_secret,
                     decryptor_->DecryptSecret(private_key, encrypted_key));
    return shared_secret;
  }

  Status AddNewKeyPair() {
    // Generate new pair of private key and public value.
    uint8_t out_public_value[kKeySize];
    uint8_t out_private_key[kKeySize];
    test::GenerateEncryptionKeyPair(out_private_key, out_public_value);

    test::TestEvent<StatusOr<Encryptor::PublicKeyId>> record_keys;
    decryptor_->RecordKeyPair(
        std::string(reinterpret_cast<const char*>(out_private_key), kKeySize),
        std::string(reinterpret_cast<const char*>(out_public_value), kKeySize),
        record_keys.cb());
    ASSIGN_OR_RETURN(Encryptor::PublicKeyId new_public_key_id,
                     record_keys.result());
    test::TestEvent<Status> set_public_key;
    encryption_module_->UpdateAsymmetricKey(
        std::string(reinterpret_cast<const char*>(out_public_value), kKeySize),
        new_public_key_id, set_public_key.cb());
    RETURN_IF_ERROR_STATUS(set_public_key.result());
    return Status::StatusOK();
  }

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

  scoped_refptr<EncryptionModuleInterface> encryption_module_;
  scoped_refptr<test::Decryptor> decryptor_;
};

TEST_F(EncryptionModuleTest, EncryptAndDecrypt) {
  constexpr char kTestString[] = "ABCDEF";

  // Register new pair of private key and public value.
  ASSERT_OK(AddNewKeyPair());

  // Encrypt the test string using the last public value.
  const auto encrypted_result = EncryptSync(kTestString);
  ASSERT_TRUE(encrypted_result.has_value()) << encrypted_result.error();

  // Decrypt shared secret with private asymmetric key.
  auto decrypt_secret_result = DecryptMatchingSecret(
      encrypted_result.value().encryption_info().public_key_id(),
      encrypted_result.value().encryption_info().encryption_key());
  ASSERT_TRUE(decrypt_secret_result.has_value())
      << decrypt_secret_result.error();

  // Decrypt back.
  const auto decrypted_result = DecryptSync(
      std::make_pair(decrypt_secret_result.value(),
                     encrypted_result.value().encrypted_wrapped_record()));
  ASSERT_TRUE(decrypted_result.has_value()) << decrypted_result.error();

  EXPECT_THAT(decrypted_result.value(), StrEq(kTestString));
}

TEST_F(EncryptionModuleTest, EncryptionDisabled) {
  constexpr char kTestString[] = "ABCDEF";

  // Disable encryption for this test.
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndDisableFeature(kEncryptedReportingFeature);

  // Encrypt the test string.
  const auto encrypted_result = EncryptSync(kTestString);
  ASSERT_TRUE(encrypted_result.has_value());

  // Expect the result to be identical to the original record,
  // and have no encryption_info.
  EXPECT_EQ(encrypted_result.value().encrypted_wrapped_record(), kTestString);
  EXPECT_FALSE(encrypted_result.value().has_encryption_info());
}

TEST_F(EncryptionModuleTest, PublicKeyUpdate) {
  constexpr char kTestString[] = "ABCDEF";

  // No key yet, attempt to encrypt the test string.
  ASSERT_FALSE(encryption_module_->has_encryption_key());
  ASSERT_TRUE(encryption_module_->need_encryption_key());
  auto encrypted_result = EncryptSync(kTestString);
  EXPECT_EQ(encrypted_result.error().error_code(), error::NOT_FOUND);

  // Register new pair of private key and public value.
  ASSERT_OK(AddNewKeyPair());
  ASSERT_TRUE(encryption_module_->has_encryption_key());
  ASSERT_FALSE(encryption_module_->need_encryption_key());

  // Encrypt the test string using the last public value.
  encrypted_result = EncryptSync(kTestString);
  ASSERT_TRUE(encrypted_result.has_value()) << encrypted_result.error();

  // Simulate short wait. Key is still available and not needed.
  task_environment_.FastForwardBy(base::Hours(8));
  ASSERT_TRUE(encryption_module_->has_encryption_key());
  ASSERT_FALSE(encryption_module_->need_encryption_key());
  encrypted_result = EncryptSync(kTestString);
  ASSERT_TRUE(encrypted_result.has_value()) << encrypted_result.error();

  // Simulate long wait. Key is still available, but is needed now.
  task_environment_.FastForwardBy(base::Days(1));
  ASSERT_TRUE(encryption_module_->has_encryption_key());
  ASSERT_TRUE(encryption_module_->need_encryption_key());
  encrypted_result = EncryptSync(kTestString);
  ASSERT_TRUE(encrypted_result.has_value()) << encrypted_result.error();

  // Register one more pair of private key and public value.
  ASSERT_OK(AddNewKeyPair());
  ASSERT_TRUE(encryption_module_->has_encryption_key());
  ASSERT_FALSE(encryption_module_->need_encryption_key());
  encrypted_result = EncryptSync(kTestString);
  ASSERT_TRUE(encrypted_result.has_value()) << encrypted_result.error();
}

TEST_F(EncryptionModuleTest, EncryptAndDecryptMultiple) {
  constexpr const char* kTestStrings[] = {"Rec1",    "Rec22",    "Rec333",
                                          "Rec4444", "Rec55555", "Rec666666"};
  // Encrypted records.
  std::vector<EncryptedRecord> encrypted_records;

  // 1. Register first key pair.
  ASSERT_OK(AddNewKeyPair());

  // 2. Encrypt 3 test strings.
  for (const char* test_string :
       {kTestStrings[0], kTestStrings[1], kTestStrings[2]}) {
    const auto encrypted_result = EncryptSync(test_string);
    ASSERT_TRUE(encrypted_result.has_value()) << encrypted_result.error();
    encrypted_records.emplace_back(encrypted_result.value());
  }

  // 3. Register second key pair.
  ASSERT_OK(AddNewKeyPair());

  // 4. Encrypt 2 test strings.
  for (const char* test_string : {kTestStrings[3], kTestStrings[4]}) {
    const auto encrypted_result = EncryptSync(test_string);
    ASSERT_TRUE(encrypted_result.has_value()) << encrypted_result.error();
    encrypted_records.emplace_back(encrypted_result.value());
  }

  // 3. Register third key pair.
  ASSERT_OK(AddNewKeyPair());

  // 4. Encrypt one more test strings.
  for (const char* test_string : {kTestStrings[5]}) {
    const auto encrypted_result = EncryptSync(test_string);
    ASSERT_TRUE(encrypted_result.has_value()) << encrypted_result.error();
    encrypted_records.emplace_back(encrypted_result.value());
  }

  // For every encrypted record:
  for (size_t i = 0; i < encrypted_records.size(); ++i) {
    // Decrypt encrypted_key with private asymmetric key.
    auto decrypt_secret_result = DecryptMatchingSecret(
        encrypted_records[i].encryption_info().public_key_id(),
        encrypted_records[i].encryption_info().encryption_key());
    ASSERT_TRUE(decrypt_secret_result.has_value())
        << decrypt_secret_result.error();

    // Decrypt back.
    const auto decrypted_result = DecryptSync(
        std::make_pair(decrypt_secret_result.value(),
                       encrypted_records[i].encrypted_wrapped_record()));
    ASSERT_TRUE(decrypted_result.has_value()) << decrypted_result.error();

    // Verify match.
    EXPECT_THAT(decrypted_result.value(), StrEq(kTestStrings[i]));
  }
}

TEST_F(EncryptionModuleTest, EncryptAndDecryptMultipleParallel) {
  // Context of single encryption. Self-destructs upon completion or failure.
  class SingleEncryptionContext {
   public:
    SingleEncryptionContext(
        std::string_view test_string,
        std::string_view public_key,
        Encryptor::PublicKeyId public_key_id,
        scoped_refptr<EncryptionModuleInterface> encryption_module,
        base::OnceCallback<void(StatusOr<EncryptedRecord>)> response)
        : test_string_(test_string),
          public_key_(public_key),
          public_key_id_(public_key_id),
          encryption_module_(encryption_module),
          response_(std::move(response)) {}

    SingleEncryptionContext(const SingleEncryptionContext& other) = delete;
    SingleEncryptionContext& operator=(const SingleEncryptionContext& other) =
        delete;

    ~SingleEncryptionContext() {
      CHECK(!response_) << "Self-destruct without prior response";
    }

    void Start() {
      base::ThreadPool::PostTask(
          FROM_HERE, base::BindOnce(&SingleEncryptionContext::SetPublicKey,
                                    base::Unretained(this)));
    }

   private:
    void Respond(StatusOr<EncryptedRecord> result) {
      std::move(response_).Run(result);
      delete this;
    }
    void SetPublicKey() {
      encryption_module_->UpdateAsymmetricKey(
          public_key_, public_key_id_,
          base::BindOnce(
              [](SingleEncryptionContext* self, Status status) {
                if (!status.ok()) {
                  self->Respond(base::unexpected(status));
                  return;
                }
                base::ThreadPool::PostTask(
                    FROM_HERE,
                    base::BindOnce(&SingleEncryptionContext::EncryptRecord,
                                   base::Unretained(self)));
              },
              base::Unretained(this)));
    }
    void EncryptRecord() {
      encryption_module_->EncryptRecord(
          test_string_,
          base::BindOnce(
              [](SingleEncryptionContext* self,
                 StatusOr<EncryptedRecord> encryption_result) {
                base::ThreadPool::PostTask(
                    FROM_HERE,
                    base::BindOnce(&SingleEncryptionContext::Respond,
                                   base::Unretained(self), encryption_result));
              },
              base::Unretained(this)));
    }

   private:
    const std::string test_string_;
    const std::string public_key_;
    const Encryptor::PublicKeyId public_key_id_;
    const scoped_refptr<EncryptionModuleInterface> encryption_module_;
    base::OnceCallback<void(StatusOr<EncryptedRecord>)> response_;
  };

  // Context of single decryption. Self-destructs upon completion or failure.
  class SingleDecryptionContext {
   public:
    SingleDecryptionContext(
        const EncryptedRecord& encrypted_record,
        scoped_refptr<test::Decryptor> decryptor,
        base::OnceCallback<void(StatusOr<std::string_view>)> response)
        : encrypted_record_(encrypted_record),
          decryptor_(decryptor),
          response_(std::move(response)) {}

    SingleDecryptionContext(const SingleDecryptionContext& other) = delete;
    SingleDecryptionContext& operator=(const SingleDecryptionContext& other) =
        delete;

    ~SingleDecryptionContext() {
      CHECK(!response_) << "Self-destruct without prior response";
    }

    void Start() {
      base::ThreadPool::PostTask(
          FROM_HERE,
          base::BindOnce(&SingleDecryptionContext::RetrieveMatchingPrivateKey,
                         base::Unretained(this)));
    }

   private:
    void Respond(StatusOr<std::string_view> result) {
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
                if (!private_key_result.has_value()) {
                  self->Respond(base::unexpected(private_key_result.error()));
                  return;
                }
                base::ThreadPool::PostTask(
                    FROM_HERE,
                    base::BindOnce(
                        &SingleDecryptionContext::DecryptSharedSecret,
                        base::Unretained(self), private_key_result.value()));
              },
              base::Unretained(this)));
    }

    void DecryptSharedSecret(std::string_view private_key) {
      // Decrypt shared secret from private key and peer public key.
      auto shared_secret_result = decryptor_->DecryptSecret(
          private_key, encrypted_record_.encryption_info().encryption_key());
      if (!shared_secret_result.has_value()) {
        Respond(base::unexpected(shared_secret_result.error()));
        return;
      }
      base::ThreadPool::PostTask(
          FROM_HERE,
          base::BindOnce(&SingleDecryptionContext::OpenRecord,
                         base::Unretained(this), shared_secret_result.value()));
    }

    void OpenRecord(std::string_view shared_secret) {
      decryptor_->OpenRecord(
          shared_secret,
          base::BindOnce(
              [](SingleDecryptionContext* self,
                 StatusOr<test::Decryptor::Handle*> handle_result) {
                if (!handle_result.has_value()) {
                  self->Respond(base::unexpected(handle_result.error()));
                  return;
                }
                base::ThreadPool::PostTask(
                    FROM_HERE,
                    base::BindOnce(&SingleDecryptionContext::AddToRecord,
                                   base::Unretained(self),
                                   base::Unretained(handle_result.value())));
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
                  self->Respond(base::unexpected(status));
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
             StatusOr<std::string_view> decryption_result) {
            self->Respond(decryption_result);
          },
          base::Unretained(this)));
    }

   private:
    const EncryptedRecord encrypted_record_;
    const scoped_refptr<test::Decryptor> decryptor_;
    base::OnceCallback<void(StatusOr<std::string_view>)> response_;
  };

  constexpr std::array<const char*, 6> kTestStrings = {
      "Rec1", "Rec22", "Rec333", "Rec4444", "Rec55555", "Rec666666"};

  // Public and private key pairs in this test are reversed strings.
  std::vector<std::string> private_key_strings;
  std::vector<std::string> public_value_strings;
  std::vector<Encryptor::PublicKeyId> public_value_ids;
  for (size_t i = 0; i < 3; ++i) {
    // Generate new pair of private key and public value.
    uint8_t out_public_value[kKeySize];
    uint8_t out_private_key[kKeySize];
    test::GenerateEncryptionKeyPair(out_private_key, out_public_value);
    private_key_strings.emplace_back(
        reinterpret_cast<const char*>(out_private_key), kKeySize);
    public_value_strings.emplace_back(
        reinterpret_cast<const char*>(out_public_value), kKeySize);
  }

  // Register all key pairs for decryption.
  std::vector<test::TestEvent<StatusOr<Encryptor::PublicKeyId>>> record_results(
      public_value_strings.size());
  for (size_t i = 0; i < public_value_strings.size(); ++i) {
    base::ThreadPool::PostTask(
        FROM_HERE,
        base::BindOnce(
            [](std::string_view private_key_string,
               std::string_view public_key_string,
               scoped_refptr<test::Decryptor> decryptor,
               base::OnceCallback<void(StatusOr<Encryptor::PublicKeyId>)>
                   done_cb) {
              decryptor->RecordKeyPair(private_key_string, public_key_string,
                                       std::move(done_cb));
            },
            private_key_strings[i], public_value_strings[i], decryptor_,
            record_results[i].cb()));
  }
  // Verify registration success.
  for (auto& record_result : record_results) {
    const auto result = record_result.result();
    ASSERT_TRUE(result.has_value()) << result.error();
    public_value_ids.push_back(result.value());
  }

  // Encrypt all records in parallel.
  std::vector<test::TestEvent<StatusOr<EncryptedRecord>>> results(
      kTestStrings.size());
  for (size_t i = 0; i < kTestStrings.size(); ++i) {
    // Choose random key pair.
    size_t i_key_pair = base::RandInt(0, public_value_strings.size() - 1);
    (new SingleEncryptionContext(
         kTestStrings[i], public_value_strings[i_key_pair],
         public_value_ids[i_key_pair], encryption_module_, results[i].cb()))
        ->Start();
  }

  // Decrypt all records in parallel.
  std::vector<test::TestEvent<StatusOr<std::string>>> decryption_results(
      kTestStrings.size());
  for (size_t i = 0; i < results.size(); ++i) {
    // Verify encryption success.
    const auto result = results[i].result();
    ASSERT_TRUE(result.has_value()) << result.error();
    // Decrypt and compare encrypted_record.
    (new SingleDecryptionContext(
         result.value(), decryptor_,
         base::BindOnce(
             [](base::OnceCallback<void(StatusOr<std::string>)>
                    decryption_result,
                StatusOr<std::string_view> result) {
               if (!result.has_value()) {
                 std::move(decryption_result)
                     .Run(base::unexpected(result.error()));
                 return;
               }
               std::move(decryption_result).Run(std::string(result.value()));
             },
             decryption_results[i].cb())))
        ->Start();
  }

  // Verify decryption results.
  for (size_t i = 0; i < decryption_results.size(); ++i) {
    const auto decryption_result = decryption_results[i].result();
    ASSERT_TRUE(decryption_result.has_value()) << decryption_result.error();
    // Verify data match.
    EXPECT_THAT(decryption_result.value(), StrEq(kTestStrings[i]));
  }
}
}  // namespace
}  // namespace reporting
