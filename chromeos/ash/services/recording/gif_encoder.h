// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_RECORDING_GIF_ENCODER_H_
#define CHROMEOS_ASH_SERVICES_RECORDING_GIF_ENCODER_H_

#include <vector>

#include "base/threading/sequence_bound.h"
#include "base/time/time.h"
#include "base/types/pass_key.h"
#include "chromeos/ash/services/recording/gif_encoding_types.h"
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

  // Creates and returns an object that specifies the capabilities of this
  // encoder.
  static std::unique_ptr<Capabilities> CreateCapabilities();

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
  EncodeAudioCallback GetEncodeAudioCallback() override;
  void FlushAndFinalize(base::OnceClosure on_done) override;

 private:
  // Writes the Logical Screen Descriptor block to the GIF file. This block
  // contains info about the dimensions of the canvas within which the images of
  // the frames will be rendered. It also contains the configuration of the
  // Global Color Table.
  // This block is written only once in the entire GIF file.
  void WriteLogicalScreenDescriptor(const gfx::Size& frame_size);

  // Writes the Netscape Application Extension block to the GIF file. This block
  // allows us to embed information about looping of the frames in the output
  // animated GIF.
  // This block is written only once in the entire GIF file.
  void WriteNetscapeExtension();

  // Writes the Graphic Control Extension block to the GIF file. This block
  // contains information about the delay after which the video frame should be
  // rendered. Hence, it uses the given `current_frame_time` to calculate this
  // delay.
  // This block repeats once per every received video frame.
  void WriteGraphicControlExtension(base::TimeTicks current_frame_time);

  // Writes the Image Descriptor block to the GIF file. It describes the bounds
  // of the frame within the total bounds of the canvas (which was defined
  // earlier in the Logical Screen Descriptor block). It also specifies whether
  // the frame has its own Local Color Table.
  // This block repeats once per every received video frame.
  void WriteImageDescriptor(const gfx::Size& frame_size,
                            uint8_t color_bit_depth);

  // Writes the `color_palette_` which has the given `color_bit_depth` to the
  // GIF file. This function can be uses to write both the Global and Local
  // color tables. However, we only write Local Color Tables in this
  // implementation.
  void WriteColorPalette(uint8_t color_bit_depth);

  // The presentation time of the most recent video frame prior to the one being
  // encoded at the moment.
  base::TimeTicks last_frame_time_;

  // The list of colors (up to 256 colors) that will be written to the GIF file
  // as the color table. These colors are extracted from each received frame
  // using some color quantization algorithm of choice, which tries to pick the
  // most important colors that represent the contents of the video frame.
  ColorTable color_palette_;

  // The color of each pixel in the received video frame is represented as an
  // index into `color_palette_` after color quantization is complete.
  ColorIndices pixel_color_indices_;

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
