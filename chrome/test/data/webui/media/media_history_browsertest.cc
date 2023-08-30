// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "content/public/test/browser_test.h"
#include "media/base/media_switches.h"

class MediaHistoryTest : public WebUIMochaBrowserTest {
 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      media::kUseMediaHistoryStore};
};

// https://crbug.com/1045500: Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_Stats DISABLED_Stats
#else
#define MAYBE_Stats Stats
#endif
IN_PROC_BROWSER_TEST_F(MediaHistoryTest, MAYBE_Stats) {
  set_test_loader_host(std::string(chrome::kChromeUIMediaHistoryHost) +
                       "#tab-stats");
  RunTestWithoutTestLoader("media/media_history_test.js",
                           "runMochaSuite('Stats')");
}

// https://crbug.com/1045500: Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_Origins DISABLED_Origins
#else
#define MAYBE_Origins Origins
#endif
IN_PROC_BROWSER_TEST_F(MediaHistoryTest, MAYBE_Origins) {
  set_test_loader_host(std::string(chrome::kChromeUIMediaHistoryHost) +
                       "#tab-origins");
  RunTestWithoutTestLoader("media/media_history_test.js",
                           "runMochaSuite('Origins')");
}

// https://crbug.com/1045500: Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_Playbacks DISABLED_Playbacks
#else
#define MAYBE_Playbacks Playbacks
#endif
IN_PROC_BROWSER_TEST_F(MediaHistoryTest, MAYBE_Playbacks) {
  set_test_loader_host(std::string(chrome::kChromeUIMediaHistoryHost) +
                       "#tab-playbacks");
  RunTestWithoutTestLoader("media/media_history_test.js",
                           "runMochaSuite('Playbacks')");
}

// https://crbug.com/1045500: Flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_Sessions DISABLED_Sessions
#else
#define MAYBE_Sessions Sessions
#endif
IN_PROC_BROWSER_TEST_F(MediaHistoryTest, MAYBE_Sessions) {
  set_test_loader_host(std::string(chrome::kChromeUIMediaHistoryHost) +
                       "#tab-sessions");
  RunTestWithoutTestLoader("media/media_history_test.js",
                           "runMochaSuite('Playbacks')");
}
