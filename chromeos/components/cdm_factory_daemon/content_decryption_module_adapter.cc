// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/cdm_factory_daemon/content_decryption_module_adapter.h"

#include <utility>

#include "base/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "media/base/cdm_promise.h"
#include "media/base/decoder_buffer.h"
#include "media/base/eme_constants.h"
#include "media/base/subsample_entry.h"
#include "media/cdm/cdm_context_ref_impl.h"

namespace {

// Copy the cypher bytes as specified by |subsamples| from |src| to |dst|.
// Clear bytes contained in |src| that are specified by |subsamples| will be
// skipped. This is used when copying all the protected data out of a sample.
//
// NOTE: Before invoking this call the |subsamples| data must have been verified
// against the length of the |src| array to ensure we won't go out of bounds or
// have overflow. This can be done with media::VerifySubsamplesMatchSize.
void ExtractSubsampleCypherBytes(
    const std::vector<media::SubsampleEntry>& subsamples,
    const uint8_t* src,
    uint8_t* dst) {
  for (const auto& subsample : subsamples) {
    src += subsample.clear_bytes;
    memcpy(dst, src, subsample.cypher_bytes);
    src += subsample.cypher_bytes;
    dst += subsample.cypher_bytes;
  }
}

// Copy the cypher bytes as specified by |subsamples| from |src| to |dst|.
// Any clear bytes mentioned in |subsamples| will be skipped in |dst|. This is
// used when copying the decrypted bytes back into the buffer, replacing the
// encrypted portions.
//
// NOTE: Before invoking this call the |subsamples| data must have been verified
// against the length of the |src| array to ensure we won't go out of bounds or
// have overflow. This can be done with media::VerifySubsamplesMatchSize.
void InsertSubsampleCypherBytes(
    const std::vector<media::SubsampleEntry>& subsamples,
    const uint8_t* src,
    uint8_t* dst) {
  for (const auto& subsample : subsamples) {
    dst += subsample.clear_bytes;
    memcpy(dst, src, subsample.cypher_bytes);
    src += subsample.cypher_bytes;
    dst += subsample.cypher_bytes;
  }
}

// Copy the decrypted data into the output buffer. The buffer will contain
// all of the data if there was no subsampling or if we were doing CBCS with
// multiple subsamples. Otherwise we need to copy based on the subsampling.
scoped_refptr<media::DecoderBuffer> CopyDecryptedDataToDecoderBuffer(
    scoped_refptr<media::DecoderBuffer> encrypted,
    const std::vector<uint8_t>& decrypted_data) {
  scoped_refptr<media::DecoderBuffer> decrypted;
  if (encrypted->decrypt_config()->subsamples().empty() ||
      (encrypted->decrypt_config()->encryption_scheme() ==
           media::EncryptionScheme::kCbcs &&
       encrypted->decrypt_config()->subsamples().size() > 1)) {
    decrypted = media::DecoderBuffer::CopyFrom(decrypted_data.data(),
                                               decrypted_data.size());
  } else {
    decrypted = media::DecoderBuffer::CopyFrom(encrypted->data(),
                                               encrypted->data_size());
    InsertSubsampleCypherBytes(encrypted->decrypt_config()->subsamples(),
                               decrypted_data.data(),
                               decrypted->writable_data());
  }

  // Copy the auxiliary fields.
  decrypted->set_timestamp(encrypted->timestamp());
  decrypted->set_duration(encrypted->duration());
  decrypted->set_is_key_frame(encrypted->is_key_frame());
  decrypted->CopySideDataFrom(encrypted->side_data(),
                              encrypted->side_data_size());
  return decrypted;
}

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
      mojo_task_runner_(base::SequencedTaskRunnerHandle::Get()) {
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
  // This can get called from decoder threads or mojo threads, so we may need
  // to repost the task.
  if (!mojo_task_runner_->RunsTasksInCurrentSequence()) {
    mojo_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&ContentDecryptionModuleAdapter::GetHwKeyData,
                                  weak_factory_.GetWeakPtr(), decrypt_config,
                                  hw_identifier, std::move(callback)));
    return;
  }
  if (!cros_cdm_remote_) {
    std::move(callback).Run(media::Decryptor::Status::kError,
                            std::vector<uint8_t>());
    return;
  }
  auto cros_decrypt_config = cdm::mojom::DecryptConfig::New();
  cros_decrypt_config->key_id = decrypt_config->key_id();
  cros_decrypt_config->iv = decrypt_config->iv();
  cros_decrypt_config->encryption_scheme = decrypt_config->encryption_scheme();

  cros_cdm_remote_->GetHwKeyData(std::move(cros_decrypt_config), hw_identifier,
                                 std::move(callback));
}

