// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CHROMEOS_CAMERA_GPU_MJPEG_DECODE_ACCELERATOR_FACTORY_H_
#define COMPONENTS_CHROMEOS_CAMERA_GPU_MJPEG_DECODE_ACCELERATOR_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "components/chromeos_camera/mjpeg_decode_accelerator.h"

namespace base {
class SingleThreadTaskRunner;
}

namespace chromeos_camera {

class GpuMjpegDecodeAcceleratorFactory {
 public:
  using CreateAcceleratorCB =
      base::OnceCallback<std::unique_ptr<MjpegDecodeAccelerator>(
          scoped_refptr<base::SingleThreadTaskRunner>)>;

  // Static query for JPEG supported. This query calls the appropriate
  // platform-specific version.
  static bool IsAcceleratedJpegDecodeSupported();

  static std::vector<CreateAcceleratorCB> GetAcceleratorFactories();
};

}  // namespace chromeos_camera

#endif  // COMPONENTS_CHROMEOS_CAMERA_GPU_MJPEG_DECODE_ACCELERATOR_FACTORY_H_
