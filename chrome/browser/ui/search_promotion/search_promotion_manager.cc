// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_promotion/search_promotion_manager.h"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/task/thread_pool.h"
#include "base/version.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/platform_experience/delegated_tasks/delegated_task_runner.h"
#include "chrome/browser/platform_experience/delegated_tasks/peh_launcher.h"
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

// Checks if a verified PEH binary exists on disk and meets `min_version_str`.
SearchPromotionPehEligibility CheckPehEligibility(
    std::unique_ptr<platform_experience::PehLauncher> launcher,
    std::string min_version_str) {
  if (!launcher) {
    return SearchPromotionPehEligibility::kLauncherUnavailable;
  }
  const base::Version min_version(min_version_str);
  if (!min_version.IsValid()) {
    return SearchPromotionPehEligibility::kMinVersionInvalid;
  }
  const base::FilePath path = launcher->GetBinaryPath();
  if (path.empty()) {
    return SearchPromotionPehEligibility::kBinaryNotFound;
  }
  if (!launcher->IsBinaryVerified(path)) {
    return SearchPromotionPehEligibility::kBinaryNotVerified;
  }
  const base::Version version = launcher->GetBinaryVersion(path);
  if (!version.IsValid()) {
    return SearchPromotionPehEligibility::kBinaryVersionInvalid;
  }
  if (version < min_version) {
    return SearchPromotionPehEligibility::kBinaryVersionTooLow;
  }
  return SearchPromotionPehEligibility::kEligible;
}

}  // namespace

SearchPromotionManager::SearchPromotionManager(
    Profile& profile,
    CreateTaskRunnerCallback create_task_runner_callback,
    CreatePehLauncherCallback create_peh_launcher_callback)
    : profile_(profile),
      create_task_runner_callback_(std::move(create_task_runner_callback)) {
  // SearchPromotionManager is currently a Windows-only feature intended to
  // promote specific search-related behaviors. On other platforms, the
  // manager remains inert.

  // Cache feature state to avoid repeated lookups on every navigation.
  if (base::FeatureList::IsEnabled(
          feature_engagement::kIPHSearchPromotionFeature)) {
    action_ = feature_engagement::kSearchPromotionAction.Get();
    ParseCohorts(feature_engagement::kSearchPromotionCohort.Get());
  }

  if (action_ != feature_engagement::SearchPromotionAction::kDisabled) {
    QueryEngagementLevel();
    QueryPehEligibility(std::move(create_peh_launcher_callback));
  }
}

SearchPromotionManager::~SearchPromotionManager() = default;

void SearchPromotionManager::OnTargetURLVisited(
    BrowserUserEducationInterface& user_education) {
  if (action_ == feature_engagement::SearchPromotionAction::kDisabled) {
    return;
  }

  // Record baseline evaluation across all evaluated users (including Control).
  base::UmaHistogramBoolean("Search.SearchPromotion.Evaluated", true);

  if (!is_peh_eligible_.value_or(false) || !IsEngagementEligible()) {
    return;
  }

  // Check the default browser state asynchronously to avoid blocking the UI
  // thread with registry access, and log it to
  // SearchPromotion.DefaultBrowserState.
  base::MakeRefCounted<shell_integration::DefaultBrowserWorker>()
      ->StartCheckIsDefault(
          base::BindOnce(&SearchPromotionManager::RecordDefaultBrowserState,
                         weak_ptr_factory_.GetWeakPtr()));

  // Control group: record default browser state for parity, but do not show UI.
  if (action_ == feature_engagement::SearchPromotionAction::kControl) {
    return;
  }

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

void SearchPromotionManager::ExecuteAction() {
  switch (action_) {
    case feature_engagement::SearchPromotionAction::kOpen:
      PerformOpen();
      break;
    case feature_engagement::SearchPromotionAction::kInstall:
      PerformInstall();
      break;
    case feature_engagement::SearchPromotionAction::kDisabled:
    case feature_engagement::SearchPromotionAction::kControl:
      break;
  }
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
  ExecuteAction();
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
                     weak_ptr_factory_.GetWeakPtr(), accepted));
}