std::unique_ptr<media::CdmContextRef>
ContentDecryptionModuleAdapter::GetCdmContextRef() {
  return std::make_unique<media::CdmContextRefImpl>(base::WrapRefCounted(this));
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
  session_closed_cb_.Run(session_id);
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
      session_id, base::Time::FromDoubleT(new_expiry_time_sec));
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
  if (!decrypt_config) {
    // If there is no DecryptConfig, then the data is unencrypted so return it
    // immediately.
    std::move(decrypt_cb).Run(kSuccess, encrypted);
    return;
  }

  cdm::mojom::DecryptConfigPtr cros_decrypt_config(
      cdm::mojom::DecryptConfig::New());
  cros_decrypt_config->key_id = decrypt_config->key_id();
  cros_decrypt_config->iv = decrypt_config->iv();
  if (decrypt_config->HasPattern()) {
    cros_decrypt_config->encryption_pattern =
        decrypt_config->encryption_pattern().value();
  }
  cros_decrypt_config->encryption_scheme = decrypt_config->encryption_scheme();

  const std::vector<media::SubsampleEntry>& subsamples =
      decrypt_config->subsamples();
  if (subsamples.empty()) {
    StoreDecryptCallback(stream_type, std::move(decrypt_cb));
    // No subsamples specified, request decryption of entire block.
    // TODO(jkardatzke): Evaluate the performance cost here of copying the data
    // and see if want to use something like MojoDecoderBufferWriter instead.
    cros_cdm_remote_->Decrypt(
        std::vector<uint8_t>(encrypted->data(),
                             encrypted->data() + encrypted->data_size()),
        std::move(cros_decrypt_config),
        base::BindOnce(&ContentDecryptionModuleAdapter::OnDecrypt,
                       base::Unretained(this), stream_type, encrypted,
                       encrypted->data_size()));
    return;
  }

  if (!VerifySubsamplesMatchSize(subsamples, encrypted->data_size())) {
    LOG(ERROR) << "Subsample sizes do not match input size";
    std::move(decrypt_cb).Run(kError, nullptr);
    return;
  }

  // Compute the size of the encrypted portion. Overflow, etc. checked by
  // the call to VerifySubsamplesMatchSize().
  size_t total_encrypted_size = 0;
  for (const auto& subsample : subsamples)
    total_encrypted_size += subsample.cypher_bytes;

  // No need to decrypt if there is no encrypted data.
  if (total_encrypted_size == 0) {
    encrypted->set_decrypt_config(nullptr);
    std::move(decrypt_cb).Run(kSuccess, encrypted);
    return;
  }

  StoreDecryptCallback(stream_type, std::move(decrypt_cb));

  // For CENC, the encrypted portions of all subsamples must form a contiguous
  // block, such that an encrypted subsample that ends away from a block
  // boundary is immediately followed by the start of the next encrypted
  // subsample. We copy all encrypted subsamples to a contiguous buffer, decrypt
  // them, then copy the decrypted bytes over the encrypted bytes in the output.
  // For CBCS, if there is more than one sample, then we need to pass the
  // subsample information or otherwise we would need to call decrypt for each
  // individual subsample since each subsample uses the same IV and it can't be
  // decrypted as one large block like CENC.
  if (decrypt_config->encryption_scheme() == media::EncryptionScheme::kCenc ||
      subsamples.size() == 1) {
    std::vector<uint8_t> encrypted_bytes(total_encrypted_size);
    ExtractSubsampleCypherBytes(subsamples, encrypted->data(),
                                encrypted_bytes.data());
    cros_cdm_remote_->Decrypt(
        std::move(encrypted_bytes), std::move(cros_decrypt_config),
        base::BindOnce(&ContentDecryptionModuleAdapter::OnDecrypt,
                       base::Unretained(this), stream_type, encrypted,
                       total_encrypted_size));
    return;
  }

  // We need to specify the subsampling and put that in the decrypt config.
  for (const auto& sample : subsamples) {
    cros_decrypt_config->subsamples.push_back(cdm::mojom::SubsampleEntry::New(
        sample.clear_bytes, sample.cypher_bytes));
  }
  // TODO(jkardatzke): Evaluate the performance cost here of copying the data
  // and see if want to use something like MojoDecoderBufferWriter instead.
  cros_cdm_remote_->Decrypt(
      std::vector<uint8_t>(encrypted->data(),
                           encrypted->data() + encrypted->data_size()),
      std::move(cros_decrypt_config),
      base::BindOnce(&ContentDecryptionModuleAdapter::OnDecrypt,
                     base::Unretained(this), stream_type, encrypted,
                     encrypted->data_size()));
}

