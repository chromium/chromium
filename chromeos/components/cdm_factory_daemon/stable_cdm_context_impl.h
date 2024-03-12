// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_STABLE_CDM_CONTEXT_IMPL_H_
#define CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_STABLE_CDM_CONTEXT_IMPL_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "media/base/callback_registry.h"
#include "media/base/cdm_context.h"
#include "media/mojo/mojom/stable/stable_video_decoder.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace chromeos {

// Provides the receiving implementation for the CdmContext Mojo interface
// used with out of process video decoding. This will run in the GPU process and
// is used by the OOPVideoDecoder. The remote end of it will run in the video
// decoder utility process launched from ash-chrome.
class COMPONENT_EXPORT(CDM_FACTORY_DAEMON) StableCdmContextImpl
    : public media::stable::mojom::StableCdmContext {
 public:
  explicit StableCdmContextImpl(media::CdmContext* cdm_context);

  StableCdmContextImpl(const StableCdmContextImpl&) = delete;
  StableCdmContextImpl& operator=(const StableCdmContextImpl&) = delete;

  ~StableCdmContextImpl() override;

  const media::CdmContext* cdm_context() const { return cdm_context_; }

  // media::stable::mojom::StableCdmContext:
  void GetHwKeyData(std::unique_ptr<media::DecryptConfig> decrypt_config,
                    const std::vector<uint8_t>& hw_identifier,
                    GetHwKeyDataCallback callback) override;
  void RegisterEventCallback(
      mojo::PendingRemote<media::stable::mojom::CdmContextEventCallback>
          callback) override;
  void GetHwConfigData(GetHwConfigDataCallback callback) override;
  void GetScreenResolutions(GetScreenResolutionsCallback callback) override;
  void AllocateSecureBuffer(uint32_t size,
                            AllocateSecureBufferCallback callback) override;
  void ParseEncryptedSliceHeader(
      uint64_t secure_handle,
      uint32_t offset,
      const std::vector<uint8_t>& stream_data,
      ParseEncryptedSliceHeaderCallback callback) override;
  void DecryptVideoBuffer(
      const scoped_refptr<media::DecoderBuffer>& decoder_buffer,
      const std::vector<uint8_t>& bytes,
      DecryptVideoBufferCallback callback) override;

 private:
  // Receives callbacks from the |cdm_context_| after we register with it.
  void CdmEventCallback(media::CdmContext::Event event);

  void OnDecryptDone(DecryptVideoBufferCallback decrypt_video_buffer_cb,
                     media::Decryptor::Status status,
                     scoped_refptr<media::DecoderBuffer> decoder_buffer);

  SEQUENCE_CHECKER(sequence_checker_);

  const raw_ptr<media::CdmContext> cdm_context_;
  std::unique_ptr<media::CdmContextRef> cdm_context_ref_;
  std::unique_ptr<media::CallbackRegistration> callback_registration_;
  mojo::RemoteSet<media::stable::mojom::CdmContextEventCallback>
      remote_event_callbacks_;

  // WeakPtrFactory to use for callbacks.
  base::WeakPtrFactory<StableCdmContextImpl> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_STABLE_CDM_CONTEXT_IMPL_H_
