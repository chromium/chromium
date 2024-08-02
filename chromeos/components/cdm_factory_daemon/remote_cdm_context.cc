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
    : public media::stable::mojom::CdmContextEventCallback {
 public:
  explicit MojoSequenceState(
      mojo::PendingRemote<media::stable::mojom::StableCdmContext>
          pending_stable_cdm_context)
      : pending_stable_cdm_context_(std::move(pending_stable_cdm_context)) {
    sequence_checker_.DetachFromSequence();
  }

  ~MojoSequenceState() override {
    CHECK(sequence_checker_.CalledOnValidSequence());
  }

  mojo::Remote<media::stable::mojom::StableCdmContext>& GetStableCdmContext() {
    CHECK(sequence_checker_.CalledOnValidSequence());
    if (!stable_cdm_context_) {
      stable_cdm_context_.Bind(std::move(pending_stable_cdm_context_));
      mojo_task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
    }
    return stable_cdm_context_;
  }

  std::unique_ptr<media::CallbackRegistration> RegisterEventCB(
      EventCB event_cb) {
    CHECK(sequence_checker_.CalledOnValidSequence());
    if (!event_callback_receiver_.is_bound()) {
      GetStableCdmContext()->RegisterEventCallback(
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
      DCHECK(task_runner || mojo_sequence_state->pending_stable_cdm_context_);
      DCHECK(task_runner || !mojo_sequence_state->stable_cdm_context_);
      DCHECK(task_runner ||
             !mojo_sequence_state->event_callback_receiver_.is_bound());
      delete mojo_sequence_state;
    }
  }

 private:
  // media::stable::mojom::CdmContextEventCallback:
  void EventCallback(media::CdmContext::Event event) override {
    CHECK(sequence_checker_.CalledOnValidSequence());
    event_callbacks_.Notify(std::move(event));
  }

  base::SequenceCheckerImpl sequence_checker_;
  mojo::PendingRemote<media::stable::mojom::StableCdmContext>
      pending_stable_cdm_context_;
  mojo::Remote<media::stable::mojom::StableCdmContext> stable_cdm_context_;
  mojo::Receiver<media::stable::mojom::CdmContextEventCallback>
      event_callback_receiver_{this};
  media::CallbackRegistry<EventCB::RunType> event_callbacks_;
  scoped_refptr<base::SequencedTaskRunner> mojo_task_runner_;
};

RemoteCdmContext::RemoteCdmContext(
    mojo::PendingRemote<media::stable::mojom::StableCdmContext>
        stable_cdm_context)
    : mojo_sequence_state_(new MojoSequenceState(std::move(stable_cdm_context)),
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
  mojo_sequence_state_->GetStableCdmContext()->GetHwKeyData(
      decrypt_config->Clone(), hw_identifier, std::move(callback));
}

void RemoteCdmContext::GetHwConfigData(GetHwConfigDataCB callback) {
  mojo_sequence_state_->GetStableCdmContext()->GetHwConfigData(
      std::move(callback));
}

void RemoteCdmContext::GetScreenResolutions(GetScreenResolutionsCB callback) {
  mojo_sequence_state_->GetStableCdmContext()->GetScreenResolutions(
      std::move(callback));
}

void RemoteCdmContext::AllocateSecureBuffer(uint32_t size,
                                            AllocateSecureBufferCB callback) {
  mojo_sequence_state_->GetStableCdmContext()->AllocateSecureBuffer(
      size, std::move(callback));
}

void RemoteCdmContext::ParseEncryptedSliceHeader(
    uint64_t secure_handle,
    uint32_t offset,
    const std::vector<uint8_t>& stream_data,
    ParseEncryptedSliceHeaderCB callback) {
  mojo_sequence_state_->GetStableCdmContext()->ParseEncryptedSliceHeader(
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
  mojo_sequence_state_->GetStableCdmContext()->DecryptVideoBuffer(
      encrypted,
      std::vector<uint8_t>(encrypted->data(),
                           encrypted->data() + encrypted->size()),
      base::BindOnce(&RemoteCdmContext::OnDecryptVideoBufferDone,
                     base::Unretained(this), std::move(decrypt_cb)));
}

void RemoteCdmContext::OnDecryptVideoBufferDone(
    DecryptCB decrypt_cb,
    media::Decryptor::Status status,
    const scoped_refptr<media::DecoderBuffer>& decoder_buffer,
    const std::vector<uint8_t>& bytes) {
  if (decoder_buffer) {
    CHECK_EQ(bytes.size(), decoder_buffer->size());
    memcpy(decoder_buffer->writable_data(), bytes.data(), bytes.size());
  }
  std::move(decrypt_cb).Run(status, decoder_buffer);
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
  NOTREACHED_IN_MIGRATION()
      << "RemoteCdmContext does not support audio decoding";
}

void RemoteCdmContext::DecryptAndDecodeVideo(
    scoped_refptr<media::DecoderBuffer> encrypted,
    VideoDecodeCB video_decode_cb) {
  NOTREACHED_IN_MIGRATION()
      << "RemoteCdmContext does not support video decoding";
}

void RemoteCdmContext::ResetDecoder(StreamType stream_type) {
  NOTREACHED_IN_MIGRATION() << "RemoteCdmContext does not support decoding";
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
