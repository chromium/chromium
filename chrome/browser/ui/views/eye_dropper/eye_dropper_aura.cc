// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "components/eye_dropper/eye_dropper_view.h"
#include "content/public/browser/eye_dropper.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/ui_base_features.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/public/cpp/shell_window_ids.h"
#include "ui/aura/window.h"
#endif

std::unique_ptr<content::EyeDropper> ShowEyeDropper(
    content::RenderFrameHost* frame,
    content::EyeDropperListener* listener) {
  if (!features::IsEyeDropperEnabled() || !frame->GetView()->HasFocus()) {
    return nullptr;
  }

  auto* web_contents = content::WebContents::FromRenderFrameHost(frame);
  auto* parent = web_contents->GetNativeView();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Parent on a top-level container to allow moving between displays.
  parent =
      parent->GetRootWindow()->GetChildById(ash::kShellWindowId_MenuContainer);
#endif
  return std::make_unique<eye_dropper::EyeDropperView>(
      parent, web_contents->GetNativeView(), listener);
}
