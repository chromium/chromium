// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <optional>
#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/webui/boca_ui/url_constants.h"
#include "ash/webui/vc_background_ui/url_constants.h"
#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "chrome/browser/ash/boca/boca_manager.h"
#include "chrome/browser/ash/boca/boca_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "chromeos/ash/components/boca/proto/bundle.pb.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "components/manta/features.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/url_loader_interceptor.h"
#include "services/network/public/cpp/data_element.h"
#include "url/gurl.h"

namespace {

std::unique_ptr<KeyedService> BuildBocaManagerWithIdentity(
    std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>* adaptor_out,
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  profile->GetPrefs()->SetInteger(
      ash::prefs::kClassManagementToolsOOBEAccessCountSetting, 1);
  profile->GetPrefs()->SetBoolean(
      ash::prefs::kClassManagementToolsNetworkRestrictionSetting, false);

  auto identity_adaptor =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile);
  identity_adaptor->identity_test_env()->MakePrimaryAccountAvailable(
      "teacher@gmail.com", signin::ConsentLevel::kSignin);
  identity_adaptor->identity_test_env()->SetAutomaticIssueOfAccessTokens(true);
  *adaptor_out = std::move(identity_adaptor);

  return std::make_unique<ash::BocaManager>(
      profile, g_browser_process->local_state(),
      g_browser_process->GetApplicationLocale());
}

std::string GetRequestBodyString(
    scoped_refptr<network::ResourceRequestBody> request_body) {
  const auto& element = (*request_body->elements())[0];
  return element.type() == network::DataElement::Tag::kBytes
             ? std::string(
                   element.As<network::DataElementBytes>().AsStringPiece())
             : "";
}

bool OnUrlIntercepted(
    base::RepeatingCallback<void()> on_get_gemini_status_cb,
    base::RepeatingCallback<void(std::string)> on_create_session_cb,
    content::URLLoaderInterceptor::RequestParams* params) {
  std::optional<std::string> response_body;
  const GURL& url = params->url_request.url;
  if (url.spec().find("getGeminiStatus") != std::string::npos) {
    // Get Gemini Status
    on_get_gemini_status_cb.Run();
    response_body = base::ReplaceStringPlaceholders(
        ash::boca::kGeminiStatusFetchResponseTemplate,
        {ash::boca::kGeminiStateEnabled}, nullptr);
  } else if (url.spec().find("/v1/teachers/") != std::string::npos &&
             url.spec().find("/sessions") != std::string::npos) {
    // Create Session
    CHECK(params->url_request.request_body &&
          params->url_request.request_body->elements()->size() > 0);
    std::string request_body =
        GetRequestBodyString(params->url_request.request_body);
    CHECK(request_body.length() > 1 && request_body[0] == '{');
    on_create_session_cb.Run(request_body);
    response_body = std::move(request_body);
    response_body->insert(1, R"("sessionId":"session_id_1",)");
  }
  if (response_body.has_value()) {
    content::URLLoaderInterceptor::WriteResponse(
        "HTTP/1.1 200 OK\nContent-type: application/json\n",
        std::move(response_body.value()), params->client.get());
    return true;
  }
  return false;
}

class BocaAppBrowserProducerTest : public WebUIMochaBrowserTest {
 protected:
  BocaAppBrowserProducerTest() {
    set_test_loader_scheme(content::kChromeUIUntrustedScheme);
    set_test_loader_host(std::string(ash::boca::kChromeBocaAppHost));

    scoped_feature_list_.InitWithFeatures(
        /* enabled_features */ {ash::features::kBoca,
                                ash::features::kBocaGeminiIntegration},
        /* disabled_features */ {});
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    WebUIMochaBrowserTest::SetUpBrowserContextKeyedServices(context);
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
    ash::BocaManagerFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildBocaManagerWithIdentity,
                                     base::Unretained(&identity_adaptor_)));
  }

  void TearDownOnMainThread() override {
    identity_adaptor_.reset();
    WebUIMochaBrowserTest::TearDownOnMainThread();
  }

  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor> identity_adaptor_;
};

IN_PROC_BROWSER_TEST_F(BocaAppBrowserProducerTest, TestMojoTranslationLayer) {
  RunTestWithoutTestLoader("chromeos/boca_ui/client_delegate_impl_test.js",
                           "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(BocaAppBrowserProducerTest, TestMainPageLoaded) {
  RunTestWithoutTestLoader("chromeos/boca_ui/producer_main_page_test.js",
                           "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(BocaAppBrowserProducerTest,
                       CreateSessionWithGeminiGuidedLearning) {
  base::test::TestFuture<void> get_gemini_status_future;
  base::test::TestFuture<std::string> create_session_future;
  auto url_loader_interceptor =
      std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
          &OnUrlIntercepted, get_gemini_status_future.GetRepeatingCallback(),
          create_session_future.GetRepeatingCallback()));
  RunTestWithoutTestLoader(
      "chromeos/boca_ui/boca_app_producer_test.js",
      "runMochaTest('CreateSession', 'WithGeminiGuidedLearning')");
  std::string create_session_request = create_session_future.Take();
  base::Value create_session_request_value =
      base::test::ParseJson(create_session_request);
  ASSERT_TRUE(create_session_request_value.is_dict());
  base::ListValue* contents =
      create_session_request_value.GetDict().FindListByDottedPath(
          "studentGroupConfigs.main.onTaskConfig.activeBundle.contentConfigs");
  ASSERT_TRUE(contents);
  ASSERT_TRUE(contents->begin()->is_dict());
  const base::DictValue& gemini_content = contents->begin()->GetDict();
  EXPECT_EQ(*gemini_content.FindString("url"),
            ash::features::kBocaGeminiGuidedLearningUrl.Get());
  EXPECT_EQ(*gemini_content.FindInt("urlType"),
            ::boca::URL_TYPE_GEMINI_GUIDED_LEARNING);
  EXPECT_EQ(*gemini_content.FindIntByDottedPath(
                "lockedNavigationOptions.navigationType"),
            ::boca::LockedNavigationOptions::BLOCK_NAVIGATION);
  EXPECT_TRUE(get_gemini_status_future.IsReady());
}

IN_PROC_BROWSER_TEST_F(BocaAppBrowserProducerTest, TestReceiver) {
  RunTestWithoutTestLoader("chromeos/boca_ui/receiver_test.js", "mocha.run()");
}

class BocaAppBrowserConsumerTest : public WebUIMochaBrowserTest {
 public:
  BocaAppBrowserConsumerTest() {
    set_test_loader_scheme(content::kChromeUIUntrustedScheme);
    set_test_loader_host(std::string(ash::boca::kChromeBocaAppHost));

    scoped_feature_list_.InitWithFeatures(
        /* enabled_features */ {ash::features::kBoca,
                                ash::features::kBocaConsumer},
        /* disabled_features */ {});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(BocaAppBrowserConsumerTest, TestMainPageLoaded) {
  RunTestWithoutTestLoader("chromeos/boca_ui/consumer_main_page_test.js",
                           "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(BocaAppBrowserConsumerTest, TestFeatureFlagNotHonored) {
  browser()->profile()->GetPrefs()->SetBoolean(
      ash::prefs::kClassManagementToolsCaptionEligibilitySetting, false);
  browser()->profile()->GetPrefs()->SetBoolean(
      ash::prefs::kClassManagementToolsViewScreenEligibilitySetting, false);
  RunTestWithoutTestLoader("chromeos/boca_ui/feature_flag_test.js",
                           "mocha.run()");
}

}  // namespace
