// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/image_fetcher/ios/webp_decoder.h"

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#include <stdint.h>

#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class WebpDecoderDelegate : public webp_transcode::WebpDecoder::Delegate {
 public:
  WebpDecoderDelegate() = default;

  WebpDecoderDelegate(const WebpDecoderDelegate&) = delete;
  WebpDecoderDelegate& operator=(const WebpDecoderDelegate&) = delete;

  NSData* data() const { return decoded_image_; }

  // WebpDecoder::Delegate methods
  void OnFinishedDecoding(bool success) override {
    if (!success)
      decoded_image_ = nil;
  }

  void SetImageFeatures(
      size_t total_size,
      webp_transcode::WebpDecoder::DecodedImageFormat format) override {
    decoded_image_ = [[NSMutableData alloc] initWithCapacity:total_size];
  }

  void OnDataDecoded(NSData* data) override {
    DCHECK(decoded_image_);
    [decoded_image_ appendData:data];
  }

 private:
  ~WebpDecoderDelegate() override {}
  NSMutableData* decoded_image_;
};

// Content-type header for WebP images.
const char kWEBPFirstMagicPattern[] = "RIFF";
const char kWEBPSecondMagicPattern[] = "WEBP";

const uint8_t kNumIfdEntries = 15;
const unsigned int kExtraDataSize = 16;
// 10b for signature/header + n * 12b entries + 4b for IFD terminator:
const unsigned int kExtraDataOffset = 10 + 12 * kNumIfdEntries + 4;
const unsigned int kHeaderSize = kExtraDataOffset + kExtraDataSize;
const int kRecompressionThreshold = 64 * 64;  // Threshold in pixels.
const CGFloat kJpegQuality = 0.85;

// Adapted from libwebp example dwebp.c.
void PutLE16(uint8_t* const dst, uint32_t value) {
  dst[0] = (value >> 0) & 0xff;
  dst[1] = (value >> 8) & 0xff;
}

void PutLE32(uint8_t* const dst, uint32_t value) {
  PutLE16(dst + 0, (value >> 0) & 0xffff);
  PutLE16(dst + 2, (value >> 16) & 0xffff);
}

void WriteTiffHeader(uint8_t* dst,
                     int width,
                     int height,
                     int bytes_per_px,
                     bool has_alpha) {
  // For non-alpha case, we omit tag 0x152 (ExtraSamples).
  const uint8_t num_ifd_entries =
      has_alpha ? kNumIfdEntries : kNumIfdEntries - 1;
  uint8_t bytes_per_px_u8 = bytes_per_px;
  uint8_t tiff_header[kHeaderSize] = {
      0x49, 0x49, 0x2a, 0x00,  // little endian signature
      8, 0, 0, 0,              // offset to the unique IFD that follows
      // IFD (offset = 8). Entries must be written in increasing tag order.
      num_ifd_entries, 0,  // Number of entries in the IFD (12 bytes each).
      0x00, 0x01, 3, 0, 1, 0, 0, 0, 0, 0, 0, 0,    //  10: Width  (TBD)
      0x01, 0x01, 3, 0, 1, 0, 0, 0, 0, 0, 0, 0,    //  22: Height (TBD)
      0x02, 0x01, 3, 0, bytes_per_px_u8, 0, 0, 0,  //  34: BitsPerSample: 8888
      kExtraDataOffset + 0, 0, 0, 0, 0x03, 0x01, 3, 0, 1, 0, 0, 0, 1, 0, 0,
      0,                                         //  46: Compression: none
      0x06, 0x01, 3, 0, 1, 0, 0, 0, 2, 0, 0, 0,  //  58: Photometric: RGB
      0x11, 0x01, 4, 0, 1, 0, 0, 0,              //  70: Strips offset:
      kHeaderSize, 0, 0, 0,                      //      data follows header
      0x12, 0x01, 3, 0, 1, 0, 0, 0, 1, 0, 0, 0,  //  82: Orientation: topleft
      0x15, 0x01, 3, 0, 1, 0, 0, 0,              //  94: SamplesPerPixels
      bytes_per_px_u8, 0, 0, 0, 0x16, 0x01, 3, 0, 1, 0, 0, 0, 0, 0, 0,
      0,                                         // 106: Rows per strip (TBD)
      0x17, 0x01, 4, 0, 1, 0, 0, 0, 0, 0, 0, 0,  // 118: StripByteCount (TBD)
      0x1a, 0x01, 5, 0, 1, 0, 0, 0,              // 130: X-resolution
      kExtraDataOffset + 8, 0, 0, 0, 0x1b, 0x01, 5, 0, 1, 0, 0,
      0,  // 142: Y-resolution
      kExtraDataOffset + 8, 0, 0, 0, 0x1c, 0x01, 3, 0, 1, 0, 0, 0, 1, 0, 0,
      0,                                         // 154: PlanarConfiguration
      0x28, 0x01, 3, 0, 1, 0, 0, 0, 2, 0, 0, 0,  // 166: ResolutionUnit (inch)
      0x52, 0x01, 3, 0, 1, 0, 0, 0, 1, 0, 0, 0,  // 178: ExtraSamples: rgbA
      0, 0, 0, 0,                                // 190: IFD terminator
      // kExtraDataOffset:
      8, 0, 8, 0, 8, 0, 8, 0,  // BitsPerSample
      72, 0, 0, 0, 1, 0, 0, 0  // 72 pixels/inch, for X/Y-resolution
  };

  // Fill placeholders in IFD:
  PutLE32(tiff_header + 10 + 8, width);
  PutLE32(tiff_header + 22 + 8, height);
  PutLE32(tiff_header + 106 + 8, height);
  PutLE32(tiff_header + 118 + 8, width * bytes_per_px * height);
  if (!has_alpha)
    PutLE32(tiff_header + 178, 0);

  memcpy(dst, tiff_header, kHeaderSize);
}

}  // namespace

