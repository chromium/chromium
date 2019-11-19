// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CHROMEOS_CAMERA_MOJO_JPEG_ENCODE_ACCELERATOR_SERVICE_H_
#define COMPONENTS_CHROMEOS_CAMERA_MOJO_JPEG_ENCODE_ACCELERATOR_SERVICE_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "components/chromeos_camera/common/jpeg_encode_accelerator.mojom.h"
#include "components/chromeos_camera/gpu_jpeg_encode_accelerator_factory.h"
#include "components/chromeos_camera/jpeg_encode_accelerator.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace chromeos_camera {

// Implementation of a chromeos_camera::mojom::JpegEncodeAccelerator which runs
// in the GPU process, and wraps a JpegEncodeAccelerator.
class MojoJpegEncodeAcceleratorService
    : public chromeos_camera::mojom::JpegEncodeAccelerator,
      public JpegEncodeAccelerator::Client {
 public:
  static void Create(
      mojo::PendingReceiver<chromeos_camera::mojom::JpegEncodeAccelerator>
          receiver);

  ~MojoJpegEncodeAcceleratorService() override;

  // JpegEncodeAccelerator::Client implementation.
  void VideoFrameReady(int32_t task_id, size_t encoded_picture_size) override;
  void NotifyError(
      int32_t task_id,
      ::chromeos_camera::JpegEncodeAccelerator::Status status) override;

 private:
  using EncodeCallbackMap =
      std::unordered_map<int32_t, EncodeWithDmaBufCallback>;

  // This constructor internally calls
  // media::GpuJpegEncodeAcceleratorFactory::GetAcceleratorFactories() to
  // fill |accelerator_factory_functions_|.
  MojoJpegEncodeAcceleratorService();

  // chromeos_camera::mojom::JpegEncodeAccelerator implementation.
  void Initialize(InitializeCallback callback) override;

  // TODO(wtlee): To be deprecated. (crbug.com/944705)
  void EncodeWithFD(int32_t task_id,
                    mojo::ScopedHandle input_fd,
                    uint32_t input_buffer_size,
                    int32_t coded_size_width,
                    int32_t coded_size_height,
                    mojo::ScopedHandle exif_fd,
                    uint32_t exif_buffer_size,
                    mojo::ScopedHandle output_fd,
                    uint32_t output_buffer_size,
                    EncodeWithFDCallback callback) override;

  void EncodeWithDmaBuf(
      int32_t task_id,
      uint32_t input_format,
      std::vector<chromeos_camera::mojom::DmaBufPlanePtr> input_planes,
      std::vector<chromeos_camera::mojom::DmaBufPlanePtr> output_planes,
      mojo::ScopedHandle exif_handle,
      uint32_t exif_buffer_size,
      int32_t coded_size_width,
      int32_t coded_size_height,
      EncodeWithDmaBufCallback callback) override;

  void NotifyEncodeStatus(
      int32_t task_id,
      size_t encoded_picture_size,
      ::chromeos_camera::JpegEncodeAccelerator::Status status);

  const std::vector<GpuJpegEncodeAcceleratorFactory::CreateAcceleratorCB>
      accelerator_factory_functions_;

  // A map from task_id to EncodeCallback.
  EncodeCallbackMap encode_cb_map_;

  std::unique_ptr<::chromeos_camera::JpegEncodeAccelerator> accelerator_;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(MojoJpegEncodeAcceleratorService);
};

}  // namespace chromeos_camera

#endif  // COMPONENTS_CHROMEOS_CAMERA_MOJO_JPEG_ENCODE_ACCELERATOR_SERVICE_H_
