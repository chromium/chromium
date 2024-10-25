// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/whats_new/whats_new_handler.h"

#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_fetcher.h"
#include "chrome/browser/ui/webui/whats_new/whats_new_util.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_version.h"
#include "chrome/common/webui_url_constants.h"
#include "components/user_education/common/user_education_features.h"
#include "components/user_education/webui/whats_new_registry.h"
#include "components/variations/service/variations_service.h"
#include "components/variations/service/variations_service_utils.h"
#include "url/gurl.h"

WhatsNewHandler::WhatsNewHandler(
    mojo::PendingReceiver<whats_new::mojom::PageHandler> receiver,
    mojo::PendingRemote<whats_new::mojom::Page> page,
    Profile* profile,
    content::WebContents* web_contents,
    const base::Time& navigation_start_time,
    const whats_new::WhatsNewRegistry* whats_new_registry)
    : profile_(profile),
      web_contents_(web_contents),
      navigation_start_time_(navigation_start_time),
      whats_new_registry_(CHECK_DEREF(whats_new_registry)),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {}

WhatsNewHandler::~WhatsNewHandler() = default;

void WhatsNewHandler::RecordTimeToLoadContent(base::Time time) {
  base::UmaHistogramTimes("UserEducation.WhatsNew.TimeToLoadContent",
                          time - navigation_start_time_);
}

void WhatsNewHandler::RecordVersionPageLoaded(bool is_auto_open) {
  base::RecordAction(base::UserMetricsAction("UserEducation.WhatsNew.Shown"));
  base::RecordAction(
      base::UserMetricsAction("UserEducation.WhatsNew.VersionShown"));
  if (!is_auto_open) {
    base::RecordAction(base::UserMetricsAction(
        "UserEducation.WhatsNew.ShownByManualNavigation"));
  }
}

void WhatsNewHandler::RecordEditionPageLoaded(const std::string& page_uid,
                                              bool is_auto_open) {
  if (user_education::features::IsWhatsNewV2()) {
    // Store that this edition has been used for this milestone.
    whats_new_registry_->SetEditionUsed(page_uid);
  }

  base::RecordAction(base::UserMetricsAction("UserEducation.WhatsNew.Shown"));

  base::RecordAction(
      base::UserMetricsAction("UserEducation.WhatsNew.EditionShown"));

  if (!page_uid.empty()) {
    std::string action_name = "UserEducation.WhatsNew.EditionShown.";
    action_name.append(page_uid);
    base::RecordComputedAction(action_name);
  }

  if (!is_auto_open) {
    base::RecordAction(base::UserMetricsAction(
        "UserEducation.WhatsNew.ShownByManualNavigation"));
  }
}

void WhatsNewHandler::RecordModuleImpression(
    const std::string& module_name,
    whats_new::mojom::ModulePosition position) {
  base::RecordAction(
      base::UserMetricsAction("UserEducation.WhatsNew.ModuleShown"));

  std::string action_name = "UserEducation.WhatsNew.ModuleShown.";
  action_name.append(module_name);
  base::RecordComputedAction(action_name);

  std::string histogram_name = "UserEducation.WhatsNew.ModuleShown.";
  histogram_name.append(module_name);
  base::UmaHistogramEnumeration(histogram_name, position);
}

void WhatsNewHandler::RecordExploreMoreToggled(bool expanded) {
  base::UmaHistogramBoolean("UserEducation.WhatsNew.ExploreMoreExpanded",
                            expanded);
}

void WhatsNewHandler::RecordScrollDepth(whats_new::mojom::ScrollDepth depth) {
  base::UmaHistogramEnumeration("UserEducation.WhatsNew.ScrollDepth", depth);
}

void WhatsNewHandler::RecordTimeOnPage(base::TimeDelta time) {
  base::UmaHistogramMediumTimes("UserEducation.WhatsNew.TimeOnPage", time);
}

void WhatsNewHandler::RecordModuleLinkClicked(
    const std::string& module_name,
    whats_new::mojom::ModulePosition position) {
  base::RecordAction(
      base::UserMetricsAction("UserEducation.WhatsNew.ModuleLinkClicked"));

  std::string action_name = "UserEducation.WhatsNew.ModuleLinkClicked.";
  action_name.append(module_name);
  base::RecordComputedAction(action_name);

  std::string histogram_name = "UserEducation.WhatsNew.ModuleLinkClicked.";
  histogram_name.append(module_name);
  base::UmaHistogramEnumeration(histogram_name, position);
}

