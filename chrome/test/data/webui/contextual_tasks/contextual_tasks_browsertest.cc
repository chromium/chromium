// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "build/config/coverage/buildflags.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/contextual_tasks/public/features.h"
#include "content/public/test/browser_test.h"

class ContextualTasksBrowserTest : public WebUIMochaBrowserTest {
 protected:
  ContextualTasksBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {contextual_tasks::kContextualTasks,
         contextual_tasks::kContextualTasksForceEntryPointEligibility},
        {});
    set_test_loader_host(chrome::kChromeUIContextualTasksHost);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    WebUIMochaBrowserTest::SetUpCommandLine(command_line);
    // TODO(crbug.com/489032845): Re-enable crash-on-JS-error for tests.
    command_line->AppendSwitch("disable-crash-on-webui-js-error");
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/487802136): Flaky on Linux.
// TODO(crbug.com/489258910): Failing on multiple platforms
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, App) {
  RunTest("contextual_tasks/app_test.js", "mocha.run();");
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, Composebox) {
  RunTest("contextual_tasks/composebox_test.js", "mocha.run();");
}

// TODO(crbug.com/480689282): Flaky on ChromeOS debug.
#if BUILDFLAG(IS_CHROMEOS) && !defined(NDEBUG)
#define MAYBE_Composebox_MiscInputs DISABLED_Composebox_MiscInputs
#else
#define MAYBE_Composebox_MiscInputs Composebox_MiscInputs
#endif
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       MAYBE_Composebox_MiscInputs) {
  RunTest("contextual_tasks/composebox_misc_inputs_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, Composebox_Submit) {
  RunTest("contextual_tasks/composebox_submit_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, Composebox_ZeroState) {
  RunTest("contextual_tasks/composebox_zero_state_test.js", "mocha.run();");
}
#endif

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, PostMessageHandler) {
  RunTest("contextual_tasks/post_message_handler_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, TopToolbarTest) {
  RunTest("contextual_tasks/top_toolbar_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, WebView) {
  RunTest("contextual_tasks/contextual_tasks_webview_browsertest.js",
          "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, ClipPath) {
  RunTest("contextual_tasks/utils/clip_path_test.js", "mocha.run();");
}
