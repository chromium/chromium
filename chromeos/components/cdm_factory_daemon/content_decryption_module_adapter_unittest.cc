// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/components/cdm_factory_daemon/content_decryption_module_adapter.h"

#include <algorithm>

#include "base/logging.h"
#include "base/test/mock_callback.h"
#include "base/test/task_environment.h"
#include "media/base/decoder_buffer.h"
#include "media/base/mock_filters.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using DaemonCdm = chromeos::cdm::mojom::ContentDecryptionModule;
using testing::_;
using testing::IsNull;

namespace chromeos {

namespace {

constexpr char kFakeEmeInitData[] = "fake_init_data";
const std::vector<uint8_t> kFakeEncryptedData = {42, 22, 26, 13, 7, 16, 8, 2};
constexpr char kFakeKeyId[] = "fake_key_id";
constexpr char kFakeIv[] = "fake_iv_16_bytes";
constexpr char kFakeServiceCertificate[] = "fake_service_cert";
constexpr char kFakeSessionId1[] = "fakeSid1";
constexpr char kFakeSessionId2[] = "fakeSid2";
constexpr char kFakeSessionUpdate[] = "fake_session_update";

constexpr int64_t kFakeTimestampSec = 42;
constexpr int64_t kFakeDurationSec = 64;
constexpr uint64_t kFakeSecureHandle = 75;

template <size_t size>
std::vector<uint8_t> ToVector(const char (&array)[size]) {
  return std::vector<uint8_t>(array, array + size - 1);
}

MATCHER_P(MatchesDecoderBuffer, buffer, "") {
  DCHECK(arg);
  return arg->MatchesForTesting(*buffer);
}

MATCHER_P(MatchesDecryptConfig, config, "") {
  if (!arg && !*config)
    return true;
  DCHECK(arg);
  return arg->Matches(**config);
}

// Mock of the mojo implementation on the Chrome OS side.
class MockDaemonCdm : public cdm::mojom::ContentDecryptionModule {
 public:
  MockDaemonCdm(mojo::PendingAssociatedReceiver<ContentDecryptionModule>
                    pending_receiver) {
    receiver_.Bind(std::move(pending_receiver));
  }
  ~MockDaemonCdm() override = default;

  MOCK_METHOD(void,
              SetServerCertificate,
              (const std::vector<uint8_t>&, SetServerCertificateCallback));
  MOCK_METHOD(void,
              GetStatusForPolicy,
              (media::HdcpVersion, GetStatusForPolicyCallback));
  MOCK_METHOD(void,
              CreateSessionAndGenerateRequest,
              (media::CdmSessionType,
               media::EmeInitDataType,
               const std::vector<uint8_t>&,
               CreateSessionAndGenerateRequestCallback));
  MOCK_METHOD(void,
              LoadSession,
              (media::CdmSessionType, const std::string&, LoadSessionCallback));
  MOCK_METHOD(void,
              UpdateSession,
              (const std::string&,
               const std::vector<uint8_t>&,
               UpdateSessionCallback));
  MOCK_METHOD(void, CloseSession, (const std::string&, CloseSessionCallback));
  MOCK_METHOD(void, RemoveSession, (const std::string&, RemoveSessionCallback));
  MOCK_METHOD(void,
              DecryptDeprecated,
              (const std::vector<uint8_t>&,
               std::unique_ptr<media::DecryptConfig>,
               DecryptDeprecatedCallback));
  MOCK_METHOD(void,
              Decrypt,
              (const std::vector<uint8_t>&,
               std::unique_ptr<media::DecryptConfig>,
               bool,
               uint64_t,
               DecryptCallback));
  MOCK_METHOD(void,
              GetHwKeyData,
              (std::unique_ptr<media::DecryptConfig>,
               const std::vector<uint8_t>&,
               GetHwKeyDataCallback));

