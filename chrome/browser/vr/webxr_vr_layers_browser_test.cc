// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/no_destructor.h"
#include "chrome/browser/vr/test/mock_xr_device_hook_base.h"
#include "chrome/browser/vr/test/multi_class_browser_test.h"
#include "chrome/browser/vr/test/ui_utils.h"
#include "chrome/browser/vr/test/webxr_vr_browser_test.h"
#include "device/vr/buildflags/buildflags.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gl/gl_switches.h"

#if BUILDFLAG(ENABLE_OPENXR)

namespace vr {

namespace {

constexpr int kColorTolerance = 10;

class MockForLayers : public MockXRDeviceHookBase {
 public:
  MockForLayers() = default;

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
    EXPECT_EQ(expected_views[i].color, last_submitted_views_[i].color);
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
      // Use a tolerance for layer color matching as color space conversions,
      // alpha blending, video/media YUV conversions, and texture samplers can
      // introduce slight rounding variations across GPU drivers and decoders.
      EXPECT_NEAR(
          static_cast<int>(SkColorGetR(expected_layers[i].face_colors[j])),
          static_cast<int>(
              SkColorGetR(last_submitted_layers_[i].face_colors[j])),
          kColorTolerance);
      EXPECT_NEAR(
          static_cast<int>(SkColorGetG(expected_layers[i].face_colors[j])),
          static_cast<int>(
              SkColorGetG(last_submitted_layers_[i].face_colors[j])),
          kColorTolerance);
      EXPECT_NEAR(
          static_cast<int>(SkColorGetB(expected_layers[i].face_colors[j])),
          static_cast<int>(
              SkColorGetB(last_submitted_layers_[i].face_colors[j])),
          kColorTolerance);
      EXPECT_NEAR(
          static_cast<int>(SkColorGetA(expected_layers[i].face_colors[j])),
          static_cast<int>(
              SkColorGetA(last_submitted_layers_[i].face_colors[j])),
          kColorTolerance);
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

  mock.WaitForTotalFrameCount(1);

  // See device/vr/openxr/test/openxr_test_helper.h.
  constexpr uint32_t view_dimension = 128;

  std::vector<device::ViewData> expected_views;
  expected_views.push_back(
      {.color = SK_ColorRED,
       .eye = device::mojom::XREye::kLeft,
       .viewport = {0, 0, view_dimension, view_dimension}});
  expected_views.push_back(
      {.color = SK_ColorRED,
       .eye = device::mojom::XREye::kRight,
       .viewport = {view_dimension, 0, view_dimension, view_dimension}});

  // The order of layers should match the order in test_openxr_layers.html.
  std::vector<device::LayerData> expected_layers;
  // The quad layer has a left-right layout, so we expect 2 entries
  // with different colors.
  expected_layers.emplace_back(device::LayerType::kQuad);
  expected_layers.back().face_colors.push_back(SK_ColorGREEN);
  expected_layers.emplace_back(device::LayerType::kQuad);
  expected_layers.back().face_colors.push_back(SK_ColorCYAN);

  // The cylinder layer has a stereo layout, so we expect 2 entries
  // with different colors.
  expected_layers.emplace_back(device::LayerType::kCylinder);
  expected_layers.back().face_colors.push_back(SK_ColorBLUE);
  expected_layers.emplace_back(device::LayerType::kCylinder);
  expected_layers.back().face_colors.push_back(SK_ColorMAGENTA);

  // The equirect layer has 0.5 opacity (half alpha and half intensity).
  expected_layers.emplace_back(device::LayerType::kEquirect);
  if (base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kUseCmdDecoder) == gl::kCmdDecoderValidatingName) {
    // When using the validating command decoder on OpenGLES with sRGB swapchain
    // formats, linear 0.5 intensity maps to sRGB (~192).
    expected_layers.back().face_colors.push_back(
        SkColorSetARGB(128, 192, 192, 0));
  } else {
    expected_layers.back().face_colors.push_back(
        SkColorSetARGB(128, 128, 128, 0));
  }

  expected_layers.emplace_back(device::LayerType::kCube);
  expected_layers.back().face_colors.push_back(SK_ColorRED);
  expected_layers.back().face_colors.push_back(SK_ColorCYAN);
  expected_layers.back().face_colors.push_back(SK_ColorGREEN);
  expected_layers.back().face_colors.push_back(SK_ColorMAGENTA);
  expected_layers.back().face_colors.push_back(SK_ColorBLUE);
  expected_layers.back().face_colors.push_back(SK_ColorYELLOW);

  mock.VerifyFrame(expected_views, expected_layers);

  t->EndTest();
}

WEBXR_VR_ALL_RUNTIMES_BROWSER_TEST_F(TestMediaLayers) {
  UiUtils::DisableOverlayForTesting();
  MockForLayers mock;

  t->LoadFileAndAwaitInitialization("test_openxr_media_layers");
  t->EnterSessionWithUserGestureOrFail();

  // Wait for the JS side verification to finish and call done()
  t->WaitOnJavaScriptStep();
  t->AssertNoJavaScriptErrors();

  // Ensure we check at least the first frame that successfully rendered
  mock.WaitForTotalFrameCount(1);

  // See device/vr/openxr/test/openxr_test_helper.h.
  constexpr uint32_t view_dimension = 128;

  std::vector<device::ViewData> expected_views;
  expected_views.push_back(
      {.color = SK_ColorRED,
       .eye = device::mojom::XREye::kLeft,
       .viewport = {0, 0, view_dimension, view_dimension}});
  expected_views.push_back(
      {.color = SK_ColorRED,
       .eye = device::mojom::XREye::kRight,
       .viewport = {view_dimension, 0, view_dimension, view_dimension}});

  std::vector<device::LayerData> expected_layers;
  expected_layers.emplace_back(device::LayerType::kQuad);
  expected_layers.back().face_colors.push_back(SK_ColorGREEN);

  mock.VerifyFrame(expected_views, expected_layers);

  t->EndTest();
}

}  // namespace vr

#endif  // BUILDFLAG(ENABLE_OPENXR)
