// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "ui/webui/mojo_web_ui_controller.h"
#include "url/gurl.h"

namespace {

// A WebUIController that enables chrome.histograms.
class TestWebUIController : public ui::MojoWebUIController {
 public:
  explicit TestWebUIController(content::WebUI* web_ui)
      : ui::MojoWebUIController(web_ui,
                                /*enable_chrome_send=*/false,
                                /*enable_chrome_histograms=*/true) {}
};

class TestWebUIControllerFactory : public content::WebUIControllerFactory {
 public:
  std::unique_ptr<content::WebUIController> CreateWebUIControllerForURL(
      content::WebUI* web_ui,
      const GURL& url) override {
    return std::make_unique<TestWebUIController>(web_ui);
  }

  content::WebUI::TypeID GetWebUIType(content::BrowserContext* browser_context,
                                      const GURL& url) override {
    return reinterpret_cast<content::WebUI::TypeID>(1);
  }

  bool UseWebUIForURL(content::BrowserContext* browser_context,
                      const GURL& url) override {
    return url.host() == "webui-test";
  }
};

}  // namespace

class ChromeHistogramsBrowserTest : public WebUIMochaBrowserTest {
 protected:
  void SetUpOnMainThread() override {
    WebUIMochaBrowserTest::SetUpOnMainThread();
    factory_registration_ =
        std::make_unique<content::ScopedWebUIControllerFactoryRegistration>(
            &factory_);
  }

 private:
  TestWebUIControllerFactory factory_;
  std::unique_ptr<content::ScopedWebUIControllerFactoryRegistration>
      factory_registration_;
};

// Tests for the chrome.histograms API in WebUIs.
IN_PROC_BROWSER_TEST_F(ChromeHistogramsBrowserTest, All) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;

  // Spawn the test from chrome://webui-test where the chrome.histograms API is
  // enabled by TestWebUIController.
  set_test_loader_host("webui-test");
  RunTest("chrome_apis/chrome_histograms_test.js", "mocha.run()");

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return user_action_tester.GetActionCount("Test.ComputedAction") == 1;
  }));

  content::FetchHistogramsFromChildProcesses();
  metrics::SubprocessMetricsProvider::MergeHistogramDeltasForTesting();

  // Verify that the histograms were recorded correctly.
  histogram_tester.ExpectUniqueSample("Test.Boolean", true, 1);
  histogram_tester.ExpectUniqueSample("Test.Percentage", 50, 1);
  histogram_tester.ExpectUniqueSample("Test.Counts100", 10, 1);
  histogram_tester.ExpectUniqueSample("Test.Counts10000", 100, 1);
  histogram_tester.ExpectUniqueSample("Test.Counts1M", 1000, 1);
  histogram_tester.ExpectUniqueSample("Test.Times", 100, 1);
  histogram_tester.ExpectUniqueSample("Test.MediumTimes", 1000, 1);
  histogram_tester.ExpectUniqueSample("Test.LongTimes", 10000, 1);
  histogram_tester.ExpectUniqueSample("Test.CustomCounts", 10, 1);
  histogram_tester.ExpectUniqueSample("Test.ExactLinear", 5, 1);
  histogram_tester.ExpectUniqueSample("Test.Enumeration", 1, 1);
  histogram_tester.ExpectUniqueSample("Test.Sparse", 10, 1);
}
