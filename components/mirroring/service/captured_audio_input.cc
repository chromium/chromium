// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mirroring/service/captured_audio_input.h"

#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "components/mirroring/mojom/session_observer.mojom.h"
#include "media/mojo/common/input_error_code_converter.h"
#include "media/mojo/mojom/audio_data_pipe.mojom.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace mirroring {

CapturedAudioInput::CapturedAudioInput(
    StreamCreatorCallback callback,
    mojo::Remote<mojom::SessionObserver>& observer)
    : stream_creator_callback_(std::move(callback)),
      logger_("CapturedAudioInput", observer) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
  DCHECK(!stream_creator_callback_.is_null());
}

CapturedAudioInput::~CapturedAudioInput() = default;

void CapturedAudioInput::CreateStream(media::AudioInputIPCDelegate* delegate,
                                      const media::AudioParameters& params,
                                      bool automatic_gain_control,
                                      uint32_t total_segments) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!automatic_gain_control);  // Invalid to be true for screen capture.
  DCHECK(delegate);
  DCHECK(!delegate_);
  logger_.LogInfo(base::StrCat(
      {"CreateStream; params = ", params.AsHumanReadableString(),
       " total_segments = ", base::NumberToString(total_segments)}));
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
  logger_.LogInfo("SetVolume to " + base::NumberToString(volume));
  stream_->SetVolume(volume);
}

void CapturedAudioInput::CloseStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  logger_.LogInfo("CloseStream");
  delegate_ = nullptr;
  stream_client_receiver_.reset();
  stream_.reset();
  stream_creator_client_receiver_.reset();
}

void CapturedAudioInput::SetOutputDeviceForAec(
    const std::string& output_device_id) {
  NOTREACHED_IN_MIGRATION();
}

void CapturedAudioInput::StreamCreated(
    mojo::PendingRemote<media::mojom::AudioInputStream> stream,
    mojo::PendingReceiver<media::mojom::AudioInputStreamClient> client_receiver,
    media::mojom::ReadOnlyAudioDataPipePtr data_pipe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_);
  DCHECK(!stream_);
  DCHECK(!stream_client_receiver_.is_bound());

  stream_.Bind(std::move(stream));
  stream_client_receiver_.Bind(std::move(client_receiver));

  DCHECK(data_pipe->socket.is_valid_platform_file());
  base::ScopedPlatformFile socket_handle = data_pipe->socket.TakePlatformFile();

  base::ReadOnlySharedMemoryRegion& shared_memory_region =
      data_pipe->shared_memory;
  DCHECK(shared_memory_region.IsValid());

  delegate_->OnStreamCreated(std::move(shared_memory_region),
                             std::move(socket_handle),
                             /* initally_muted */ false);
}

void CapturedAudioInput::OnError(media::mojom::InputStreamErrorCode code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_);
  logger_.LogError("InputStreamErrorCode " +
                   base::NumberToString(static_cast<int>(code)));
  delegate_->OnError(media::ConvertToCaptureCallbackCode(code));
}

void CapturedAudioInput::OnMutedStateChanged(bool is_muted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(delegate_);
  logger_.LogInfo("OnMuteStateChanged; is_muted = " +
                  base::NumberToString(is_muted));
  delegate_->OnMuted(is_muted);
}

}  // namespace mirroring
