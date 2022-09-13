// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_IMAGE_FETCHER_IOS_WEBP_DECODER_H_
#define COMPONENTS_IMAGE_FETCHER_IOS_WEBP_DECODER_H_

#import <Foundation/Foundation.h>
#include <stddef.h>

#include <memory>

#include "base/memory/ref_counted.h"
#include "third_party/libwebp/src/src/webp/decode.h"

@class NSData;

namespace webp_transcode {

// Decodes a WebP image into either JPEG, PNG or uncompressed TIFF.
class WebpDecoder : public base::RefCountedThreadSafe<WebpDecoder> {
 public:
  // Format of the decoded image.
  enum DecodedImageFormat { JPEG = 1, PNG, TIFF };

  class Delegate : public base::RefCountedThreadSafe<WebpDecoder::Delegate> {
   public:
    virtual void OnFinishedDecoding(bool success) = 0;
    virtual void SetImageFeatures(size_t total_size,  // In bytes.
                                  DecodedImageFormat format) = 0;
    virtual void OnDataDecoded(NSData* data) = 0;

   protected:
    friend class base::RefCountedThreadSafe<WebpDecoder::Delegate>;
    virtual ~Delegate() {}
  };

  explicit WebpDecoder(WebpDecoder::Delegate* delegate);

  // Returns an NSData object containing the decoded image data of the given
  // webp_image. Returns nil in case of failure.
  static NSData* DecodeWebpImage(NSData* webp_image);

  // Returns true if the given image_data is a WebP image.
  //
  // Every WebP file contains a 12 byte file header in the beginning of the
  // file.
  // A WebP file header starts with the four ASCII characters "RIFF". The next
  // four bytes contain the image size and the last four header bytes contain
  // the four ASCII characters "WEBP".
  //
  // WebP file header:
  //                                  1 1
  // Byte Nr.     0 1 2 3 4 5 6 7 8 9 0 1
  // Byte value [ R I F F ? ? ? ? W E B P  ]
  //
  // For more information see:
  // https://developers.google.com/speed/webp/docs/riff_container#webp_file_header
  static bool IsWebpImage(const std::string& image_data);

  // For tests.
  static size_t GetHeaderSize();

  // Main entry point.
  void OnDataReceived(NSData* data);

  // Stops the decoding.
  void Stop();

 private:
  struct WebPIDecoderDeleter {
    inline void operator()(WebPIDecoder* ptr) const { WebPIDelete(ptr); }
  };

  enum State { READING_FEATURES, READING_DATA, DONE };

  friend class base::RefCountedThreadSafe<WebpDecoder>;
  virtual ~WebpDecoder();

  // Implements WebP image decoding state machine steps.
  void DoReadFeatures(NSData* data);
  void DoReadData(NSData* data);
  bool DoSendData();

  scoped_refptr<WebpDecoder::Delegate> delegate_;
  WebPDecoderConfig config_;
  WebpDecoder::State state_;
  std::unique_ptr<WebPIDecoder, WebPIDecoderDeleter> incremental_decoder_;
  __strong NSData* output_buffer_;
  __strong NSMutableData* features_;
  int has_alpha_;
};

}  // namespace webp_transcode

#endif  // COMPONENTS_IMAGE_FETCHER_IOS_WEBP_DECODER_H_
