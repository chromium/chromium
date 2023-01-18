// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/content_element.h"

#include "chrome/browser/vr/content_input_delegate.h"
#include "chrome/browser/vr/model/text_input_info.h"
#include "chrome/browser/vr/text_input_delegate.h"
#include "chrome/browser/vr/ui_element_renderer.h"
#include "chrome/browser/vr/ui_scene_constants.h"
#include "chrome/browser/vr/vr_geometry_util.h"
#include "ui/gfx/geometry/rect_f.h"

namespace vr {

namespace {

// If the screen space bounds or the aspect ratio of the content quad change
// beyond these thresholds we propagate the new content bounds so that the
// content's resolution can be adjusted.
static constexpr float kContentBoundsPropagationThreshold = 0.2f;
// Changes of the aspect ratio lead to a
// distorted content much more quickly. Thus, have a smaller threshold here.
static constexpr float kContentAspectRatioPropagationThreshold = 0.01f;

gfx::Vector3dF GetNormalFromTransform(const gfx::Transform& transform) {
  gfx::Vector3dF x_axis = transform.MapVector(gfx::Vector3dF(1, 0, 0));
  gfx::Vector3dF y_axis = transform.MapVector(gfx::Vector3dF(0, 1, 0));
  gfx::Vector3dF normal = CrossProduct(x_axis, y_axis);
  normal.GetNormalized(&normal);
  return normal;
}

}  // namespace

ContentElement::ContentElement(ContentInputDelegate* delegate)
    : content_delegate_(delegate) {
  DCHECK(delegate);
  SetDelegate(delegate);
}

ContentElement::~ContentElement() = default;

void ContentElement::Render(UiElementRenderer* renderer,
                            const CameraModel& model) const {
  if (uses_quad_layer_) {
    renderer->DrawTexturedQuad(0, 0, texture_location(),
                               model.view_proj_matrix * world_space_transform(),
                               GetClipRect(), computed_opacity(), size(),
                               corner_radius(), false);
    return;
  }

  unsigned int overlay_texture_id =
      overlay_texture_non_empty_ ? overlay_texture_id_ : 0;
  if (texture_id() || overlay_texture_id) {
    renderer->DrawTexturedQuad(
        texture_id(), overlay_texture_id, texture_location(),
        model.view_proj_matrix * world_space_transform(), GetClipRect(),
        computed_opacity(), size(), corner_radius(), true);
  }
}

void ContentElement::OnFocusChanged(bool focused) {
  if (content_delegate_)
    content_delegate_->OnFocusChanged(focused);

  focused_ = focused;
  if (event_handlers_.focus_change)
    event_handlers_.focus_change.Run(focused);
}

void ContentElement::OnInputEdited(const EditedText& info) {
  if (content_delegate_)
    content_delegate_->OnWebInputEdited(info, false);
}

void ContentElement::OnInputCommitted(const EditedText& info) {
  if (content_delegate_)
    content_delegate_->OnWebInputEdited(info, true);
}

void ContentElement::SetOverlayTextureId(unsigned int texture_id) {
  overlay_texture_id_ = texture_id;
}

void ContentElement::SetOverlayTextureLocation(GlTextureLocation location) {
  overlay_texture_location_ = location;
}

void ContentElement::SetOverlayTextureEmpty(bool empty) {
  overlay_texture_non_empty_ = !empty;
}

bool ContentElement::GetOverlayTextureEmpty() {
  return !overlay_texture_non_empty_;
}

void ContentElement::SetProjectionMatrix(const gfx::Transform& matrix) {
  projection_matrix_ = matrix;
}

void ContentElement::SetTextInputDelegate(
    TextInputDelegate* text_input_delegate) {
  text_input_delegate_ = text_input_delegate;
}

void ContentElement::RequestFocus() {
  if (!text_input_delegate_)
    return;

  text_input_delegate_->RequestFocus(id());
}

void ContentElement::RequestUnfocus() {
  if (!text_input_delegate_)
    return;

  text_input_delegate_->RequestUnfocus(id());
}

void ContentElement::UpdateInput(const EditedText& info) {
  if (text_input_delegate_ && focused_)
    text_input_delegate_->UpdateInput(info.current);
}

void ContentElement::OnSizeAnimated(const gfx::SizeF& size,
                                    int target_property_id,
                                    gfx::KeyframeModel* animation) {
  if (target_property_id == BOUNDS && on_size_changed_callback_) {
    on_size_changed_callback_.Run(size);
  }
  UiElement::OnSizeAnimated(size, target_property_id, animation);
}

bool ContentElement::OnBeginFrame(const gfx::Transform& head_pose) {
  // TODO(mthiesse): This projection matrix is always going to be a frame
  // behind when computing the content size. We'll need to address this somehow
  // when we allow content resizing, or we could end up triggering an extra
  // incorrect resize.
  if (projection_matrix_.IsIdentity())
    return false;

  // Determine if the projected size of the content quad changed more than a
  // given threshold. If so, propagate this info so that the content's
  // resolution and size can be adjusted. For the calculation, we cannot take
  // the content quad's actual size (main_content_->size()) if this property
  // is animated. If we took the actual size during an animation we would
  // surpass the threshold with differing projected sizes and aspect ratios
  // (depending on the animation's timing). The differing values may cause
  // visual artefacts if, for instance, the fullscreen aspect ratio is not 16:9.
  // As a workaround, take the final size of the content quad after the
  // animation as the basis for the calculation.
  gfx::SizeF target_size = GetTargetSize();
  // We take the target transform in case the content quad's parent's translate
  // is animated. This approach only works with the current scene hierarchy and
  // set of animated properties.
  gfx::Transform target_transform = ComputeTargetWorldSpaceTransform();

  gfx::Point3F target_center = target_transform.MapPoint(gfx::Point3F());
  gfx::Vector3dF target_normal = GetNormalFromTransform(target_transform);
  float distance = gfx::DotProduct(target_center - kOrigin, -target_normal);
  gfx::SizeF screen_size =
      CalculateScreenSize(projection_matrix_, distance, target_size);

  float aspect_ratio = target_size.width() / target_size.height();
  gfx::SizeF screen_bounds;
  if (screen_size.width() < screen_size.height() * aspect_ratio) {
    screen_bounds.set_width(screen_size.height() * aspect_ratio);
    screen_bounds.set_height(screen_size.height());
  } else {
    screen_bounds.set_width(screen_size.width());
    screen_bounds.set_height(screen_size.width() / aspect_ratio);
  }

  if (std::abs(screen_bounds.width() - last_content_screen_bounds_.width()) >
          kContentBoundsPropagationThreshold ||
      std::abs(screen_bounds.height() - last_content_screen_bounds_.height()) >
          kContentBoundsPropagationThreshold ||
      std::abs(aspect_ratio - last_content_aspect_ratio_) >
          kContentAspectRatioPropagationThreshold) {
    last_content_screen_bounds_.set_width(screen_bounds.width());
    last_content_screen_bounds_.set_height(screen_bounds.height());
    last_content_aspect_ratio_ = aspect_ratio;
    return true;
  }
  return false;
}

void ContentElement::SetUsesQuadLayer(bool uses_quad_layer) {
  uses_quad_layer_ = uses_quad_layer;
}

}  // namespace vr
