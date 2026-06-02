// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_PROMOTION_SEARCH_PROMOTION_MANAGER_H_
#define CHROME_BROWSER_UI_SEARCH_PROMOTION_SEARCH_PROMOTION_MANAGER_H_

#include <string_view>

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/shell_integration.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

class BrowserUserEducationInterface;
class Profile;

// SearchPromotionManager coordinates promotional states and actions for
// search-related features. At times, the promos may be OS-specific.
//
// This is modeled as a KeyedService to automatically bind its lifecycle
// to the active user session, ensuring safe lifetime management and dependency
// resolution for tab and UI-level features.
class SearchPromotionManager : public KeyedService {
 public:
  explicit SearchPromotionManager(Profile& profile);
  SearchPromotionManager(const SearchPromotionManager&) = delete;
  SearchPromotionManager& operator=(const SearchPromotionManager&) = delete;
  ~SearchPromotionManager() override;

  // Called by the navigation observer when the target URL is visited.
  // We pass the `BrowserUserEducationInterface` reference to trigger the User
  // Education promo bubble.
  virtual void OnTargetURLVisited(
      BrowserUserEducationInterface& user_education);

  // Called when the IPH promo is accepted.
  void OnPromoAccepted();

  // Returns true if the promo is allowed by feature flags.
  bool IsPromoAllowedForTesting() const;

  // Returns true if the profile meets the required low engagement level.
  bool IsEngagementLowEnoughForTesting() const;

 private:
  // Checks whether the current user profile belongs to the low engagement tier
  // (defined as being active fewer than 9 days out of the last 28 days).
  // Uses cached results from SegmentationPlatformService.
  bool IsEngagementLowEnough() const;

  void PerformArmA();
  void PerformArmB();

  void RecordDefaultBrowserState(
      shell_integration::DefaultWebClientState state);
  void OnPromoClosed();
  void OnDefaultBrowserNameRetrieved(bool accepted,
                                     std::string_view arm,
                                     const std::u16string& name);

  bool is_promo_allowed_ = false;
  std::string_view arm_ = feature_engagement::kSearchPromotionArmDefault;
  bool was_accepted_ = false;

  const raw_ref<Profile> profile_;

  base::WeakPtrFactory<SearchPromotionManager> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_SEARCH_PROMOTION_SEARCH_PROMOTION_MANAGER_H_
