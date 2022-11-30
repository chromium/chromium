// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/vector_icon_button.h"

#include "chrome/browser/vr/elements/rect.h"
#include "chrome/browser/vr/elements/ui_element.h"
#include "chrome/browser/vr/elements/ui_element_name.h"
#include "chrome/browser/vr/elements/vector_icon.h"
#include "chrome/browser/vr/ui_scene_constants.h"

#include "ui/gfx/geometry/point_f.h"

namespace vr {

namespace {

constexpr float kDefaultIconScaleFactor = 0.5f;

}  // namespace

VectorIconButton::VectorIconButton(
    base::RepeatingCallback<void()> click_handler,
    const gfx::VectorIcon& icon,
    AudioDelegate* audio_delegate)
    : Button(click_handler, audio_delegate),
      icon_scale_factor_(kDefaultIconScaleFactor) {
  auto vector_icon = std::make_unique<VectorIcon>(512);
  vector_icon->SetType(kTypeButtonForeground);
  vector_icon->SetIcon(icon);
  vector_icon->set_hit_testable(false);
  foreground_ = vector_icon.get();

  background()->AddChild(std::move(vector_icon));
}

VectorIconButton::~VectorIconButton() = default;

void VectorIconButton::SetIcon(const gfx::VectorIcon& icon) {
  foreground_->SetIcon(icon);
}

void VectorIconButton::SetIconScaleFactor(float factor) {
  icon_scale_factor_ = factor;
  OnSetSize(size());
}

void VectorIconButton::SetIconTranslation(float x, float y) {
  foreground_->SetTranslate(x, y, 0);
}

void VectorIconButton::OnStateUpdated() {
  Button::OnStateUpdated();
  foreground_->SetColor(colors().GetForegroundColor(!enabled()));
}

void VectorIconButton::OnSetDrawPhase() {
  Button::OnSetDrawPhase();
  foreground_->SetDrawPhase(draw_phase());
}

void VectorIconButton::OnSetName() {
  Button::OnSetName();
  foreground_->set_owner_name_for_test(name());
}

void VectorIconButton::OnSetSize(const gfx::SizeF& size) {
  Button::OnSetSize(size);
  // Maintain aspect ratio of the icon, even if the button isn't square.
  float new_size = std::min(size.width(), size.height()) * icon_scale_factor_;
  foreground()->SetSize(new_size, new_size);
}

}  // namespace vr
