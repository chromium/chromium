// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_RECORDING_GIF_ENCODER_H_
#define CHROMEOS_ASH_SERVICES_RECORDING_GIF_ENCODER_H_

#include "base/threading/sequence_bound.h"
#include "base/types/pass_key.h"
#include "chromeos/ash/services/recording/gif_file_writer.h"
#include "chromeos/ash/services/recording/lzw_pixel_color_indices_writer.h"
#include "chromeos/ash/services/recording/recording_encoder.h"

namespace recording {

// Encapsulates encoding video frames into an animated GIF and writes the
// encoded output to a file that it creates at the given `gif_file_path`. An
// instance of this object can only be interacted with via a
// `base::SequenceBound` wrapper, which guarantees that all encoding operations
// as well as the destruction of the instance are done on the sequenced
// `blocking_task_runner` given to `Create()`. This prevents expensive encoding
// operations from blocking the main thread of the recording service, on which
// the video frames are delivered.
class GifEncoder : public RecordingEncoder {
 private:
  using PassKey = base::PassKey<GifEncoder>;

 public:
  // Creates an instance of this class that is bound to the given sequenced
  // `blocking_task_runner` on which all operations as well as the destruction
  // of the instance will happen. `video_encoder_options` will be used to
  // initialize the encoder upon construction. The output of GIF encoding will
  // be written directly to a file created at the given `gif_file_path`. If
  // `drive_fs_quota_delegate` is provided, that means the file `gif_file_path`
  // lives on DriveFS, and the remaining DriveFS quota will be calculated
  // through this delegate.
  // `on_failure_callback` will be called to inform the owner of this object of
  // a failure, after which all subsequent calls to `EncodeVideo()` will be
  // ignored.
  //
  // By default, `on_failure_callback` will be called on the same sequence of
  // `blocking_task_runner` (unless the caller binds the given callback to a
  // different sequence by means of `base::BindPostTask()`).
  static base::SequenceBound<GifEncoder> Create(
      scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
      const media::VideoEncoder::Options& video_encoder_options,
      mojo::PendingRemote<mojom::DriveFsQuotaDelegate> drive_fs_quota_delegate,
      const base::FilePath& gif_file_path,
      OnFailureCallback on_failure_callback);

  GifEncoder(
      PassKey,
      const media::VideoEncoder::Options& video_encoder_options,
      mojo::PendingRemote<mojom::DriveFsQuotaDelegate> drive_fs_quota_delegate,
      const base::FilePath& gif_file_path,
      OnFailureCallback on_failure_callback);
  GifEncoder(const GifEncoder&) = delete;
  GifEncoder& operator=(const GifEncoder&) = delete;
  ~GifEncoder() override;

  // RecordingEncoder:
  void InitializeVideoEncoder(
      const media::VideoEncoder::Options& video_encoder_options) override;
  void EncodeVideo(scoped_refptr<media::VideoFrame> frame) override;
  void EncodeAudio(std::unique_ptr<media::AudioBus> audio_bus,
                   base::TimeTicks capture_time) override;
  void FlushAndFinalize(base::OnceClosure on_done) override;

 private:
  // Abstracts writing bytes to the GIF file, and takes care of handling IO
  // errors and remaining disk space / DriveFS quota issues.
  GifFileWriter gif_file_writer_;

  // Abstracts encoding the video frame's image color indices using the
  // Variable-Length-Code LZW compression algorithm and writing the output
  // stream to the GIF file.
  LzwPixelColorIndicesWriter lzw_encoder_;
};

}  // namespace recording

#endif  // CHROMEOS_ASH_SERVICES_RECORDING_GIF_ENCODER_H_
