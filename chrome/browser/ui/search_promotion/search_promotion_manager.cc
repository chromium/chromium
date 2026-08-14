// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_promotion/search_promotion_manager.h"

#include <memory>
#include <string_view>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/platform_experience/delegated_tasks/delegated_task_runner.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/search_promotion/register_search_promotion_task.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "components/feature_engagement/public/event_constants.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/segmentation_platform/embedder/default_model/chrome_user_engagement.h"
#include "components/segmentation_platform/public/result.h"
#include "components/segmentation_platform/public/segmentation_platform_service.h"
#include "url/gurl.h"

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(DefaultBrowserType)
enum class DefaultBrowserType {
  kUnknown = 0,
  kChrome = 1,
  kEdge = 2,
  kSafari = 3,
  kFirefox = 4,
  kOther = 5,
  kMaxValue = kOther,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/search/enums.xml:SearchPromotionDefaultBrowserType)

}  // namespace

SearchPromotionManager::SearchPromotionManager(
    Profile& profile,
    CreateTaskRunnerCallback create_task_runner_callback)
    : profile_(profile),
      create_task_runner_callback_(std::move(create_task_runner_callback)) {
  // SearchPromotionManager is currently a Windows-only feature intended to
  // promote specific search-related behaviors. On other platforms, the
  // manager remains inert.

  // Cache feature state to avoid repeated lookups on every navigation.
  is_promo_allowed_ = base::FeatureList::IsEnabled(
      feature_engagement::kIPHSearchPromotionFeature);
  if (is_promo_allowed_) {
    std::string arm_str = feature_engagement::kSearchPromotionArm.Get();
    if (arm_str == feature_engagement::kSearchPromotionArmA) {
      arm_ = feature_engagement::kSearchPromotionArmA;
    } else if (arm_str == feature_engagement::kSearchPromotionArmB) {
      arm_ = feature_engagement::kSearchPromotionArmB;
    } else if (arm_str == feature_engagement::kSearchPromotionArmC) {
      arm_ = feature_engagement::kSearchPromotionArmC;
    } else if (arm_str == feature_engagement::kSearchPromotionArmD) {
      arm_ = feature_engagement::kSearchPromotionArmD;
    } else {
      // If no valid experiment arm is specified, disable the promotion.
      is_promo_allowed_ = false;
    }
  }

  if (is_promo_allowed_) {
    QueryEngagementLevel();
  }
}

SearchPromotionManager::~SearchPromotionManager() = default;

void SearchPromotionManager::OnTargetURLVisited(
    BrowserUserEducationInterface& user_education) {
  if (!is_promo_allowed_) {
    return;
  }

  if (!IsEngagementEligibleForArm()) {
    return;
  }

  // Check the default browser state asynchronously to avoid blocking the UI
  // thread with registry access, and log it to
  // SearchPromotion.DefaultBrowserState.
  base::MakeRefCounted<shell_integration::DefaultBrowserWorker>()
      ->StartCheckIsDefault(
          base::BindOnce(&SearchPromotionManager::RecordDefaultBrowserState,
                         weak_ptr_factory_.GetWeakPtr()));

  user_education::FeaturePromoParams params(
      feature_engagement::kIPHSearchPromotionFeature);
  params.close_callback = base::BindOnce(&SearchPromotionManager::OnPromoClosed,
                                         weak_ptr_factory_.GetWeakPtr());

  user_education.MaybeShowFeaturePromo(std::move(params));
}

void SearchPromotionManager::RecordDefaultBrowserState(
    shell_integration::DefaultWebClientState state) {
  // Records the user's default browser state asynchronously. Uses the
  // 3-argument template overload of base::UmaHistogramEnumeration because
  // DefaultWebClientState lacks kMaxValue and defines NUM_DEFAULT_STATES as the
  // boundary instead.
  base::UmaHistogramEnumeration(
      "Search.SearchPromotion.DefaultBrowserState", state,
      shell_integration::DefaultWebClientState::NUM_DEFAULT_STATES);
}

void SearchPromotionManager::OnPromoAccepted() {
  // Prevent duplicate acceptance handling if triggered multiple times (e.g.
  // accidental double clicks).
  if (was_accepted_) {
    return;
  }
  was_accepted_ = true;
  if (auto* tracker = feature_engagement::TrackerFactory::GetForBrowserContext(
          &profile_.get())) {
    tracker->NotifyEvent(feature_engagement::events::kSearchPromotionAccepted);
  }
  if (arm_ == feature_engagement::kSearchPromotionArmA) {
    PerformOpen();
  } else if (arm_ == feature_engagement::kSearchPromotionArmB ||
             arm_ == feature_engagement::kSearchPromotionArmC ||
             arm_ == feature_engagement::kSearchPromotionArmD) {
    PerformInstall();
  }
}

void SearchPromotionManager::OnPromoClosed() {
  // Capture current class state to determine if the user accepted the promo.
  bool accepted = was_accepted_;
  // Reset the accepted flag. This prevents subsequent triggers from
  // reusing a stale "accepted" state if the promo is closed and re-shown.
  was_accepted_ = false;

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::BEST_EFFORT},
      base::BindOnce([]() {
        return shell_integration::GetApplicationNameForScheme(
            GURL("https://google.com"));
      }),
      base::BindOnce(&SearchPromotionManager::OnDefaultBrowserNameRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), accepted, arm_));
}

