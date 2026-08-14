// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_PROMOTION_SEARCH_PROMOTION_MANAGER_H_
#define CHROME_BROWSER_UI_SEARCH_PROMOTION_SEARCH_PROMOTION_MANAGER_H_

#include <memory>
#include <string>
#include <string_view>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/platform_experience/delegated_tasks/delegated_task_runner.h"
#include "chrome/browser/shell_integration.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/keyed_service/core/keyed_service.h"

class BrowserUserEducationInterface;
class Profile;
class RegisterSearchPromotionTask;

namespace segmentation_platform {
struct ClassificationResult;
}

// SearchPromotionManager coordinates promotional states and actions for
// search-related features. At times, the promos may be OS-specific.
//
// This is modeled as a KeyedService to automatically bind its lifecycle
// to the active user session, ensuring safe lifetime management and dependency
// resolution for tab and UI-level features.
class SearchPromotionManager : public KeyedService {
 public:
  // String labels returned by the SegmentationPlatform's `ChromeUserEngagement`
  // model. See:
  // components/segmentation_platform/embedder/default_model/chrome_user_engagement.cc
  // for example of usage.
  // Engagement tiers based on user activity over the last 28 days.
  static constexpr std::string_view kEngagementLabelOneDay = "OneDay";
  static constexpr std::string_view kEngagementLabelLow = "Low";
  static constexpr std::string_view kEngagementLabelMedium = "Medium";
  static constexpr std::string_view kEngagementLabelPower = "Power";

  using CreateTaskRunnerCallback = base::RepeatingCallback<
      std::unique_ptr<platform_experience::DelegatedTaskRunner>()>;

  SearchPromotionManager(Profile& profile,
                         CreateTaskRunnerCallback create_task_runner_callback);
  SearchPromotionManager(const SearchPromotionManager&) = delete;
  SearchPromotionManager& operator=(const SearchPromotionManager&) = delete;
  ~SearchPromotionManager() override;

  // Called by the navigation observer when a Google Search URL is loaded.
  // Triggers the User Education promo bubble using the provided
  // `BrowserUserEducationInterface`.
  virtual void OnTargetURLVisited(
      BrowserUserEducationInterface& user_education);

  // Called when the IPH promo is accepted.
  void OnPromoAccepted();

  // Returns true if the promo is allowed by feature flags.
  bool IsPromoAllowedForTesting() const;

  std::string_view GetEngagementLabelForTesting() const;

 private:
  // Checks whether the user's engagement matches the requirements of the
  // configured experiment arm.
  bool IsEngagementEligibleForArm() const;

  void QueryEngagementLevel();
  void OnEngagementResultRetrieved(
      const segmentation_platform::ClassificationResult& result);

  void PerformOpen();
  void PerformInstall();

  void RunRegisterTask(std::unique_ptr<RegisterSearchPromotionTask> task);
  void OnTaskCompleted(platform_experience::DelegatedTaskResult result);

  void RecordDefaultBrowserState(
      shell_integration::DefaultWebClientState state);
  void OnPromoClosed();
  void OnDefaultBrowserNameRetrieved(bool accepted,
                                     std::string_view arm,
                                     const std::u16string& name);

  bool is_promo_allowed_ = false;
  std::string_view arm_ = feature_engagement::kSearchPromotionArmDefault;
  std::string engagement_label_;
  bool was_accepted_ = false;

  const raw_ref<Profile> profile_;

  CreateTaskRunnerCallback create_task_runner_callback_;
  std::unique_ptr<platform_experience::DelegatedTaskRunner> task_runner_;

  base::WeakPtrFactory<SearchPromotionManager> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_SEARCH_PROMOTION_SEARCH_PROMOTION_MANAGER_H_
