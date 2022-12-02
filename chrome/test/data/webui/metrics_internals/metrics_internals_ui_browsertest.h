// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_DATA_WEBUI_METRICS_INTERNALS_METRICS_INTERNALS_UI_BROWSERTEST_H_
#define CHROME_TEST_DATA_WEBUI_METRICS_INTERNALS_METRICS_INTERNALS_UI_BROWSERTEST_H_

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/web_ui_browser_test.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents_observer.h"

// Test fixture used in JS browser tests that closes a log upon navigating to
// chrome://metrics-internals.
class MetricsInternalsUIBrowserTest : public WebUIBrowserTest,
                                      public content::WebContentsObserver {
 public:
  MetricsInternalsUIBrowserTest() = default;
  ~MetricsInternalsUIBrowserTest() override = default;

  // WebUIBrowserTest:
  void SetUp() override;
  void SetUpOnMainThread() override;

 private:
  // content::WebContentsObserver:
  void DOMContentLoaded(content::RenderFrameHost* render_frame_host) override;

  bool metrics_enabled_ = true;

  base::test::ScopedFeatureList feature_list_;
};

#endif  // CHROME_TEST_DATA_WEBUI_METRICS_INTERNALS_METRICS_INTERNALS_UI_BROWSERTEST_H_
