// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/contextual_tasks/public/features.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/interaction/element_identifier.h"

namespace {
DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kFirstTab);
}

class ContextualTasksPageActionControllerInteractiveTest
    : public InteractiveBrowserTest {
 public:
  void SetUp() override {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kPageActionsMigration, {}},
         {contextual_tasks::kContextualTasks,
          {{"ContextualTasksEntryPoint", "page-action-revisit"}}},
         {features::kTabbedBrowserUseNewLayout, {}}},
        {});
    InteractiveBrowserTest::SetUp();
  }
  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksPageActionControllerInteractiveTest,
                       ShowContextualTaskPageAction) {
  RunTestSequence(
      InstrumentTab(kFirstTab),
      // The contextual task page action should not show on the new tab page
      NavigateWebContents(kFirstTab, GURL(chrome::kChromeUINewTabPageURL)),
      WaitForHide(kContextualTasksPageActionElementId),
      NavigateWebContents(kFirstTab, embedded_test_server()->GetURL(
                                         "example.com", "/title1.html")),
      WaitForShow(kContextualTasksPageActionElementId));
}
