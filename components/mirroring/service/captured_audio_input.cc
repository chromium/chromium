// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/captured_audio_input.h"

#include "base/logging.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace mirroring {

CapturedAudioInput::CapturedAudioInput(StreamCreatorCallback callback)
    : stream_creator_callback_(std::move(callback)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(!stream_creator_callback_.is_null());
}

CapturedAudioInput::~CapturedAudioInput() {}

void CapturedAudioInput::CreateStream(media::AudioInputIPCDelegate* delegate,
                                      const media::AudioParameters& params,
                                      bool automatic_gain_control,
                                      uint32_t total_segments) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!automatic_gain_control);  // Invalid to be true for screen capture.
  DCHECK(delegate);
  DCHECK(!delegate_);
  delegate_ = delegate;
  stream_creator_callback_.Run(
      stream_creator_client_receiver_.BindNewPipeAndPassRemote(), params,
      total_segments);
}

void CapturedAudioInput::RecordStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(stream_.is_bound());
  stream_->Record();
}

void CapturedAudioInput::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(stream_.is_bound());
  stream_->SetVolume(volume);
}

void CapturedAudioInput::CloseStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_ = nullptr;
  stream_client_receiver_.reset();
  stream_.reset();
}

void CapturedAudioInput::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  NOTREACHED();
}

void CapturedAudioInput::StreamCreated(
    mojo::PendingRemote<media::mojom::AudioInputStream> stream,
    mojo::PendingReceiver<media::mojom::AudioInputStreamClient> client_receiver,
    media::mojom::ReadOnlyAudioDataPipePtr data_pipe,
    bool initially_muted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_);
  DCHECK(!stream_);
  DCHECK(!stream_client_receiver_.is_bound());

  stream_.Bind(std::move(stream));
  stream_client_receiver_.Bind(std::move(client_receiver));

  base::PlatformFile socket_handle;
  auto result =
      mojo::UnwrapPlatformFile(std::move(data_pipe->socket), &socket_handle);
  DCHECK_EQ(result, MOJO_RESULT_OK);

  base::ReadOnlySharedMemoryRegion& shared_memory_region =
      data_pipe->shared_memory;
  DCHECK(shared_memory_region.IsValid());

  delegate_->OnStreamCreated(std::move(shared_memory_region), socket_handle,
                             initially_muted);
}

void CapturedAudioInput::OnError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_);
  delegate_->OnError();
}

void CapturedAudioInput::OnMutedStateChanged(bool is_muted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_);
  delegate_->OnMuted(is_muted);
}

}  // namespace mirroring
