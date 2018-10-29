// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/immersive_context_mus.h"

#include "ash/public/cpp/immersive/immersive_fullscreen_controller.h"
#include "ash/public/cpp/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/views/mus/mus_client.h"
#include "ui/views/widget/desktop_aura/desktop_capture_client.h"

// static
ImmersiveContextMus* ImmersiveContextMus::instance_ = nullptr;

ImmersiveContextMus::ImmersiveContextMus() {
  DCHECK(!instance_);
  instance_ = this;
}

ImmersiveContextMus::~ImmersiveContextMus() {
  DCHECK_EQ(instance_, this);
  instance_ = nullptr;
}

void ImmersiveContextMus::OnEnteringOrExitingImmersive(
    ash::ImmersiveFullscreenController* controller,
    bool entering) {
  aura::Window* window =
      controller->widget()->GetNativeWindow()->GetRootWindow();
  // Auto hide the shelf in immersive fullscreen instead of hiding it.
  window->SetProperty(ash::kHideShelfWhenFullscreenKey, !entering);
  // Update the window's immersive mode state for the window manager.
  window->SetProperty(aura::client::kImmersiveFullscreenKey, entering);
}

gfx::Rect ImmersiveContextMus::GetDisplayBoundsInScreen(views::Widget* widget) {
  return widget->GetWindowBoundsInScreen();
}

bool ImmersiveContextMus::DoesAnyWindowHaveCapture() {
  return views::DesktopCaptureClient::GetCaptureWindowGlobal() != nullptr;
}

bool ImmersiveContextMus::IsMouseEventsEnabled() {
  // TODO: http://crbug.com/640374.
  NOTIMPLEMENTED();
  return true;
}
