// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/test_with_browser_view.h"

#include <memory>

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/search_engines/chrome_template_url_service_client.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/signin/fake_gaia_cookie_manager_service_builder.h"
#include "chrome/browser/signin/gaia_cookie_manager_service_factory.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/core/browser/fake_gaia_cookie_manager_service.h"
#include "content/public/test/test_utils.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/mock_input_method_manager_impl.h"
#endif

namespace {

std::unique_ptr<KeyedService> CreateTemplateURLService(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  return std::make_unique<TemplateURLService>(
      profile->GetPrefs(), std::make_unique<UIThreadSearchTermsData>(profile),
      WebDataServiceFactory::GetKeywordWebDataForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      std::make_unique<ChromeTemplateURLServiceClient>(
          HistoryServiceFactory::GetForProfile(
              profile, ServiceAccessType::EXPLICIT_ACCESS)),
      nullptr, nullptr, base::Closure());
}

std::unique_ptr<KeyedService> CreateAutocompleteClassifier(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  return std::make_unique<AutocompleteClassifier>(
      std::make_unique<AutocompleteController>(
          std::make_unique<ChromeAutocompleteProviderClient>(profile), nullptr,
          AutocompleteClassifier::DefaultOmniboxProviders()),
      std::make_unique<TestSchemeClassifier>());
}

}  // namespace

TestWithBrowserView::TestWithBrowserView() = default;

TestWithBrowserView::TestWithBrowserView(Browser::Type browser_type,
                                         bool hosted_app)
    : BrowserWithTestWindowTest(browser_type, hosted_app) {}

TestWithBrowserView::~TestWithBrowserView() {
}

void TestWithBrowserView::SetUp() {
#if defined(OS_CHROMEOS)
  chromeos::input_method::InitializeForTesting(
      new chromeos::input_method::MockInputMethodManagerImpl);
#endif
  BrowserWithTestWindowTest::SetUp();
  browser_view_ = static_cast<BrowserView*>(browser()->window());
}

void TestWithBrowserView::TearDown() {
  // Both BrowserView and BrowserWithTestWindowTest believe they have ownership
  // of the Browser. Force BrowserWithTestWindowTest to give up ownership.
  ASSERT_TRUE(release_browser());

  // Clean up any tabs we opened, otherwise Browser crashes in destruction.
  browser_view_->browser()->tab_strip_model()->CloseAllTabs();
  // Ensure the Browser is reset before BrowserWithTestWindowTest cleans up
  // the Profile.
  browser_view_->GetWidget()->CloseNow();
  browser_view_ = nullptr;
  content::RunAllTasksUntilIdle();
  BrowserWithTestWindowTest::TearDown();
#if defined(OS_CHROMEOS)
  chromeos::input_method::Shutdown();
#endif
}

TestingProfile* TestWithBrowserView::CreateProfile() {
  TestingProfile* profile = BrowserWithTestWindowTest::CreateProfile();
  // TemplateURLService is normally null during testing. Instant extended
  // needs this service so set a custom factory function.
  TemplateURLServiceFactory::GetInstance()->SetTestingFactory(
      profile, base::BindRepeating(&CreateTemplateURLService));
  // TODO(jamescook): Eliminate this by introducing a mock toolbar or mock
  // location bar.
  AutocompleteClassifierFactory::GetInstance()->SetTestingFactory(
      profile, base::BindRepeating(&CreateAutocompleteClassifier));

  // Configure the GaiaCookieManagerService to return no accounts.
  FakeGaiaCookieManagerService* gcms =
      static_cast<FakeGaiaCookieManagerService*>(
          GaiaCookieManagerServiceFactory::GetForProfile(profile));
  gcms->SetListAccountsResponseHttpNotFound();
  return profile;
}

BrowserWindow* TestWithBrowserView::CreateBrowserWindow() {
  // Allow BrowserWithTestWindowTest to use Browser to create the default
  // BrowserView and BrowserFrame.
  return nullptr;
}

TestingProfile::TestingFactories TestWithBrowserView::GetTestingFactories() {
  return {{GaiaCookieManagerServiceFactory::GetInstance(),
           base::BindRepeating(&BuildFakeGaiaCookieManagerService)}};
}
