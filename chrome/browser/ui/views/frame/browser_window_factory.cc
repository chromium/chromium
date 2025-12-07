// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "build/build_config.h"
#include "chrome/browser/ui/browser_window_deleter.h"
#include "chrome/browser/ui/views/frame/browser_native_widget_factory.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_widget.h"
#include "chrome/browser/ui/webui_browser/webui_browser.h"
#include "chrome/browser/ui/webui_browser/webui_browser_window.h"
#include "chrome/grit/branded_strings.h"
#include "components/safe_browsing/core/browser/password_protection/metrics_util.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/widget/widget.h"

#if defined(USE_AURA)
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_occlusion_tracker.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/views/frame/browser_view_ash.h"
#include "chromeos/components/kiosk/kiosk_utils.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/common/chrome_features.h"
#endif

// static
std::unique_ptr<BrowserWindow, BrowserWindowDeleter>
BrowserWindow::CreateBrowserWindow(Browser* browser,
                                   bool user_gesture,
                                   bool in_tab_dragging) {
  if (webui_browser::IsWebUIBrowserEnabled() && browser->is_type_normal()) {
    return std::unique_ptr<BrowserWindow, BrowserWindowDeleter>(
        new WebUIBrowserWindow(browser));
  }

#if defined(USE_AURA)
  // Avoid generating too many occlusion tracking calculation events before this
  // function returns. The occlusion status will be computed only once once this
  // function returns.
  // See crbug.com/1183894#c4
  aura::WindowOcclusionTracker::ScopedPause pause_occlusion;
#endif
  // Create the view and the frame. The frame will attach itself via the view
  // so we don't need to do anything with the pointer.
  BrowserView* view = nullptr;
#if BUILDFLAG(IS_CHROMEOS)
  view = new BrowserViewAsh(browser);
#else
  view = new BrowserView(browser);
#endif
  auto browser_widget = std::make_unique<BrowserWidget>(view);
  view->set_browser_widget(std::move(browser_widget));
  if (in_tab_dragging) {
    view->browser_widget()->SetTabDragKind(TabDragKind::kAllTabs);
  }
  view->browser_widget()->InitBrowserWidget();

#if BUILDFLAG(IS_MAC)
  if (view->UsesImmersiveFullscreenMode()) {
    // This needs to happen after BrowserWidget has been initialized. It creates
    // a new Widget that copies the theme from BrowserWidget.
    view->CreateMacOverlayView();
  }
#endif  // IS_MAC

#if defined(USE_AURA)
  // For now, all browser windows are true. This only works when USE_AURA
  // because it requires gfx::NativeWindow to be an aura::Window*.
  view->GetWidget()->GetNativeWindow()->SetProperty(
      aura::client::kCreatedByUserGesture, user_gesture);
#endif
#if BUILDFLAG(IS_CHROMEOS)
  if (chromeos::IsKioskSession()) {
    view->SetForceFullscreen(true);
  }
#endif

  return std::unique_ptr<BrowserWindow, BrowserWindowDeleter>(view);
}
