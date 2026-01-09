// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_test_utils.h"

#include "base/run_loop.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/reload_button_web_view.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/element_tracker_views.h"

void WaitUntilInitialWebUIPaintForTesting(BrowserWindowInterface* browser) {
  if (!browser || !features::IsWebUIReloadButtonEnabled()) {
    return;
  }

  auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser);
  if (!browser_view) {
    return;
  }

  base::RunLoop run_loop;
  ui::TrackedElement* element =
      BrowserElements::From(browser)->GetElement(kReloadButtonElementId);
  ReloadButtonWebView* reload_button = views::AsViewClass<ReloadButtonWebView>(
      element->AsA<views::TrackedElementViews>()->view());
  if (!reload_button) {
    return;
  }

  reload_button->SetDidFirstNonEmptyPaintCallbackForTesting(
      run_loop.QuitClosure());
  run_loop.Run();
}
