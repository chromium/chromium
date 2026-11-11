// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/settings_localized_strings_provider.h"

#include <memory>
#include <string>

#include "base/memory/raw_ref.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/test_web_ui_data_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

namespace settings {
namespace {

using ::testing::AllOf;
using ::testing::HasSubstr;
using ::testing::Pointee;

// ScopedResourceOverride allows overriding localized strings in the shared
// instance of the resource bundle, while restoring the bundle state on
// destruction.
class ScopedResourceOverride {
 public:
  ScopedResourceOverride()
      : bundle_(ui::ResourceBundle::GetSharedInstance()),
        app_locale_(g_browser_process->GetApplicationLocale()) {}

  ScopedResourceOverride(const ScopedResourceOverride&) = delete;
  ScopedResourceOverride& operator=(const ScopedResourceOverride&) = delete;

  ~ScopedResourceOverride() {
    // Reloading the resources will discard all overrides.
    bundle_->ReloadLocaleResources(app_locale_);
  }

  void OverrideLocaleStringResource(int string_id, const std::u16string& str) {
    bundle_->OverrideLocaleStringResource(string_id, str);
  }

 private:
  const raw_ref<ui::ResourceBundle> bundle_;
  const std::string app_locale_;
};

TEST(SettingsLocalizedStringsProviderTest,
     AutofillStringsEscapeHtmlAttributes) {
  // Override locale strings before creating `BrowserTaskEnvironment` (and
  // restore them after `BrowserTaskEnvironment` is destroyed) so that
  // `ui::ResourceBundle` is only mutated while no background worker threads are
  // running.
  ScopedResourceOverride resource_override;
  resource_override.OverrideLocaleStringResource(IDS_SETTINGS_OPENS_IN_NEW_TAB,
                                                 u"Opens in a \"new\" tab");
  resource_override.OverrideLocaleStringResource(
      IDS_AUTOFILL_GMAIL_OTP_OPT_IN_LEARN_MORE_A11Y_NAME,
      u"Learn more about \"Autofill\" verification codes");

  content::BrowserTaskEnvironment task_environment;
  TestingProfile profile;
  content::TestWebContentsFactory web_contents_factory;
  content::WebContents* web_contents =
      web_contents_factory.CreateWebContents(&profile);

  std::unique_ptr<content::TestWebUIDataSource> html_source =
      content::TestWebUIDataSource::Create(chrome::kChromeUISettingsHost);
  AddAutofillStringsForTesting(html_source->GetWebUIDataSource(), &profile,
                               web_contents);

  const base::DictValue& strings = html_source->GetLocalizedStrings();
  constexpr char kExpectedAriaDescription[] =
      "aria-description=\"Opens in a &quot;new&quot; tab\"";
  constexpr char kExpectedAriaLabel[] =
      "aria-label=\"Learn more about &quot;Autofill&quot; verification codes\"";

  EXPECT_THAT(strings.FindString("enableGmailOtpFillingDescription"),
              Pointee(AllOf(HasSubstr(kExpectedAriaDescription),
                            HasSubstr(kExpectedAriaLabel))));
  EXPECT_THAT(strings.FindString("gmailOtpRequiredStep1"),
              Pointee(HasSubstr(kExpectedAriaDescription)));
  EXPECT_THAT(strings.FindString("gmailOtpRequiredStep2"),
              Pointee(HasSubstr(kExpectedAriaDescription)));
  EXPECT_THAT(strings.FindString("cardBenefitsToggleSublabel"),
              Pointee(HasSubstr(kExpectedAriaDescription)));
  EXPECT_THAT(strings.FindString("autofillPayOverTimeSettingsSublabel"),
              Pointee(HasSubstr(kExpectedAriaDescription)));
}

}  // namespace
}  // namespace settings
