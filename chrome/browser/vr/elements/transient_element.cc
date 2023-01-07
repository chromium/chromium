// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/elements/transient_element.h"
#include "base/functional/callback_helpers.h"

namespace vr {

TransientElement::TransientElement(const base::TimeDelta& timeout)
    : timeout_(timeout) {
  SetVisibleImmediately(false);
}

TransientElement::~TransientElement() {}

void TransientElement::SetVisible(bool visible) {
  bool will_be_visible = GetTargetOpacity() == opacity_when_visible();
  // We're already at the desired visibility, no-op.
  if (visible == will_be_visible)
    return;

  if (visible)
    Reset();

  super::SetVisible(visible);
}

void TransientElement::SetVisibleImmediately(bool visible) {
  bool will_be_visible = GetTargetOpacity() == opacity_when_visible();
  if (!will_be_visible && visible)
    Reset();

  super::SetVisibleImmediately(visible);
}

void TransientElement::RefreshVisible() {
  // Do nothing if we're not going to be visible.
  if (GetTargetOpacity() != opacity_when_visible())
    return;
  Reset();
}

void TransientElement::Reset() {
  set_visible_time_ = base::TimeTicks();
}

SimpleTransientElement::SimpleTransientElement(const base::TimeDelta& timeout)
    : super(timeout) {}

SimpleTransientElement::~SimpleTransientElement() {}

bool SimpleTransientElement::OnBeginFrame(const gfx::Transform& head_pose) {
  // Do nothing if we're not going to be visible.
  if (GetTargetOpacity() != opacity_when_visible())
    return false;

  // SetVisible may have been called during initialization which means that the
  // last frame time would be zero.
  if (set_visible_time_.is_null() && opacity() > 0.0f)
    set_visible_time_ = last_frame_time();

  base::TimeDelta duration = last_frame_time() - set_visible_time_;

  if (!set_visible_time_.is_null() && duration >= timeout_) {
    super::SetVisible(false);
    return true;
  }
  return false;
}

}  // namespace vr
