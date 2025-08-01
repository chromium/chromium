// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_USER_EDUCATION_USER_EDUCATION_SERVICE_H_
#define CHROME_BROWSER_USER_EDUCATION_USER_EDUCATION_SERVICE_H_

#include <memory>

#include "base/feature_list.h"
#include "base/memory/raw_ref.h"
#include "base/types/pass_key.h"
#include "chrome/browser/user_education/browser_tutorial_service.h"
#include "chrome/browser/user_education/browser_user_education_storage_service.h"
#include "chrome/browser/user_education/recent_session_observer.h"
#include "chrome/browser/user_education/recent_session_tracker.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/user_education/common/feature_promo/feature_promo_controller.h"
#include "components/user_education/common/feature_promo/feature_promo_registry.h"
#include "components/user_education/common/feature_promo/feature_promo_session_policy.h"
#include "components/user_education/common/help_bubble/help_bubble_factory_registry.h"
#include "components/user_education/common/new_badge/new_badge_controller.h"
#include "components/user_education/common/ntp_promo/ntp_promo_controller.h"
#include "components/user_education/common/ntp_promo/ntp_promo_registry.h"
#include "components/user_education/common/product_messaging_controller.h"
#include "components/user_education/common/session/user_education_session_manager.h"
#include "components/user_education/common/tutorial/tutorial.h"
#include "components/user_education/common/tutorial/tutorial_registry.h"
#include "components/user_education/common/user_education_storage_service.h"
#include "content/public/browser/browser_context.h"

// Kill switch for recent session tracking. Enabled by default.
BASE_DECLARE_FEATURE(kAllowRecentSessionTracking);

class BrowserHelpBubble;
class BrowserUserEducationInterfaceImpl;
class ToolbarButtonMenuHighlighter;
class UserEducationInternalsPageHandlerImpl;

namespace web_app {
class WebAppUiManagerImpl;
}

class UserEducationService : public KeyedService {
 public:
  explicit UserEducationService(Profile* profile, bool allows_promos);
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
  user_education::UserEducationStorageService&
  user_education_storage_service() {
    return *user_education_storage_service_;
  }
  user_education::UserEducationSessionManager&
  user_education_session_manager() {
    return user_education_session_manager_;
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
  user_education::NtpPromoRegistry* ntp_promo_registry() {
    return ntp_promo_registry_.get();
  }
  user_education::NtpPromoController* ntp_promo_controller() {
    return ntp_promo_controller_.get();
  }
  Profile& profile() { return *profile_; }

  // Only a limited number of non-test classes are allowed direct access to the
  // feature promo controller.
  template <typename T>
    requires std::same_as<T, BrowserHelpBubble> ||
             std::same_as<T, BrowserUserEducationInterfaceImpl> ||
             std::same_as<T, ToolbarButtonMenuHighlighter> ||
             std::same_as<T, UserEducationInternalsPageHandlerImpl> ||
             std::same_as<T, web_app::WebAppUiManagerImpl>
  const user_education::FeaturePromoController* GetFeaturePromoController(
      base::PassKey<T>) const {
    return feature_promo_controller_.get();
  }
  template <typename T>
  user_education::FeaturePromoController* GetFeaturePromoController(
      base::PassKey<T> key) {
    return const_cast<user_education::FeaturePromoController*>(
        const_cast<const UserEducationService*>(this)
            ->GetFeaturePromoController(std::move(key)));
  }

  user_education::FeaturePromoController*
  GetFeaturePromoControllerForTesting() {
    return feature_promo_controller_.get();
  }

  // Sets the promo controller (typically for setting a mock).
  // Note: in the vast majority of cases you probably want to mock
  // BrowserUserEducationInterface, since that's the API most production code
  // actually uses.
  void SetFeaturePromoControllerForTesting(
      std::unique_ptr<user_education::FeaturePromoController>
          feature_promo_controller) {
    feature_promo_controller_ = std::move(feature_promo_controller);
  }

  // KeyedService:
  void Shutdown() override;

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

  const raw_ref<Profile> profile_;
  user_education::TutorialRegistry tutorial_registry_;
  user_education::HelpBubbleFactoryRegistry help_bubble_factory_registry_;
  user_education::FeaturePromoRegistry feature_promo_registry_;
  BrowserTutorialService tutorial_service_;
  std::unique_ptr<BrowserUserEducationStorageService>
      user_education_storage_service_;
  user_education::UserEducationSessionManager user_education_session_manager_;
  std::unique_ptr<user_education::FeaturePromoSessionPolicy>
      feature_promo_session_policy_;
  user_education::ProductMessagingController product_messaging_controller_;
  std::unique_ptr<user_education::NewBadgeRegistry> new_badge_registry_;
  std::unique_ptr<user_education::NewBadgeController> new_badge_controller_;
  std::unique_ptr<RecentSessionTracker> recent_session_tracker_;
  std::unique_ptr<RecentSessionObserver> recent_session_observer_;
  std::unique_ptr<user_education::NtpPromoRegistry> ntp_promo_registry_;
  std::unique_ptr<user_education::NtpPromoController> ntp_promo_controller_;
  std::unique_ptr<user_education::FeaturePromoController>
      feature_promo_controller_;
};

#endif  // CHROME_BROWSER_USER_EDUCATION_USER_EDUCATION_SERVICE_H_
