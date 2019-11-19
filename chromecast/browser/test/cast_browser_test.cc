// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/test/cast_browser_test.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/browser/cast_browser_context.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/cast_content_window.h"
#include "chromecast/browser/cast_web_service.h"
#include "chromecast/browser/cast_web_view_factory.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"

namespace chromecast {
namespace shell {

CastBrowserTest::CastBrowserTest() {}

CastBrowserTest::~CastBrowserTest() {}

void CastBrowserTest::SetUp() {
  SetUpCommandLine(base::CommandLine::ForCurrentProcess());

  BrowserTestBase::SetUp();
}

void CastBrowserTest::SetUpCommandLine(base::CommandLine* command_line) {
  command_line->AppendSwitch(switches::kNoWifi);
  command_line->AppendSwitchASCII(switches::kTestType, "browser");
}

void CastBrowserTest::PreRunTestOnMainThread() {
  // Pump startup related events.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::RunLoop().RunUntilIdle();

  metrics::CastMetricsHelper::GetInstance()->SetDummySessionIdForTesting();
  web_view_factory_ = std::make_unique<CastWebViewFactory>(
      CastBrowserProcess::GetInstance()->browser_context());
  web_service_ = std::make_unique<CastWebService>(
      CastBrowserProcess::GetInstance()->browser_context(),
      web_view_factory_.get(), nullptr /* window_manager */);
}

void CastBrowserTest::PostRunTestOnMainThread() {
  cast_web_view_.reset();
}

content::WebContents* CastBrowserTest::CreateWebView() {
  CastWebView::CreateParams params;
  params.delegate = weak_factory_.GetWeakPtr();
  params.web_contents_params.delegate = weak_factory_.GetWeakPtr();
  params.web_contents_params.use_cma_renderer = true;
  params.web_contents_params.enabled_for_dev = true;
  params.window_params.delegate = weak_factory_.GetWeakPtr();
  cast_web_view_ =
      web_service_->CreateWebView(params, nullptr, /* site_instance */
                                  GURL() /* initial_url */);

  return cast_web_view_->web_contents();
}

content::WebContents* CastBrowserTest::NavigateToURL(const GURL& url) {
  content::WebContents* web_contents =
      cast_web_view_ ? cast_web_view_->web_contents() : CreateWebView();

  content::WaitForLoadStop(web_contents);
  content::TestNavigationObserver same_tab_observer(web_contents, 1);

  cast_web_view_->LoadUrl(url);

  same_tab_observer.Wait();

  return web_contents;
}

void CastBrowserTest::OnWindowDestroyed() {}

void CastBrowserTest::OnVisibilityChange(VisibilityType visibility_type) {}

bool CastBrowserTest::CanHandleGesture(GestureType gesture_type) {
  return false;
}

bool CastBrowserTest::ConsumeGesture(GestureType gesture_type) {
  return false;
}

std::string CastBrowserTest::GetId() {
  return "";
}

}  // namespace shell
}  // namespace chromecast
