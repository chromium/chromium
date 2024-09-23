// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/components/cdm_factory_daemon/content_decryption_module_adapter.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chromeos/components/cdm_factory_daemon/chromeos_cdm_factory.h"
#include "media/base/cdm_promise.h"
#include "media/base/decoder_buffer.h"
#include "media/base/eme_constants.h"
#include "media/base/subsample_entry.h"
#include "media/cdm/cdm_context_ref_impl.h"

namespace {

void RejectPromiseConnectionLost(std::unique_ptr<media::CdmPromise> promise) {
  promise->reject(media::CdmPromise::Exception::INVALID_STATE_ERROR, 0,
                  "Mojo connection lost");
}

void ReportSystemCodeUMA(uint32_t system_code) {
  base::UmaHistogramSparse("Media.EME.CrosPlatformCdm.SystemCode", system_code);
}

}  // namespace

namespace chromeos {

ContentDecryptionModuleAdapter::ContentDecryptionModuleAdapter(
    std::unique_ptr<CdmStorageAdapter> storage,
    mojo::AssociatedRemote<cdm::mojom::ContentDecryptionModule> cros_cdm_remote,
    const media::SessionMessageCB& session_message_cb,
    const media::SessionClosedCB& session_closed_cb,
    const media::SessionKeysChangeCB& session_keys_change_cb,
    const media::SessionExpirationUpdateCB& session_expiration_update_cb)
    : storage_(std::move(storage)),
      cros_cdm_remote_(std::move(cros_cdm_remote)),
      session_message_cb_(session_message_cb),
      session_closed_cb_(session_closed_cb),
      session_keys_change_cb_(session_keys_change_cb),
      session_expiration_update_cb_(session_expiration_update_cb),
      mojo_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()) {
  DVLOG(1) << "Created ContentDecryptionModuleAdapter";
  cros_cdm_remote_.set_disconnect_handler(
      base::BindOnce(&ContentDecryptionModuleAdapter::OnConnectionError,
                     base::Unretained(this)));
}

mojo::PendingAssociatedRemote<cdm::mojom::ContentDecryptionModuleClient>
ContentDecryptionModuleAdapter::GetClientInterface() {
  CHECK(!cros_client_receiver_.is_bound());
  auto ret = cros_client_receiver_.BindNewEndpointAndPassRemote();
  cros_client_receiver_.set_disconnect_handler(
      base::BindOnce(&ContentDecryptionModuleAdapter::OnConnectionError,
                     base::Unretained(this)));
  return ret;
}

void ContentDecryptionModuleAdapter::SetServerCertificate(
    const std::vector<uint8_t>& certificate_data,
    std::unique_ptr<media::SimpleCdmPromise> promise) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(2) << __func__;
  if (!cros_cdm_remote_) {
    RejectPromiseConnectionLost(std::move(promise));
    return;
  }
  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  cros_cdm_remote_->SetServerCertificate(
      certificate_data,
      base::BindOnce(&ContentDecryptionModuleAdapter::OnSimplePromiseResult,
                     base::Unretained(this), promise_id));
}

void ContentDecryptionModuleAdapter::GetStatusForPolicy(
    media::HdcpVersion min_hdcp_version,
    std::unique_ptr<media::KeyStatusCdmPromise> promise) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(2) << __func__;
  if (!cros_cdm_remote_) {
    RejectPromiseConnectionLost(std::move(promise));
    return;
  }
  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  cros_cdm_remote_->GetStatusForPolicy(
      min_hdcp_version,
      base::BindOnce(&ContentDecryptionModuleAdapter::OnGetStatusForPolicy,
                     base::Unretained(this), promise_id));
}

void ContentDecryptionModuleAdapter::CreateSessionAndGenerateRequest(
    media::CdmSessionType session_type,
    media::EmeInitDataType init_data_type,
    const std::vector<uint8_t>& init_data,
    std::unique_ptr<media::NewSessionCdmPromise> promise) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(2) << __func__;
  if (!cros_cdm_remote_) {
    RejectPromiseConnectionLost(std::move(promise));
    return;
  }
  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  cros_cdm_remote_->CreateSessionAndGenerateRequest(
      session_type, init_data_type, init_data,
      base::BindOnce(&ContentDecryptionModuleAdapter::OnSessionPromiseResult,
                     base::Unretained(this), promise_id));
}

