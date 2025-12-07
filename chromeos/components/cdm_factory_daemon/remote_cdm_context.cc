// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chromeos/components/cdm_factory_daemon/remote_cdm_context.h"

#include "base/functional/callback.h"
#include "base/sequence_checker_impl.h"
#include "base/task/sequenced_task_runner.h"
#include "media/base/callback_registry.h"
#include "media/cdm/cdm_context_ref_impl.h"
#include "media/mojo/common/media_type_converters.h"
#include "media/mojo/common/validation_utils.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace chromeos {

namespace {
class RemoteCdmContextRef final : public media::CdmContextRef {
 public:
  explicit RemoteCdmContextRef(scoped_refptr<RemoteCdmContext> cdm_context)
      : cdm_context_(std::move(cdm_context)) {}

  RemoteCdmContextRef(const RemoteCdmContextRef&) = delete;
  RemoteCdmContextRef& operator=(const RemoteCdmContextRef&) = delete;

  ~RemoteCdmContextRef() final = default;

  // CdmContextRef:
  media::CdmContext* GetCdmContext() final { return cdm_context_.get(); }

 private:
  scoped_refptr<RemoteCdmContext> cdm_context_;
};
}  // namespace

class RemoteCdmContext::MojoSequenceState
    : public media::mojom::CdmContextEventCallback {
 public:
  explicit MojoSequenceState(
      mojo::PendingRemote<media::mojom::CdmContextForOOPVD>
          pending_cdm_context_for_oopvd)
      : pending_cdm_context_for_oopvd_(
            std::move(pending_cdm_context_for_oopvd)) {
    sequence_checker_.DetachFromSequence();
  }

  ~MojoSequenceState() override {
    CHECK(sequence_checker_.CalledOnValidSequence());
  }

  mojo::Remote<media::mojom::CdmContextForOOPVD>& GetCdmContextForOOPVD() {
    CHECK(sequence_checker_.CalledOnValidSequence());
    if (!cdm_context_for_oopvd_) {
      cdm_context_for_oopvd_.Bind(std::move(pending_cdm_context_for_oopvd_));
      mojo_task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
    }
    return cdm_context_for_oopvd_;
  }

  std::unique_ptr<media::CallbackRegistration> RegisterEventCB(
      EventCB event_cb) {
    CHECK(sequence_checker_.CalledOnValidSequence());
    if (!event_callback_receiver_.is_bound()) {
      GetCdmContextForOOPVD()->RegisterEventCallback(
          event_callback_receiver_.BindNewPipeAndPassRemote());
    }
    auto registration = event_callbacks_.Register(std::move(event_cb));
    return registration;
  }

  static void DeleteOnCorrectSequence(MojoSequenceState* mojo_sequence_state) {
    scoped_refptr<base::SequencedTaskRunner> task_runner =
        mojo_sequence_state->mojo_task_runner_;
    if (task_runner && !task_runner->RunsTasksInCurrentSequence()) {
      // When DeleteSoon() returns false, |mojo_sequence_state| will be leaked,
      // which is okay.
      task_runner->DeleteSoon(FROM_HERE, mojo_sequence_state);
    } else {
      // We're either on the right sequence or the |mojo_sequence_state| was
      // never bound to a sequence (i.e., it was constructed but never used).
      DCHECK(task_runner ||
             mojo_sequence_state->pending_cdm_context_for_oopvd_);
      DCHECK(task_runner || !mojo_sequence_state->cdm_context_for_oopvd_);
      DCHECK(task_runner ||
             !mojo_sequence_state->event_callback_receiver_.is_bound());
      delete mojo_sequence_state;
    }
  }

 private:
  // media::mojom::CdmContextEventCallback:
  void EventCallback(media::CdmContext::Event event) override {
    CHECK(sequence_checker_.CalledOnValidSequence());
    event_callbacks_.Notify(std::move(event));
  }

  base::SequenceCheckerImpl sequence_checker_;
  mojo::PendingRemote<media::mojom::CdmContextForOOPVD>
      pending_cdm_context_for_oopvd_;
  mojo::Remote<media::mojom::CdmContextForOOPVD> cdm_context_for_oopvd_;
  mojo::Receiver<media::mojom::CdmContextEventCallback>
      event_callback_receiver_{this};
  media::CallbackRegistry<EventCB::RunType> event_callbacks_;
  scoped_refptr<base::SequencedTaskRunner> mojo_task_runner_;
};

RemoteCdmContext::RemoteCdmContext(
    mojo::PendingRemote<media::mojom::CdmContextForOOPVD> cdm_context_for_oopvd)
    : mojo_sequence_state_(
          new MojoSequenceState(std::move(cdm_context_for_oopvd)),
          &MojoSequenceState::DeleteOnCorrectSequence) {}

std::unique_ptr<media::CallbackRegistration> RemoteCdmContext::RegisterEventCB(
    EventCB event_cb) {
  return mojo_sequence_state_->RegisterEventCB(std::move(event_cb));
}

RemoteCdmContext::~RemoteCdmContext() {}

media::Decryptor* RemoteCdmContext::GetDecryptor() {
  return this;
}

ChromeOsCdmContext* RemoteCdmContext::GetChromeOsCdmContext() {
  return this;
}

