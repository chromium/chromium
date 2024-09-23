// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/recording/gif_encoder.h"

#include <cmath>
#include <cstdint>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chromeos/ash/services/recording/lzw_pixel_color_indices_writer.h"
#include "chromeos/ash/services/recording/octree_color_quantizer.h"
#include "chromeos/ash/services/recording/recording_encoder.h"
#include "chromeos/ash/services/recording/rgb_video_frame.h"
#include "media/base/video_frame.h"

namespace recording {

namespace {

constexpr uint8_t kMaxColorBitDepth = 8;

// The value of the first byte of any extension block, such as the Netscape
// Extension, and the Graphic Control Extension.
constexpr uint8_t kExtensionIntroducer = 0x21;

// If building a quantized color palette for a given video frame exceeds the
// below duration, future palette rebuilds will have to be done asynchronously,
// potentially every few frames. See `SetQuantizer()` below.
constexpr base::TimeDelta kMaxDurationForSyncPaletteRebuilds =
    base::Milliseconds(20);

// If the screen doesn't have any damage, video frames may never be generated.
// As a result, there can be a large time interval between one frame and the
// next, in which case the existing color palette might be very stale, and needs
// to be rebuilt.
constexpr base::TimeDelta kMaxDurationBetweenSuccessiveFrames =
    base::Seconds(1);

// The maximum number of frames that can be encoded using an old stale color
// palette without rebuilding a new one.
constexpr int kMaxNumFramesWithStaleColorPalette = 20;

// Calculates and returns the color bit depth based on the size of the given
// `color_palette`. The color bit depth is the least number of bits needed to be
// able to represent the size of the palette as a binary number.
uint8_t CalculateColorBitDepth(const ColorTable& color_palette) {
  DCHECK_LE(color_palette.size(), kMaxNumberOfColorsInPalette);

  uint8_t bit_depth = 1;
  while ((1u << bit_depth) < color_palette.size()) {
    ++bit_depth;
  }

  DCHECK_LE(bit_depth, kMaxColorBitDepth);
  return bit_depth;
}

// -----------------------------------------------------------------------------
// GlobalColorTableFields:

// The bit fields packed in a single byte, which define the configuration of
// the Global Color Table as part of the Logical Screen Descriptor block of the
// GIF file.
union GlobalColorTableFields {
  struct {
    // The least significant 3 bits, which define the number of colors in the
    // table (2 ^ (global_color_table_size + 1) colors). The value
    // `global_color_table_size + 1` is known as the bit depth. For example, if
    // `global_color_table_size` is 7 (i.e. 0b111), this means there are 2 ^ 8
    // colors in the table, where 8 is the color bit depth.
    // This value is meaningful only if `global_color_table_flag` is set to 1.
    uint8_t global_color_table_size : 3;

    // This bit indicates whether the global color table is sorted (0b1) or not
    // (0b0).
    // This value is meaningful only if `global_color_table_flag` is set to 1.
    uint8_t sort_flag : 1;

    // For our purposes, this value is always set to 7 (0b111). Its explanation
    // is copied as is from the GIF specs below:
    // "Number of bits per primary color available to the original image, minus
    // 1. This value represents the size of the entire palette from which the
    // colors in the graphic were selected, not the number of colors actually
    // used in the graphic. It indicates the richness of the original palette".
    uint8_t color_resolution : 3;

    // This is the most significant bit, and it indicates whether a global color
    // table will immediately follow the Logical Screen Descriptor block (0b1)
    // or not (0b0).
    uint8_t global_color_table_flag : 1;
  };

  // The packed value of the above bit fields as an 8-bit byte.
  uint8_t value;
};

static_assert(sizeof(GlobalColorTableFields) == 1,
              "Unexpected size of GlobalColorTableFields");

// -----------------------------------------------------------------------------
// GraphicControlExtensionFields:

// The bit fields packed in a single byte, which define some settings that the
// decoder can use while decoding and rendering the GIF file.
union GraphicControlExtensionFields {
  struct {
    // Indicates whether a transparency color index is given as part of the
    // Graphic Control Extension block.
    uint8_t transparent_color_flag : 1;

    // Indicates whether or not user input is expected before continuing. If the
    // flag is set, processing will continue when user input is entered.
    uint8_t user_input_flag : 1;

    // Indicates the way in which the decoded image frame is to be treated after
    // being displayed.
    // 0b000: No disposal specified. The decoder is not required to take any
    //        action.
    // 0b001: Do not dispose. The image is to be left in place.
    // 0b010: The area used by the image must be restored to the background
    //        color.
    // 0b011: Restore to previous. The decoder is required to restore the area
    //        overwritten by the image with what was there prior to rendering
    //        it.
    // 0b100 - 0b111: Not defined.
    uint8_t disposal_method : 3;

