// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USER_EDUCATION_USER_EDUCATION_SERVICE_H_
#define CHROME_BROWSER_USER_EDUCATION_USER_EDUCATION_SERVICE_H_

#include <memory>

#include "base/feature_list.h"
#include "chrome/browser/user_education/browser_feature_promo_storage_service.h"
#include "chrome/browser/user_education/browser_tutorial_service.h"
#include "chrome/browser/user_education/recent_session_observer.h"
#include "chrome/browser/user_education/recent_session_tracker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/user_education/common/feature_promo_registry.h"
#include "components/user_education/common/feature_promo_session_manager.h"
#include "components/user_education/common/feature_promo_session_policy.h"
#include "components/user_education/common/feature_promo_storage_service.h"
#include "components/user_education/common/help_bubble_factory_registry.h"
#include "components/user_education/common/new_badge_controller.h"
#include "components/user_education/common/product_messaging_controller.h"
#include "components/user_education/common/tutorial.h"
#include "components/user_education/common/tutorial_registry.h"
#include "content/public/browser/browser_context.h"

// Kill switch for recent session tracking. Enabled by default.
BASE_DECLARE_FEATURE(kAllowRecentSessionTracking);

class UserEducationService : public KeyedService {
 public:
  explicit UserEducationService(
      std::unique_ptr<BrowserFeaturePromoStorageService> storage_service,
      bool allows_promos);
  ~UserEducationService() override;

  user_education::TutorialRegistry& tutorial_registry() {
    return tutorial_registry_;
  }
  user_education::TutorialService& tutorial_service() {
    return tutorial_service_;
  }
  user_education::HelpBubbleFactoryRegistry& help_bubble_factory_registry() {
    return help_bubble_factory_registry_;
  }
  user_education::FeaturePromoRegistry& feature_promo_registry() {
    return feature_promo_registry_;
  }
  user_education::ProductMessagingController& product_messaging_controller() {
    return product_messaging_controller_;
  }
  user_education::FeaturePromoStorageService& feature_promo_storage_service() {
    return *feature_promo_storage_service_;
  }
  user_education::FeaturePromoSessionManager& feature_promo_session_manager() {
    return feature_promo_session_manager_;
  }
  user_education::FeaturePromoSessionPolicy& feature_promo_session_policy() {
    return *feature_promo_session_policy_;
  }
  user_education::NewBadgeRegistry* new_badge_registry() {
    return new_badge_registry_.get();
  }
  user_education::NewBadgeController* new_badge_controller() {
    return new_badge_controller_.get();
  }
  RecentSessionTracker* recent_session_tracker() {
    return recent_session_tracker_.get();
  }
  RecentSessionObserver* recent_session_observer() {
    return recent_session_observer_.get();
  }

  // Utility methods for when a browser [window] isn't available; for example,
  // when only a WebContents is available:

  // Checks if a "New" Badge should be shown for the given `context` (or
  // profile), for `feature`.
  static user_education::DisplayNewBadge MaybeShowNewBadge(
      content::BrowserContext* context,
      const base::Feature& feature);

  // Notifies that a feature associated with a "New" Badge was used in `context`
  // (or profile), if the context supports User Education.
  static void MaybeNotifyNewBadgeFeatureUsed(content::BrowserContext* context,
                                             const base::Feature& feature);

 private:
  friend class UserEducationServiceFactory;

  user_education::TutorialRegistry tutorial_registry_;
  user_education::HelpBubbleFactoryRegistry help_bubble_factory_registry_;
  user_education::FeaturePromoRegistry feature_promo_registry_;
  BrowserTutorialService tutorial_service_;
  user_education::ProductMessagingController product_messaging_controller_;
  std::unique_ptr<BrowserFeaturePromoStorageService>
      feature_promo_storage_service_;
  user_education::FeaturePromoSessionManager feature_promo_session_manager_;
  std::unique_ptr<user_education::FeaturePromoSessionPolicy>
      feature_promo_session_policy_;
  std::unique_ptr<user_education::NewBadgeRegistry> new_badge_registry_;
  std::unique_ptr<user_education::NewBadgeController> new_badge_controller_;
  std::unique_ptr<RecentSessionTracker> recent_session_tracker_;
  std::unique_ptr<RecentSessionObserver> recent_session_observer_;
};

#endif  // CHROME_BROWSER_USER_EDUCATION_USER_EDUCATION_SERVICE_H_
