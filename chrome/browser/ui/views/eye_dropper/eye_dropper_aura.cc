// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "build/build_config.h"
#include "build/config/linux/dbus/buildflags.h"
#include "components/eye_dropper/eye_dropper_view.h"
#include "content/public/browser/eye_dropper.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/ui_base_features.h"

#if BUILDFLAG(IS_LINUX)
#include "base/environment.h"
#include "base/nix/xdg_util.h"
#if BUILDFLAG(USE_DBUS)
#include "chrome/browser/ui/views/eye_dropper/eye_dropper_portal.h"
#endif
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/public/cpp/shell_window_ids.h"
#include "ui/aura/window.h"
#endif

std::unique_ptr<content::EyeDropper> ShowEyeDropper(
    content::RenderFrameHost* frame,
    content::EyeDropperListener* listener) {
  if (!features::IsEyeDropperEnabled() || !frame->GetView()->HasFocus()) {
    return nullptr;
  }

#if BUILDFLAG(IS_LINUX)
  // Check the session type from the environment variable (XDG_SESSION_TYPE)
  // instead of the Ozone platform, because XWayland sessions still require
  // the portal eye dropper for reliable screen capture.
  base::Environment env;
  if (base::nix::GetSessionType(env) == base::nix::SessionType::kWayland) {
#if BUILDFLAG(USE_DBUS)
    return EyeDropperPortal::Create(frame, listener);
#else
    return nullptr;
#endif
  }
#endif

  auto* web_contents = content::WebContents::FromRenderFrameHost(frame);
  auto* parent = web_contents->GetNativeView();
#if BUILDFLAG(IS_CHROMEOS)
  // Parent on a top-level container to allow moving between displays.
  parent =
      parent->GetRootWindow()->GetChildById(ash::kShellWindowId_MenuContainer);
#endif
  return std::make_unique<eye_dropper::EyeDropperView>(
      parent, web_contents->GetNativeView(), listener);
}
