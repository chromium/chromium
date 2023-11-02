// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/test_chrome_web_ui_controller_factory.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::WebContents;
using content::WebUI;
using content::WebUIController;
using ::testing::_;
using ::testing::Eq;
using ::testing::StrictMock;

namespace {

// Returns a new WebUI object for the WebContents from |arg0|.
ACTION(ReturnNewWebUI) {
  return std::make_unique<WebUIController>(arg0);
}

// Mock the TestChromeWebUIControllerFactory::WebUIProvider to prove that we are
// called as expected.
class MockWebUIProvider
    : public TestChromeWebUIControllerFactory::WebUIProvider {
 public:
  MOCK_METHOD2(NewWebUI,
               std::unique_ptr<WebUIController>(content::WebUI* web_ui,
                                                const GURL& url));
};

// Dummy URL location for us to override.
const std::string kChromeTestChromeWebUIControllerFactory =
    content::GetWebUIURLString("ChromeTestChromeWebUIControllerFactory/");

// Sets up and tears down the factory override for our url's host. It is
// necessary to do this here, rather than in the test declaration, which is too
// late to catch the possibility of an initial browse to about:blank mistakenly
// going to this handler.
class TestChromeWebUIControllerFactoryTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    test_factory_ = std::make_unique<TestChromeWebUIControllerFactory>();
    factory_registration_ =
        std::make_unique<content::ScopedWebUIControllerFactoryRegistration>(
            test_factory_.get(), ChromeWebUIControllerFactory::GetInstance());

    test_factory_->AddFactoryOverride(
        GURL(kChromeTestChromeWebUIControllerFactory).host(), &mock_provider_);
  }

  void TearDownOnMainThread() override {
    test_factory_->RemoveFactoryOverride(
        GURL(kChromeTestChromeWebUIControllerFactory).host());
  }

 protected:
  StrictMock<MockWebUIProvider> mock_provider_;
  std::unique_ptr<TestChromeWebUIControllerFactory> test_factory_;
  std::unique_ptr<content::ScopedWebUIControllerFactoryRegistration>
      factory_registration_;
};

}  // namespace

// Test that browsing to our test url causes us to be called once.
IN_PROC_BROWSER_TEST_F(TestChromeWebUIControllerFactoryTest,
                       TestWebUIProvider) {
  const GURL kChromeTestChromeWebUIControllerFactoryURL(
      kChromeTestChromeWebUIControllerFactory);
  EXPECT_CALL(mock_provider_,
              NewWebUI(_, Eq(kChromeTestChromeWebUIControllerFactoryURL)))
      .WillOnce(ReturnNewWebUI());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), kChromeTestChromeWebUIControllerFactoryURL));
}
