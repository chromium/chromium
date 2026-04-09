// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/mock_xr_device_hook_base.h"
#include "chrome/browser/vr/test/multi_class_browser_test.h"
#include "chrome/browser/vr/test/ui_utils.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "device/vr/buildflags/buildflags.h"

#if BUILDFLAG(ENABLE_OPENXR)

namespace vr {

namespace {
class MockForLayers : public MockXRDeviceHookBase {
 public:
  void ProcessSubmittedFrameUnlocked(
      const std::vector<device::ViewData>& views,
      const std::vector<device::LayerData>& layers) final;
  void VerifyFrame(const std::vector<device::ViewData>& expected_views,
                   const std::vector<device::LayerData>& expected_layers);

 private:
  base::Lock lock_;
  std::vector<device::ViewData> last_submitted_views_ GUARDED_BY(lock_);
  std::vector<device::LayerData> last_submitted_layers_ GUARDED_BY(lock_);
};

void MockForLayers::ProcessSubmittedFrameUnlocked(
    const std::vector<device::ViewData>& views,
    const std::vector<device::LayerData>& layers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(mock_device_sequence_);
  base::AutoLock lock(lock_);
  last_submitted_views_ = views;
  last_submitted_layers_ = layers;
}

void MockForLayers::VerifyFrame(
    const std::vector<device::ViewData>& expected_views,
    const std::vector<device::LayerData>& expected_layers) {
  base::AutoLock lock(lock_);
  ASSERT_EQ(expected_views.size(), last_submitted_views_.size());
  for (size_t i = 0; i < expected_views.size(); ++i) {
    LOG(INFO) << "Verifying view " << i;
    EXPECT_EQ(expected_views[i].color.r, last_submitted_views_[i].color.r);
    EXPECT_EQ(expected_views[i].color.g, last_submitted_views_[i].color.g);
    EXPECT_EQ(expected_views[i].color.b, last_submitted_views_[i].color.b);
    EXPECT_EQ(expected_views[i].color.a, last_submitted_views_[i].color.a);
    EXPECT_EQ(expected_views[i].eye, last_submitted_views_[i].eye);
    EXPECT_EQ(expected_views[i].viewport, last_submitted_views_[i].viewport);
  }

  ASSERT_EQ(expected_layers.size(), last_submitted_layers_.size());
  for (size_t i = 0; i < expected_layers.size(); ++i) {
    LOG(INFO) << "Verifying layer " << i;
    EXPECT_EQ(expected_layers[i].type, last_submitted_layers_[i].type);
    ASSERT_EQ(expected_layers[i].face_colors.size(),
              last_submitted_layers_[i].face_colors.size());
    for (size_t j = 0; j < expected_layers[i].face_colors.size(); ++j) {
      LOG(INFO) << "Verifying face " << j;
      EXPECT_EQ(expected_layers[i].face_colors[j].r,
                last_submitted_layers_[i].face_colors[j].r);
      EXPECT_EQ(expected_layers[i].face_colors[j].g,
                last_submitted_layers_[i].face_colors[j].g);
      EXPECT_EQ(expected_layers[i].face_colors[j].b,
                last_submitted_layers_[i].face_colors[j].b);
      EXPECT_EQ(expected_layers[i].face_colors[j].a,
                last_submitted_layers_[i].face_colors[j].a);
    }
  }
}
}  // namespace

// Test all kinds of layers in WebXR. This test requests the 'layers' feature.
WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestLayers) {
  UiUtils::DisableOverlayForTesting();
  MockForLayers mock;

  t->LoadFileAndAwaitInitialization("test_openxr_layers");
  t->EnterSessionWithUserGestureOrFail();

  t->WaitOnJavaScriptStep();
  t->AssertNoJavaScriptErrors();

  t->EndTest();

  mock.WaitForTotalFrameCount(1);

  // See test_openxr_layers.html for the constants that should be used here.
  // The order of layers should also match the order there.
  constexpr device::Color red = {255, 0, 0, 255};
  constexpr device::Color green = {0, 255, 0, 255};
  constexpr device::Color blue = {0, 0, 255, 255};
  constexpr device::Color yellow = {255, 255, 0, 255};
  constexpr device::Color half_yellow = {128, 128, 0, 128};
  constexpr device::Color cyan = {0, 255, 255, 255};
  constexpr device::Color magenta = {255, 0, 255, 255};

  // See device/vr/openxr/test/openxr_test_helper.h.
  constexpr uint32_t view_dimension = 128;

  std::vector<device::ViewData> expected_views;
  expected_views.push_back(
      {.color = red,
       .eye = device::XrEye::kLeft,
       .viewport = {0, 0, view_dimension, view_dimension}});
  expected_views.push_back(
      {.color = red,
       .eye = device::XrEye::kRight,
       .viewport = {view_dimension, 0, view_dimension, view_dimension}});

  std::vector<device::LayerData> expected_layers;
  // The quad layer has a left-right layout, so we expect 2 entries
  // with different colors.
  expected_layers.emplace_back(device::LayerType::kQuad);
  expected_layers.back().face_colors.push_back(green);
  expected_layers.emplace_back(device::LayerType::kQuad);
  expected_layers.back().face_colors.push_back(cyan);

  // The cylinder layer has a stereo layout, so we expect 2 entries
  // with different colors.
  expected_layers.emplace_back(device::LayerType::kCylinder);
  expected_layers.back().face_colors.push_back(blue);
  expected_layers.emplace_back(device::LayerType::kCylinder);
  expected_layers.back().face_colors.push_back(magenta);

  // The equirect layer has 0.5 opacity.
  expected_layers.emplace_back(device::LayerType::kEquirect);
  expected_layers.back().face_colors.push_back(half_yellow);

  expected_layers.emplace_back(device::LayerType::kCube);
  expected_layers.back().face_colors.push_back(red);
  expected_layers.back().face_colors.push_back(cyan);
  expected_layers.back().face_colors.push_back(green);
  expected_layers.back().face_colors.push_back(magenta);
  expected_layers.back().face_colors.push_back(blue);
  expected_layers.back().face_colors.push_back(yellow);

  mock.VerifyFrame(expected_views, expected_layers);
}

}  // namespace vr

#endif  // BUILDFLAG(ENABLE_OPENXR)