namespace webp_transcode {

// static
NSData* WebpDecoder::DecodeWebpImage(NSData* webp_image) {
  scoped_refptr<WebpDecoderDelegate> delegate(new WebpDecoderDelegate);

  scoped_refptr<webp_transcode::WebpDecoder> decoder(
      new webp_transcode::WebpDecoder(delegate.get()));

  decoder->OnDataReceived(webp_image);
  DLOG_IF(ERROR, !delegate->data()) << "WebP image decoding failed.";
  return delegate->data();
}

// static
bool WebpDecoder::IsWebpImage(const std::string& image_data) {
  if (image_data.length() < 12)
    return false;
  return image_data.compare(0, 4, kWEBPFirstMagicPattern) == 0 &&
         image_data.compare(8, 4, kWEBPSecondMagicPattern) == 0;
}

// static
size_t WebpDecoder::GetHeaderSize() {
  return kHeaderSize;
}

WebpDecoder::WebpDecoder(WebpDecoder::Delegate* delegate)
    : delegate_(delegate), state_(READING_FEATURES), has_alpha_(0) {
  DCHECK(delegate_.get());
  const bool rv = WebPInitDecoderConfig(&config_);
  DCHECK(rv);
}

WebpDecoder::~WebpDecoder() {
  WebPFreeDecBuffer(&config_.output);
}

void WebpDecoder::OnDataReceived(NSData* data) {
  DCHECK(data);
  switch (state_) {
    case READING_FEATURES:
      DoReadFeatures(data);
      break;
    case READING_DATA:
      DoReadData(data);
      break;
    case DONE:
      DLOG(WARNING) << "Received WebP data but decoding is finished. Ignoring.";
      break;
  }
}

void WebpDecoder::Stop() {
  if (state_ != DONE) {
    state_ = DONE;
    DLOG(WARNING) << "Unexpected end of WebP data.";
    delegate_->OnFinishedDecoding(false);
  }
}

void WebpDecoder::DoReadFeatures(NSData* data) {
  DCHECK_EQ(READING_FEATURES, state_);
  DCHECK(data);
  if (features_)
    [features_ appendData:data];
  else
    features_ = [[NSMutableData alloc] initWithData:data];
  VP8StatusCode status =
      WebPGetFeatures(static_cast<const uint8_t*>([features_ bytes]),
                      [features_ length], &config_.input);
  switch (status) {
    case VP8_STATUS_OK: {
      has_alpha_ = config_.input.has_alpha;
      const uint32_t width = config_.input.width;
      const uint32_t height = config_.input.height;
      const size_t bytes_per_px = has_alpha_ ? 4 : 3;
      const int stride = bytes_per_px * width;
      const size_t image_data_size = stride * height;
      const size_t total_size = image_data_size + kHeaderSize;
      // Force pre-multiplied alpha.
      config_.output.colorspace = has_alpha_ ? MODE_rgbA : MODE_RGB;
      config_.output.u.RGBA.stride = stride;
      // Create the output buffer.
      config_.output.u.RGBA.size = image_data_size;
      uint8_t* dst = static_cast<uint8_t*>(malloc(total_size));
      if (!dst) {
        DLOG(ERROR) << "Could not allocate WebP decoding buffer (size = "
                    << total_size << ").";
        delegate_->OnFinishedDecoding(false);
        state_ = DONE;
        break;
      }
      WriteTiffHeader(dst, width, height, bytes_per_px, has_alpha_);
      output_buffer_ = [[NSData alloc] initWithBytesNoCopy:dst
                                                    length:total_size
                                              freeWhenDone:YES];
      config_.output.is_external_memory = 1;
      config_.output.u.RGBA.rgba = dst + kHeaderSize;
      // Start decoding.
      state_ = READING_DATA;
      incremental_decoder_.reset(WebPINewDecoder(&config_.output));
      DoReadData(features_);
      features_ = nil;
      break;
    }
    case VP8_STATUS_NOT_ENOUGH_DATA:
      // Do nothing.
      break;
    default:
      DLOG(ERROR) << "Error in WebP image features.";
      delegate_->OnFinishedDecoding(false);
      state_ = DONE;
      break;
  }
}

void WebpDecoder::DoReadData(NSData* data) {
  DCHECK_EQ(READING_DATA, state_);
  DCHECK(incremental_decoder_);
  DCHECK(data);
  VP8StatusCode status =
      WebPIAppend(incremental_decoder_.get(),
                  static_cast<const uint8_t*>([data bytes]), [data length]);
  switch (status) {
    case VP8_STATUS_SUSPENDED:
      // Do nothing: re-compression to JPEG or PNG cannot be done incrementally.
      // Wait for the whole image to be decoded.
      break;
    case VP8_STATUS_OK: {
      bool rv = DoSendData();
      DLOG_IF(ERROR, !rv) << "Error in WebP image conversion.";
      state_ = DONE;
      delegate_->OnFinishedDecoding(rv);
      break;
    }
    default:
      DLOG(ERROR) << "Error in WebP image decoding.";
      delegate_->OnFinishedDecoding(false);
      state_ = DONE;
      break;
  }
}

bool WebpDecoder::DoSendData() {
  DCHECK_EQ(READING_DATA, state_);
  int width, height;
  uint8_t* data_ptr = WebPIDecGetRGB(incremental_decoder_.get(), nullptr,
                                     &width, &height, nullptr);
  if (!data_ptr)
    return false;
  DCHECK_EQ(static_cast<const uint8_t*>([output_buffer_ bytes]) + kHeaderSize,
            data_ptr);
  NSData* result_data = nil;
  // When the WebP image is larger than |kRecompressionThreshold| it is
  // compressed to JPEG or PNG. Otherwise, the uncompressed TIFF is used.
  DecodedImageFormat format = TIFF;
  if (width * height > kRecompressionThreshold) {
    UIImage* tiff_image = [[UIImage alloc] initWithData:output_buffer_];
    if (!tiff_image)
      return false;
    // Compress to PNG if the image is transparent, JPEG otherwise.
    // TODO(droger): Use PNG instead of JPEG if the WebP image is lossless.
    if (has_alpha_) {
      result_data = UIImagePNGRepresentation(tiff_image);
      format = PNG;
    } else {
      result_data = UIImageJPEGRepresentation(tiff_image, kJpegQuality);
      format = JPEG;
    }
    if (!result_data)
      return false;
  } else {
    result_data = output_buffer_;
  }
  delegate_->SetImageFeatures([result_data length], format);
  delegate_->OnDataDecoded(result_data);
  output_buffer_ = nil;
  return true;
}

}  // namespace webp_transcode