    // Reserved for future use.
    uint8_t reserved : 3;
  };

  // The packed value of the above bit fields as an 8-bit byte.
  uint8_t value;
};

static_assert(sizeof(GraphicControlExtensionFields) == 1,
              "Unexpected size of GraphicControlExtensionFields");

// -----------------------------------------------------------------------------
// ImageDescriptorFields:

// The bit fields packed in a single byte, which define the configuration of
// the Local Color Table as part of the Image Descriptor block of the GIF file.
union ImageDescriptorFields {
  struct {
    // Similar to `GlobalColorTableFields::global_color_table_size`.
    uint8_t local_color_table_size : 3;

    // Reserved for future use.
    uint8_t reserved : 2;

    // Similar to `GlobalColorTableFields::sort_flag`.
    uint8_t sort_flag : 1;

    // This bit is always set to 0 for our purposes.
    uint8_t interlace_flag : 1;

    // Indicates whether or not a Local Color Table will immediately follow the
    // Image Descriptor block in the GIF file.
    uint8_t local_color_table_flag : 1;
  };

  // The packed value of the above bit fields as an 8-bit byte.
  uint8_t value;
};

static_assert(sizeof(ImageDescriptorFields) == 1,
              "Unexpected size of ImageDescriptorFields");

// -----------------------------------------------------------------------------
// GifEncoderCapabilities:

// Implements the capabilities for GIF encoding.
class GifEncoderCapabilities : public RecordingEncoder::Capabilities {
 public:
  GifEncoderCapabilities() = default;
  GifEncoderCapabilities(const GifEncoderCapabilities&) = delete;
  GifEncoderCapabilities& operator=(const GifEncoderCapabilities&) = delete;
  ~GifEncoderCapabilities() override = default;

  // RecordingEncoder::Capabilities:
  media::VideoPixelFormat GetSupportedPixelFormat() const override {
    return media::PIXEL_FORMAT_ARGB;
  }

  bool SupportsVideoFrameSizeChanges() const override {
    // The GIF file specifies the dimensions of the canvas only once in
    // `InitializeVideoEncoder()` when the Logical Screen Descriptor is written.
    // As a result, we cannot change the dimensions of the image after the fact
    // in the middle of recording.
    // TODO(afakhry): Figure out if we can allow this if the dimensions change
    // to something smaller than the initial size.
    return false;
  }