void SearchPromotionManager::OnDefaultBrowserNameRetrieved(
    bool accepted,
    std::string_view arm,
    const std::u16string& name) {
  // Retrieve the localized name of the default browser application and map it
  // to a categorized DefaultBrowserType enum.
  DefaultBrowserType type = DefaultBrowserType::kOther;
  if (name.empty()) {
    type = DefaultBrowserType::kUnknown;
  } else if (name.contains(u"Chrome") || name.contains(u"Chromium")) {
    type = DefaultBrowserType::kChrome;
  } else if (name.contains(u"Edge")) {
    type = DefaultBrowserType::kEdge;
  } else if (name.contains(u"Safari")) {
    type = DefaultBrowserType::kSafari;
  } else if (name.contains(u"Firefox")) {
    type = DefaultBrowserType::kFirefox;
  }

  std::string_view arm_suffix;
  if (arm == feature_engagement::kSearchPromotionArmA) {
    arm_suffix = "ArmA";
  } else if (arm == feature_engagement::kSearchPromotionArmB) {
    arm_suffix = "ArmB";
  } else if (arm == feature_engagement::kSearchPromotionArmC) {
    arm_suffix = "ArmC";
  } else if (arm == feature_engagement::kSearchPromotionArmD) {
    arm_suffix = "ArmD";
  }

  if (!arm_suffix.empty()) {
    std::string histogram_name =
        base::StrCat({"Search.SearchPromotion.DefaultBrowserType.",
                      accepted ? "Accepted." : "Dismissed.", arm_suffix});
    base::UmaHistogramEnumeration(histogram_name, type);
  }
}

bool SearchPromotionManager::IsPromoAllowedForTesting() const {
  return is_promo_allowed_;
}

std::string_view SearchPromotionManager::GetEngagementLabelForTesting() const {
  return engagement_label_;
}

bool SearchPromotionManager::IsEngagementEligibleForArm() const {
  if (arm_ == feature_engagement::kSearchPromotionArmA ||
      arm_ == feature_engagement::kSearchPromotionArmB) {
    return engagement_label_ == kEngagementLabelOneDay ||
           engagement_label_ == kEngagementLabelLow;
  }
  if (arm_ == feature_engagement::kSearchPromotionArmC) {
    return engagement_label_ == kEngagementLabelMedium;
  }
  if (arm_ == feature_engagement::kSearchPromotionArmD) {
    return engagement_label_ == kEngagementLabelPower;
  }
  return false;
}

void SearchPromotionManager::QueryEngagementLevel() {
  // SearchPromotionManagerFactory instantiates this KeyedService only for
  // regular, non-incognito profiles using ProfileSelection::kOriginalOnly
  // in chrome/browser/ui/search_promotion/search_promotion_manager_factory.cc.
  auto* service =
      segmentation_platform::SegmentationPlatformServiceFactory::GetForProfile(
          &profile_.get());
  if (!service) {
    return;
  }

  segmentation_platform::PredictionOptions options;
  options.on_demand_execution = false;

  // Query the segmentation platform for the user engagement result.
  service->GetClassificationResult(
      segmentation_platform::ChromeUserEngagement::kChromeUserEngagementKey,
      options, nullptr,
      base::BindOnce(&SearchPromotionManager::OnEngagementResultRetrieved,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SearchPromotionManager::OnEngagementResultRetrieved(
    const segmentation_platform::ClassificationResult& result) {
  if (result.status == segmentation_platform::PredictionStatus::kSucceeded &&
      !result.ordered_labels.empty()) {
    // The computed classification label is at index 0.
    engagement_label_ = result.ordered_labels[0];
  }
}

void SearchPromotionManager::RunRegisterTask(
    std::unique_ptr<RegisterSearchPromotionTask> task) {
  // Guard against invalid tasks or tasks already in flight.
  if (!task || task_runner_ || !is_promo_allowed_) {
    return;
  }

  task_runner_ = create_task_runner_callback_.Run();
  task_runner_->Run(std::move(task),
                    feature_engagement::kSearchPromotionMinPehVersion.Get(),
                    base::BindOnce(&SearchPromotionManager::OnTaskCompleted,
                                   weak_ptr_factory_.GetWeakPtr()));
}

void SearchPromotionManager::OnTaskCompleted(
    platform_experience::DelegatedTaskResult result) {
  if (result.exit_code_or_status.has_value()) {
    auto exit_code = static_cast<SearchPromotionExitCode>(
        result.exit_code_or_status.value());
    std::string_view variant = SearchPromotionExitCodeToString(exit_code);
    if (!variant.empty()) {
      base::UmaHistogramSparse("Search.SearchPromotion.DelegatedTaskExitCode",
                               static_cast<int>(exit_code));
      base::UmaHistogramMediumTimes(
          base::StrCat({"Search.SearchPromotion.Duration.", variant}),
          result.execution_time);
    }
  }
  task_runner_.reset();
}

void SearchPromotionManager::PerformOpen() {
  std::string store_url_str =
      feature_engagement::kSearchPromotionStoreUrl.Get();
  GURL store_url(store_url_str);
  if (!store_url.is_valid()) {
    return;
  }
  RunRegisterTask(std::make_unique<RegisterSearchPromotionTask>(
      /*post_install_url=*/store_url, /*extension_id=*/""));
}

void SearchPromotionManager::PerformInstall() {
  std::string extension_id =
      feature_engagement::kSearchPromotionExtensionId.Get();
  std::string instructions_url_str =
      feature_engagement::kSearchPromotionInstructionsUrl.Get();
  GURL instructions_url(instructions_url_str);
  if (extension_id.empty() || !instructions_url.is_valid()) {
    return;
  }
  RunRegisterTask(std::make_unique<RegisterSearchPromotionTask>(
      /*post_install_url=*/instructions_url, /*extension_id=*/extension_id));
}
