// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/extensions/extensions_hats_handler.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "components/version_info/version_info.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extension_prefs_factory.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension_id.h"

namespace extensions {

ExtensionsHatsHandler::ExtensionsHatsHandler(Profile* profile)
    : profile_(profile) {
  InitExtensionStats();
}

ExtensionsHatsHandler::~ExtensionsHatsHandler() = default;

void ExtensionsHatsHandler::RegisterMessages() {
  // Usage of base::Unretained(this) is safe, because web_ui() owns `this` and
  // won't release ownership until destruction.
  web_ui()->RegisterMessageCallback(
      "extensionsSafetyHubTriggerSurvey",
      base::BindRepeating(
          &ExtensionsHatsHandler::HandleExtensionsSafetyHubTriggerSurvey,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "extensionsSafetyHubExtensionKept",
      base::BindRepeating(
          &ExtensionsHatsHandler::HandleExtensionsSafetyHubExtensionKept,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "extensionsSafetyHubExtensionRemoved",
      base::BindRepeating(
          &ExtensionsHatsHandler::HandleExtensionsSafetyHubExtensionRemoved,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "extensionsSafetyHubNonTriggerExtensionRemoved",
      base::BindRepeating(
          &ExtensionsHatsHandler::
              HandleExtensionsSafetyHubNonTriggerExtensionRemoved,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "extensionsSafetyHubRemoveAll",
      base::BindRepeating(
          &ExtensionsHatsHandler::HandleExtensionsSafetyHubRemoveAll,
          base::Unretained(this)));
}

void ExtensionsHatsHandler::HandleExtensionsSafetyHubTriggerSurvey(
    const base::Value::List& args) {
  content::WebContentsObserver::Observe(web_ui()->GetWebContents());
  RequestHatsSurvey(true, CreateSurveyStringsForNoInteraction());
}

void ExtensionsHatsHandler::HandleExtensionsSafetyHubExtensionKept(
    const base::Value::List& args) {
  content::WebContentsObserver::Observe(web_ui()->GetWebContents());
  number_of_extensions_kept_++;
}

void ExtensionsHatsHandler::HandleExtensionsSafetyHubExtensionRemoved(
    const base::Value::List& args) {
  content::WebContentsObserver::Observe(web_ui()->GetWebContents());
  number_of_triggering_extensions_removed_++;
}

void ExtensionsHatsHandler::HandleExtensionsSafetyHubNonTriggerExtensionRemoved(
    const base::Value::List& args) {
  content::WebContentsObserver::Observe(web_ui()->GetWebContents());
  number_of_nontriggering_extensions_removed_++;
}

void ExtensionsHatsHandler::HandleExtensionsSafetyHubRemoveAll(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  auto number_extensions_removed = args[0].GetInt();
  content::WebContentsObserver::Observe(web_ui()->GetWebContents());
  number_of_triggering_extensions_removed_ += number_extensions_removed;
}

void ExtensionsHatsHandler::InitExtensionStats() {
  time_extension_page_opened_ = base::Time::Now();
  time_since_last_extension_install_ = base::TimeDelta::Max();
  const ExtensionSet installed_extensions =
      ExtensionRegistry::Get(profile_)->GenerateInstalledExtensionsSet();
  ExtensionPrefs* extension_prefs =
      ExtensionPrefsFactory::GetForBrowserContext(profile_);
  client_channel_ =
      std::string(version_info::GetChannelString(chrome::GetChannel()));
  for (const auto& extension : installed_extensions) {
    if (extension->location() == mojom::ManifestLocation::kComponent ||
        extension->location() == mojom::ManifestLocation::kExternalComponent) {
      continue;
    }
    number_installed_extensions_on_load_++;
    base::TimeDelta time_since_install = base::Time::Now() -
                         extension_prefs->GetFirstInstallTime(extension->id());
    avg_extension_age_ += time_since_install;
    if (time_since_last_extension_install_ > time_since_install) {
      time_since_last_extension_install_ = time_since_install;
    }
  }
  avg_extension_age_ =
    number_installed_extensions_on_load_ > 0 ?
        avg_extension_age_ / number_installed_extensions_on_load_ :
        base::TimeDelta::Min();
}

SurveyStringData ExtensionsHatsHandler::CreateSurveyStringsForNoInteraction() {
  base::TimeDelta time_spent_on_page =
      base::Time::Now() - time_extension_page_opened_;
  // SurveyStringData for surveys on page load.
  return {{"Average extension age in days",
           base::NumberToString(avg_extension_age_.InDays())},
          {"Time since last extension was installed in days",
           base::NumberToString(time_since_last_extension_install_.InDays())},
          {"Number of extensions installed",
           base::NumberToString(number_installed_extensions_on_load_)},
          {"Time on extension page in minutes",
           base::NumberToString(time_spent_on_page.InMinutes())},
          {"Client Channel", client_channel_}};
}

void ExtensionsHatsHandler::RequestHatsSurvey(bool require_same_origin,
                                              SurveyStringData string_data) {
  HatsService* hats_service = HatsServiceFactory::GetForProfile(
      profile_, /* create_if_necessary = */ true);
  // The HaTS service may not be available for the profile, for example if it
  // is a guest profile.
  if (!hats_service) {
    return;
  }
  hats_service->LaunchDelayedSurveyForWebContents(
      "HappinessTrackingSurveysExtensionsSafetyHub", web_ui()->GetWebContents(),
      features::kHappinessTrackingSurveysExtensionsSafetyHubTime.Get()
          .InMilliseconds(),
      {}, string_data, require_same_origin);
}

void ExtensionsHatsHandler::PrimaryPageChanged(content::Page& page) {
  Browser* browser = chrome::FindBrowserWithTab(web_ui()->GetWebContents());
  // We want to check that the primary page change was not a window or tab being
  // closed.
  if ((!browser || browser->tab_strip_model()->empty() ||
       web_ui()->GetWebContents()->IsBeingDestroyed()) &&
      !test_navigation_) {
    return;
  }
  if (number_of_triggering_extensions_removed_ + number_of_extensions_kept_) {
    base::TimeDelta time_spent_on_page =
        base::Time::Now() - time_extension_page_opened_;
    // SurveyStringData for when a user interacts with the review
    // panel.
    SurveyStringData survey_data = {
        {"Average extension age in days",
         base::NumberToString(avg_extension_age_.InDays())},
        {"Time since last extension was installed in days",
         base::NumberToString(time_since_last_extension_install_.InDays())},
        {"Number of extensions installed",
         base::NumberToString(number_installed_extensions_on_load_)},
        {"Time on extension page in minutes",
         base::NumberToString(time_spent_on_page.InMinutes())},
        {"Number of extensions removed",
         base::NumberToString(number_of_triggering_extensions_removed_)},
        {"Number of extensions kept",
         base::NumberToString(number_of_extensions_kept_)},
        {"Number of non-trigger extensions removed",
         base::NumberToString(number_of_nontriggering_extensions_removed_)},
        {"Client Channel", client_channel_}};
    RequestHatsSurvey(true, survey_data);
  }
}

}  // namespace extensions
