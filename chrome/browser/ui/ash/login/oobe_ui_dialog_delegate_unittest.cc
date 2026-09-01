// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/login/oobe_ui_dialog_delegate.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/login/oobe_configuration.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_controller_client.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/browser/web_ui_controller_factory.h"
#include "content/public/test/web_contents_tester.h"
#include "content/public/test/scoped_web_ui_controller_factory_registration.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

class NonOobeWebUIController : public content::WebUIController {
 public:
  explicit NonOobeWebUIController(content::WebUI* web_ui)
      : content::WebUIController(web_ui) {}

  WEB_UI_CONTROLLER_TYPE_DECL();
};

WEB_UI_CONTROLLER_TYPE_IMPL(NonOobeWebUIController)

class TestWebUIControllerFactory : public content::WebUIControllerFactory {
 public:
  TestWebUIControllerFactory() = default;
  TestWebUIControllerFactory(const TestWebUIControllerFactory&) = delete;
  TestWebUIControllerFactory& operator=(const TestWebUIControllerFactory&) = delete;

  // content::WebUIControllerFactory:
  std::unique_ptr<content::WebUIController> CreateWebUIControllerForURL(
      content::WebUI* web_ui,
      const GURL& url) override {
    if (url.host() == "oobe") {
      return std::make_unique<OobeUI>(web_ui, GURL("chrome://oobe/gaia-signin"));
    }
    if (url.host() == "nonoobe") {
      return std::make_unique<NonOobeWebUIController>(web_ui);
    }
    return nullptr;
  }

  content::WebUI::TypeID GetWebUIType(content::BrowserContext* browser_context,
                                      const GURL& url) override {
    if (url.host() == "oobe" || url.host() == "nonoobe")
      return const_cast<TestWebUIControllerFactory*>(this);
    return content::WebUI::kNoWebUI;
  }

  bool UseWebUIForURL(content::BrowserContext* browser_context,
                      const GURL& url) override {
    return GetWebUIType(browser_context, url) != content::WebUI::kNoWebUI;
  }
};

class OobeUIDialogDelegateTest : public AshTestBase {
 public:
  OobeUIDialogDelegateTest()
      : AshTestBase(std::unique_ptr<base::test::TaskEnvironment>(
            std::make_unique<content::BrowserTaskEnvironment>())) {}
  ~OobeUIDialogDelegateTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();
    TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(
        test_url_loader_factory_.GetSafeWeakWrapper());
    chrome_keyboard_controller_client_ =
        ChromeKeyboardControllerClient::CreateForTest();
    network_handler_test_helper_ = std::make_unique<NetworkHandlerTestHelper>();
    oobe_configuration_ = std::make_unique<OobeConfiguration>();
    profile_ = std::make_unique<TestingProfile>();
        test_web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
    factory_registration_ = std::make_unique<content::ScopedWebUIControllerFactoryRegistration>(&factory_);
  }

  void TearDown() override {
    test_web_contents_.reset();
    factory_registration_.reset();
    profile_.reset();
    oobe_configuration_.reset();
    network_handler_test_helper_.reset();
    chrome_keyboard_controller_client_.reset();
    TestingBrowserProcess::GetGlobal()->SetSharedURLLoaderFactory(nullptr);
    AshTestBase::TearDown();
  }

 protected:
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<ChromeKeyboardControllerClient>
      chrome_keyboard_controller_client_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  std::unique_ptr<OobeConfiguration> oobe_configuration_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> test_web_contents_;
  TestWebUIControllerFactory factory_;
  std::unique_ptr<content::ScopedWebUIControllerFactoryRegistration> factory_registration_;
};

// Verifies that OobeUIDialogDelegate::GetOobeUI() returns nullptr when
// controller on WebUI is not an instance of OobeUI, and returns a
// valid pointer when controller is an instance of OobeUI.
TEST_F(OobeUIDialogDelegateTest, GetOobeUI) {
  auto* delegate = new OobeUIDialogDelegate(
      /*controller=*/nullptr, test_web_contents_.get());

  // 1. WebUI is attached early, so GetOobeUI() returns a valid pointer.
  EXPECT_NE(nullptr, delegate->GetOobeUI());

  // 2. Controller is not an instance of OobeUI.
  content::WebContentsTester::For(test_web_contents_.get())
      ->NavigateAndCommit(GURL("chrome://nonoobe"));
  EXPECT_EQ(nullptr, delegate->GetOobeUI());

  // 3. Controller is an instance of OobeUI.
  content::WebContentsTester::For(test_web_contents_.get())
      ->NavigateAndCommit(GURL("chrome://oobe/gaia-signin"));
  EXPECT_NE(nullptr, delegate->GetOobeUI());

  test_web_contents_.reset();
  delegate->GetWebDialogWidget()->CloseNow();
  base::RunLoop().RunUntilIdle();
}
}  // namespace
}  // namespace ash
