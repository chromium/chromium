// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/components/cdm_factory_daemon/stable_cdm_context_impl.h"

#include "base/functional/callback.h"
#include "base/task/bind_post_task.h"
#include "chromeos/components/cdm_factory_daemon/chromeos_cdm_context.h"
#include "chromeos/components/cdm_factory_daemon/chromeos_cdm_factory.h"

namespace chromeos {

StableCdmContextImpl::StableCdmContextImpl(media::CdmContext* cdm_context)
    : cdm_context_(cdm_context) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(cdm_context_);
  DCHECK(cdm_context_->GetChromeOsCdmContext());
  cdm_context_ref_ = cdm_context_->GetChromeOsCdmContext()->GetCdmContextRef();
}

StableCdmContextImpl::~StableCdmContextImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void StableCdmContextImpl::GetHwKeyData(
    std::unique_ptr<media::DecryptConfig> decrypt_config,
    const std::vector<uint8_t>& hw_identifier,
    GetHwKeyDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  cdm_context_->GetChromeOsCdmContext()->GetHwKeyData(
      decrypt_config.get(), hw_identifier,
      base::BindPostTaskToCurrentDefault(std::move(callback)));
}

void StableCdmContextImpl::RegisterEventCallback(
    mojo::PendingRemote<media::stable::mojom::CdmContextEventCallback>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Note: we don't need to use base::BindPostTaskToCurrentDefault() for either
  // |callback| or the callback we pass to RegisterEventCB() because the
  // documentation for media::CdmContext::RegisterEventCB() says that "[t]he
  // registered callback will always be called on the thread where
  // RegisterEventCB() is called."
  remote_event_callbacks_.Add(std::move(callback));
  if (!callback_registration_) {
    callback_registration_ = cdm_context_->RegisterEventCB(
        base::BindRepeating(&StableCdmContextImpl::CdmEventCallback,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

void StableCdmContextImpl::GetHwConfigData(GetHwConfigDataCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ChromeOsCdmFactory::GetHwConfigData(
      base::BindPostTaskToCurrentDefault(std::move(callback)));
}

void StableCdmContextImpl::GetScreenResolutions(
    GetScreenResolutionsCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ChromeOsCdmFactory::GetScreenResolutions(
      base::BindPostTaskToCurrentDefault(std::move(callback)));
}

void StableCdmContextImpl::AllocateSecureBuffer(
    uint32_t size,
    AllocateSecureBufferCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ChromeOsCdmFactory::AllocateSecureBuffer(
      size, base::BindPostTaskToCurrentDefault(std::move(callback)));
}

void StableCdmContextImpl::ParseEncryptedSliceHeader(
    uint64_t secure_handle,
    uint32_t offset,
    const std::vector<uint8_t>& stream_data,
    ParseEncryptedSliceHeaderCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ChromeOsCdmFactory::ParseEncryptedSliceHeader(
      secure_handle, offset, stream_data,
      base::BindPostTaskToCurrentDefault(std::move(callback)));
}

void StableCdmContextImpl::DecryptVideoBuffer(
    const scoped_refptr<media::DecoderBuffer>& decoder_buffer,
    const std::vector<uint8_t>& bytes,
    DecryptVideoBufferCallback callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(cdm_context_->GetDecryptor());
  CHECK_EQ(decoder_buffer->size(), bytes.size());
  memcpy(decoder_buffer->writable_data(), bytes.data(), bytes.size());
  cdm_context_->GetDecryptor()->Decrypt(
      media::Decryptor::StreamType::kVideo, decoder_buffer,
      base::BindPostTaskToCurrentDefault(
          base::BindOnce(&StableCdmContextImpl::OnDecryptDone,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback))));
}

void StableCdmContextImpl::CdmEventCallback(media::CdmContext::Event event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (auto& cb : remote_event_callbacks_)
    cb->EventCallback(event);
}

void StableCdmContextImpl::OnDecryptDone(
    DecryptVideoBufferCallback decrypt_video_buffer_cb,
    media::Decryptor::Status status,
    scoped_refptr<media::DecoderBuffer> decoder_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<uint8_t> bytes;
  if (decoder_buffer) {
    bytes.insert(bytes.begin(), decoder_buffer->data(),
                 decoder_buffer->data() + decoder_buffer->size());
  }
  std::move(decrypt_video_buffer_cb).Run(status, decoder_buffer, bytes);
}

}  // namespace chromeos