  bool SupportsRgbVideoFrame() const override { return true; }
};

// -----------------------------------------------------------------------------

QuantizerPaletteData CreateQuantizer(const RgbVideoFrame& rgb_video_frame) {
  return QuantizerPaletteData(rgb_video_frame, base::TimeTicks::Now());
}

}  // namespace

// -----------------------------------------------------------------------------
// QuantizerPaletteData:

QuantizerPaletteData::QuantizerPaletteData(const RgbVideoFrame& rgb_video_frame,
                                           base::TimeTicks start_time)
    : quantizer(OctreeColorQuantizer(rgb_video_frame)) {
  color_palette.reserve(kMaxNumberOfColorsInPalette);
  quantizer.ExtractColorPalette(color_palette);
  duration = base::TimeTicks::Now() - start_time;
}

QuantizerPaletteData::QuantizerPaletteData(QuantizerPaletteData&&) = default;

QuantizerPaletteData& QuantizerPaletteData::operator=(QuantizerPaletteData&&) =
    default;

QuantizerPaletteData::~QuantizerPaletteData() = default;

// -----------------------------------------------------------------------------
// GifEncoder:

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

// static
std::unique_ptr<RecordingEncoder::Capabilities>
GifEncoder::CreateCapabilities() {
  return std::make_unique<GifEncoderCapabilities>();
}

GifEncoder::GifEncoder(
    PassKey,
    const media::VideoEncoder::Options& video_encoder_options,
    mojo::PendingRemote<mojom::DriveFsQuotaDelegate> drive_fs_quota_delegate,
    const base::FilePath& gif_file_path,
    OnFailureCallback on_failure_callback)
    : RecordingEncoder(std::move(on_failure_callback)),
      color_palette_task_runner_(base::ThreadPool::CreateSequencedTaskRunner(
          {base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      gif_file_writer_(std::move(drive_fs_quota_delegate),
                       gif_file_path,
                       /*file_io_helper_delegate=*/this),
      lzw_encoder_(&gif_file_writer_) {
  InitializeVideoEncoder(video_encoder_options);
}

GifEncoder::~GifEncoder() = default;

void GifEncoder::InitializeVideoEncoder(
    const media::VideoEncoder::Options& video_encoder_options) {
  // There can be a maximum of 256 colors in our color palette, and
  // `width * height` pixels.
  color_palette_.reserve(kMaxNumberOfColorsInPalette);
  pixel_color_indices_.resize(video_encoder_options.frame_size.GetArea());

  gif_file_writer_.WriteString("GIF89a");  // The GIF header.
  WriteLogicalScreenDescriptor(video_encoder_options.frame_size);
  WriteNetscapeExtension();
}

void GifEncoder::EncodeVideo(scoped_refptr<media::VideoFrame> frame) {
  NOTREACHED_IN_MIGRATION();
}

void GifEncoder::EncodeRgbVideo(RgbVideoFrame rgb_video_frame) {
  ++frame_count_;

  const auto frame_time = rgb_video_frame.frame_time();

  // `min_num_frames_before_palette_rebuild_` is initialized to `1`, meaning
  // that for the very first frame ever, or if it's a fast enough operation on
  // the current device, that we can build a palette synchronously once every
  // frame, we do so, and proceed with the rest of encoding.
  // Otherwise, we can keep using the same color palette that we have without
  // rebuilding it, until `min_num_frames_before_palette_rebuild_` frames are
  // received since the last time we built a color palette. At which point, we
  // send a request to rebuild a new color palette on the
  // `color_palette_task_runner_` sequence, so as not to block the encoding task
  // sequence.
  // Note that we don't allow the duration between any two successive frames to
  // exceed `kMaxDurationBetweenSuccessiveFrames` without rebuilding the color
  // palette as it may be very stale.
  if (min_num_frames_before_palette_rebuild_ == 1) {
    SetQuantizer(CreateQuantizer(rgb_video_frame));
  } else if (frame_count_ % min_num_frames_before_palette_rebuild_ == 0 ||
             (!last_frame_time_.is_null() &&
              frame_time - last_frame_time_ >=
                  kMaxDurationBetweenSuccessiveFrames)) {
    // Note that we have to clone the `rgb_video_frame` as the one we have
    // here will be disposed once this function returns.
    color_palette_task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&CreateQuantizer, rgb_video_frame.Clone()),
        base::BindOnce(&GifEncoder::SetQuantizer,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  color_quantizer_.ExtractPixelColorIndices(rgb_video_frame, color_palette_,
                                            pixel_color_indices_);

  DCHECK_EQ(pixel_color_indices_.size(), rgb_video_frame.num_pixels());

  WriteGraphicControlExtension(frame_time);
  const auto color_bit_depth = CalculateColorBitDepth(color_palette_);
  WriteImageDescriptor(rgb_video_frame, color_bit_depth);
  WriteColorPalette(color_bit_depth);
  lzw_encoder_.EncodeAndWrite(pixel_color_indices_, color_bit_depth);

  last_frame_time_ = frame_time;
}

EncodeAudioCallback GifEncoder::GetEncodeAudioCallback() {
  NOTREACHED();
}

void GifEncoder::FlushAndFinalize(base::OnceClosure on_done) {
  // Write the character ';' (or `0x3B` in hex), which is the GIF trailer byte,
  // and flush the contents of the file.
  gif_file_writer_.WriteByte(0x3B);
  gif_file_writer_.FlushFile();

  std::move(on_done).Run();
}

void GifEncoder::WriteLogicalScreenDescriptor(const gfx::Size& frame_size) {
  gif_file_writer_.WriteShort(frame_size.width());
  gif_file_writer_.WriteShort(frame_size.height());

  // We will not use a global color table, so we will set its flag to 0. The
  // palette will be written once for every frame in the local color table after
  // the image descriptor.
  gif_file_writer_.WriteByte(GlobalColorTableFields{
      .global_color_table_size = 0b000,
      .sort_flag = 0b0,
      .color_resolution = 0b111,
      .global_color_table_flag = 0b0,
  }
                                 .value);

  gif_file_writer_.WriteByte(0x00);  // Background color index.
  gif_file_writer_.WriteByte(0x00);  // Pixel aspect ratio.
}

void GifEncoder::WriteNetscapeExtension() {
  gif_file_writer_.WriteByte(kExtensionIntroducer);
  // Extension Label. Always set to 0xFF for application extensions, which the
  // Netscape extension is an example of.
  gif_file_writer_.WriteByte(0xFF);
  // The size of the following block that contains the application identifier,
  // and the authentication code ("NETSCAPE2.0" in our case; i.e. 11 bytes).
  gif_file_writer_.WriteByte(11);
  gif_file_writer_.WriteString("NETSCAPE2.0");

  // Next are the application data sub-blocks. Each sub-block begins with a byte
  // that indicates its size, followed by a byte for its ID, followed by the
  // payload of the sub-block itself, and finally a block terminator byte which
  // is always `0x00`.
  // For the Netscape extension, we have only 1 sub-block that defines the
  // config for the looping of the animated GIF image.
  // The sub-block is 3 bytes (excluding the block terminator).
  gif_file_writer_.WriteByte(3);
  // The sub block ID is 1.
  gif_file_writer_.WriteByte(1);
  // GIF animation loop config as a 16-bit unsigned integer. The value `0` means
  // keep looping forever.
  gif_file_writer_.WriteShort(0);
  // The block terminator.
  gif_file_writer_.WriteByte(0);
}

void GifEncoder::WriteGraphicControlExtension(
    base::TimeTicks current_frame_time) {
  gif_file_writer_.WriteByte(kExtensionIntroducer);
  // The Extension Label. Always set to 0xF9 for Graphic Control Extension.
  gif_file_writer_.WriteByte(0xF9);
  // The number of bytes that follow (excluding the block terminator).
  gif_file_writer_.WriteByte(4);

  // Write the packed fields, such that we specify that we won't provide a
  // transparent color index, and user input is not expected to proceed to the
  // next frame.
  gif_file_writer_.WriteByte(GraphicControlExtensionFields{
      .transparent_color_flag = 0b0,
      .user_input_flag = 0b0,
      .disposal_method = 0b000,
      .reserved = 0b000,
  }
                                 .value);

  // Write the delay after which this frame that is being encoded should be
  // rendered. The delay is specified in units of 1/100 (hundredth) of a second.
  if (last_frame_time_.is_null()) {
    // If this is the first ever frame, then there should be no delay.
    gif_file_writer_.WriteShort(0);
  } else {
    const uint16_t delay = std::floor(
        (current_frame_time - last_frame_time_).InSecondsF() * 100.f);
    gif_file_writer_.WriteShort(delay);
  }

  // We're not using a transparent color (see above), so the value we write here
  // doesn't matter and will be ignored.
  gif_file_writer_.WriteByte(0);

  // The block terminator.
  gif_file_writer_.WriteByte(0);
}

void GifEncoder::WriteImageDescriptor(const RgbVideoFrame& rgb_video_frame,
                                      uint8_t color_bit_depth) {
  DCHECK_LE(color_bit_depth, kMaxColorBitDepth);

  // The Image Separator byte. Always 0x2C.
  gif_file_writer_.WriteByte(0x2C);
  // The "left" (or X coordinate) of the frame within the canvas bounds (which
  // was defined earlier in the Logical Screen Descriptor when the encoder was
  // initialized).
  gif_file_writer_.WriteShort(0);
  // The "top" (or Y coordinate) of the frame.
  gif_file_writer_.WriteShort(0);
  // The frame size.
  gif_file_writer_.WriteShort(rgb_video_frame.width());
  gif_file_writer_.WriteShort(rgb_video_frame.height());

  // Write the Image Descriptor bitfields such that we specify that we're using
  // a non-sorted, non-interlaced local color table of size 2 ^ color_bit_depth
  // colors.
  gif_file_writer_.WriteByte(ImageDescriptorFields{
      .local_color_table_size = static_cast<uint8_t>((color_bit_depth - 1)),
      .reserved = 0b00,
      .sort_flag = 0b0,
      .interlace_flag = 0b0,
      .local_color_table_flag = 0b1,
  }
                                 .value);
}

void GifEncoder::WriteColorPalette(uint8_t color_bit_depth) {
  DCHECK_LE(color_bit_depth, kMaxColorBitDepth);

  const size_t table_size = 1u << color_bit_depth;
  const size_t end = std::min(table_size, color_palette_.size());
  for (size_t i = 0; i < end; ++i) {
    const auto& color = color_palette_[i];
    gif_file_writer_.WriteByte(color.r);
    gif_file_writer_.WriteByte(color.g);
    gif_file_writer_.WriteByte(color.b);
  }

  // The color table size that we write to the GIF file has to be a multiple of
  // 2 (2 ^ color_bit_depth). See
  // `ImageDescriptorFields::local_color_table_size`.
  // However, we may have less colors in our palette that we extracted from the
  // current video frame than a value that is a multiple of 2. We fill the
  // remaining colors in the table as zeros.
  for (size_t i = end; i < table_size; ++i) {
    gif_file_writer_.WriteByte(0);
    gif_file_writer_.WriteByte(0);
    gif_file_writer_.WriteByte(0);
  }
}

void GifEncoder::SetQuantizer(QuantizerPaletteData&& quantizer_data) {
  color_quantizer_ = std::move(quantizer_data.quantizer);
  color_palette_ = std::move(quantizer_data.color_palette);

  min_num_frames_before_palette_rebuild_ = std::clamp<int>(
      std::ceil(quantizer_data.duration / kMaxDurationForSyncPaletteRebuilds),
      1, kMaxNumFramesWithStaleColorPalette);
}

}  // namespace recording
