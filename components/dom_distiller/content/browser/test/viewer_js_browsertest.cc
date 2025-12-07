// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "build/build_config.h"
#include "components/dom_distiller/content/browser/distiller_javascript_utils.h"
#include "components/dom_distiller/content/browser/test/test_util.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/content_browser_test.h"
#include "content/shell/browser/shell.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace dom_distiller {
namespace {

class ViewerJsTest : public content::ContentBrowserTest {
 protected:
  ~ViewerJsTest() override = default;

  void SetUpOnMainThread() override {
    if (!DistillerJavaScriptWorldIdIsSet()) {
      SetDistillerJavaScriptWorldId(content::ISOLATED_WORLD_ID_CONTENT_END);
    }

    AddComponentsResources();
    distilled_page_ = SetUpTestServerWithDistilledPage(embedded_test_server());
  }

  void LoadAndExecuteTestScript(const std::string& file) {
    distilled_page_->AppendScriptFile(file);
    distilled_page_->Load(embedded_test_server(), shell()->web_contents());
    EXPECT_TRUE(content::ExecJs(shell()->web_contents(),
                                "mocha.run(); window.completePromise"));
  }

  std::unique_ptr<FakeDistilledPage> distilled_page_;
};

IN_PROC_BROWSER_TEST_F(ViewerJsTest, SanitizeLinks) {
  LoadAndExecuteTestScript("viewer_js_tester.js");
}

}  // namespace
}  // namespace dom_distiller
