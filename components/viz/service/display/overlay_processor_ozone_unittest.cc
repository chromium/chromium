// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_ozone.h"

#include "components/viz/test/test_context_provider.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/native_pixmap_handle.h"

using ::testing::_;
using ::testing::Return;

namespace viz {

namespace {

class FakeOverlayCandidatesOzone : public ui::OverlayCandidatesOzone {
 public:
  ~FakeOverlayCandidatesOzone() override = default;

  // Mark every overlay candidate as handled since we don't really care about
  // OverlayCandidatesOzone internals in this test suite.
  void CheckOverlaySupport(
      std::vector<ui::OverlaySurfaceCandidate>* candidates) override {
    for (auto& candidate : *candidates) {
      candidate.overlay_handled = true;
    }
  }
};

class FakeNativePixmap : public gfx::NativePixmap {
 public:
  FakeNativePixmap(gfx::Size size, gfx::BufferFormat format)
      : size_(size), format_(format) {}
  bool AreDmaBufFdsValid() const override { return false; }
  int GetDmaBufFd(size_t plane) const override { return -1; }
  uint32_t GetDmaBufPitch(size_t plane) const override { return 0; }
  size_t GetDmaBufOffset(size_t plane) const override { return 0; }
  size_t GetDmaBufPlaneSize(size_t plane) const override { return 0; }
  uint64_t GetBufferFormatModifier() const override { return 0; }
  gfx::BufferFormat GetBufferFormat() const override { return format_; }
  size_t GetNumberOfPlanes() const override { return 0; }
  gfx::Size GetBufferSize() const override { return size_; }
  uint32_t GetUniqueId() const override { return 0; }
  bool ScheduleOverlayPlane(
      gfx::AcceleratedWidget widget,
      int plane_z_order,
      gfx::OverlayTransform plane_transform,
      const gfx::Rect& display_bounds,
      const gfx::RectF& crop_rect,
      bool enable_blend,
      std::vector<gfx::GpuFence> acquire_fences,
      std::vector<gfx::GpuFence> release_fences) override {
    return false;
  }
  gfx::NativePixmapHandle ExportHandle() override {
    return gfx::NativePixmapHandle();
  }

 private:
  ~FakeNativePixmap() override = default;
  gfx::Size size_;
  gfx::BufferFormat format_;
};

class MockSharedImageInterface : public TestSharedImageInterface {
 public:
  MOCK_METHOD1(GetNativePixmap,
               scoped_refptr<gfx::NativePixmap>(const gpu::Mailbox& mailbox));
};

}  // namespace

// TODO(crbug.com/1138568): Fuchsia claims support for presenting primary
// plane as overlay, but does not provide a mailbox. Handle this case.
#if !defined(OS_FUCHSIA)
TEST(OverlayProcessorOzoneTest, PrimaryPlaneSizeAndFormatMatches) {
  // Set up the primary plane.
  gfx::Size size(128, 128);
  OverlayProcessorInterface::OutputSurfaceOverlayPlane primary_plane;
  primary_plane.resource_size = size;
  primary_plane.format = gfx::BufferFormat::BGRA_8888;
  primary_plane.mailbox = gpu::Mailbox::GenerateForSharedImage();

  // Set up a dummy OverlayCandidate.
  OverlayCandidate candidate;
  candidate.resource_size_in_pixels = size;
  candidate.format = gfx::BufferFormat::BGRA_8888;
  candidate.mailbox = gpu::Mailbox::GenerateForSharedImage();
  candidate.overlay_handled = false;
  OverlayCandidateList candidates;
  candidates.push_back(candidate);

  // Initialize a MockSharedImageInterface that returns a NativePixmap with
  // matching params to the primary plane.
  std::unique_ptr<MockSharedImageInterface> sii =
      std::make_unique<MockSharedImageInterface>();
  scoped_refptr<gfx::NativePixmap> primary_plane_pixmap =
      base::MakeRefCounted<FakeNativePixmap>(size,
                                             gfx::BufferFormat::BGRA_8888);
  scoped_refptr<gfx::NativePixmap> candidate_pixmap =
      base::MakeRefCounted<FakeNativePixmap>(size,
                                             gfx::BufferFormat::BGRA_8888);
  EXPECT_CALL(*sii, GetNativePixmap(_))
      .WillOnce(Return(primary_plane_pixmap))
      .WillOnce(Return(candidate_pixmap));
  OverlayProcessorOzone processor(
      std::make_unique<FakeOverlayCandidatesOzone>(), {}, sii.get());

  processor.CheckOverlaySupport(&primary_plane, &candidates);

  // Since the |OutputSurfaceOverlayPlane|'s size and format match those of
  // primary plane's NativePixmap, the overlay candidate is promoted.
  EXPECT_TRUE(candidates.at(0).overlay_handled);
}

TEST(OverlayProcessorOzoneTest, PrimaryPlaneFormatMismatch) {
  // Set up the primary plane.
  gfx::Size size(128, 128);
  OverlayProcessorInterface::OutputSurfaceOverlayPlane primary_plane;
  primary_plane.resource_size = size;
  primary_plane.format = gfx::BufferFormat::BGRA_8888;
  primary_plane.mailbox = gpu::Mailbox::GenerateForSharedImage();

  // Set up a dummy OverlayCandidate.
  OverlayCandidate candidate;
  candidate.resource_size_in_pixels = size;
  candidate.format = gfx::BufferFormat::BGRA_8888;
  candidate.mailbox = gpu::Mailbox::GenerateForSharedImage();
  candidate.overlay_handled = false;
  OverlayCandidateList candidates;
  candidates.push_back(candidate);

  // Initialize a MockSharedImageInterface that returns a NativePixmap with
  // a different buffer format than that of the primary plane.
  std::unique_ptr<MockSharedImageInterface> sii =
      std::make_unique<MockSharedImageInterface>();
  scoped_refptr<gfx::NativePixmap> primary_plane_pixmap =
      base::MakeRefCounted<FakeNativePixmap>(size, gfx::BufferFormat::R_8);
  EXPECT_CALL(*sii, GetNativePixmap(_)).WillOnce(Return(primary_plane_pixmap));
  OverlayProcessorOzone processor(
      std::make_unique<FakeOverlayCandidatesOzone>(), {}, sii.get());

  processor.CheckOverlaySupport(&primary_plane, &candidates);

  // Since the |OutputSurfaceOverlayPlane|'s format doesn't match that of the
  // primary plane's NativePixmap, the overlay candidate is NOT promoted.
  EXPECT_FALSE(candidates.at(0).overlay_handled);
}
#endif

}  // namespace viz
