// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
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

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, All) {
  RunTest("contextual_tasks/contextual_tasks_browsertest.js", "mocha.run();");
}