 private:
  mojo::AssociatedReceiver<ContentDecryptionModule> receiver_{this};
};

cdm::mojom::CdmPromiseResultPtr CreatePromise(bool success) {
  cdm::mojom::CdmPromiseResultPtr promise = cdm::mojom::CdmPromiseResult::New();
  promise->success = success;
  if (!success) {
    promise->error_message = "error";
  }
  return promise;
}

scoped_refptr<media::DecoderBuffer> CreateDecoderBuffer(
    base::span<const uint8_t> data) {
  scoped_refptr<media::DecoderBuffer> buffer =
      media::DecoderBuffer::CopyFrom(data);
  buffer->set_timestamp(base::Seconds(kFakeTimestampSec));
  buffer->set_duration(base::Seconds(kFakeDurationSec));
  return buffer;
}

}  // namespace

class ContentDecryptionModuleAdapterTest : public testing::Test {
 protected:
  ContentDecryptionModuleAdapterTest() {
    mojo::AssociatedRemote<cdm::mojom::ContentDecryptionModule> daemon_cdm_mojo;
    mock_daemon_cdm_ = std::make_unique<MockDaemonCdm>(
        daemon_cdm_mojo.BindNewEndpointAndPassDedicatedReceiver());
    cdm_adapter_ = base::WrapRefCounted<ContentDecryptionModuleAdapter>(
        new ContentDecryptionModuleAdapter(
            nullptr /* storage */, std::move(daemon_cdm_mojo),
            mock_session_message_cb_.Get(), mock_session_closed_cb_.Get(),
            mock_session_keys_change_cb_.Get(),
            mock_session_expiration_update_cb_.Get()));
  }

  ~ContentDecryptionModuleAdapterTest() override {
    // Destroy the CdmAdapter first so it can invoke any session closed
    // callbacks on destruction before we destroy the callback mockers.
    cdm_adapter_.reset();
  }

  void LoadSession() {
    EXPECT_CALL(*mock_daemon_cdm_,
                LoadSession(media::CdmSessionType::kPersistentLicense,
                            kFakeSessionId1, _))
        .WillOnce([](media::CdmSessionType session_type,
                     const std::string& session_id,
                     MockDaemonCdm::LoadSessionCallback callback) {
          std::move(callback).Run(CreatePromise(true), kFakeSessionId2);
        });
    std::string session_id;
    std::unique_ptr<media::MockCdmSessionPromise> promise =
        std::make_unique<media::MockCdmSessionPromise>(true, &session_id);
    cdm_adapter_->LoadSession(media::CdmSessionType::kPersistentLicense,
                              kFakeSessionId1, std::move(promise));
    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(session_id, kFakeSessionId2);
  }

  scoped_refptr<ContentDecryptionModuleAdapter> cdm_adapter_;
  std::unique_ptr<MockDaemonCdm> mock_daemon_cdm_;

  base::MockCallback<media::SessionMessageCB> mock_session_message_cb_;
  base::MockCallback<media::SessionClosedCB> mock_session_closed_cb_;
  base::MockCallback<media::SessionKeysChangeCB> mock_session_keys_change_cb_;
  base::MockCallback<media::SessionExpirationUpdateCB>
      mock_session_expiration_update_cb_;

