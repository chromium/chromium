// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CHROMEOS_CAMERA_JPEG_ENCODE_ACCELERATOR_H_
#define COMPONENTS_CHROMEOS_CAMERA_JPEG_ENCODE_ACCELERATOR_H_

#include <stdint.h>

#include "media/base/bitstream_buffer.h"
#include "media/base/video_frame.h"

namespace chromeos_camera {

// JPEG encoder interface.
class JpegEncodeAccelerator {
 public:
  static constexpr int32_t kInvalidBitstreamBufferId = -1;
  enum Status {
    ENCODE_OK,

    HW_JPEG_ENCODE_NOT_SUPPORTED,

    // Eg. creation of encoder thread failed.
    THREAD_CREATION_FAILED,

    // Invalid argument was passed to an API method, e.g. the format of
    // VideoFrame is not supported.
    INVALID_ARGUMENT,

    // Output buffer is inaccessible, e.g. failed to map on another process.
    INACCESSIBLE_OUTPUT_BUFFER,

    // Failed to parse the incoming YUV image.
    PARSE_IMAGE_FAILED,

    // A fatal failure occurred in the GPU process layer or one of its
    // dependencies. Examples of such failures include hardware failures,
    // driver failures, library failures, and so on. Client is responsible for
    // destroying JEA after receiving this.
    PLATFORM_FAILURE,

    // Largest used enum. This should be adjusted when new errors are added.
    LARGEST_ERROR_ENUM = PLATFORM_FAILURE,
  };

  class Client {
   public:
    // Callback called after each successful Encode().
    // Parameters:
    //  |buffer_id| is |output_buffer.id()| of the corresponding Encode() call.
    //  |encoded_picture_size| is the actual size of encoded JPEG image in
    //  the BitstreamBuffer provided through encode().
    virtual void VideoFrameReady(int32_t buffer_id,
                                 size_t encoded_picture_size) = 0;

    // Callback to notify errors. Client is responsible for destroying JEA when
    // receiving a fatal error, i.e. PLATFORM_FAILURE. For other errors, client
    // is informed about the buffer that failed to encode and may continue
    // using the same instance of JEA.
    // Parameters:
    //  |buffer_id| is |output_buffer.id()| of the corresponding Encode() call
    //  that resulted in the error.
    //  |status| would be one of the values of Status except ENCODE_OK.
    virtual void NotifyError(int32_t buffer_id, Status status) = 0;

   protected:
    virtual ~Client() {}
  };

  // Destroys the encoder: all pending inputs are dropped immediately. This
  // call may asynchronously free system resources, but its client-visible
  // effects are synchronous. After destructor returns, no more callbacks
  // will be made on the client.
  virtual ~JpegEncodeAccelerator() = 0;

  // Initializes the JPEG encoder. Should be called once per encoder
  // construction. This call is synchronous and returns ENCODE_OK iff
  // initialization is successful.
  // Parameters:
  //  |client| is the Client interface for encode callback. The provided
  //  pointer must be valid until destructor is called.
  virtual Status Initialize(Client* client) = 0;

  // Gets the maximum possible encoded result size.
  virtual size_t GetMaxCodedBufferSize(const gfx::Size& picture_size) = 0;

  // Encodes the given |video_frame| that contains a YUV image. Client will
  // receive the encoded result in Client::VideoFrameReady() callback with the
  // corresponding |output_buffer.id()|, or receive
  // Client::NotifyError() callback.
  // Parameters:
  //  |video_frame| contains the YUV image to be encoded.
  //  |quality| of JPEG image. The range is from 1~100. High value means high
  //  quality.
  //  |exif_buffer| contains Exif data to be inserted into JPEG image. If it's
  //  nullptr, the JFIF APP0 segment will be inserted.
  //  |output_buffer| that contains output buffer for encoded result. Clients
  //  should call GetMaxCodedBufferSize() and allocate the buffer accordingly.
  //  The buffer needs to be valid until VideoFrameReady() or NotifyError() is
  //  called.
  virtual void Encode(scoped_refptr<media::VideoFrame> video_frame,
                      int quality,
                      media::BitstreamBuffer* exif_buffer,
                      media::BitstreamBuffer output_buffer) = 0;

  // Encodes the given |video_frame| that contains a YUV image. Client will
  // receive the encoded result in Client::VideoFrameReady() callback, or
  // receive Client::NotifyError() callback.
  // Parameters:
  //  |input_frame| contains the YUV image to be encoded.
  //  |output_frame| is used to represent the output Dma-buf layout.
  //  |quality| of JPEG image. The range is from 1~100. High value means high
  //  quality.
  //  |task_id| is an identifier started from zero that passed from the client
  //  side. Could be used to identify different encode tasks.
  //  |exif_buffer| contains Exif data to be inserted into JPEG image. If it's
  //  nullptr, the JFIF APP0 segment will be inserted.
  virtual void EncodeWithDmaBuf(scoped_refptr<media::VideoFrame> input_frame,
                                scoped_refptr<media::VideoFrame> output_frame,
                                int quality,
                                int32_t task_id,
                                media::BitstreamBuffer* exif_buffer) = 0;
};

}  // namespace chromeos_camera

#endif  // COMPONENTS_CHROMEOS_CAMERA_JPEG_ENCODE_ACCELERATOR_H_