void RemoteCdmContext::GetHwKeyData(const media::DecryptConfig* decrypt_config,
                                    const std::vector<uint8_t>& hw_identifier,
                                    GetHwKeyDataCB callback) {
  CHECK(decrypt_config);
  media::mojom::DecryptConfigPtr mojo_decrypt_config =
      media::mojom::DecryptConfig::From(*decrypt_config);
  CHECK(mojo_decrypt_config);
  mojo_sequence_state_->GetCdmContextForOOPVD()->GetHwKeyData(
      std::move(mojo_decrypt_config), hw_identifier, std::move(callback));
}

void RemoteCdmContext::GetHwConfigData(GetHwConfigDataCB callback) {
  mojo_sequence_state_->GetCdmContextForOOPVD()->GetHwConfigData(
      std::move(callback));
}

void RemoteCdmContext::GetScreenResolutions(GetScreenResolutionsCB callback) {
  mojo_sequence_state_->GetCdmContextForOOPVD()->GetScreenResolutions(
      std::move(callback));
}

void RemoteCdmContext::AllocateSecureBuffer(uint32_t size,
                                            AllocateSecureBufferCB callback) {
  mojo_sequence_state_->GetCdmContextForOOPVD()->AllocateSecureBuffer(
      size, std::move(callback));
}

void RemoteCdmContext::ParseEncryptedSliceHeader(
    uint64_t secure_handle,
    uint32_t offset,
    const std::vector<uint8_t>& stream_data,
    ParseEncryptedSliceHeaderCB callback) {
  mojo_sequence_state_->GetCdmContextForOOPVD()->ParseEncryptedSliceHeader(
      secure_handle, offset, stream_data, std::move(callback));
}

std::unique_ptr<media::CdmContextRef> RemoteCdmContext::GetCdmContextRef() {
  return std::make_unique<RemoteCdmContextRef>(base::WrapRefCounted(this));
}

bool RemoteCdmContext::UsingArcCdm() const {
  return false;
}

bool RemoteCdmContext::IsRemoteCdm() const {
  return true;
}

void RemoteCdmContext::Decrypt(StreamType stream_type,
                               scoped_refptr<media::DecoderBuffer> encrypted,
                               DecryptCB decrypt_cb) {
  DCHECK_EQ(stream_type, Decryptor::kVideo);
  CHECK(encrypted);
  media::mojom::DecoderBufferPtr encrypted_mojo_buffer =
      media::mojom::DecoderBuffer::From(*encrypted);
  CHECK(encrypted_mojo_buffer);
  mojo_sequence_state_->GetCdmContextForOOPVD()->DecryptVideoBuffer(
      std::move(encrypted_mojo_buffer),
      std::vector<uint8_t>(encrypted->begin(), encrypted->end()),
      base::BindOnce(&RemoteCdmContext::OnDecryptVideoBufferDone,
                     base::Unretained(this), std::move(decrypt_cb)));
}

void RemoteCdmContext::OnDecryptVideoBufferDone(
    DecryptCB decrypt_cb,
    media::Decryptor::Status status,
    media::mojom::DecoderBufferPtr decoder_buffer,
    const std::vector<uint8_t>& bytes) {
  scoped_refptr<media::DecoderBuffer> media_decoder_buffer;
  if (decoder_buffer) {
    media_decoder_buffer =
        media::ValidateAndConvertMojoDecoderBuffer(std::move(decoder_buffer));
    if (!media_decoder_buffer) {
      CHECK(mojo::IsInMessageDispatch());
      mojo::ReportBadMessage("Invalid DecoderBuffer received");
      return;
    }
    CHECK_EQ(bytes.size(), media_decoder_buffer->size());
    memcpy(media_decoder_buffer->writable_data(), bytes.data(), bytes.size());
  }
  std::move(decrypt_cb).Run(status, media_decoder_buffer);
}

void RemoteCdmContext::CancelDecrypt(StreamType stream_type) {
  // This method is racey since decryption is on another thread, so don't do
  // anything special for cancellation since the caller needs to handle the case
  // where the normal callback occurs even after calling CancelDecrypt anyways.
}

void RemoteCdmContext::InitializeAudioDecoder(
    const media::AudioDecoderConfig& config,
    DecoderInitCB init_cb) {
  // RemoteCdmContext does not support audio decoding.
  std::move(init_cb).Run(false);
}

void RemoteCdmContext::InitializeVideoDecoder(
    const media::VideoDecoderConfig& config,
    DecoderInitCB init_cb) {
  // RemoteCdmContext does not support video decoding.
  std::move(init_cb).Run(false);
}

void RemoteCdmContext::DecryptAndDecodeAudio(
    scoped_refptr<media::DecoderBuffer> encrypted,
    AudioDecodeCB audio_decode_cb) {
  NOTREACHED() << "RemoteCdmContext does not support audio decoding";
}

void RemoteCdmContext::DecryptAndDecodeVideo(
    scoped_refptr<media::DecoderBuffer> encrypted,
    VideoDecodeCB video_decode_cb) {
  NOTREACHED() << "RemoteCdmContext does not support video decoding";
}

void RemoteCdmContext::ResetDecoder(StreamType stream_type) {
  NOTREACHED() << "RemoteCdmContext does not support decoding";
}

void RemoteCdmContext::DeinitializeDecoder(StreamType stream_type) {
  // We do not support audio/video decoding, but since this can be called any
  // time after InitializeAudioDecoder/InitializeVideoDecoder, nothing to be
  // done here.
}

bool RemoteCdmContext::CanAlwaysDecrypt() {
  return false;
}

}  // namespace chromeos
