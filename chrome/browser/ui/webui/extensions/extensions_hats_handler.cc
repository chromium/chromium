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
      "extensionsSafetyHubPanelShown",
      base::BindRepeating(
          &ExtensionsHatsHandler::HandleExtensionsSafetyHubPanelShown,
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

void ExtensionsHatsHandler::HandleExtensionsSafetyHubPanelShown(
    const base::Value::List& args) {
  content::WebContentsObserver::Observe(web_ui()->GetWebContents());
  review_panel_shown_ = args[0].GetBool();
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
    base::Time install_date =
        extension_prefs->GetFirstInstallTime(extension->id());
    if (install_date != base::Time()) {
      base::TimeDelta time_since_install = base::Time::Now() - install_date;
      avg_extension_age_ += time_since_install;
      if (time_since_last_extension_install_ > time_since_install) {
        time_since_last_extension_install_ = time_since_install;
      }
    }
  }
  avg_extension_age_ =
      number_installed_extensions_on_load_ > 0
          ? avg_extension_age_ / number_installed_extensions_on_load_
          : base::TimeDelta::Min();
}

SurveyStringData ExtensionsHatsHandler::CreateSurveyStrings() {
  time_spent_on_page_ = base::Time::Now() - time_extension_page_opened_;
  std::string review_panel_shown = review_panel_shown_ ? "True" : "False";
  // SurveyStringData for surveys on page load.
  return {{"Average extension age in days",
           base::NumberToString(avg_extension_age_.InDays())},
          {"Age of profile in days",
           base::NumberToString(
               (base::Time::Now() - profile_->GetCreationTime()).InDays())},
          {"Time since last extension was installed in days",
           base::NumberToString(time_since_last_extension_install_.InDays())},
          {"Number of extensions installed",
           base::NumberToString(number_installed_extensions_on_load_)},
          {"Time on extension page in seconds",
           base::NumberToString(time_spent_on_page_.InSeconds())},
          {"Extension review panel shown", review_panel_shown},
          {"Number of extensions removed",
           base::NumberToString(number_of_triggering_extensions_removed_)},
          {"Number of extensions kept",
           base::NumberToString(number_of_extensions_kept_)},
          {"Number of non-trigger extensions removed",
           base::NumberToString(number_of_nontriggering_extensions_removed_)},
          {"Client Channel", client_channel_}};
}

void ExtensionsHatsHandler::RequestHatsSurvey(SurveyStringData survey_data) {
  HatsService* hats_service = HatsServiceFactory::GetForProfile(
      profile_, /* create_if_necessary = */ true);
  // The HaTS service may not be available for the profile, for example if it
  // is a guest profile.
  if (!hats_service) {
    return;
  }
  hats_service->LaunchDelayedSurvey(kHatsSurveyTriggerExtensions, 0, {},
                                    survey_data);
}

void ExtensionsHatsHandler::HandleUserNavigation() {
  Browser* browser = chrome::FindBrowserWithTab(web_ui()->GetWebContents());
  // We check that the primary page change was not a window.
  if ((!browser || browser->tab_strip_model()->empty() ||
       web_ui()->GetWebContents()->IsBeingDestroyed() ||
       !base::FeatureList::IsEnabled(
           features::kHappinessTrackingSurveysExtensionsSafetyHub)) &&
      !test_navigation_) {
    return;
  }
  SurveyStringData survey_data = CreateSurveyStrings();
  // If the user has interacted with the review panel we will show them a
  // survey.
  bool panel_interaction =
      number_of_triggering_extensions_removed_ + number_of_extensions_kept_;
  features::ExtensionsSafetyHubHaTSArms survey_arm =
      features::kHappinessTrackingSurveysExtensionsSurveyArm.Get();
  if (panel_interaction &&
      survey_arm ==
          features::ExtensionsSafetyHubHaTSArms::kReviewPanelInteraction) {
    RequestHatsSurvey(survey_data);
  } else if (!panel_interaction &&
             static_cast<int>(survey_arm) == review_panel_shown_ &&
             time_spent_on_page_ >
                 features::kHappinessTrackingSurveysExtensionsSafetyHubTime
                     .Get()) {
    RequestHatsSurvey(survey_data);
  }
}

void ExtensionsHatsHandler::RenderFrameDeleted(
    content::RenderFrameHost* render_frame_host) {
  HandleUserNavigation();
}

void ExtensionsHatsHandler::PrimaryPageChanged(content::Page& page) {
  HandleUserNavigation();
}

}  // namespace extensions
