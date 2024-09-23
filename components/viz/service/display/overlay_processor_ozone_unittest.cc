// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/overlay_processor_ozone.h"

#include <utility>

#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/viz/common/features.h"
#include "components/viz/test/test_context_provider.h"
#include "gpu/command_buffer/client/shared_image_interface.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/linux/native_pixmap_dmabuf.h"
#include "ui/gfx/native_pixmap.h"
#include "ui/gfx/native_pixmap_handle.h"
#include "ui/ozone/public/hardware_capabilities.h"

using ::testing::_;
using ::testing::Return;

namespace viz {

namespace {

class FakeOverlayCandidatesOzone : public ui::OverlayCandidatesOzone {
 public:
  ~FakeOverlayCandidatesOzone() override = default;

  // We don't really care about OverlayCandidatesOzone internals, but we do need
  // to detect if the OverlayProcessor skipped a candidate. In that case,
  // ui::OverlaySurfaceCandidate would be default constructed (except for the Z
  // order). Therefore, we use the buffer size of the candidate to decide
  // whether to mark the candidate as handled.
  void CheckOverlaySupport(
      std::vector<ui::OverlaySurfaceCandidate>* candidates) override {
    for (auto& candidate : *candidates) {
      candidate.overlay_handled = !candidate.buffer_size.IsEmpty();
    }
  }

  // Capture the callback so we can call it at will in tests.
  void ObserveHardwareCapabilities(
      ui::HardwareCapabilitiesCallback receive_callback) override {
    receive_callback_ = std::move(receive_callback);
  }

  ui::HardwareCapabilitiesCallback& receive_callback() {
    return receive_callback_;
  }

 private:
  ui::HardwareCapabilitiesCallback receive_callback_;
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
  bool SupportsZeroCopyWebGPUImport() const override { return false; }
  gfx::Size GetBufferSize() const override { return size_; }
  uint32_t GetUniqueId() const override { return 0; }
  bool ScheduleOverlayPlane(
      gfx::AcceleratedWidget widget,
      const gfx::OverlayPlaneData& overlay_plane_data,
      std::vector<gfx::GpuFence> acquire_fences,
      std::vector<gfx::GpuFence> release_fences) override {
    return false;
  }
  gfx::NativePixmapHandle ExportHandle() const override {
    return gfx::NativePixmapHandle();
  }

 private:
  ~FakeNativePixmap() override = default;
  gfx::Size size_;
  gfx::BufferFormat format_;
};

class MockSharedImageInterface : public gpu::TestSharedImageInterface {
 public:
  MOCK_METHOD1(GetNativePixmap,
               scoped_refptr<gfx::NativePixmap>(const gpu::Mailbox& mailbox));