void ContentDecryptionModuleAdapter::LoadSession(
    media::CdmSessionType session_type,
    const std::string& session_id,
    std::unique_ptr<media::NewSessionCdmPromise> promise) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(2) << __func__;
  if (!cros_cdm_remote_) {
    RejectPromiseConnectionLost(std::move(promise));
    return;
  }
  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  cros_cdm_remote_->LoadSession(
      session_type, session_id,
      base::BindOnce(&ContentDecryptionModuleAdapter::OnSessionPromiseResult,
                     base::Unretained(this), promise_id));
}

void ContentDecryptionModuleAdapter::UpdateSession(
    const std::string& session_id,
    const std::vector<uint8_t>& response,
    std::unique_ptr<media::SimpleCdmPromise> promise) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(2) << __func__;
  if (!cros_cdm_remote_) {
    RejectPromiseConnectionLost(std::move(promise));
    return;
  }
  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  cros_cdm_remote_->UpdateSession(
      session_id, response,
      base::BindOnce(&ContentDecryptionModuleAdapter::OnSimplePromiseResult,
                     base::Unretained(this), promise_id));
}

void ContentDecryptionModuleAdapter::CloseSession(
    const std::string& session_id,
    std::unique_ptr<media::SimpleCdmPromise> promise) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(2) << __func__;
  if (!cros_cdm_remote_) {
    RejectPromiseConnectionLost(std::move(promise));
    return;
  }
  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  cros_cdm_remote_->CloseSession(
      session_id,
      base::BindOnce(&ContentDecryptionModuleAdapter::OnSimplePromiseResult,
                     base::Unretained(this), promise_id));
}

void ContentDecryptionModuleAdapter::RemoveSession(
    const std::string& session_id,
    std::unique_ptr<media::SimpleCdmPromise> promise) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(2) << __func__;
  if (!cros_cdm_remote_) {
    RejectPromiseConnectionLost(std::move(promise));
    return;
  }
  uint32_t promise_id = cdm_promise_adapter_.SavePromise(std::move(promise));
  cros_cdm_remote_->RemoveSession(
      session_id,
      base::BindOnce(&ContentDecryptionModuleAdapter::OnSimplePromiseResult,
                     base::Unretained(this), promise_id));
}

media::CdmContext* ContentDecryptionModuleAdapter::GetCdmContext() {
  return this;
}

void ContentDecryptionModuleAdapter::DeleteOnCorrectThread() const {
  DVLOG(1) << __func__;

  if (!mojo_task_runner_->RunsTasksInCurrentSequence()) {
    // When DeleteSoon returns false, |this| will be leaked, which is okay.
    mojo_task_runner_->DeleteSoon(FROM_HERE, this);
  } else {
    delete this;
  }
}

std::unique_ptr<media::CallbackRegistration>
ContentDecryptionModuleAdapter::RegisterEventCB(EventCB event_cb) {
  return event_callbacks_.Register(std::move(event_cb));
}

media::Decryptor* ContentDecryptionModuleAdapter::GetDecryptor() {
  return this;
}

ChromeOsCdmContext* ContentDecryptionModuleAdapter::GetChromeOsCdmContext() {
  return this;
}

void ContentDecryptionModuleAdapter::GetHwKeyData(
    const media::DecryptConfig* decrypt_config,
    const std::vector<uint8_t>& hw_identifier,
    GetHwKeyDataCB callback) {
  // Take the fields we want out of the |decrypt_config| in case the pointer
  // becomes invalid when we are re-posting the task.
  GetHwKeyDataInternal(decrypt_config->Clone(), hw_identifier,
                       std::move(callback));
}

void ContentDecryptionModuleAdapter::GetHwKeyDataInternal(
    std::unique_ptr<media::DecryptConfig> decrypt_config,
    const std::vector<uint8_t>& hw_identifier,
    GetHwKeyDataCB callback) {
  // This can get called from decoder threads or mojo threads, so we may need
  // to repost the task.
  if (!mojo_task_runner_->RunsTasksInCurrentSequence()) {
    mojo_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ContentDecryptionModuleAdapter::GetHwKeyDataInternal,
                       weak_factory_.GetWeakPtr(), std::move(decrypt_config),
                       hw_identifier, std::move(callback)));
    return;
  }
  if (!cros_cdm_remote_) {
    std::move(callback).Run(media::Decryptor::Status::kError,
                            std::vector<uint8_t>());
    return;
  }

  cros_cdm_remote_->GetHwKeyData(std::move(decrypt_config), hw_identifier,
                                 std::move(callback));
}

