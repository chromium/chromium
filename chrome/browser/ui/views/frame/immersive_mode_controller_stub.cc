// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/immersive_mode_controller_stub.h"

#include "base/notreached.h"

void ImmersiveModeControllerStub::Init(BrowserView* browser_view) {
}

void ImmersiveModeControllerStub::SetEnabled(bool enabled) {
  NOTREACHED();
}

bool ImmersiveModeControllerStub::IsEnabled() const {
  return false;
}

bool ImmersiveModeControllerStub::ShouldHideTopViews() const {
  return false;
}

bool ImmersiveModeControllerStub::IsRevealed() const {
  return false;
}

int ImmersiveModeControllerStub::GetTopContainerVerticalOffset(
    const gfx::Size& top_container_size) const {
  return 0;
}

std::unique_ptr<ImmersiveRevealedLock>
ImmersiveModeControllerStub::GetRevealedLock(AnimateReveal animate_reveal) {
  return nullptr;
}

void ImmersiveModeControllerStub::OnFindBarVisibleBoundsChanged(
    const gfx::Rect& new_visible_bounds_in_screen) {
}

bool ImmersiveModeControllerStub::ShouldStayImmersiveAfterExitingFullscreen() {
  return false;
}

void ImmersiveModeControllerStub::OnWidgetActivationChanged(
    views::Widget* widget,
    bool active) {}

int ImmersiveModeControllerStub::GetMinimumContentOffset() const {
  return 0;
}

int ImmersiveModeControllerStub::GetExtraInfobarOffset() const {
  return 0;
}

void ImmersiveModeControllerStub::OnContentFullscreenChanged(
    bool is_content_fullscreen) {}
