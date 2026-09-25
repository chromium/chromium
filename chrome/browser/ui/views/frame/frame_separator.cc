// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/frame_separator.h"

#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/property_effects.h"
#include "ui/views/widget/widget.h"

FrameSeparator::FrameSeparator() = default;
FrameSeparator::~FrameSeparator() = default;

std::optional<ui::ColorId> FrameSeparator::GetInactiveColorId() const {
  return inactive_color_id_;
}

void FrameSeparator::SetInactiveColorId(
    std::optional<ui::ColorId> inactive_color_id) {
  if (inactive_color_id_ == inactive_color_id) {
    return;
  }

  inactive_color_id_ = inactive_color_id;
  MaybeUpdateActiveSubscription();
  OnPropertyChanged(&inactive_color_id_, views::PropertyEffects::kPaint);
}

void FrameSeparator::AddedToWidget() {
  View::AddedToWidget();
  MaybeUpdateActiveSubscription();
}

void FrameSeparator::RemovedFromWidget() {
  MaybeUpdateActiveSubscription();
  View::RemovedFromWidget();
}

SkColor FrameSeparator::GetForegroundColor() const {
  if (inactive_color_id_ && !GetWidget()->ShouldPaintAsActive()) {
    return GetColorProvider()->GetColor(*inactive_color_id_);
  }
  return Separator::GetForegroundColor();
}

void FrameSeparator::MaybeUpdateActiveSubscription() {
  if (!inactive_color_id_ || !GetWidget()) {
    window_active_subscription_ = base::CallbackListSubscription();
    return;
  }

  if (window_active_subscription_) {
    return;
  }

  window_active_subscription_ =
      GetWidget()->RegisterPaintAsActiveChangedCallback(base::BindRepeating(
          &Separator::SchedulePaint, base::Unretained(this)));
}

BEGIN_METADATA(FrameSeparator)
ADD_PROPERTY_METADATA(std::optional<ui::ColorId>, InactiveColorId)
END_METADATA