void SearchPromotionManager::OnDefaultBrowserNameRetrieved(
    bool accepted,
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

  std::string_view action_suffix;
  switch (action_) {
    case feature_engagement::SearchPromotionAction::kOpen:
      action_suffix = "Open";
      break;
    case feature_engagement::SearchPromotionAction::kInstall:
      action_suffix = "Install";
      break;
    case feature_engagement::SearchPromotionAction::kDisabled:
    case feature_engagement::SearchPromotionAction::kControl:
      break;
  }

  if (!action_suffix.empty()) {
    std::string histogram_name =
        base::StrCat({"Search.SearchPromotion.DefaultBrowserType.",
                      accepted ? "Accepted." : "Dismissed.", action_suffix});
    base::UmaHistogramEnumeration(histogram_name, type);
  }
}

bool SearchPromotionManager::IsPromoAllowedForTesting() const {
  return action_ == feature_engagement::SearchPromotionAction::kOpen ||
         action_ == feature_engagement::SearchPromotionAction::kInstall;
}

std::string_view SearchPromotionManager::GetEngagementLabelForTesting() const {
  return engagement_label_;
}

std::optional<bool> SearchPromotionManager::IsPehEligibleForTesting() const {
  return is_peh_eligible_;
}

const base::flat_set<feature_engagement::SearchPromotionCohort>&
SearchPromotionManager::GetAllowedCohortsForTesting() const {
  return allowed_cohorts_;
}

void SearchPromotionManager::QueryPehEligibility(
    CreatePehLauncherCallback create_peh_launcher_callback) {
  auto launcher = create_peh_launcher_callback
                      ? std::move(create_peh_launcher_callback).Run()
                      : std::make_unique<platform_experience::PehLauncher>();
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&CheckPehEligibility, std::move(launcher),
                     feature_engagement::kSearchPromotionMinPehVersion.Get()),
      base::BindOnce(&SearchPromotionManager::OnPehEligibilityRetrieved,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SearchPromotionManager::OnPehEligibilityRetrieved(
    SearchPromotionPehEligibility eligibility) {
  is_peh_eligible_ = (eligibility == SearchPromotionPehEligibility::kEligible);
  base::UmaHistogramEnumeration("Search.SearchPromotion.PehEligible",
                                eligibility);
}

void SearchPromotionManager::ParseCohorts(std::string_view cohort_param) {
  for (std::string_view token :
       base::SplitStringPiece(cohort_param, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    if (token == feature_engagement::kSearchPromotionCohortAll) {
      allowed_cohorts_.insert(feature_engagement::SearchPromotionCohort::kAll);
    } else if (token == feature_engagement::kSearchPromotionCohortLow) {
      allowed_cohorts_.insert(feature_engagement::SearchPromotionCohort::kLow);
    } else if (token == feature_engagement::kSearchPromotionCohortMedium) {
      allowed_cohorts_.insert(
          feature_engagement::SearchPromotionCohort::kMedium);
    } else if (token == feature_engagement::kSearchPromotionCohortPower) {
      allowed_cohorts_.insert(
          feature_engagement::SearchPromotionCohort::kPower);
    }
  }

  // If empty, omitted, or if all tokens are unrecognized, safely default to
  // targeting all cohorts.
  if (allowed_cohorts_.empty()) {
    allowed_cohorts_.insert(feature_engagement::SearchPromotionCohort::kAll);
  }
}

bool SearchPromotionManager::IsEngagementEligible() const {
  if (allowed_cohorts_.contains(
          feature_engagement::SearchPromotionCohort::kAll)) {
    return true;
  }
  if (allowed_cohorts_.contains(
          feature_engagement::SearchPromotionCohort::kLow) &&
      (engagement_label_ == kEngagementLabelOneDay ||
       engagement_label_ == kEngagementLabelLow)) {
    return true;
  }
  if (allowed_cohorts_.contains(
          feature_engagement::SearchPromotionCohort::kMedium) &&
      engagement_label_ == kEngagementLabelMedium) {
    return true;
  }
  if (allowed_cohorts_.contains(
          feature_engagement::SearchPromotionCohort::kPower) &&
      engagement_label_ == kEngagementLabelPower) {
    return true;
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
  if (!task || task_runner_ ||
      action_ == feature_engagement::SearchPromotionAction::kDisabled) {
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
