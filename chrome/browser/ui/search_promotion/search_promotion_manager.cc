// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/search_promotion/search_promotion_manager.h"

#include <string_view>
#include <utility>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/notimplemented.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/segmentation_platform/segmentation_platform_service_factory.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/segmentation_platform/public/constants.h"
#include "components/segmentation_platform/public/segment_selection_result.h"
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

SearchPromotionManager::SearchPromotionManager(Profile& profile)
    : profile_(profile) {
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
    } else {
      // If no valid experiment arm is specified, disable the promotion.
      is_promo_allowed_ = false;
    }
  }
}

SearchPromotionManager::~SearchPromotionManager() = default;

void SearchPromotionManager::OnTargetURLVisited(
    BrowserUserEducationInterface& user_education) {
  if (!is_promo_allowed_) {
    return;
  }

  if (!IsEngagementLowEnough()) {
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
  was_accepted_ = true;
  if (arm_ == feature_engagement::kSearchPromotionArmA) {
    PerformArmA();
  } else if (arm_ == feature_engagement::kSearchPromotionArmB) {
    PerformArmB();
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

  // `arm` is compared using std::string_view to avoid raw pointer
  // comparisons, which can fail if compiler optimizations assign different
  // addresses to the same string across translation units.
  if (accepted) {
    if (arm == feature_engagement::kSearchPromotionArmA) {
      base::UmaHistogramEnumeration(
          "Search.SearchPromotion.DefaultBrowserType.Accepted.ArmA", type);
    } else if (arm == feature_engagement::kSearchPromotionArmB) {
      base::UmaHistogramEnumeration(
          "Search.SearchPromotion.DefaultBrowserType.Accepted.ArmB", type);
    }
  } else {
    if (arm == feature_engagement::kSearchPromotionArmA) {
      base::UmaHistogramEnumeration(
          "Search.SearchPromotion.DefaultBrowserType.Dismissed.ArmA", type);
    } else if (arm == feature_engagement::kSearchPromotionArmB) {
      base::UmaHistogramEnumeration(
          "Search.SearchPromotion.DefaultBrowserType.Dismissed.ArmB", type);
    }
  }
}

bool SearchPromotionManager::IsPromoAllowedForTesting() const {
  return is_promo_allowed_;
}

bool SearchPromotionManager::IsEngagementLowEnoughForTesting() const {
  return IsEngagementLowEnough();
}

bool SearchPromotionManager::IsEngagementLowEnough() const {
  // Checking profile_->IsRegularProfile() is not required here.
  // SearchPromotionManagerFactory instantiates this KeyedService only for
  // regular, non-incognito profiles using ProfileSelection::kOriginalOnly
  // in chrome/browser/ui/search_promotion/search_promotion_manager_factory.cc.

  auto* service =
      segmentation_platform::SegmentationPlatformServiceFactory::GetForProfile(
          &profile_.get());
  if (!service) {
    return false;
  }

  // Query the segmentation platform for the cached low user engagement result
  // (defined as active fewer than 9 days out of the last 28 days). Caching on
  // startup ensures this lookup is fast and non-blocking.
  segmentation_platform::SegmentSelectionResult result =
      service->GetCachedSegmentResult(
          segmentation_platform::kChromeLowUserEngagementSegmentationKey);

  if (!result.is_ready || !result.segment.has_value()) {
    return false;
  }

  return result.segment.value() ==
         segmentation_platform::proto::SegmentId::
             OPTIMIZATION_TARGET_SEGMENTATION_CHROME_LOW_USER_ENGAGEMENT;
}

void SearchPromotionManager::PerformArmA() {
  NOTIMPLEMENTED();
}

void SearchPromotionManager::PerformArmB() {
  NOTIMPLEMENTED();
}
