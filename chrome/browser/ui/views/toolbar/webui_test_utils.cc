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
#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/interaction/element_tracker_views.h"

void WaitUntilInitialWebUIPaintAndFlushMetricsForTesting(
    BrowserWindowInterface* browser) {
  if (!browser || !features::IsWebUIReloadButtonEnabled()) {
    return;
  }

  base::RunLoop run_loop;
  BrowserElements* browser_elements = BrowserElements::From(browser);
  if (!browser_elements) {
    return;
  }
  ui::TrackedElement* element =
      browser_elements->GetElement(kReloadButtonElementId);
  if (!element) {
    return;
  }
  WebUIToolbarWebView* reload_button = views::AsViewClass<WebUIToolbarWebView>(
      element->AsA<views::TrackedElementViews>()->view());
  if (!reload_button) {
    return;
  }

  reload_button->SetDidFirstNonEmptyPaintCallbackForTesting(
      run_loop.QuitClosure());
  run_loop.Run();

  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();
}