void ContentDecryptionModuleAdapter::GetHwConfigData(
    GetHwConfigDataCB callback) {
  ChromeOsCdmFactory::GetHwConfigData(std::move(callback));
}

void ContentDecryptionModuleAdapter::GetScreenResolutions(
    GetScreenResolutionsCB callback) {
  ChromeOsCdmFactory::GetScreenResolutions(std::move(callback));
}

std::unique_ptr<media::CdmContextRef>
ContentDecryptionModuleAdapter::GetCdmContextRef() {
  return std::make_unique<media::CdmContextRefImpl>(base::WrapRefCounted(this));
}

bool ContentDecryptionModuleAdapter::UsingArcCdm() const {
  return false;
}

bool ContentDecryptionModuleAdapter::IsRemoteCdm() const {
  return false;
}

void ContentDecryptionModuleAdapter::AllocateSecureBuffer(
    uint32_t size,
    AllocateSecureBufferCB callback) {
  ChromeOsCdmFactory::AllocateSecureBuffer(size, std::move(callback));
}

void ContentDecryptionModuleAdapter::ParseEncryptedSliceHeader(
    uint64_t secure_handle,
    uint32_t offset,
    const std::vector<uint8_t>& stream_data,
    ParseEncryptedSliceHeaderCB callback) {
  ChromeOsCdmFactory::ParseEncryptedSliceHeader(
      secure_handle, offset, stream_data, std::move(callback));
}

void ContentDecryptionModuleAdapter::OnSessionMessage(
    const std::string& session_id,
    media::CdmMessageType message_type,
    const std::vector<uint8_t>& message) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(2) << __func__;
  session_message_cb_.Run(session_id, message_type, message);
}

void ContentDecryptionModuleAdapter::OnSessionClosed(
    const std::string& session_id) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(2) << __func__;
  cdm_session_tracker_.RemoveSession(session_id);
  // TODO(crbug.com/40181810): Update cdm::mojom::ContentDecryptionModuleClient
  // to support CdmSessionClosedReason.
  session_closed_cb_.Run(session_id, media::CdmSessionClosedReason::kClose);
}

void ContentDecryptionModuleAdapter::OnSessionKeysChange(
    const std::string& session_id,
    bool has_additional_usable_key,
    std::vector<std::unique_ptr<media::CdmKeyInformation>> keys_info) {
  DVLOG(2) << __func__
           << " has_additional_usable_key: " << has_additional_usable_key;
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (has_additional_usable_key)
    event_callbacks_.Notify(Event::kHasAdditionalUsableKey);

  session_keys_change_cb_.Run(session_id, has_additional_usable_key,
                              std::move(keys_info));
}

void ContentDecryptionModuleAdapter::OnSessionExpirationUpdate(
    const std::string& session_id,
    double new_expiry_time_sec) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  session_expiration_update_cb_.Run(
      session_id, base::Time::FromSecondsSinceUnixEpoch(new_expiry_time_sec));
}

