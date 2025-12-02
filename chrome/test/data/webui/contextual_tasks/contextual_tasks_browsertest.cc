// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "build/config/coverage/buildflags.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/contextual_tasks/public/features.h"
#include "content/public/test/browser_test.h"

class ContextualTasksBrowserTest : public WebUIMochaBrowserTest {
 protected:
  ContextualTasksBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        contextual_tasks::kContextualTasks);
    set_test_loader_host(chrome::kChromeUIContextualTasksHost);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, App) {
  RunTest("contextual_tasks/contextual_tasks_browsertest.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, Composebox) {
  RunTest("contextual_tasks/composebox_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, PostMessageHandler) {
  RunTest("contextual_tasks/post_message_handler_test.js", "mocha.run();");
}

#if BUILDFLAG(USE_JAVASCRIPT_COVERAGE)
// TODO(crbug.com/40284073): Test fails with JS coverage turned on. Since the
// webview needs to make a request to test the request headers, disabling this
// test on JS coverage for now. Re-enable once bug is fixed.
#define MAYBE_WebView DISABLED_WebView
#else
#define MAYBE_WebView WebView
#endif
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, MAYBE_WebView) {
  RunTest("contextual_tasks/contextual_tasks_webview_browsertest.js",
          "mocha.run();");
}
