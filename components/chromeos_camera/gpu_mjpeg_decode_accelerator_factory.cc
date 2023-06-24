// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/chromeos_camera/gpu_mjpeg_decode_accelerator_factory.h"

#include <memory>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/chromeos_camera/fake_mjpeg_decode_accelerator.h"
#include "media/base/media_switches.h"
#include "media/gpu/buildflags.h"

#if BUILDFLAG(USE_V4L2_CODEC) && defined(ARCH_CPU_ARM_FAMILY)
#define USE_V4L2_MJPEG_DECODE_ACCELERATOR
#endif

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_mjpeg_decode_accelerator.h"
#endif

#if defined(USE_V4L2_MJPEG_DECODE_ACCELERATOR)
#include "media/gpu/v4l2/v4l2_device.h"
#include "media/gpu/v4l2/v4l2_mjpeg_decode_accelerator.h"
#endif

namespace chromeos_camera {

namespace {

#if defined(USE_V4L2_MJPEG_DECODE_ACCELERATOR)
std::unique_ptr<MjpegDecodeAccelerator> CreateV4L2MjpegDecodeAccelerator(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
  return std::make_unique<media::V4L2MjpegDecodeAccelerator>(
      new media::V4L2Device(), std::move(io_task_runner));
}
#endif

#if BUILDFLAG(USE_VAAPI)
std::unique_ptr<MjpegDecodeAccelerator> CreateVaapiMjpegDecodeAccelerator(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
  return std::make_unique<media::VaapiMjpegDecodeAccelerator>(
      std::move(io_task_runner));
}
#endif

std::unique_ptr<MjpegDecodeAccelerator> CreateFakeMjpegDecodeAccelerator(
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner) {
  return std::make_unique<FakeMjpegDecodeAccelerator>(
      std::move(io_task_runner));
}

}  // namespace

// static
bool GpuMjpegDecodeAcceleratorFactory::IsAcceleratedJpegDecodeSupported() {
  auto accelerator_factory_functions = GetAcceleratorFactories();
  for (auto& factory_function : accelerator_factory_functions) {
    std::unique_ptr<MjpegDecodeAccelerator> accelerator =
        std::move(factory_function)
            .Run(base::SingleThreadTaskRunner::GetCurrentDefault());
    if (accelerator && accelerator->IsSupported())
      return true;
  }
  return false;
}

// static
std::vector<GpuMjpegDecodeAcceleratorFactory::CreateAcceleratorCB>
GpuMjpegDecodeAcceleratorFactory::GetAcceleratorFactories() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kUseFakeMjpegDecodeAccelerator)) {
    std::vector<CreateAcceleratorCB> result;
    result.push_back(base::BindOnce(&CreateFakeMjpegDecodeAccelerator));
    return result;
  }

  // This list is ordered by priority of use.
  std::vector<CreateAcceleratorCB> result;
#if defined(USE_V4L2_MJPEG_DECODE_ACCELERATOR)
  result.push_back(base::BindOnce(&CreateV4L2MjpegDecodeAccelerator));
#endif
#if BUILDFLAG(USE_VAAPI)
  result.push_back(base::BindOnce(&CreateVaapiMjpegDecodeAccelerator));
#endif
  return result;
}

}  // namespace chromeos_camera