 protected:
  ~MockSharedImageInterface() override = default;
};

}  // namespace

// TODO(crbug.com/40153057): Fuchsia claims support for presenting primary
// plane as overlay, but does not provide a mailbox. Handle this case.
#if !BUILDFLAG(IS_FUCHSIA)
TEST(OverlayProcessorOzoneTest, PrimaryPlaneSizeAndFormatMatches) {
  // Set up the primary plane.
  gfx::Size size(128, 128);
  OverlayProcessorInterface::OutputSurfaceOverlayPlane primary_plane;
  primary_plane.resource_size = size;
  primary_plane.format = SinglePlaneFormat::kBGRA_8888;
  primary_plane.mailbox = gpu::Mailbox::Generate();

  // Set up a dummy OverlayCandidate.
  OverlayCandidate candidate;
  candidate.resource_size_in_pixels = size;
  candidate.format = SinglePlaneFormat::kBGRA_8888;
  candidate.mailbox = gpu::Mailbox::Generate();
  candidate.overlay_handled = false;
  OverlayCandidateList candidates;
  candidates.push_back(candidate);

  // Initialize a MockSharedImageInterface that returns a NativePixmap with
  // matching params to the primary plane.
  scoped_refptr<MockSharedImageInterface> sii =
      base::MakeRefCounted<MockSharedImageInterface>();
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
  primary_plane.format = SinglePlaneFormat::kBGRA_8888;
  primary_plane.mailbox = gpu::Mailbox::Generate();

  // Set up a dummy OverlayCandidate.
  OverlayCandidate candidate;
  candidate.resource_size_in_pixels = size;
  candidate.format = SinglePlaneFormat::kBGRA_8888;
  candidate.mailbox = gpu::Mailbox::Generate();
  candidate.overlay_handled = false;
  OverlayCandidateList candidates;
  candidates.push_back(candidate);

  // Initialize a MockSharedImageInterface that returns a NativePixmap with
  // a different buffer format than that of the primary plane.
  scoped_refptr<MockSharedImageInterface> sii =
      base::MakeRefCounted<MockSharedImageInterface>();
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

TEST(OverlayProcessorOzoneTest, ColorSpaceMismatch) {
  // Set up the primary plane.
  gfx::Size size(128, 128);
  OverlayProcessorInterface::OutputSurfaceOverlayPlane primary_plane;
  primary_plane.resource_size = size;
  primary_plane.format = SinglePlaneFormat::kBGRA_8888;
  primary_plane.mailbox = gpu::Mailbox::Generate();

  // Set up a dummy OverlayCandidate.
  OverlayCandidate candidate;
  candidate.resource_size_in_pixels = size;
  candidate.format = SinglePlaneFormat::kBGRA_8888;
  candidate.mailbox = gpu::Mailbox::Generate();
  candidate.overlay_handled = false;
  OverlayCandidateList candidates;
  candidates.push_back(candidate);

  // Initialize a MockSharedImageInterface that returns a NativePixmap with
  // matching params to the primary plane.
  scoped_refptr<MockSharedImageInterface> sii =
      base::MakeRefCounted<::testing::NiceMock<MockSharedImageInterface>>();
  scoped_refptr<gfx::NativePixmap> primary_plane_pixmap =
      base::MakeRefCounted<FakeNativePixmap>(size,
                                             gfx::BufferFormat::BGRA_8888);
  scoped_refptr<gfx::NativePixmap> candidate_pixmap =
      base::MakeRefCounted<FakeNativePixmap>(size,
                                             gfx::BufferFormat::BGRA_8888);
  ON_CALL(*sii, GetNativePixmap(primary_plane.mailbox))
      .WillByDefault(Return(primary_plane_pixmap));
  ON_CALL(*sii, GetNativePixmap(candidate.mailbox))
      .WillByDefault(Return(candidate_pixmap));
  OverlayProcessorOzone processor(
      std::make_unique<FakeOverlayCandidatesOzone>(), {}, sii.get());

  // In Chrome OS, we don't allow the promotion of the candidate if the
  // ContentColorUsage is different from the primary plane (e.g., SDR vs. HDR).
  // In other platforms, this is not a restriction.
  primary_plane.color_space = gfx::ColorSpace::CreateSRGB();
  candidates[0].color_space = gfx::ColorSpace::CreateHDR10();
  processor.CheckOverlaySupport(&primary_plane, &candidates);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_FALSE(candidates.at(0).overlay_handled);
#else
  EXPECT_TRUE(candidates.at(0).overlay_handled);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  candidates[0] = candidate;

  primary_plane.color_space = gfx::ColorSpace::CreateHDR10();
  candidates[0].color_space = gfx::ColorSpace::CreateHLG();
  processor.CheckOverlaySupport(&primary_plane, &candidates);
  EXPECT_TRUE(candidates.at(0).overlay_handled);

  candidates[0] = candidate;

  // Also, if the candidate requires an overlay, then it should be promoted
  // regardless of the color space mismatch.
  primary_plane.color_space = gfx::ColorSpace::CreateSRGB();
  candidates[0].color_space = gfx::ColorSpace::CreateHDR10();
  candidates[0].requires_overlay = true;
  processor.CheckOverlaySupport(&primary_plane, &candidates);
  EXPECT_TRUE(candidates.at(0).overlay_handled);

  candidates[0] = candidate;

  // And finally, if the candidate's color space is invalid, then it also should
  // be promoted.
  primary_plane.color_space = gfx::ColorSpace::CreateHDR10();
  candidates[0].color_space = gfx::ColorSpace();
  EXPECT_FALSE(candidates[0].color_space.IsValid());
  processor.CheckOverlaySupport(&primary_plane, &candidates);
  EXPECT_TRUE(candidates.at(0).overlay_handled);
}

#endif  // !BUILDFLAG(IS_FUCHSIA)

// Exposing max_overlays_considered_ saves us from retesting a lot of logic
// that's already tested in overlay_unittest.cc.
class TestOverlayProcessorOzone : public OverlayProcessorOzone {
 public:
  using OverlayProcessorOzone::OverlayProcessorOzone;

  int MaxOverlaysConsidered() { return max_overlays_considered_; }
};

TEST(OverlayProcessorOzoneTest, ObserveHardwareCapabilites) {
  OverlayCandidateList candidates;
  // Enable 4 overlays
  const std::vector<base::test::FeatureRefAndParams> feature_and_params_list = {
      {features::kUseMultipleOverlays, {{features::kMaxOverlaysParam, "4"}}}};
  base::test::ScopedFeatureList scoped_features;
  scoped_features.InitWithFeaturesAndParameters(feature_and_params_list, {});

  auto fake_candidates_unique = std::make_unique<FakeOverlayCandidatesOzone>();
  auto* fake_candidates = fake_candidates_unique.get();

  TestOverlayProcessorOzone processor(std::move(fake_candidates_unique), {},
                                      nullptr);
  // No receive_callback yet.
  EXPECT_TRUE(fake_candidates->receive_callback().is_null());

  processor.CheckOverlaySupport(nullptr, &candidates);

  // Receive callback is set.
  EXPECT_FALSE(fake_candidates->receive_callback().is_null());
  // Max overlays is still 1.
  EXPECT_EQ(processor.MaxOverlaysConsidered(), 1);

  ui::HardwareCapabilities hc;
  hc.is_valid = true;
  hc.num_overlay_capable_planes = 6;
  fake_candidates->receive_callback().Run(hc);

  // Uses max_overlays_config_ = 4.
  EXPECT_EQ(processor.MaxOverlaysConsidered(), 4);

  hc.is_valid = true;
  hc.num_overlay_capable_planes = 4;
  fake_candidates->receive_callback().Run(hc);

  // Uses (num_overlay_capable_planes - 1) = 3.
  EXPECT_EQ(processor.MaxOverlaysConsidered(), 3);

  hc.is_valid = false;
  hc.num_overlay_capable_planes = 0;
  fake_candidates->receive_callback().Run(hc);

  // Defaults to 1 overlay when receiving an invalid response.
  EXPECT_EQ(processor.MaxOverlaysConsidered(), 1);
}

}  // namespace viz
