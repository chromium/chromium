// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/button.h"

#include "base/functional/bind.h"
#include "chrome/browser/vr/elements/invisible_hit_target.h"
#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/elements/ui_element_name.h"
#include "chrome/browser/vr/elements/vector_icon.h"
#include "chrome/browser/vr/ui_scene_constants.h"

#include "ui/gfx/geometry/point_f.h"

namespace vr {

namespace {

constexpr float kHitPlaneScaleFactorHovered = 1.2f;
constexpr float kDefaultHoverOffsetDMM = 0.048f;

}  // namespace

Button::Button(base::RepeatingCallback<void()> click_handler,
               AudioDelegate* audio_delegate)
    : click_handler_(click_handler), hover_offset_(kDefaultHoverOffsetDMM) {
  auto background = std::make_unique<Rect>();
  background->SetType(kTypeButtonBackground);
  background->set_bubble_events(true);
  background->set_contributes_to_parent_bounds(false);
  background->SetColor(colors_.background);
  background->SetTransitionedProperties(
      {TRANSFORM, BACKGROUND_COLOR, FOREGROUND_COLOR});
  background_ = background.get();
  AddChild(std::move(background));

  auto hit_plane = std::make_unique<InvisibleHitTarget>();
  hit_plane->SetType(kTypeButtonHitTarget);
  hit_plane->set_focusable(false);
  hit_plane->set_bubble_events(true);
  hit_plane->set_contributes_to_parent_bounds(false);
  hit_plane_ = hit_plane.get();
  background_->AddChild(std::move(hit_plane));

  EventHandlers event_handlers;
  event_handlers.hover_enter =
      base::BindRepeating(&Button::HandleHoverEnter, base::Unretained(this));
  event_handlers.hover_move =
      base::BindRepeating(&Button::HandleHoverMove, base::Unretained(this));
  event_handlers.hover_leave =
      base::BindRepeating(&Button::HandleHoverLeave, base::Unretained(this));
  event_handlers.button_down =
      base::BindRepeating(&Button::HandleButtonDown, base::Unretained(this));
  event_handlers.button_up =
      base::BindRepeating(&Button::HandleButtonUp, base::Unretained(this));
  set_event_handlers(event_handlers);

  Sounds sounds;
  sounds.hover_enter = kSoundButtonHover;
  sounds.button_down = kSoundButtonClick;
  SetSounds(sounds, audio_delegate);

  disabled_sounds_.hover_enter = kSoundNone;
  disabled_sounds_.button_down = kSoundInactiveButtonClick;
}

Button::~Button() = default;

void Button::Render(UiElementRenderer* renderer,
                    const CameraModel& model) const {}

void Button::SetButtonColors(const ButtonColors& colors) {
  colors_ = colors;
  OnStateUpdated();
}

void Button::SetEnabled(bool enabled) {
  enabled_ = enabled;
  OnStateUpdated();
}

void Button::HandleHoverEnter() {
  hovered_ = enabled_;
  OnStateUpdated();
}

void Button::HandleHoverMove(const gfx::PointF& position) {
  hovered_ = hit_plane_->LocalHitTest(position) && enabled_;
  OnStateUpdated();
}

void Button::HandleHoverLeave() {
  hovered_ = false;
  OnStateUpdated();
}

void Button::HandleButtonDown() {
  down_ = enabled_;
  OnStateUpdated();
}

void Button::HandleButtonUp() {
  down_ = false;
  OnStateUpdated();
  if (hovered() && click_handler_)
    click_handler_.Run();
}

void Button::OnSetColors(const ButtonColors& colors) {}

void Button::OnStateUpdated() {
  pressed_ = hovered_ ? down_ : false;
  background_->SetColor(colors_.GetBackgroundColor(hovered_, pressed_));

  OnSetColors(colors_);

  if (hover_offset_ == 0.0f)
    return;

  if (hovered()) {
    background_->SetTranslate(0.0, 0.0, hover_offset_);
    hit_plane_->SetScale(kHitPlaneScaleFactorHovered,
                         kHitPlaneScaleFactorHovered, 1.0f);
  } else {
    background_->SetTranslate(0.0, 0.0, 0.0);
    hit_plane_->SetScale(1.0f, 1.0f, 1.0f);
  }
}

void Button::OnSetDrawPhase() {
  background_->SetDrawPhase(draw_phase());
  hit_plane_->SetDrawPhase(draw_phase());
}

void Button::OnSetName() {
  background_->set_owner_name_for_test(name());
  hit_plane_->set_owner_name_for_test(name());
}

void Button::OnSetSize(const gfx::SizeF& size) {
  if (!background_->contributes_to_parent_bounds()) {
    background_->SetSize(size.width(), size.height());
  }
  hit_plane_->SetSize(size.width(), size.height());
}

void Button::OnSetCornerRadii(const CornerRadii& radii) {
  background_->SetCornerRadii(radii);
  hit_plane_->SetCornerRadii(radii);
}

void Button::OnSizeAnimated(const gfx::SizeF& size,
                            int target_property_id,
                            gfx::KeyframeModel* animation) {
  // We could have OnSetSize called in UiElement's Notify handler instead, but
  // this may have expensive implications (such as regenerating textures on
  // every frame of an animation).  For now, keep this elements-specific.
  if (target_property_id == BOUNDS) {
    OnSetSize(size);
  }
  UiElement::OnSizeAnimated(size, target_property_id, animation);
}

const Sounds& Button::GetSounds() const {
  if (!enabled()) {
    return disabled_sounds_;
  }
  return UiElement::GetSounds();
}

}  // namespace vr
