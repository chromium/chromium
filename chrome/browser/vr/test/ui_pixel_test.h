// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_TEST_UI_PIXEL_TEST_H_
#define CHROME_BROWSER_VR_TEST_UI_PIXEL_TEST_H_

#include "chrome/browser/vr/model/location_bar_state.h"
#include "chrome/browser/vr/test/gl_test_environment.h"
#include "chrome/browser/vr/test/mock_content_input_delegate.h"
#include "chrome/browser/vr/test/mock_ui_browser_interface.h"
#include "chrome/browser/vr/ui.h"
#include "chrome/browser/vr/ui_input_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"

namespace vr {

class UiPixelTest : public testing::Test {
 public:
  UiPixelTest();
  ~UiPixelTest() override;
  void SetUp() override;
  void TearDown() override;

 protected:
  void MakeUi(const UiInitialState& ui_initial_state,
              const LocationBarState& location_bar_state);
  void DrawUi(const gfx::Vector3dF& laser_direction,
              const gfx::Point3F& laser_origin,
              ControllerModel::ButtonState button_state,
              float controller_opacity,
              const gfx::Transform& controller_transform,
              const gfx::Transform& view_matrix,
              const gfx::Transform& proj_matrix);
  std::unique_ptr<SkBitmap> SaveCurrentFrameBufferToSkBitmap();
  bool SaveSkBitmapToPng(const SkBitmap& bitmap, const std::string& filename);

 private:
  std::unique_ptr<GlTestEnvironment> gl_test_environment_;
  std::unique_ptr<MockUiBrowserInterface> browser_;
  std::unique_ptr<MockContentInputDelegate> content_input_delegate_;
  GLuint content_texture_ = 0;
  GLuint content_overlay_texture_ = 0;
  gfx::Size frame_buffer_size_;
  std::unique_ptr<UiInterface> ui_;

  // Indicates if the test is running on a supported OS level.
  bool os_supported_ = true;
};

}  // namespace vr

#endif  // CHROME_BROWSER_VR_TEST_UI_PIXEL_TEST_H_