void ContentDecryptionModuleAdapter::Decrypt(
    StreamType stream_type,
    scoped_refptr<media::DecoderBuffer> encrypted,
    DecryptCB decrypt_cb) {
  // This can get called from decoder threads or mojo threads, so we may need
  // to repost the task.
  if (!mojo_task_runner_->RunsTasksInCurrentSequence()) {
    mojo_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ContentDecryptionModuleAdapter::Decrypt,
                                  weak_factory_.GetWeakPtr(), stream_type,
                                  encrypted, std::move(decrypt_cb)));
    return;
  }
  DVLOG(2) << __func__ << ": " << encrypted->AsHumanReadableString(true);
  if (!cros_cdm_remote_) {
    std::move(decrypt_cb).Run(media::Decryptor::kError, nullptr);
    return;
  }

  const media::DecryptConfig* decrypt_config = encrypted->decrypt_config();
  if (!encrypted->decrypt_config()) {
    // We still want to send this to the decryptor even if it is not encrypted
    // because we need that for tracking video on AMD of the clear headers. This
    // will not be inefficient in other cases because we won't be invoked for
    // clear content otherwise.
    DCHECK_EQ(stream_type, Decryptor::kVideo);
    cros_cdm_remote_->Decrypt(
        std::vector<uint8_t>(encrypted->data(),
                             encrypted->data() + encrypted->size()),
        nullptr, true,
        encrypted->has_side_data() ? encrypted->side_data()->secure_handle : 0,
        base::BindOnce(&ContentDecryptionModuleAdapter::OnDecrypt,
                       base::Unretained(this), stream_type, encrypted,
                       std::move(decrypt_cb)));
    return;
  }

  // Subsampling will be undone in the daemon itself, don't undo it here. We
  // need the clear samples as well on AMD.
  const std::vector<media::SubsampleEntry>& subsamples =
      decrypt_config->subsamples();
  if (!subsamples.empty() &&
      !VerifySubsamplesMatchSize(subsamples, encrypted->size())) {
    LOG(ERROR) << "Subsample sizes do not match input size";
    std::move(decrypt_cb).Run(kError, nullptr);
    return;
  }

  // TODO(jkardatzke): Evaluate the performance cost here of copying the data
  // and see if want to use something like MojoDecoderBufferWriter instead.
  cros_cdm_remote_->Decrypt(
      std::vector<uint8_t>(encrypted->data(),
                           encrypted->data() + encrypted->size()),
      decrypt_config->Clone(), stream_type == Decryptor::kVideo,
      encrypted->has_side_data() ? encrypted->side_data()->secure_handle : 0,
      base::BindOnce(&ContentDecryptionModuleAdapter::OnDecrypt,
                     base::Unretained(this), stream_type, encrypted,
                     std::move(decrypt_cb)));
}

void ContentDecryptionModuleAdapter::CancelDecrypt(StreamType stream_type) {
  // This method is racey since decryption is on another thread, so don't do
  // anything special for cancellation since the caller needs to handle the case
  // where the normal callback occurs even after calling CancelDecrypt anyways.
}

void ContentDecryptionModuleAdapter::InitializeAudioDecoder(
    const media::AudioDecoderConfig& config,
    DecoderInitCB init_cb) {
  // ContentDecryptionModuleAdapter does not support audio decoding.
  std::move(init_cb).Run(false);
}

void ContentDecryptionModuleAdapter::InitializeVideoDecoder(
    const media::VideoDecoderConfig& config,
    DecoderInitCB init_cb) {
  // ContentDecryptionModuleAdapter does not support video decoding.
  std::move(init_cb).Run(false);
}

void ContentDecryptionModuleAdapter::DecryptAndDecodeAudio(
    scoped_refptr<media::DecoderBuffer> encrypted,
    AudioDecodeCB audio_decode_cb) {
  NOTREACHED_IN_MIGRATION()
      << "ContentDecryptionModuleAdapter does not support audio decoding";
}

void ContentDecryptionModuleAdapter::DecryptAndDecodeVideo(
    scoped_refptr<media::DecoderBuffer> encrypted,
    VideoDecodeCB video_decode_cb) {
  NOTREACHED_IN_MIGRATION()
      << "ContentDecryptionModuleAdapter does not support video decoding";
}

void ContentDecryptionModuleAdapter::ResetDecoder(StreamType stream_type) {
  NOTREACHED_IN_MIGRATION()
      << "ContentDecryptionModuleAdapter does not support decoding";
}

void ContentDecryptionModuleAdapter::DeinitializeDecoder(
    StreamType stream_type) {
  // We do not support audio/video decoding, but since this can be called any
  // time after InitializeAudioDecoder/InitializeVideoDecoder, nothing to be
  // done here.
}

bool ContentDecryptionModuleAdapter::CanAlwaysDecrypt() {
  return false;
}

ContentDecryptionModuleAdapter::~ContentDecryptionModuleAdapter() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(2) << __func__;
  cdm_session_tracker_.CloseRemainingSessions(
      session_closed_cb_, media::CdmSessionClosedReason::kInternalError);
}

