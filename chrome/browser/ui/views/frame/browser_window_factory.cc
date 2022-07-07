// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/views/frame/custom_tab_browser_frame.h"
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/ui/views/frame/browser_frame_lacros.h"
#endif
#include "chrome/browser/ui/views/frame/native_browser_frame_factory.h"
#include "chrome/grit/chromium_strings.h"
#include "components/safe_browsing/core/browser/password_protection/metrics_util.h"
#if defined(USE_AURA)
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_occlusion_tracker.h"
#endif
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"

// static
BrowserWindow* BrowserWindow::CreateBrowserWindow(
    std::unique_ptr<Browser> browser,
    bool user_gesture,
    bool in_tab_dragging) {
#if defined(USE_AURA)
  // Avoid generating too many occlusion tracking calculation events before this
  // function returns. The occlusion status will be computed only once once this
  // function returns.
  // See crbug.com/1183894#c4
  aura::WindowOcclusionTracker::ScopedPause pause_occlusion;
#endif
  // Create the view and the frame. The frame will attach itself via the view
  // so we don't need to do anything with the pointer.
  BrowserView* view = new BrowserView(std::move(browser));
  BrowserFrame* browser_frame = nullptr;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (view->browser()->is_type_custom_tab())
    browser_frame = new CustomTabBrowserFrame(view);
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  browser_frame = new BrowserFrameLacros(view);
#endif
  if (!browser_frame)
    browser_frame = new BrowserFrame(view);
  if (in_tab_dragging)
    browser_frame->SetTabDragKind(TabDragKind::kAllTabs);
  browser_frame->InitBrowserFrame();

  view->GetWidget()->non_client_view()->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));

  if (view->GetIsPictureInPictureType() && view->GetLockAspectRatio()) {
    gfx::SizeF aspect_ratio(view->GetInitialAspectRatio(), 1.0f);
    view->GetWidget()->SetAspectRatio(aspect_ratio);
  }

#if defined(USE_AURA)
  // For now, all browser windows are true. This only works when USE_AURA
  // because it requires gfx::NativeWindow to be an aura::Window*.
  view->GetWidget()->GetNativeWindow()->SetProperty(
      aura::client::kCreatedByUserGesture, user_gesture);
#endif
  if (profiles::IsKioskSession())
    view->SetForceFullscreen(true);

  return view;
}
