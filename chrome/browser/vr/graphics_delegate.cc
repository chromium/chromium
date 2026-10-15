// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/graphics_delegate.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "base/check.h"
#include "base/notimplemented.h"
#include "base/numerics/angle_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/vr/fov_rectangle.h"
#include "chrome/browser/vr/frame_type.h"
#include "chrome/browser/vr/model/camera_model.h"
#include "chrome/browser/vr/render_info.h"
#include "device/vr/public/mojom/vr_service.mojom.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/geometry/transform.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/vr/graphics_delegate_win.h"
#elif BUILDFLAG(IS_ANDROID)
#include "chrome/browser/vr/graphics_delegate_android.h"
#endif

namespace vr {

namespace {
constexpr float kZNear = 0.1f;
constexpr float kZFar = 10000.0f;
constexpr gfx::RectF kFullUvBounds{0.0f, 0.0f, 1.0f, 1.0f};
#if BUILDFLAG(IS_ANDROID)
constexpr int kOverlayResolutionDivisor = 2;
#endif

CameraModel CameraModelViewProjFromXRView(
    const device::mojom::XRViewPtr& view) {
  CameraModel model = {};

  // TODO(crbug.com/40684534): mojo space is currently equivalent to
  // world space, so the view matrix is world_from_view.
  model.view_matrix = view->geometry->mojo_from_view;

  bool is_invertible = model.view_matrix.GetInverse(&model.view_matrix);
  DCHECK(is_invertible);

  const auto& fov = view->geometry->field_of_view;
  float up_tan = tanf(base::DegToRad(fov->up_degrees));
  float left_tan = tanf(base::DegToRad(fov->left_degrees));
  float right_tan = tanf(base::DegToRad(fov->right_degrees));
  float down_tan = tanf(base::DegToRad(fov->down_degrees));
  float x_scale = 2.0f / (left_tan + right_tan);
  float y_scale = 2.0f / (up_tan + down_tan);
  // clang-format off
  gfx::Transform proj_matrix = gfx::Transform::RowMajor(
      x_scale, 0, -((left_tan - right_tan) * x_scale * 0.5), 0,
      0, y_scale, ((up_tan - down_tan) * y_scale * 0.5), 0,
      0, 0, (kZFar + kZNear) / (kZNear - kZFar),
          2 * kZFar * kZNear / (kZNear - kZFar),
      0, 0, -1, 0);
  // clang-format on
  model.view_proj_matrix = proj_matrix * model.view_matrix;
  return model;
}

// Projects `min_fov` (the minimal FOV bounding active overlay elements) into
// `recommended_fov`'s tangent space to produce a normalized [0, 1] UV bounding
// box for a single eye. Returns an empty RectF when no overlay elements are
// visible inside the view frustum.
gfx::RectF ComputeNormalizedFovBounds(
    const device::mojom::VRFieldOfView& recommended_fov,
    const FovRectangle& min_fov) {
  if (min_fov.left + min_fov.right <= 0.0f ||
      min_fov.bottom + min_fov.top <= 0.0f) {
    return gfx::RectF();
  }
  const float recommended_left =
      std::tan(base::DegToRad(recommended_fov.left_degrees));
  const float recommended_right =
      std::tan(base::DegToRad(recommended_fov.right_degrees));
  const float recommended_down =
      std::tan(base::DegToRad(recommended_fov.down_degrees));
  const float recommended_up =
      std::tan(base::DegToRad(recommended_fov.up_degrees));
  const float total_width = recommended_left + recommended_right;
  const float total_height = recommended_down + recommended_up;
  if (total_width <= 0.0f || total_height <= 0.0f) {
    return kFullUvBounds;
  }
  const float min_left = std::tan(base::DegToRad(min_fov.left));
  const float min_right = std::tan(base::DegToRad(min_fov.right));
  const float min_down = std::tan(base::DegToRad(min_fov.bottom));
  const float min_up = std::tan(base::DegToRad(min_fov.top));

  // Small UV margin to prevent linear-filtering edge clipping around the
  // overlay quad border.
  constexpr float kBorderPadding = 0.005f;
  const float x0 = std::clamp(
      (recommended_left - min_left) / total_width - kBorderPadding, 0.0f, 1.0f);
  const float x1 =
      std::clamp((recommended_left + min_right) / total_width + kBorderPadding,
                 0.0f, 1.0f);
  const float y0 =
      std::clamp((recommended_down - min_down) / total_height - kBorderPadding,
                 0.0f, 1.0f);
  const float y1 = std::clamp(
      (recommended_down + min_up) / total_height + kBorderPadding, 0.0f, 1.0f);
  if (x1 <= x0 || y1 <= y0) {
    return gfx::RectF();
  }
  return gfx::RectF(x0, y0, x1 - x0, y1 - y0);
}

}  // namespace

std::unique_ptr<GraphicsDelegate> GraphicsDelegate::Create() {
#if BUILDFLAG(IS_WIN)
  return std::make_unique<GraphicsDelegateWin>();
#elif BUILDFLAG(IS_ANDROID)
  return std::make_unique<GraphicsDelegateAndroid>();
#else
  NOTIMPLEMENTED();
  return nullptr;
#endif
}

GraphicsDelegate::GraphicsDelegate() = default;
GraphicsDelegate::~GraphicsDelegate() = default;

float GraphicsDelegate::GetZNear() {
  return kZNear;
}

void GraphicsDelegate::SetXrViews(
    const std::vector<device::mojom::XRViewPtr>& views) {
  // Store the first left and right views.
  for (const auto& view : views) {
    if (view->eye == device::mojom::XREye::kLeft) {
      left_ = view.Clone();
    } else if (view->eye == device::mojom::XREye::kRight) {
      right_ = view.Clone();
    }
  }

  DCHECK(left_);
  DCHECK(right_);
#if BUILDFLAG(IS_ANDROID)
  // The overlay only renders small 2D UI indicators and is composited with
  // linear filtering, so half-resolution per axis (4x fewer pixels) keeps the
  // UI crisp while cutting GPU fillrate and memory bandwidth by 75%.
  left_->viewport = gfx::Rect(
      0, 0, std::max(1, left_->viewport.width() / kOverlayResolutionDivisor),
      std::max(1, left_->viewport.height() / kOverlayResolutionDivisor));
  right_->viewport = gfx::Rect(
      left_->viewport.width(), 0,
      std::max(1, right_->viewport.width() / kOverlayResolutionDivisor),
      std::max(1, right_->viewport.height() / kOverlayResolutionDivisor));
#endif
  left_bounds_ = kFullUvBounds;
  right_bounds_ = kFullUvBounds;
}

gfx::RectF GraphicsDelegate::GetLeft() {
#if BUILDFLAG(IS_ANDROID)
  return left_bounds_;
#else
  gfx::Size size = GetTextureSize();
  return gfx::RectF(
      0, 0, static_cast<float>(left_->viewport.width()) / size.width(),
      static_cast<float>(left_->viewport.height()) / size.height());
#endif
}

gfx::RectF GraphicsDelegate::GetRight() {
#if BUILDFLAG(IS_ANDROID)
  return right_bounds_;
#else
  gfx::Size size = GetTextureSize();
  return gfx::RectF(
      static_cast<float>(left_->viewport.width()) / size.width(), 0,
      static_cast<float>(right_->viewport.width()) / size.width(),
      static_cast<float>(right_->viewport.height()) / size.height());
#endif
}

FovRectangles GraphicsDelegate::GetRecommendedFovs() {
  DCHECK(left_);
  DCHECK(right_);
  FovRectangle left = {
      left_->geometry->field_of_view->left_degrees,
      left_->geometry->field_of_view->right_degrees,
      left_->geometry->field_of_view->down_degrees,
      left_->geometry->field_of_view->up_degrees,
  };

  FovRectangle right = {
      right_->geometry->field_of_view->left_degrees,
      right_->geometry->field_of_view->right_degrees,
      right_->geometry->field_of_view->down_degrees,
      right_->geometry->field_of_view->up_degrees,
  };

  return std::pair<FovRectangle, FovRectangle>(left, right);
}

RenderInfo GraphicsDelegate::GetRenderInfo(FrameType frame_type,
                                           const gfx::Transform& head_pose) {
  RenderInfo info;
  info.head_pose = head_pose;

  CameraModel left = CameraModelViewProjFromXRView(left_);
  left.eye_type = kLeftEye;
  left.viewport =
      gfx::Rect(0, 0, left_->viewport.width(), left_->viewport.height());
  info.left_eye_model = left;

  CameraModel right = CameraModelViewProjFromXRView(right_);
  right.eye_type = kRightEye;
  right.viewport =
      gfx::Rect(left_->viewport.width(), 0, right_->viewport.width(),
                right_->viewport.height());
  info.right_eye_model = right;
  cached_info_ = info;
  if (frame_type == kWebXrFrame) {
    left_bounds_ = gfx::RectF();
    right_bounds_ = gfx::RectF();
  } else {
    left_bounds_ = kFullUvBounds;
    right_bounds_ = kFullUvBounds;
  }
  return info;
}

RenderInfo GraphicsDelegate::GetOptimizedRenderInfoForFovs(
    const FovRectangles& fovs) {
  left_bounds_ =
      ComputeNormalizedFovBounds(*left_->geometry->field_of_view, fovs.first);
  right_bounds_ =
      ComputeNormalizedFovBounds(*right_->geometry->field_of_view, fovs.second);
  return cached_info_;
}

gfx::Size GraphicsDelegate::GetTextureSize() {
  int width = left_->viewport.width() + right_->viewport.width();
  int height = std::max(left_->viewport.height(), right_->viewport.height());

  return gfx::Size(width, height);
}

}  // namespace vr
