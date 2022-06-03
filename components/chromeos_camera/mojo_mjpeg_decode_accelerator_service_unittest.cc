// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/chromeos_camera/mojo_mjpeg_decode_accelerator_service.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/memory/platform_shared_memory_region.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "base/threading/thread.h"
#include "media/base/media_switches.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/system/platform_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos_camera {

static const int32_t kArbitraryBitstreamBufferId = 123;

// Test fixture for the unit that is created via the mojom interface for
// class MojoMjpegDecodeAcceleratorService. Uses a FakeJpegDecodeAccelerator
// to simulate the actual decoding without the need for special hardware.
class MojoMjpegDecodeAcceleratorServiceTest : public ::testing::Test {
 public:
  MojoMjpegDecodeAcceleratorServiceTest() = default;
  ~MojoMjpegDecodeAcceleratorServiceTest() override = default;

  void SetUp() override {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kUseFakeMjpegDecodeAccelerator);
  }

  void OnInitializeDone(base::OnceClosure continuation, bool success) {
    EXPECT_TRUE(success);
    std::move(continuation).Run();
  }

  void OnDecodeAck(base::OnceClosure continuation,
                   int32_t bitstream_buffer_id,
                   MjpegDecodeAccelerator::Error error) {
    EXPECT_EQ(kArbitraryBitstreamBufferId, bitstream_buffer_id);
    std::move(continuation).Run();
  }

 private:
  // This is required to allow base::ThreadTaskRunnerHandle::Get() from the
  // test execution thread.
  base::test::TaskEnvironment task_environment_;
};

TEST_F(MojoMjpegDecodeAcceleratorServiceTest, InitializeAndDecode) {
  mojo::Remote<chromeos_camera::mojom::MjpegDecodeAccelerator> jpeg_decoder;
  MojoMjpegDecodeAcceleratorService::Create(
      jpeg_decoder.BindNewPipeAndPassReceiver());

  base::RunLoop run_loop;
  jpeg_decoder->Initialize(
      base::BindOnce(&MojoMjpegDecodeAcceleratorServiceTest::OnInitializeDone,
                     base::Unretained(this), run_loop.QuitClosure()));
  run_loop.Run();

  const size_t kInputBufferSizeInBytes = 512;
  const size_t kOutputFrameSizeInBytes = 1024;
  const gfx::Size kDummyFrameCodedSize(10, 10);
  const char kKeyId[] = "key id";
  const char kIv[] = "0123456789abcdef";
  std::vector<media::SubsampleEntry> subsamples;
  subsamples.push_back(media::SubsampleEntry(10, 5));
  subsamples.push_back(media::SubsampleEntry(15, 7));

  base::RunLoop run_loop2;
  base::subtle::PlatformSharedMemoryRegion shm_region =
      base::subtle::PlatformSharedMemoryRegion::CreateUnsafe(
          kInputBufferSizeInBytes);

  // mojo::SharedBufferHandle::Create will make a writable region, but an unsafe
  // one is needed.
  mojo::ScopedSharedBufferHandle output_frame_handle =
      mojo::WrapPlatformSharedMemoryRegion(
          base::subtle::PlatformSharedMemoryRegion::CreateUnsafe(
              kOutputFrameSizeInBytes));

  media::BitstreamBuffer bitstream_buffer(kArbitraryBitstreamBufferId,
                                          std::move(shm_region),
                                          kInputBufferSizeInBytes);
  bitstream_buffer.SetDecryptionSettings(kKeyId, kIv, subsamples);

  jpeg_decoder->Decode(
      std::move(bitstream_buffer), kDummyFrameCodedSize,
      std::move(output_frame_handle),
      base::checked_cast<uint32_t>(kOutputFrameSizeInBytes),
      base::BindOnce(&MojoMjpegDecodeAcceleratorServiceTest::OnDecodeAck,
                     base::Unretained(this), run_loop2.QuitClosure()));
  run_loop2.Run();
}

}  // namespace chromeos_camera
