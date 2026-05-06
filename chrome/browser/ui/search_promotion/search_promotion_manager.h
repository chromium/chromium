// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SEARCH_PROMOTION_SEARCH_PROMOTION_MANAGER_H_
#define CHROME_BROWSER_UI_SEARCH_PROMOTION_SEARCH_PROMOTION_MANAGER_H_

#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

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
  virtual void OnTargetURLVisited(const GURL& url);

  // Called when the IPH promo is accepted.
  void OnPromoAccepted();

  // Returns true if the promo is allowed by feature flags.
  bool IsPromoAllowedForTesting();

 private:
  bool CheckEngagementLevel();

  void PerformArmA();
  void PerformArmB();
  void OnRegistryWriteComplete(const GURL& url, bool success);

  bool is_promo_allowed_ = false;
  const char* arm_ = feature_engagement::kSearchPromotionArmDefault;

  const raw_ref<Profile> profile_;
};

#endif  // CHROME_BROWSER_UI_SEARCH_PROMOTION_SEARCH_PROMOTION_MANAGER_H_
