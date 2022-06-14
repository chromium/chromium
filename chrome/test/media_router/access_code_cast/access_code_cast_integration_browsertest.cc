// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/media_router/access_code_cast/access_code_cast_integration_browsertest.h"

#include "base/auto_reset.h"
#include "base/memory/ptr_util.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/media/router/chrome_media_router_factory.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/media_router/browser/media_router_factory.h"
#include "content/public/test/browser_test_utils.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/ash/cast_config_controller_media_router.h"
#endif

namespace {
class TestMediaRouter : public media_router::MockMediaRouter {
 public:
  static std::unique_ptr<KeyedService> Create(
      content::BrowserContext* context) {
    return std::make_unique<TestMediaRouter>();
  }

  media_router::LoggerImpl* GetLogger() override {
    if (!logger_)
      logger_ = std::make_unique<media_router::LoggerImpl>();
    return logger_.get();
  }

  void RegisterMediaRoutesObserver(
      media_router::MediaRoutesObserver* observer) override {
    routes_observers_.push_back(observer);
  }

  void UnregisterMediaRoutesObserver(
      media_router::MediaRoutesObserver* observer) override {
    base::Erase(routes_observers_, observer);
  }

 private:
  std::vector<media_router::MediaRoutesObserver*> routes_observers_;
  std::unique_ptr<media_router::LoggerImpl> logger_;
};
}  // namespace

namespace media_router {

AccessCodeCastIntegrationBrowserTest::AccessCodeCastIntegrationBrowserTest() {
  feature_list_.InitAndEnableFeature(features::kAccessCodeCastUI);
}

AccessCodeCastIntegrationBrowserTest::~AccessCodeCastIntegrationBrowserTest() =
    default;

void AccessCodeCastIntegrationBrowserTest::SetUp() {
// This makes sure CastDeviceCache is not initialized until after the
// MockMediaRouter is ready. (MockMediaRouter can't be constructed yet.)
#if BUILDFLAG(IS_CHROMEOS_ASH)
  CastConfigControllerMediaRouter::SetMediaRouterForTest(nullptr);
#endif

  ASSERT_TRUE(embedded_test_server()->Start());
  InProcessBrowserTest::SetUp();
}

void AccessCodeCastIntegrationBrowserTest::SetUpInProcessBrowserTestFixture() {
  subscription_ =
      BrowserContextDependencyManager::GetInstance()
          ->RegisterCreateServicesCallbackForTesting(
              base::BindRepeating(&AccessCodeCastIntegrationBrowserTest::
                                      OnWillCreateBrowserContextServices,
                                  base::Unretained(this)));
}

void AccessCodeCastIntegrationBrowserTest::OnWillCreateBrowserContextServices(
    content::BrowserContext* context) {
  IdentityTestEnvironmentProfileAdaptor::
      SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);

  media_router_ = static_cast<TestMediaRouter*>(
      media_router::MediaRouterFactory::GetInstance()->SetTestingFactoryAndUse(
          context, base::BindRepeating(&TestMediaRouter::Create)));

  AccessCodeCastSinkServiceFactory::GetInstance()->SetTestingFactory(
      context, base::BindRepeating(&AccessCodeCastIntegrationBrowserTest::
                                       CreateAccessCodeCastSinkService,
                                   base::Unretained(this)));
}

void AccessCodeCastIntegrationBrowserTest::SetUpPrimaryAccountWithHostedDomain(
    signin::ConsentLevel consent_level) {
  // Ensure that the stub user is signed in.
  CoreAccountInfo account_info =
      identity_test_env()->MakePrimaryAccountAvailable(
          user_manager::kStubUserEmail, consent_level);

  ASSERT_EQ(account_info.email, user_manager::kStubUserEmail);

  identity_test_env()->SimulateSuccessfulFetchOfAccountInfo(
      account_info.account_id, account_info.email, account_info.gaia,
      "foo_school.com", "full_name", "given_name", "locale",
      "http://picture.example.com/picture.jpg");
  identity_test_env()->SetAutomaticIssueOfAccessTokens(true);
  base::RunLoop().RunUntilIdle();
}

