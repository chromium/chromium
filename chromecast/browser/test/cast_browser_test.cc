// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/test/cast_browser_test.h"

#include "base/check_op.h"
#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/uuid.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/browser/cast_browser_context.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/cast_content_window.h"
#include "chromecast/browser/cast_web_service.h"
#include "chromecast/cast_core/cast_core_switches.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/media_playback_renderer_type.mojom.h"
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
  command_line->AppendSwitchASCII(switches::kTestType, "browser");
  command_line->AppendSwitchASCII(
      cast::core::kCastCoreRuntimeIdSwitch,
      base::Uuid::GenerateRandomV4().AsLowercaseString());
  command_line->AppendSwitchASCII(
      cast::core::kRuntimeServicePathSwitch,
      "unix:/tmp/runtime-service.sock." +
          base::Uuid::GenerateRandomV4().AsLowercaseString());
}

void CastBrowserTest::PreRunTestOnMainThread() {
  // Pump startup related events.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::RunLoop().RunUntilIdle();

  metrics::CastMetricsHelper::GetInstance()->SetDummySessionIdForTesting();
  web_service_ = std::make_unique<CastWebService>(
      CastBrowserProcess::GetInstance()->browser_context(),
      nullptr /* window_manager */);
}

void CastBrowserTest::PostRunTestOnMainThread() {
  cast_web_view_.reset();
}

content::WebContents* CastBrowserTest::CreateWebView() {
  ::chromecast::mojom::CastWebViewParamsPtr params =
      ::chromecast::mojom::CastWebViewParams::New();
  // MOJO_RENDERER is CMA renderer on Chromecast
  params->renderer_type = ::chromecast::mojom::RendererType::MOJO_RENDERER;
  params->enabled_for_dev = true;
  params->log_js_console_messages = true;
  cast_web_view_ = web_service_->CreateWebViewInternal(std::move(params));

  return cast_web_view_->web_contents();
}

content::WebContents* CastBrowserTest::NavigateToURL(const GURL& url) {
  content::WebContents* web_contents =
      cast_web_view_ ? cast_web_view_->web_contents() : CreateWebView();

  content::WaitForLoadStop(web_contents);
  content::TestNavigationObserver same_tab_observer(web_contents, 1);

  cast_web_view_->cast_web_contents()->LoadUrl(url);

  same_tab_observer.Wait();

  return web_contents;
}

}  // namespace shell
}  // namespace chromecast