 private:
  base::test::TaskEnvironment task_environment_;
};

TEST_F(ContentDecryptionModuleAdapterTest, GetClientInterface) {
  auto pending_remote = cdm_adapter_->GetClientInterface();
  EXPECT_TRUE(pending_remote.is_valid());
  // If we try to get it again, it should fail.
  ASSERT_DEATH(cdm_adapter_->GetClientInterface(), "");
}

TEST_F(ContentDecryptionModuleAdapterTest, SetServerCertificate_Failure) {
  EXPECT_CALL(*mock_daemon_cdm_,
              SetServerCertificate(ToVector(kFakeServiceCertificate), _))
      .WillOnce([](const std::vector<uint8_t>& cert,
                   MockDaemonCdm::SetServerCertificateCallback callback) {
        std::move(callback).Run(CreatePromise(false));
      });
  std::unique_ptr<media::MockCdmPromise> promise =
      std::make_unique<media::MockCdmPromise>(false);
  cdm_adapter_->SetServerCertificate(ToVector(kFakeServiceCertificate),
                                     std::move(promise));
  base::RunLoop().RunUntilIdle();
}

TEST_F(ContentDecryptionModuleAdapterTest, SetServerCertificate_Success) {
  EXPECT_CALL(*mock_daemon_cdm_,
              SetServerCertificate(ToVector(kFakeServiceCertificate), _))
      .WillOnce([](const std::vector<uint8_t>& cert,
                   MockDaemonCdm::SetServerCertificateCallback callback) {
        std::move(callback).Run(CreatePromise(true));
      });
  std::unique_ptr<media::MockCdmPromise> promise =
      std::make_unique<media::MockCdmPromise>(true);
  cdm_adapter_->SetServerCertificate(ToVector(kFakeServiceCertificate),
                                     std::move(promise));
  base::RunLoop().RunUntilIdle();
}

TEST_F(ContentDecryptionModuleAdapterTest, GetStatusForPolicy_Failure) {
  EXPECT_CALL(*mock_daemon_cdm_,
              GetStatusForPolicy(media::HdcpVersion::kHdcpVersion1_4, _))
      .WillOnce([](media::HdcpVersion hdcp_version,
                   MockDaemonCdm::GetStatusForPolicyCallback callback) {
        std::move(callback).Run(
            CreatePromise(false),
            media::CdmKeyInformation::KeyStatus::OUTPUT_RESTRICTED);
      });
  media::CdmKeyInformation::KeyStatus key_status;
  std::unique_ptr<media::MockCdmKeyStatusPromise> promise =
      std::make_unique<media::MockCdmKeyStatusPromise>(false, &key_status);
  cdm_adapter_->GetStatusForPolicy(media::HdcpVersion::kHdcpVersion1_4,
                                   std::move(promise));
  base::RunLoop().RunUntilIdle();
}

TEST_F(ContentDecryptionModuleAdapterTest,
       GetStatusForPolicy_SuccessRestricted) {
  EXPECT_CALL(*mock_daemon_cdm_,
              GetStatusForPolicy(media::HdcpVersion::kHdcpVersion2_2, _))
      .WillOnce([](media::HdcpVersion hdcp_version,
                   MockDaemonCdm::GetStatusForPolicyCallback callback) {
        std::move(callback).Run(
            CreatePromise(true),
            media::CdmKeyInformation::KeyStatus::OUTPUT_RESTRICTED);
      });
  media::CdmKeyInformation::KeyStatus key_status;
  std::unique_ptr<media::MockCdmKeyStatusPromise> promise =
      std::make_unique<media::MockCdmKeyStatusPromise>(true, &key_status);
  cdm_adapter_->GetStatusForPolicy(media::HdcpVersion::kHdcpVersion2_2,
                                   std::move(promise));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(key_status, media::CdmKeyInformation::KeyStatus::OUTPUT_RESTRICTED);
}

TEST_F(ContentDecryptionModuleAdapterTest, GetStatusForPolicy_SuccessUsable) {
  EXPECT_CALL(*mock_daemon_cdm_,
              GetStatusForPolicy(media::HdcpVersion::kHdcpVersion2_0, _))
      .WillOnce([](media::HdcpVersion hdcp_version,
                   MockDaemonCdm::GetStatusForPolicyCallback callback) {
        std::move(callback).Run(CreatePromise(true),
                                media::CdmKeyInformation::KeyStatus::USABLE);
      });
  media::CdmKeyInformation::KeyStatus key_status;
  std::unique_ptr<media::MockCdmKeyStatusPromise> promise =
      std::make_unique<media::MockCdmKeyStatusPromise>(true, &key_status);
  cdm_adapter_->GetStatusForPolicy(media::HdcpVersion::kHdcpVersion2_0,
                                   std::move(promise));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(key_status, media::CdmKeyInformation::KeyStatus::USABLE);
}

TEST_F(ContentDecryptionModuleAdapterTest,
       CreateSessionAndGenerateRequest_Failure) {
  EXPECT_CALL(*mock_daemon_cdm_,
              CreateSessionAndGenerateRequest(media::CdmSessionType::kTemporary,
                                              media::EmeInitDataType::CENC,
                                              ToVector(kFakeEmeInitData), _))
      .WillOnce(
          [](media::CdmSessionType session_type,
             media::EmeInitDataType init_type,
             const std::vector<uint8_t>& init_data,
             MockDaemonCdm::CreateSessionAndGenerateRequestCallback callback) {
            std::move(callback).Run(CreatePromise(false), "");
          });
  std::string session_id;
  std::unique_ptr<media::MockCdmSessionPromise> promise =
      std::make_unique<media::MockCdmSessionPromise>(false, &session_id);
  cdm_adapter_->CreateSessionAndGenerateRequest(
      media::CdmSessionType::kTemporary, media::EmeInitDataType::CENC,
      ToVector(kFakeEmeInitData), std::move(promise));
  base::RunLoop().RunUntilIdle();
}

TEST_F(ContentDecryptionModuleAdapterTest,
       CreateSessionAndGenerateRequest_Success) {
  EXPECT_CALL(*mock_daemon_cdm_,
              CreateSessionAndGenerateRequest(media::CdmSessionType::kTemporary,
                                              media::EmeInitDataType::CENC,
                                              ToVector(kFakeEmeInitData), _))
      .WillOnce(
          [](media::CdmSessionType session_type,
             media::EmeInitDataType init_type,
             const std::vector<uint8_t>& init_data,
             MockDaemonCdm::CreateSessionAndGenerateRequestCallback callback) {
            std::move(callback).Run(CreatePromise(true), kFakeSessionId1);
          });
  std::string session_id;
  std::unique_ptr<media::MockCdmSessionPromise> promise =
      std::make_unique<media::MockCdmSessionPromise>(true, &session_id);
  cdm_adapter_->CreateSessionAndGenerateRequest(
      media::CdmSessionType::kTemporary, media::EmeInitDataType::CENC,
      ToVector(kFakeEmeInitData), std::move(promise));
  // We should also be getting a session closed callback for any open sessions.
  EXPECT_CALL(mock_session_closed_cb_, Run(kFakeSessionId1, _));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(session_id, kFakeSessionId1);
}

TEST_F(ContentDecryptionModuleAdapterTest, LoadSession_Failure) {
  EXPECT_CALL(*mock_daemon_cdm_,
              LoadSession(media::CdmSessionType::kPersistentLicense,
                          kFakeSessionId1, _))
      .WillOnce([](media::CdmSessionType session_type,
                   const std::string& session_id,
                   MockDaemonCdm::LoadSessionCallback callback) {
        std::move(callback).Run(CreatePromise(false), "");
      });
  std::string session_id;
  std::unique_ptr<media::MockCdmSessionPromise> promise =
      std::make_unique<media::MockCdmSessionPromise>(false, &session_id);
  cdm_adapter_->LoadSession(media::CdmSessionType::kPersistentLicense,
                            kFakeSessionId1, std::move(promise));
  base::RunLoop().RunUntilIdle();
}

TEST_F(ContentDecryptionModuleAdapterTest, LoadSession_Success) {
  LoadSession();
  // We should also be getting a session closed callback for any open sessions.
  EXPECT_CALL(mock_session_closed_cb_, Run(kFakeSessionId2, _));
}

TEST_F(ContentDecryptionModuleAdapterTest, UpdateSession_Failure) {
  EXPECT_CALL(*mock_daemon_cdm_,
              UpdateSession(kFakeSessionId1, ToVector(kFakeSessionUpdate), _))
      .WillOnce([](const std::string& session_id,
                   const std::vector<uint8_t>& response,
                   MockDaemonCdm::UpdateSessionCallback callback) {
        std::move(callback).Run(CreatePromise(false));
      });
  std::unique_ptr<media::MockCdmPromise> promise =
      std::make_unique<media::MockCdmPromise>(false);
  cdm_adapter_->UpdateSession(kFakeSessionId1, ToVector(kFakeSessionUpdate),
                              std::move(promise));
  base::RunLoop().RunUntilIdle();
}

TEST_F(ContentDecryptionModuleAdapterTest, UpdateSession_Success) {
  EXPECT_CALL(*mock_daemon_cdm_,
              UpdateSession(kFakeSessionId1, ToVector(kFakeSessionUpdate), _))
      .WillOnce([](const std::string& session_id,
                   const std::vector<uint8_t>& response,
                   MockDaemonCdm::UpdateSessionCallback callback) {
        std::move(callback).Run(CreatePromise(true));
      });
  std::unique_ptr<media::MockCdmPromise> promise =
      std::make_unique<media::MockCdmPromise>(true);
  cdm_adapter_->UpdateSession(kFakeSessionId1, ToVector(kFakeSessionUpdate),
                              std::move(promise));
  base::RunLoop().RunUntilIdle();
}

TEST_F(ContentDecryptionModuleAdapterTest, CloseSession_Failure) {
  EXPECT_CALL(*mock_daemon_cdm_, CloseSession(kFakeSessionId1, _))
      .WillOnce([](const std::string& session_id,
                   MockDaemonCdm::CloseSessionCallback callback) {
        std::move(callback).Run(CreatePromise(false));
      });
  std::unique_ptr<media::MockCdmPromise> promise =
      std::make_unique<media::MockCdmPromise>(false);
  cdm_adapter_->CloseSession(kFakeSessionId1, std::move(promise));
  base::RunLoop().RunUntilIdle();
}

TEST_F(ContentDecryptionModuleAdapterTest, CloseSession_Success) {
  EXPECT_CALL(*mock_daemon_cdm_, CloseSession(kFakeSessionId1, _))
      .WillOnce([](const std::string& session_id,
                   MockDaemonCdm::CloseSessionCallback callback) {
        std::move(callback).Run(CreatePromise(true));
      });
  std::unique_ptr<media::MockCdmPromise> promise =
      std::make_unique<media::MockCdmPromise>(true);
  cdm_adapter_->CloseSession(kFakeSessionId1, std::move(promise));
  base::RunLoop().RunUntilIdle();
}

TEST_F(ContentDecryptionModuleAdapterTest, RemoveSession_Failure) {
  EXPECT_CALL(*mock_daemon_cdm_, RemoveSession(kFakeSessionId1, _))
      .WillOnce([](const std::string& session_id,
                   MockDaemonCdm::RemoveSessionCallback callback) {
        std::move(callback).Run(CreatePromise(false));
      });
  std::unique_ptr<media::MockCdmPromise> promise =
      std::make_unique<media::MockCdmPromise>(false);
  cdm_adapter_->RemoveSession(kFakeSessionId1, std::move(promise));
  base::RunLoop().RunUntilIdle();
}

TEST_F(ContentDecryptionModuleAdapterTest, RemoveSession_Success) {
  EXPECT_CALL(*mock_daemon_cdm_, RemoveSession(kFakeSessionId1, _))
      .WillOnce([](const std::string& session_id,
                   MockDaemonCdm::RemoveSessionCallback callback) {
        std::move(callback).Run(CreatePromise(true));
      });
  std::unique_ptr<media::MockCdmPromise> promise =
      std::make_unique<media::MockCdmPromise>(true);
  cdm_adapter_->RemoveSession(kFakeSessionId1, std::move(promise));
  base::RunLoop().RunUntilIdle();
}

TEST_F(ContentDecryptionModuleAdapterTest, OnSessionMessage) {
  EXPECT_CALL(mock_session_message_cb_,
              Run(kFakeSessionId1, media::CdmMessageType::LICENSE_REQUEST,
                  ToVector(kFakeSessionUpdate)));
  cdm_adapter_->OnSessionMessage(kFakeSessionId1,
                                 media::CdmMessageType::LICENSE_REQUEST,
                                 ToVector(kFakeSessionUpdate));
}

TEST_F(ContentDecryptionModuleAdapterTest, OnSessionClosed) {
  LoadSession();
  EXPECT_CALL(mock_session_closed_cb_, Run(kFakeSessionId2, _));
  cdm_adapter_->OnSessionClosed(kFakeSessionId2);
}

TEST_F(ContentDecryptionModuleAdapterTest, OnSessionKeysChange) {
  EXPECT_CALL(mock_session_keys_change_cb_, Run(kFakeSessionId1, true, _));
  cdm_adapter_->OnSessionKeysChange(kFakeSessionId1, true, {});
}

TEST_F(ContentDecryptionModuleAdapterTest, OnSessionExpirationUpdate) {
  constexpr double kFakeExpiration = 123456;
  EXPECT_CALL(mock_session_expiration_update_cb_,
              Run(kFakeSessionId2,
                  base::Time::FromSecondsSinceUnixEpoch(kFakeExpiration)));
  cdm_adapter_->OnSessionExpirationUpdate(kFakeSessionId2, kFakeExpiration);
}

TEST_F(ContentDecryptionModuleAdapterTest, RegisterNewKeyCB) {
  base::MockCallback<media::CdmContext::EventCB> event_cb_1;
  base::MockCallback<media::CdmContext::EventCB> event_cb_2;
  auto cb_registration_1 = cdm_adapter_->RegisterEventCB(event_cb_1.Get());
  auto cb_registration_2 = cdm_adapter_->RegisterEventCB(event_cb_2.Get());

  // All registered event callbacks should be invoked.
  EXPECT_CALL(event_cb_1,
              Run(media::CdmContext::Event::kHasAdditionalUsableKey));
  EXPECT_CALL(event_cb_2,
              Run(media::CdmContext::Event::kHasAdditionalUsableKey));
  EXPECT_CALL(mock_session_keys_change_cb_, Run(kFakeSessionId1, true, _));
  cdm_adapter_->OnSessionKeysChange(kFakeSessionId1, true, {});
  base::RunLoop().RunUntilIdle();

  // If no keys change, no registered event callbacks should be invoked.
  EXPECT_CALL(event_cb_1, Run(_)).Times(0);
  EXPECT_CALL(event_cb_2, Run(_)).Times(0);
  EXPECT_CALL(mock_session_keys_change_cb_, Run(kFakeSessionId1, false, _));
  cdm_adapter_->OnSessionKeysChange(kFakeSessionId1, false, {});
  base::RunLoop().RunUntilIdle();
}

TEST_F(ContentDecryptionModuleAdapterTest, Decrypt_TranscryptUnencrypted) {
  std::unique_ptr<media::DecryptConfig> expected_decrypt_config;
  EXPECT_CALL(
      *mock_daemon_cdm_,
      Decrypt(kFakeEncryptedData,
              MatchesDecryptConfig(&expected_decrypt_config), true, 0, _))
      .WillOnce([](const std::vector<uint8_t>& data,
                   std::unique_ptr<media::DecryptConfig> decrypt_config,
                   bool is_video, uint64_t secure_handle,
                   MockDaemonCdm::DecryptCallback callback) {
        // Simulate transcryption (that's the only reason this is called with
        // clear data), by just reversing the data.
        std::vector<uint8_t> decrypted = data;
        std::reverse(std::begin(decrypted), std::end(decrypted));
        std::unique_ptr<media::DecryptConfig> transcrypt_config =
            media::DecryptConfig::CreateCencConfig(
                kFakeKeyId, std::string(16, '0'),
                {media::SubsampleEntry(3, 5)});
        std::move(callback).Run(media::Decryptor::kSuccess,
                                std::move(decrypted),
                                std::move(transcrypt_config));
      });
  scoped_refptr<media::DecoderBuffer> encrypted_buffer =
      CreateDecoderBuffer(kFakeEncryptedData);
  encrypted_buffer->set_is_key_frame(true);
  std::vector<uint8_t> transcrypted_data = kFakeEncryptedData;
  std::reverse(std::begin(transcrypted_data), std::end(transcrypted_data));
  scoped_refptr<media::DecoderBuffer> transcrypted_buffer =
      CreateDecoderBuffer(transcrypted_data);
  transcrypted_buffer->set_decrypt_config(
      media::DecryptConfig::CreateCencConfig(kFakeKeyId, std::string(16, '0'),
                                             {media::SubsampleEntry(3, 5)}));
  transcrypted_buffer->set_is_key_frame(true);
  base::MockCallback<media::Decryptor::DecryptCB> callback;
  EXPECT_CALL(callback, Run(media::Decryptor::kSuccess,
                            MatchesDecoderBuffer(transcrypted_buffer)));
  cdm_adapter_->Decrypt(media::Decryptor::kVideo, encrypted_buffer,
                        callback.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(ContentDecryptionModuleAdapterTest, Decrypt_Failure) {
  EXPECT_CALL(*mock_daemon_cdm_, Decrypt(_, _, _, _, _))
      .WillOnce([](const std::vector<uint8_t>& data,
                   std::unique_ptr<media::DecryptConfig> decrypt_config,
                   bool is_video, uint64_t secure_handle,
                   MockDaemonCdm::DecryptCallback callback) {
        std::move(callback).Run(media::Decryptor::kError, {}, nullptr);
      });
  base::MockCallback<media::Decryptor::DecryptCB> callback;
  EXPECT_CALL(callback, Run(media::Decryptor::kError, IsNull()));
  scoped_refptr<media::DecoderBuffer> encrypted_buffer =
      CreateDecoderBuffer(kFakeEncryptedData);
  encrypted_buffer->set_decrypt_config(media::DecryptConfig::CreateCbcsConfig(
      kFakeKeyId, kFakeIv, {}, media::EncryptionPattern(6, 9)));
  cdm_adapter_->Decrypt(media::Decryptor::kVideo, encrypted_buffer,
                        callback.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(ContentDecryptionModuleAdapterTest, Decrypt_NoKey) {
  EXPECT_CALL(*mock_daemon_cdm_, Decrypt(_, _, _, _, _))
      .WillOnce([](const std::vector<uint8_t>& data,
                   std::unique_ptr<media::DecryptConfig> decrypt_config,
                   bool is_video, uint64_t secure_handle,
                   MockDaemonCdm::DecryptCallback callback) {
        std::move(callback).Run(media::Decryptor::kNoKey, {}, nullptr);
      });
  base::MockCallback<media::Decryptor::DecryptCB> callback;
  EXPECT_CALL(callback, Run(media::Decryptor::kNoKey, IsNull()));
  scoped_refptr<media::DecoderBuffer> encrypted_buffer =
      CreateDecoderBuffer(kFakeEncryptedData);
  encrypted_buffer->set_decrypt_config(media::DecryptConfig::CreateCbcsConfig(
      kFakeKeyId, kFakeIv, {}, media::EncryptionPattern(6, 9)));
  cdm_adapter_->Decrypt(media::Decryptor::kVideo, encrypted_buffer,
                        callback.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(ContentDecryptionModuleAdapterTest, Decrypt_MismatchedSubsamples) {
  EXPECT_CALL(*mock_daemon_cdm_, Decrypt(_, _, _, _, _)).Times(0);
  base::MockCallback<media::Decryptor::DecryptCB> callback;
  EXPECT_CALL(callback, Run(media::Decryptor::kError, IsNull()));
  scoped_refptr<media::DecoderBuffer> encrypted_buffer =
      CreateDecoderBuffer(kFakeEncryptedData);
  encrypted_buffer->set_decrypt_config(media::DecryptConfig::CreateCencConfig(
      kFakeKeyId, kFakeIv, {media::SubsampleEntry(1, 1)}));
  cdm_adapter_->Decrypt(media::Decryptor::kVideo, encrypted_buffer,
                        callback.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(ContentDecryptionModuleAdapterTest, Decrypt_Success) {
  std::unique_ptr<media::DecryptConfig> expected_decrypt_config =
      media::DecryptConfig::CreateCbcsConfig(kFakeKeyId, kFakeIv, {},
                                             media::EncryptionPattern(6, 9));
  EXPECT_CALL(
      *mock_daemon_cdm_,
      Decrypt(kFakeEncryptedData,
              MatchesDecryptConfig(&expected_decrypt_config), true, 0, _))
      .WillOnce([](const std::vector<uint8_t>& data,
                   std::unique_ptr<media::DecryptConfig> decrypt_config,
                   bool is_video, uint64_t secure_handle,
                   MockDaemonCdm::DecryptCallback callback) {
        // For decryption, just reverse the data.
        std::vector<uint8_t> decrypted = data;
        std::reverse(std::begin(decrypted), std::end(decrypted));
        std::move(callback).Run(media::Decryptor::kSuccess,
                                std::move(decrypted), nullptr);
      });
  std::vector<uint8_t> decrypted_data = kFakeEncryptedData;
  std::reverse(std::begin(decrypted_data), std::end(decrypted_data));
  scoped_refptr<media::DecoderBuffer> decrypted_buffer =
      CreateDecoderBuffer(decrypted_data);
  decrypted_buffer->set_is_key_frame(true);

  base::MockCallback<media::Decryptor::DecryptCB> callback;
  EXPECT_CALL(callback, Run(media::Decryptor::kSuccess,
                            MatchesDecoderBuffer(decrypted_buffer)));
  scoped_refptr<media::DecoderBuffer> encrypted_buffer =
      CreateDecoderBuffer(kFakeEncryptedData);
  encrypted_buffer->set_is_key_frame(true);
  encrypted_buffer->set_decrypt_config(media::DecryptConfig::CreateCbcsConfig(
      kFakeKeyId, kFakeIv, {}, media::EncryptionPattern(6, 9)));
  cdm_adapter_->Decrypt(media::Decryptor::kVideo, encrypted_buffer,
                        callback.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(ContentDecryptionModuleAdapterTest, Decrypt_SecureHandleUnencrypted) {
  std::unique_ptr<media::DecryptConfig> no_config;
  EXPECT_CALL(*mock_daemon_cdm_,
              Decrypt(kFakeEncryptedData, MatchesDecryptConfig(&no_config),
                      true, kFakeSecureHandle, _))
      .WillOnce([](const std::vector<uint8_t>& data,
                   std::unique_ptr<media::DecryptConfig> decrypt_config,
                   bool is_video, uint64_t secure_handle,
                   MockDaemonCdm::DecryptCallback callback) {
        // For secure handles, there is no decrypted data returned.
        std::move(callback).Run(media::Decryptor::kSuccess, {}, nullptr);
      });

  base::MockCallback<media::Decryptor::DecryptCB> callback;
  scoped_refptr<media::DecoderBuffer> clear_buffer =
      CreateDecoderBuffer(kFakeEncryptedData);
  clear_buffer->set_is_key_frame(true);
  clear_buffer->WritableSideData().secure_handle = kFakeSecureHandle;
  EXPECT_CALL(callback, Run(media::Decryptor::kSuccess,
                            MatchesDecoderBuffer(clear_buffer)));
  cdm_adapter_->Decrypt(media::Decryptor::kVideo, clear_buffer, callback.Get());
  base::RunLoop().RunUntilIdle();
}

TEST_F(ContentDecryptionModuleAdapterTest, Decrypt_SecureHandleEncrypted) {
  std::unique_ptr<media::DecryptConfig> expected_decrypt_config =
      media::DecryptConfig::CreateCbcsConfig(kFakeKeyId, kFakeIv, {},
                                             media::EncryptionPattern(6, 9));
  EXPECT_CALL(*mock_daemon_cdm_,
              Decrypt(kFakeEncryptedData,
                      MatchesDecryptConfig(&expected_decrypt_config), true,
                      kFakeSecureHandle, _))
      .WillOnce([](const std::vector<uint8_t>& data,
                   std::unique_ptr<media::DecryptConfig> decrypt_config,
                   bool is_video, uint64_t secure_handle,
                   MockDaemonCdm::DecryptCallback callback) {
        // For secure handles, there is no decrypted data returned.
        std::move(callback).Run(media::Decryptor::kSuccess, {}, nullptr);
      });

  scoped_refptr<media::DecoderBuffer> encrypted_buffer =
      CreateDecoderBuffer(kFakeEncryptedData);
  encrypted_buffer->set_is_key_frame(true);
  encrypted_buffer->set_decrypt_config(media::DecryptConfig::CreateCbcsConfig(
      kFakeKeyId, kFakeIv, {}, media::EncryptionPattern(6, 9)));
  encrypted_buffer->WritableSideData().secure_handle = kFakeSecureHandle;
  base::MockCallback<media::Decryptor::DecryptCB> callback;
  EXPECT_CALL(callback, Run(media::Decryptor::kSuccess,
                            MatchesDecoderBuffer(encrypted_buffer)));
  cdm_adapter_->Decrypt(media::Decryptor::kVideo, encrypted_buffer,
                        callback.Get());
  base::RunLoop().RunUntilIdle();
}

}  // namespace chromeos
