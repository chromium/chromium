// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/test_with_browser_view.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/autocomplete/chrome_autocomplete_provider_client.h"
#include "chrome/browser/extensions/extension_action_test_util.h"
#include "chrome/browser/extensions/load_error_reporter.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_test_util.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_controller.h"
#include "components/omnibox/browser/autocomplete_controller_config.h"
#include "components/omnibox/browser/test_scheme_classifier.h"
#include "components/signin/public/base/list_accounts_test_utils.h"
#include "content/public/test/test_utils.h"
#include "services/network/test/test_url_loader_factory.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/input_method/input_method_configuration.h"
#include "ui/base/ime/ash/mock_input_method_manager_impl.h"
#endif

namespace {

std::unique_ptr<KeyedService> CreateAutocompleteClassifier(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<AutocompleteClassifier>(
      std::make_unique<AutocompleteController>(
          std::make_unique<ChromeAutocompleteProviderClient>(profile),
          AutocompleteControllerConfig{
              .provider_types =
                  AutocompleteClassifier::DefaultOmniboxProviders()}),
      std::make_unique<TestSchemeClassifier>());
}

}  // namespace

TestWithBrowserView::~TestWithBrowserView() = default;

void TestWithBrowserView::SetUp() {
#if BUILDFLAG(IS_CHROMEOS)
  ash::input_method::InitializeForTesting(
      new ash::input_method::MockInputMethodManagerImpl);
#endif
  BrowserWithTestWindowTest::SetUp();
  browser_view_ = static_cast<BrowserView*>(browser()->window());
}

void TestWithBrowserView::TearDown() {
  // Destroy Browsers directly managed by TestWithBrowserView.
  for (std::unique_ptr<Browser>& browser : additional_browsers_) {
    browser->tab_strip_model()->CloseAllTabs();
    browser.reset();
  }

  // Because CreateBrowserWindow() is overridden to return null, a real
  // BrowserView is created. Nullify the BrowserView pointer before destroying
  // the Browser to avoid dangling pointers.
  browser_view_->browser()->tab_strip_model()->CloseAllTabs();
  browser_view_ = nullptr;
  ASSERT_TRUE(release_browser());

  BrowserWithTestWindowTest::TearDown();
#if BUILDFLAG(IS_CHROMEOS)
  ash::input_method::Shutdown();
#endif
}

TestingProfile* TestWithBrowserView::CreateProfile(
    const std::string& profile_name) {
  TestingProfile* profile =
      BrowserWithTestWindowTest::CreateProfile(profile_name);
  // TemplateURLService is normally null during testing. Instant extended
  // needs this service so set a custom factory function.
  TemplateURLServiceFactory::GetInstance()->SetTestingFactory(
      profile,
      TemplateURLServiceTestUtil::GetTemplateURLServiceTestingFactory());
  // TODO(jamescook): Eliminate this by introducing a mock toolbar or mock
  // location bar.
  AutocompleteClassifierFactory::GetInstance()->SetTestingFactory(
      profile, base::BindRepeating(&CreateAutocompleteClassifier));
  TemplateURLServiceFactory::GetInstance()->SetTestingFactory(
      profile,
      TemplateURLServiceTestUtil::GetTemplateURLServiceTestingFactory());
  // ToolbarActionsModel must exist before the toolbar initializes the
  // extensions area.
  extensions::LoadErrorReporter::Init(/*enable_noisy_errors=*/false);
  extensions::extension_action_test_util::CreateToolbarModelForProfile(profile);

  // Configure the GaiaCookieManagerService to return no accounts.
  signin::SetListAccountsResponseHttpNotFound(test_url_loader_factory());
  return profile;
}

std::unique_ptr<BrowserWindow> TestWithBrowserView::CreateBrowserWindow() {
  // Allow BrowserWithTestWindowTest to use Browser to create the default
  // BrowserView and BrowserWidget.
  return nullptr;
}

TestingProfile::TestingFactories TestWithBrowserView::GetTestingFactories() {
  return {TestingProfile::TestingFactory{
      ChromeSigninClientFactory::GetInstance(),
      base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                          test_url_loader_factory())}};
}

Browser* TestWithBrowserView::CreateBrowserWithBrowserView(
    Profile* profile,
    Browser::Type browser_type) {
  additional_browsers_.emplace_back(CreateBrowser(
      profile, browser_type, /*hosted_app=*/false, /*browser_window=*/nullptr));
  return additional_browsers_.back().get();
}
