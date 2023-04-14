// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_H_

#include "build/build_config.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/test_renderer_host.h"

// Consider to implement web app specific test harness independent of
// RenderViewHost.
class WebAppTest : public content::RenderViewHostTestHarness {
 public:
  using content::RenderViewHostTestHarness::RenderViewHostTestHarness;

  WebAppTest();

  void SetUp() override;
  void TearDown() override;

  TestingProfile* profile() { return profile_.get(); }
  TestingProfileManager& profile_manager() { return testing_profile_manager_; }

 protected:
  // content::RenderViewHostTestHarness.
  content::BrowserContext* GetBrowserContext() final;

 private:
  TestingProfileManager testing_profile_manager_{
      TestingBrowserProcess::GetGlobal()};
  raw_ptr<TestingProfile> profile_;
};

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_TEST_WEB_APP_TEST_H_
