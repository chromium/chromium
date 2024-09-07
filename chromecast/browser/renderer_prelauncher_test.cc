// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/renderer_prelauncher.h"

#include <memory>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/base/metrics/cast_metrics_helper.h"
#include "chromecast/browser/cast_browser_context.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/test/cast_browser_test.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"

namespace chromecast {

class RendererPrelauncherTest : public shell::CastBrowserTest {
 protected:
  // CastBrowserTest implementation:
  void PreRunTestOnMainThread() override;
};

void RendererPrelauncherTest::PreRunTestOnMainThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::RunLoop().RunUntilIdle();

  metrics::CastMetricsHelper::GetInstance()->SetDummySessionIdForTesting();
}

IN_PROC_BROWSER_TEST_F(RendererPrelauncherTest, ReusedRenderer) {
  GURL gurl("https://www.google.com/");
  content::BrowserContext* browser_context =
      shell::CastBrowserProcess::GetInstance()->browser_context();
  EXPECT_TRUE(browser_context);

  // Prelaunch a renderer process for the url.
  auto prelauncher =
      std::make_unique<RendererPrelauncher>(browser_context, gurl);
  prelauncher->Prelaunch();
  scoped_refptr<content::SiteInstance> site_instance =
      prelauncher->site_instance();
  EXPECT_TRUE(site_instance->HasProcess());

  // Launch a web contents for the site instance.
  content::WebContents::CreateParams create_params(browser_context, nullptr);
  create_params.site_instance = site_instance;
  std::unique_ptr<content::WebContents> web_contents(
      content::WebContents::Create(create_params));
  EXPECT_EQ(site_instance->GetProcess(),
            web_contents->GetSiteInstance()->GetProcess());

  // Nativgate to the url.
  content::NavigationController::LoadURLParams params(gurl);
  web_contents->GetController().LoadURLWithParams(params);
  EXPECT_EQ(site_instance->GetProcess(),
            web_contents->GetSiteInstance()->GetProcess());

  // Ensure that the renderer process terminates.
  prelauncher.reset();
  web_contents.reset();
  if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSingleProcess)) {
    EXPECT_FALSE(site_instance->HasProcess());
  }
}

}  // namespace chromecast
