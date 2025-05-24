// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_CDM_CONTEXT_FOR_OOPVD_IMPL_H_
#define CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_CDM_CONTEXT_FOR_OOPVD_IMPL_H_

#include <memory>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "media/base/callback_registry.h"
#include "media/base/cdm_context.h"
#include "media/mojo/mojom/cdm_context_for_oopvd.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace chromeos {

// Provides the receiving implementation for the CdmContext Mojo interface
// used with out of process video decoding. This will run in the GPU process and
// is used by the OOPVideoDecoder. The remote end of it will run in the video
// decoder utility process launched from ash-chrome.
class COMPONENT_EXPORT(CDM_FACTORY_DAEMON) CdmContextForOOPVDImpl
    : public media::mojom::CdmContextForOOPVD {
 public:
  explicit CdmContextForOOPVDImpl(media::CdmContext* cdm_context);

  CdmContextForOOPVDImpl(const CdmContextForOOPVDImpl&) = delete;
  CdmContextForOOPVDImpl& operator=(const CdmContextForOOPVDImpl&) = delete;

  ~CdmContextForOOPVDImpl() override;

  const media::CdmContext* cdm_context() const { return cdm_context_; }

  // media::mojom::CdmContextForOOPVD
  void GetHwKeyData(media::mojom::DecryptConfigPtr decrypt_config,
                    const std::vector<uint8_t>& hw_identifier,
                    GetHwKeyDataCallback callback) override;
  void RegisterEventCallback(
      mojo::PendingRemote<media::mojom::CdmContextEventCallback> callback)
      override;
  void GetHwConfigData(GetHwConfigDataCallback callback) override;
  void GetScreenResolutions(GetScreenResolutionsCallback callback) override;
  void AllocateSecureBuffer(uint32_t size,
                            AllocateSecureBufferCallback callback) override;
  void ParseEncryptedSliceHeader(
      uint64_t secure_handle,
      uint32_t offset,
      const std::vector<uint8_t>& stream_data,
      ParseEncryptedSliceHeaderCallback callback) override;
  void DecryptVideoBuffer(media::mojom::DecoderBufferPtr decoder_buffer,
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
  mojo::RemoteSet<media::mojom::CdmContextEventCallback>
      remote_event_callbacks_;

  // WeakPtrFactory to use for callbacks.
  base::WeakPtrFactory<CdmContextForOOPVDImpl> weak_ptr_factory_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_CDM_CONTEXT_FOR_OOPVD_IMPL_H_