void WhatsNewHandler::RecordBrowserCommandExecuted() {
  base::RecordAction(
      base::UserMetricsAction("UserEducation.WhatsNew.BrowserCommandExecuted"));
}

void WhatsNewHandler::RecordModuleVideoStarted(
    const std::string& module_name,
    whats_new::mojom::ModulePosition position) {
  std::string histogram_name = "UserEducation.WhatsNew.VideoStarted.";
  histogram_name.append(module_name);
  base::UmaHistogramEnumeration(histogram_name, position);
}

void WhatsNewHandler::RecordModuleVideoEnded(
    const std::string& module_name,
    whats_new::mojom::ModulePosition position) {
  std::string histogram_name = "UserEducation.WhatsNew.VideoEnded.";
  histogram_name.append(module_name);
  base::UmaHistogramEnumeration(histogram_name, position);
}

void WhatsNewHandler::RecordModulePlayClicked(
    const std::string& module_name,
    whats_new::mojom::ModulePosition position) {
  base::RecordAction(
      base::UserMetricsAction("UserEducation.WhatsNew.PlayClicked"));

  std::string histogram_name = "UserEducation.WhatsNew.PlayClicked.";
  histogram_name.append(module_name);
  base::UmaHistogramEnumeration(histogram_name, position);
}

void WhatsNewHandler::RecordModulePauseClicked(
    const std::string& module_name,
    whats_new::mojom::ModulePosition position) {
  base::RecordAction(
      base::UserMetricsAction("UserEducation.WhatsNew.PauseClicked"));

  std::string histogram_name = "UserEducation.WhatsNew.PauseClicked.";
  histogram_name.append(module_name);
  base::UmaHistogramEnumeration(histogram_name, position);
}

void WhatsNewHandler::RecordModuleRestartClicked(
    const std::string& module_name,
    whats_new::mojom::ModulePosition position) {
  base::RecordAction(
      base::UserMetricsAction("UserEducation.WhatsNew.RestartClicked"));

  std::string histogram_name = "UserEducation.WhatsNew.RestartClicked.";
  histogram_name.append(module_name);
  base::UmaHistogramEnumeration(histogram_name, position);
}

void WhatsNewHandler::GetServerUrl(bool is_staging,
                                   GetServerUrlCallback callback) {
  GURL result = GURL("");
  if (!whats_new::IsRemoteContentDisabled()) {
    if (user_education::features::IsWhatsNewV2()) {
      result =
          whats_new::GetV2ServerURLForRender(*whats_new_registry_, is_staging);
    } else {
      result = whats_new::GetServerURL(true, is_staging);
    }
  }
  std::move(callback).Run(result);

  TryShowHatsSurveyWithTimeout();
}

void WhatsNewHandler::TryShowHatsSurveyWithTimeout() {
  HatsService* hats_service =
      HatsServiceFactory::GetForProfile(profile_,
                                        /* create_if_necessary = */ true);
  if (!hats_service) {
    return;
  }

  // Look for a survey override associated with any editions that we
  // requested from the server.
  const auto survey_override = whats_new_registry_->GetActiveEditionSurvey();
  if (survey_override.has_value()) {
    hats_service->LaunchDelayedSurveyForWebContents(
        kHatsSurveyTriggerWhatsNew, web_contents_,
        features::kHappinessTrackingSurveysForDesktopWhatsNewTime.Get()
            .InMilliseconds(),
        /*product_specific_bits_data=*/{},
        /*product_specific_string_data=*/{},
        /*navigation_behaviour=*/HatsService::REQUIRE_SAME_ORIGIN,
        base::DoNothing(), base::DoNothing(), survey_override.value());
  } else {
    hats_service->LaunchDelayedSurveyForWebContents(
        kHatsSurveyTriggerWhatsNew, web_contents_,
        features::kHappinessTrackingSurveysForDesktopWhatsNewTime.Get()
            .InMilliseconds(),
        /*product_specific_bits_data=*/{},
        /*product_specific_string_data=*/{},
        /*navigation_behaviour=*/HatsService::REQUIRE_SAME_ORIGIN);
  }
}