void ContentDecryptionModuleAdapter::OnConnectionError() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DVLOG(1) << __func__;
  // The mojo connection doesn't manage our lifecycle, that's handled by the
  // owner of the media::ContentDecryptionModule implementation we provide; so
  // only drop the bindings and notify the other end about all of the closed
  // sessions; don't destruct here.
  cros_client_receiver_.reset();
  cros_cdm_remote_.reset();

  // We've lost our communication, so reject all outstanding promises and close
  // any open sessions.
  cdm_promise_adapter_.Clear(
      media::CdmPromiseAdapter::ClearReason::kConnectionError);
  cdm_session_tracker_.CloseRemainingSessions(
      session_closed_cb_, media::CdmSessionClosedReason::kInternalError);
}

void ContentDecryptionModuleAdapter::RejectTrackedPromise(
    uint32_t promise_id,
    cdm::mojom::CdmPromiseResultPtr promise_result) {
  ReportSystemCodeUMA(promise_result->system_code);
  cdm_promise_adapter_.RejectPromise(promise_id, promise_result->exception,
                                     promise_result->system_code,
                                     promise_result->error_message);
}

void ContentDecryptionModuleAdapter::OnSimplePromiseResult(
    uint32_t promise_id,
    cdm::mojom::CdmPromiseResultPtr promise_result) {
  DVLOG(1) << __func__ << " received result: " << promise_result->success;
  if (!promise_result->success) {
    RejectTrackedPromise(promise_id, std::move(promise_result));
    return;
  }
  cdm_promise_adapter_.ResolvePromise(promise_id);
}

void ContentDecryptionModuleAdapter::OnGetStatusForPolicy(
    uint32_t promise_id,
    cdm::mojom::CdmPromiseResultPtr promise_result,
    media::CdmKeyInformation::KeyStatus key_status) {
  if (!promise_result->success) {
    RejectTrackedPromise(promise_id, std::move(promise_result));
    return;
  }
  cdm_promise_adapter_.ResolvePromise(promise_id, key_status);
}

void ContentDecryptionModuleAdapter::OnSessionPromiseResult(
    uint32_t promise_id,
    cdm::mojom::CdmPromiseResultPtr promise_result,
    const std::string& session_id) {
  DVLOG(1) << __func__ << " received result: " << promise_result->success
           << " session: " << session_id;
  if (!promise_result->success) {
    RejectTrackedPromise(promise_id, std::move(promise_result));
    return;
  }
  cdm_session_tracker_.AddSession(session_id);
  cdm_promise_adapter_.ResolvePromise(promise_id, session_id);
}

void ContentDecryptionModuleAdapter::OnDecrypt(
    StreamType stream_type,
    scoped_refptr<media::DecoderBuffer> encrypted,
    media::Decryptor::DecryptCB decrypt_cb,
    media::Decryptor::Status status,
    const std::vector<uint8_t>& decrypted_data,
    std::unique_ptr<media::DecryptConfig> decrypt_config_out) {
  if (status != media::Decryptor::kSuccess) {
    if (status == media::Decryptor::kNoKey) {
      DVLOG(1) << "Decryption failed due to no key";
    } else {
      LOG(ERROR) << "Failure decrypting data: " << status;
    }
    std::move(decrypt_cb).Run(status, nullptr);
    return;
  }

  // If we decrypted to secure memory, then just send the original buffer back
  // because the result is stored in the secure world.
  if (encrypted->has_side_data() && encrypted->side_data()->secure_handle) {
    std::move(decrypt_cb).Run(media::Decryptor::kSuccess, std::move(encrypted));
    return;
  }

  scoped_refptr<media::DecoderBuffer> decrypted =
      media::DecoderBuffer::CopyFrom(decrypted_data);
  // Copy the auxiliary fields.
  decrypted->set_timestamp(encrypted->timestamp());
  decrypted->set_duration(encrypted->duration());
  decrypted->set_is_key_frame(encrypted->is_key_frame());
  decrypted->set_side_data(encrypted->side_data());

  if (decrypt_config_out)
    decrypted->set_decrypt_config(std::move(decrypt_config_out));

  std::move(decrypt_cb).Run(media::Decryptor::kSuccess, std::move(decrypted));
}

}  // namespace chromeos