void ContentDecryptionModuleAdapter::CancelDecrypt(StreamType stream_type) {
  // This can get called from decoder threads or mojo threads, so we may need
  // to repost the task.
  if (!mojo_task_runner_->RunsTasksInCurrentSequence()) {
    mojo_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&ContentDecryptionModuleAdapter::CancelDecrypt,
                       weak_factory_.GetWeakPtr(), stream_type));
    return;
  }
  media::Decryptor::DecryptCB callback =
      std::move(stream_type == kVideo ? pending_video_decrypt_cb_
                                      : pending_audio_decrypt_cb_);
  if (callback)
    std::move(callback).Run(media::Decryptor::kSuccess, nullptr);
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
    const AudioDecodeCB& audio_decode_cb) {
  NOTREACHED()
      << "ContentDecryptionModuleAdapter does not support audio decoding";
}

void ContentDecryptionModuleAdapter::DecryptAndDecodeVideo(
    scoped_refptr<media::DecoderBuffer> encrypted,
    const VideoDecodeCB& video_decode_cb) {
  NOTREACHED()
      << "ContentDecryptionModuleAdapter does not support video decoding";
}

void ContentDecryptionModuleAdapter::ResetDecoder(StreamType stream_type) {
  NOTREACHED() << "ContentDecryptionModuleAdapter does not support decoding";
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
  cdm_session_tracker_.CloseRemainingSessions(session_closed_cb_);
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
  cdm_promise_adapter_.Clear();
  cdm_session_tracker_.CloseRemainingSessions(session_closed_cb_);
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

void ContentDecryptionModuleAdapter::StoreDecryptCallback(
    StreamType stream_type,
    DecryptCB decrypt_cb) {
  if (stream_type == kVideo) {
    DCHECK(!pending_video_decrypt_cb_);
    pending_video_decrypt_cb_ = std::move(decrypt_cb);
  } else {
    DCHECK(!pending_audio_decrypt_cb_);
    pending_audio_decrypt_cb_ = std::move(decrypt_cb);
  }
}

void ContentDecryptionModuleAdapter::OnDecrypt(
    StreamType stream_type,
    scoped_refptr<media::DecoderBuffer> encrypted,
    size_t expected_decrypt_size,
    media::Decryptor::Status status,
    const std::vector<uint8_t>& decrypted_data) {
  media::Decryptor::DecryptCB callback =
      std::move(stream_type == kVideo ? pending_video_decrypt_cb_
                                      : pending_audio_decrypt_cb_);
  if (!callback) {
    // This happens if CancelDecrypt was called.
    DVLOG(1) << __func__ << " decrypt callback empty";
    return;
  }
  if (status != media::Decryptor::kSuccess) {
    if (status == media::Decryptor::kNoKey) {
      DVLOG(1) << "Decryption failed due to no key";
    } else {
      LOG(ERROR) << "Failure decrypting data: " << status;
    }
    std::move(callback).Run(status, nullptr);
    return;
  }

  if (decrypted_data.size() != expected_decrypt_size) {
    LOG(ERROR) << "Decrypted data size mismatch got: " << decrypted_data.size()
               << " expected: " << expected_decrypt_size;
    std::move(callback).Run(media::Decryptor::kError, nullptr);
    return;
  }

  std::move(callback).Run(
      media::Decryptor::kSuccess,
      CopyDecryptedDataToDecoderBuffer(std::move(encrypted), decrypted_data));
}

}  // namespace chromeos
