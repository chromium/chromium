// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/test/ui_pixel_test.h"

#include "base/check.h"
#include "build/build_config.h"
#include "chrome/browser/vr/gl_texture_location.h"
#include "chrome/browser/vr/model/model.h"
#include "chrome/browser/vr/render_info.h"
#include "chrome/browser/vr/test/animation_utils.h"
#include "chrome/browser/vr/test/constants.h"
#include "chrome/browser/vr/text_input_delegate.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkEncodedImageFormat.h"
#include "third_party/skia/include/core/SkImageEncoder.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "third_party/skia/include/core/SkStream.h"

#if BUILDFLAG(IS_WIN)
#include "base/win/windows_version.h"
#endif

namespace vr {

UiPixelTest::UiPixelTest() : frame_buffer_size_(kPixelHalfScreen) {
#if BUILDFLAG(IS_WIN)
  // VR is not supported on Windows 7.
  os_supported_ = base::win::GetVersion() > base::win::Version::WIN7;
#endif
}

UiPixelTest::~UiPixelTest() = default;

void UiPixelTest::SetUp() {
  if (!os_supported_)
    return;
  gl_test_environment_ =
      std::make_unique<GlTestEnvironment>(frame_buffer_size_);

  // Make content texture.
  content_texture_ = gl_test_environment_->CreateTexture(GL_TEXTURE_2D);
  content_overlay_texture_ = gl_test_environment_->CreateTexture(GL_TEXTURE_2D);

  // TODO(tiborg): Make GL_TEXTURE_EXTERNAL_OES texture for content and fill it
  // with fake content.
  ASSERT_EQ(glGetError(), (GLenum)GL_NO_ERROR);

  browser_ = std::make_unique<MockUiBrowserInterface>();
}

void UiPixelTest::TearDown() {
  if (!os_supported_)
    return;
  ui_.reset();
  glDeleteTextures(1, &content_texture_);
  gl_test_environment_.reset();
}

void UiPixelTest::MakeUi(const UiInitialState& ui_initial_state,
                         const LocationBarState& location_bar_state) {
  DCHECK(os_supported_);
  ui_ = std::make_unique<Ui>(browser_.get(), nullptr, nullptr, nullptr, nullptr,
                             ui_initial_state);
  ui_->OnGlInitialized(kGlTextureLocationLocal, content_texture_,
                       content_overlay_texture_, 0);
  ui_->GetBrowserUiWeakPtr()->SetLocationBarState(location_bar_state);
}

void UiPixelTest::DrawUi(const gfx::Vector3dF& laser_direction,
                         const gfx::Point3F& laser_origin,
                         ControllerModel::ButtonState button_state,
                         float controller_opacity,
                         const gfx::Transform& controller_transform,
                         const gfx::Transform& view_matrix,
                         const gfx::Transform& proj_matrix) {
  DCHECK(os_supported_);
  ControllerModel controller_model;
  controller_model.laser_direction = kForwardVector;
  controller_model.transform = controller_transform;
  controller_model.opacity = controller_opacity;
  controller_model.laser_origin = laser_origin;
  controller_model.touchpad_button_state = button_state;
  controller_model.app_button_state = ControllerModel::ButtonState::kUp;
  controller_model.home_button_state = ControllerModel::ButtonState::kUp;
  RenderInfo render_info;
  render_info.head_pose = view_matrix;
  render_info.left_eye_model.view_matrix = view_matrix;
  render_info.left_eye_model.proj_matrix = proj_matrix;
  render_info.left_eye_model.view_proj_matrix = proj_matrix * view_matrix;
  render_info.right_eye_model = render_info.left_eye_model;
  render_info.left_eye_model.viewport = {0, 0, frame_buffer_size_.width(),
                                         frame_buffer_size_.height()};
  render_info.right_eye_model.viewport = {0, 0, 0, 0};

  InputEventList input_event_list;
  ReticleModel reticle_model;
  EXPECT_TRUE(ui_->OnBeginFrame(base::TimeTicks(), render_info.head_pose));
  ui_->HandleInput(gfx::MsToTicks(1), render_info, controller_model,
                   &reticle_model, &input_event_list);
  std::vector<ControllerModel> controllers;
  controllers.push_back(controller_model);
  ui_->OnControllersUpdated(controllers, reticle_model);
  ui_->Draw(render_info);

  // We produce GL errors while rendering. Clear them all so that we can check
  // for errors of subsequent calls.
  // TODO(768905): Fix producing errors while rendering.
  while (glGetError() != GL_NO_ERROR) {
  }
}

std::unique_ptr<SkBitmap> UiPixelTest::SaveCurrentFrameBufferToSkBitmap() {
  DCHECK(os_supported_);
  // Create buffer.
  std::unique_ptr<SkBitmap> bitmap = std::make_unique<SkBitmap>();
  if (!bitmap->tryAllocN32Pixels(frame_buffer_size_.width(),
                                 frame_buffer_size_.height(), false)) {
    return nullptr;
  }
  SkPixmap pixmap;
  if (!bitmap->peekPixels(&pixmap)) {
    return nullptr;
  }

  // Read pixels from frame buffer.
  glReadPixels(0, 0, frame_buffer_size_.width(), frame_buffer_size_.height(),
               GL_RGBA, GL_UNSIGNED_BYTE, pixmap.writable_addr());
  if (glGetError() != GL_NO_ERROR) {
    return nullptr;
  }

  // Flip image vertically since SkBitmap expects the pixels in a different
  // order.
  for (int col = 0; col < frame_buffer_size_.width(); col++) {
    for (int row = 0; row < frame_buffer_size_.height() / 2; row++) {
      std::swap(
          *pixmap.writable_addr32(col, row),
          *pixmap.writable_addr32(col, frame_buffer_size_.height() - row - 1));
    }
  }

  return bitmap;
}

bool UiPixelTest::SaveSkBitmapToPng(const SkBitmap& bitmap,
                                    const std::string& filename) {
  DCHECK(os_supported_);
  SkFILEWStream stream(filename.c_str());
  if (!stream.isValid()) {
    return false;
  }
  if (!SkEncodeImage(&stream, bitmap, SkEncodedImageFormat::kPNG, 100)) {
    return false;
  }
  return true;
}

}  // namespace vr
