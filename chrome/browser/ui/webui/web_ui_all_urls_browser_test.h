// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEB_UI_ALL_URLS_BROWSER_TEST_H_
#define CHROME_BROWSER_UI_WEBUI_WEB_UI_ALL_URLS_BROWSER_TEST_H_

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/enterprise/browser/controller/fake_browser_dm_token_storage.h"

// Class which performs necessary setup to load all chrome urls. If a URL
// requires a feature flag or other special setup to load correctly, add
// this setup here.
class WebUIAllUrlsBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<const char*> {
 public:
  WebUIAllUrlsBrowserTest();
  WebUIAllUrlsBrowserTest(const WebUIAllUrlsBrowserTest&) = delete;
  WebUIAllUrlsBrowserTest& operator=(const WebUIAllUrlsBrowserTest&) = delete;
  ~WebUIAllUrlsBrowserTest() override;
  static std::string ParamInfoToString(
      const ::testing::TestParamInfo<const char*>& info);

 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override;
#if BUILDFLAG(IS_CHROMEOS)
  void SetUpOnMainThread() override;
#endif
  void WaitBeforeNavigation();

 private:
  base::test::ScopedFeatureList feature_list_;
#if !BUILDFLAG(IS_CHROMEOS)
  policy::FakeBrowserDMTokenStorage fake_dm_token_storage_;
#endif
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEB_UI_ALL_URLS_BROWSER_TEST_H_
