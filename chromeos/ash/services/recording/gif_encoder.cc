// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/recording/gif_encoder.h"

#include "base/notreached.h"
#include "chromeos/ash/services/recording/lzw_pixel_color_indices_writer.h"
#include "chromeos/ash/services/recording/recording_encoder.h"
#include "media/base/audio_bus.h"
#include "media/base/video_frame.h"

namespace recording {

// static
base::SequenceBound<GifEncoder> GifEncoder::Create(
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    const media::VideoEncoder::Options& video_encoder_options,
    mojo::PendingRemote<mojom::DriveFsQuotaDelegate> drive_fs_quota_delegate,
    const base::FilePath& gif_file_path,
    OnFailureCallback on_failure_callback) {
  return base::SequenceBound<GifEncoder>(
      std::move(blocking_task_runner), PassKey(), video_encoder_options,
      std::move(drive_fs_quota_delegate), gif_file_path,
      std::move(on_failure_callback));
}

GifEncoder::GifEncoder(
    PassKey,
    const media::VideoEncoder::Options& video_encoder_options,
    mojo::PendingRemote<mojom::DriveFsQuotaDelegate> drive_fs_quota_delegate,
    const base::FilePath& gif_file_path,
    OnFailureCallback on_failure_callback)
    : RecordingEncoder(std::move(on_failure_callback)),
      gif_file_writer_(std::move(drive_fs_quota_delegate),
                       gif_file_path,
                       /*file_io_helper_delegate=*/this),
      lzw_encoder_(&gif_file_writer_) {
  InitializeVideoEncoder(video_encoder_options);
}

GifEncoder::~GifEncoder() = default;

void GifEncoder::InitializeVideoEncoder(
    const media::VideoEncoder::Options& video_encoder_options) {}

void GifEncoder::EncodeVideo(scoped_refptr<media::VideoFrame> frame) {}

void GifEncoder::EncodeAudio(std::unique_ptr<media::AudioBus> audio_bus,
                             base::TimeTicks capture_time) {
  NOTREACHED();
}

void GifEncoder::FlushAndFinalize(base::OnceClosure on_done) {
  std::move(on_done).Run();
}

}  // namespace recording
