// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CHROMEOS_CAMERA_MOJO_MJPEG_DECODE_ACCELERATOR_SERVICE_H_
#define COMPONENTS_CHROMEOS_CAMERA_MOJO_MJPEG_DECODE_ACCELERATOR_SERVICE_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "components/chromeos_camera/common/mjpeg_decode_accelerator.mojom.h"
#include "components/chromeos_camera/gpu_mjpeg_decode_accelerator_factory.h"
#include "components/chromeos_camera/mjpeg_decode_accelerator.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace chromeos_camera {

// Implementation of a chromeos_camera::mojom::MjpegDecodeAccelerator which runs
// in the GPU process, and wraps a JpegDecodeAccelerator.
class MojoMjpegDecodeAcceleratorService
    : public chromeos_camera::mojom::MjpegDecodeAccelerator,
      public MjpegDecodeAccelerator::Client {
 public:
  static void Create(
      mojo::PendingReceiver<chromeos_camera::mojom::MjpegDecodeAccelerator>
          receiver);

  ~MojoMjpegDecodeAcceleratorService() override;

  // MjpegDecodeAccelerator::Client implementation.
  void VideoFrameReady(int32_t buffer_id) override;
  void NotifyError(
      int32_t buffer_id,
      ::chromeos_camera::MjpegDecodeAccelerator::Error error) override;

 private:
  // A common wrapper type for Mojo callbacks of Decode* functions.
  using MojoCallback = base::OnceCallback<void(
      ::chromeos_camera::MjpegDecodeAccelerator::Error)>;
  using MojoCallbackMap = std::map<int32_t, MojoCallback>;

  // This constructor internally calls
  // GpuMjpegDecodeAcceleratorFactory::GetAcceleratorFactories() to
  // fill |accelerator_factory_functions_|.
  MojoMjpegDecodeAcceleratorService();

  // chromeos_camera::mojom::MjpegDecodeAccelerator implementation.
  void Initialize(InitializeCallback callback) override;
  void Decode(media::BitstreamBuffer input_buffer,
              const gfx::Size& coded_size,
              mojo::ScopedSharedBufferHandle output_handle,
              uint32_t output_buffer_size,
              DecodeCallback callback) override;
  void DecodeWithFD(int32_t buffer_id,
                    mojo::ScopedHandle input_fd,
                    uint32_t input_buffer_size,
                    int32_t coded_size_width,
                    int32_t coded_size_height,
                    mojo::ScopedHandle output_fd,
                    uint32_t output_buffer_size,
                    DecodeWithFDCallback callback) override;
  void DecodeWithDmaBuf(int32_t task_id,
                        mojo::ScopedHandle src_dmabuf_fd,
                        uint32_t src_size,
                        uint32_t src_offset,
                        mojom::DmaBufVideoFramePtr dst_frame,
                        DecodeWithDmaBufCallback callback) override;
  void Uninitialize() override;

  void NotifyDecodeStatus(
      int32_t bitstream_buffer_id,
      ::chromeos_camera::MjpegDecodeAccelerator::Error error);

  const std::vector<GpuMjpegDecodeAcceleratorFactory::CreateAcceleratorCB>
      accelerator_factory_functions_;

  // A map from |task_id| to MojoCallback.
  MojoCallbackMap mojo_cb_map_;

  std::unique_ptr<::chromeos_camera::MjpegDecodeAccelerator> accelerator_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(MojoMjpegDecodeAcceleratorService);
};

}  // namespace chromeos_camera

#endif  // COMPONENTS_CHROMEOS_CAMERA_MOJO_MJPEG_DECODE_ACCELERATOR_SERVICE_H_