void AccessCodeCastIntegrationBrowserTest::PreShow() {
  browser()->profile()->GetPrefs()->SetBoolean(
      media_router::prefs::kAccessCodeCastEnabled, true);
  base::RunLoop().RunUntilIdle();
}

content::WebContents* AccessCodeCastIntegrationBrowserTest::ShowDialog() {
  content::WebContentsAddedObserver observer;

  CastModeSet tab_mode = {MediaCastMode::TAB_MIRROR};
  std::unique_ptr<MediaRouteStarter> starter =
      std::make_unique<MediaRouteStarter>(tab_mode, web_contents(), nullptr);
  AccessCodeCastDialog::Show(
      tab_mode, std::move(starter),
      AccessCodeCastDialogOpenLocation::kBrowserCastMenu);

  EXPECT_TRUE(content::WaitForLoadStop(web_contents()));
  content::WebContents* dialog_contents = observer.GetWebContents();

  EXPECT_TRUE(content::WaitForLoadStop(dialog_contents));

  auto* web_ui = dialog_contents->GetWebUI();
  EXPECT_TRUE(web_ui);
  SetWebUIInstance(web_ui);
  EXPECT_TRUE(web_ui->CanCallJavascript());

  return dialog_contents;
}

void AccessCodeCastIntegrationBrowserTest::CloseDialog(
    content::WebContents* dialog_contents) {
  ASSERT_TRUE(dialog_contents);
  EXPECT_TRUE(ExecuteScript(dialog_contents, std::string(GetElementScript()) +
                                                 ".cancelButtonPressed();"));
}

void AccessCodeCastIntegrationBrowserTest::SetAccessCode(
    std::string access_code,
    content::WebContents* dialog_contents) {
  ASSERT_TRUE(dialog_contents);
  EXPECT_TRUE(ExecuteScript(dialog_contents,
                            GetElementScript() + ".switchToCodeInput();"));
  EXPECT_TRUE(ExecuteScript(
      dialog_contents,
      GetElementScript() + ".setAccessCodeForTest('" + access_code + "');"));
}

void AccessCodeCastIntegrationBrowserTest::PressSubmit(
    content::WebContents* dialog_contents) {
  ASSERT_TRUE(dialog_contents);
  EXPECT_TRUE(ExecuteScript(dialog_contents,
                            GetElementScript() + ".addSinkAndCast();"));
}

int AccessCodeCastIntegrationBrowserTest::WaitForAddSinkErrorCode(
    content::WebContents* dialog_contents) {
  // Spin the run loop until we get any error code (0 represents no error).
  while (0 ==
         EvalJs(GetErrorElementScript() + ".getMessageCode();", dialog_contents)
             .ExtractInt()) {
    SpinRunLoop();
  }

  return EvalJs(GetErrorElementScript() + ".getMessageCode();", dialog_contents)
      .ExtractInt();
}

void AccessCodeCastIntegrationBrowserTest::SetUpOnMainThread() {
  identity_test_env_adaptor_ =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
          browser()->profile());
  InProcessBrowserTest::SetUpOnMainThread();
}
void AccessCodeCastIntegrationBrowserTest::TearDownOnMainThread() {
  identity_test_env_adaptor_.reset();
  InProcessBrowserTest::TearDownOnMainThread();
}

std::unique_ptr<KeyedService>
AccessCodeCastIntegrationBrowserTest::CreateAccessCodeCastSinkService(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return base::WrapUnique(new AccessCodeCastSinkService(
      profile, media_router_, nullptr, DiscoveryNetworkMonitor::GetInstance(),
      profile->GetPrefs()));
}

void AccessCodeCastIntegrationBrowserTest::SpinRunLoop() {
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), base::Milliseconds(20));
  run_loop.Run();
}

content::EvalJsResult AccessCodeCastIntegrationBrowserTest::EvalJs(
    const std::string& string_value,
    content::WebContents* web_contents) {
  return content::EvalJs(web_contents, string_value,
                         content::EXECUTE_SCRIPT_DEFAULT_OPTIONS);
}

}  // namespace media_router
