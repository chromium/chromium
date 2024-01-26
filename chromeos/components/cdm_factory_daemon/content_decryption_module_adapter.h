// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_CONTENT_DECRYPTION_MODULE_ADAPTER_H_
#define CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_CONTENT_DECRYPTION_MODULE_ADAPTER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/sequenced_task_runner_helpers.h"
#include "base/threading/thread_checker.h"
#include "chromeos/components/cdm_factory_daemon/cdm_storage_adapter.h"
#include "chromeos/components/cdm_factory_daemon/chromeos_cdm_context.h"
#include "chromeos/components/cdm_factory_daemon/mojom/content_decryption_module.mojom.h"
#include "media/base/callback_registry.h"
#include "media/base/cdm_context.h"
#include "media/base/cdm_key_information.h"
#include "media/base/cdm_promise_adapter.h"
#include "media/base/cdm_session_tracker.h"
#include "media/base/content_decryption_module.h"
#include "media/base/decrypt_config.h"
#include "media/base/decryptor.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace chromeos {

// This class adapts the calls between both the media::ContentDecryptionModule
// and media::Decryptor interface over to the
// chromeos::cdm::mojom::ContentDecryptionModule interface. It also adapts the
// chromeos::cdm::mojom::ContentDecryptionModuleClient interface to the
// callbacks passed into the constructor which come from
// media::CdmFactory::Create. Decryption interface only supports decrypting to
// clear and not decoding.
//
// This implementation runs in the GPU process and expects all calls to be
// executed on the mojo thread.  Decrypt, RegisterEventCB, CancelDecrypt,
// GetCdmContext, GetDecryptor, GetChromeOsCdmContext, GetHwKeyData and
// GetCdmContextRef are exceptions, and can be called from any thread.
//
// Instances of this class will always be destructed on the mojo thread
// automatically.
class COMPONENT_EXPORT(CDM_FACTORY_DAEMON) ContentDecryptionModuleAdapter
    : public cdm::mojom::ContentDecryptionModuleClient,
      public media::ContentDecryptionModule,
      public media::CdmContext,
      public chromeos::ChromeOsCdmContext,
      public media::Decryptor {
 public:
  ContentDecryptionModuleAdapter(
      std::unique_ptr<CdmStorageAdapter> storage,
      mojo::AssociatedRemote<cdm::mojom::ContentDecryptionModule>
          cros_cdm_remote,
      const media::SessionMessageCB& session_message_cb,
      const media::SessionClosedCB& session_closed_cb,
      const media::SessionKeysChangeCB& session_keys_change_cb,
      const media::SessionExpirationUpdateCB& session_expiration_update_cb);

  ContentDecryptionModuleAdapter(const ContentDecryptionModuleAdapter&) =
      delete;
  ContentDecryptionModuleAdapter& operator=(
      const ContentDecryptionModuleAdapter&) = delete;

  // This can only be called once after construction.
  mojo::PendingAssociatedRemote<cdm::mojom::ContentDecryptionModuleClient>
  GetClientInterface();

  // media::ContentDecryptionModule:
  void SetServerCertificate(
      const std::vector<uint8_t>& certificate_data,
      std::unique_ptr<media::SimpleCdmPromise> promise) override;
  void GetStatusForPolicy(
      media::HdcpVersion min_hdcp_version,
      std::unique_ptr<media::KeyStatusCdmPromise> promise) override;
  void CreateSessionAndGenerateRequest(
      media::CdmSessionType session_type,
      media::EmeInitDataType init_data_type,
      const std::vector<uint8_t>& init_data,
      std::unique_ptr<media::NewSessionCdmPromise> promise) override;
  void LoadSession(
      media::CdmSessionType session_type,
      const std::string& session_id,
      std::unique_ptr<media::NewSessionCdmPromise> promise) override;
  void UpdateSession(const std::string& session_id,
                     const std::vector<uint8_t>& response,
                     std::unique_ptr<media::SimpleCdmPromise> promise) override;
  void CloseSession(const std::string& session_id,
                    std::unique_ptr<media::SimpleCdmPromise> promise) override;
  void RemoveSession(const std::string& session_id,
                     std::unique_ptr<media::SimpleCdmPromise> promise) override;
  media::CdmContext* GetCdmContext() override;
  void DeleteOnCorrectThread() const override;

  // media::CdmContext:
  std::unique_ptr<media::CallbackRegistration> RegisterEventCB(
      EventCB event_cb) override;
  Decryptor* GetDecryptor() override;
  ChromeOsCdmContext* GetChromeOsCdmContext() override;

  // chromeos::ChromeOsCdmContext:
  void GetHwKeyData(const media::DecryptConfig* decrypt_config,
                    const std::vector<uint8_t>& hw_identifier,
                    GetHwKeyDataCB callback) override;
  void GetHwConfigData(GetHwConfigDataCB callback) override;
  void GetScreenResolutions(GetScreenResolutionsCB callback) override;
  std::unique_ptr<media::CdmContextRef> GetCdmContextRef() override;
  bool UsingArcCdm() const override;
  bool IsRemoteCdm() const override;
  void AllocateSecureBuffer(uint32_t size,
                            AllocateSecureBufferCB callback) override;
  void ParseEncryptedSliceHeader(uint64_t secure_handle,
                                 uint32_t offset,
                                 const std::vector<uint8_t>& stream_data,
                                 ParseEncryptedSliceHeaderCB callback) override;

  // cdm::mojom::ContentDecryptionModuleClient:
  void OnSessionMessage(const std::string& session_id,
                        media::CdmMessageType message_type,
                        const std::vector<uint8_t>& message) override;
  void OnSessionClosed(const std::string& session_id) override;
  void OnSessionKeysChange(
      const std::string& session_id,
      bool has_additional_usable_key,
      std::vector<std::unique_ptr<media::CdmKeyInformation>> keys_info)
      override;
  void OnSessionExpirationUpdate(const std::string& session_id,
                                 double new_expiry_time_sec) override;

  // media::Decryptor:
  void Decrypt(StreamType stream_type,
               scoped_refptr<media::DecoderBuffer> encrypted,
               DecryptCB decrypt_cb) override;
  void CancelDecrypt(StreamType stream_type) override;
  void InitializeAudioDecoder(const media::AudioDecoderConfig& config,
                              DecoderInitCB init_cb) override;
  void InitializeVideoDecoder(const media::VideoDecoderConfig& config,
                              DecoderInitCB init_cb) override;
  void DecryptAndDecodeAudio(scoped_refptr<media::DecoderBuffer> encrypted,
                             AudioDecodeCB audio_decode_cb) override;
  void DecryptAndDecodeVideo(scoped_refptr<media::DecoderBuffer> encrypted,
                             VideoDecodeCB video_decode_cb) override;
  void ResetDecoder(StreamType stream_type) override;
  void DeinitializeDecoder(StreamType stream_type) override;
  bool CanAlwaysDecrypt() override;

 private:
  // For DeleteSoon() in DeleteOnCorrectThread().
  friend class base::DeleteHelper<ContentDecryptionModuleAdapter>;

  ~ContentDecryptionModuleAdapter() override;
  void OnConnectionError();
  void RejectTrackedPromise(uint32_t promise_id,
                            cdm::mojom::CdmPromiseResultPtr promise_result);
  void OnSimplePromiseResult(
      uint32_t promise_id,
      cdm::mojom::CdmPromiseResultPtr cros_promise_result);
  void OnGetStatusForPolicy(uint32_t promise_id,
                            cdm::mojom::CdmPromiseResultPtr cros_promise_result,
                            media::CdmKeyInformation::KeyStatus key_status);
  void OnSessionPromiseResult(
      uint32_t promise_id,
      cdm::mojom::CdmPromiseResultPtr cros_promise_result,
      const std::string& session_id);
  void OnDecrypt(StreamType stream_type,
                 scoped_refptr<media::DecoderBuffer> encrypted,
                 media::Decryptor::DecryptCB decrypt_cb,
                 media::Decryptor::Status status,
                 const std::vector<uint8_t>& decrypted_data,
                 std::unique_ptr<media::DecryptConfig> decrypt_config_out);
  void GetHwKeyDataInternal(
      std::unique_ptr<media::DecryptConfig> decrypt_config,
      const std::vector<uint8_t>& hw_identifier,
      GetHwKeyDataCB callback);

  THREAD_CHECKER(thread_checker_);

  std::unique_ptr<CdmStorageAdapter> storage_;

  mojo::AssociatedReceiver<cdm::mojom::ContentDecryptionModuleClient>
      cros_client_receiver_{this};
  mojo::AssociatedRemote<cdm::mojom::ContentDecryptionModule> cros_cdm_remote_;

  media::SessionMessageCB session_message_cb_;
  media::SessionClosedCB session_closed_cb_;
  media::SessionKeysChangeCB session_keys_change_cb_;
  media::SessionExpirationUpdateCB session_expiration_update_cb_;

  scoped_refptr<base::SequencedTaskRunner> mojo_task_runner_;

  // Track what sessions are open so that if we lose our mojo connection we can
  // invoke the closed callback on all of them.
  media::CdmSessionTracker cdm_session_tracker_;

  // Keep track of outstanding promises.
  media::CdmPromiseAdapter cdm_promise_adapter_;

  media::CallbackRegistry<EventCB::RunType> event_callbacks_;

  // WeakPtrFactory to use for callbacks.
  base::WeakPtrFactory<ContentDecryptionModuleAdapter> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_CONTENT_DECRYPTION_MODULE_ADAPTER_H_
