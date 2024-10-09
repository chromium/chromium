// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_USER_EDUCATION_BROWSER_USER_EDUCATION_INTERFACE_H_
#define CHROME_BROWSER_UI_USER_EDUCATION_BROWSER_USER_EDUCATION_INTERFACE_H_

#include <concepts>

#include "base/feature_list.h"
#include "base/types/pass_key.h"
#include "components/user_education/common/feature_promo_controller.h"
#include "components/user_education/common/feature_promo_handle.h"
#include "components/user_education/common/feature_promo_result.h"
#include "components/user_education/common/new_badge_controller.h"

class AppMenuButton;
class BrowserFeaturePromoController;
class UserEducationInternalsPageHandlerImpl;

namespace content {
class WebContents;
}

namespace web_app {
class WebAppUiManagerImpl;
}

// Describes what to do when the feature associated with an IPH is used.
enum class FeaturePromoFeatureUsedAction {
  // If the promo is showing, it is dismissed. If it is queued, it will be
  // canceled. In most cases, the promo will not be shown again.
  kClosePromoIfPresent,
  // If the promo is showing or queued, it continues to be showing or queued.
  // However, marking the feature as used may prevent the promo from showing
  // in the future. If you intend to use e.g. CloseFeaturePromoAndContinue(),
  // this option will avoid terminating the promo prematurely.
  kIgnorePromoIfPresent,
};

// Provides the interface for common User Education actions.
class BrowserUserEducationInterface {
 public:
  BrowserUserEducationInterface() = default;
  BrowserUserEducationInterface(const BrowserUserEducationInterface&) = delete;
  void operator=(const BrowserUserEducationInterface&) = delete;
  virtual ~BrowserUserEducationInterface() = default;

  // Gets the windows's FeaturePromoController which manages display of
  // in-product help. Will return null in incognito and guest profiles.
  user_education::FeaturePromoController*
  GetFeaturePromoControllerForTesting() {
    return GetFeaturePromoControllerImpl();
  }

  // Only a limited number of non-test classes are allowed direct access to the
  // feature promo controller.
  template <typename T>
    requires std::same_as<T, AppMenuButton> ||
             std::same_as<T, BrowserFeaturePromoController> ||
             std::same_as<T, UserEducationInternalsPageHandlerImpl> ||
             std::same_as<T, web_app::WebAppUiManagerImpl>
  user_education::FeaturePromoController* GetFeaturePromoController(
      base::PassKey<T>) {
    return GetFeaturePromoControllerImpl();
  }

  // Returns whether the promo associated with `iph_feature` is running.
  //
  // Includes promos with visible bubbles and those which have been continued
  // with CloseFeaturePromoAndContinue() and are still running in the
  // background.
  virtual bool IsFeaturePromoActive(const base::Feature& iph_feature) const = 0;

  // Returns whether `MaybeShowFeaturePromo()` would succeed if called now.
  //
  // USAGE NOTE: Only call this method if figuring out whether to try to show an
  // IPH would involve significant expense. This method may itself have
  // non-trivial cost.
  virtual user_education::FeaturePromoResult CanShowFeaturePromo(
      const base::Feature& iph_feature) const = 0;

  // Maybe shows an in-product help promo. Returns true if the promo is shown.
  // In cases where there is no promo controller, immediately returns false.
  //
  // If this feature promo is likely to be shown at browser startup, prefer
  // calling `MaybeShowStartupFeaturePromo()` instead.
  //
  // If determining whether to call this method would involve significant
  // expense, you *may* first call `CanShowFeaturePromo()` before doing the
  // required computation; otherwise just call this method.
  virtual void MaybeShowFeaturePromo(
      user_education::FeaturePromoParams params) = 0;

  // Maybe shows an in-product help promo at startup, whenever the Feature
  // Engagement system is fully initialized. If the promo cannot be queued for
  // whatever reason, fails and returns false. The promo may still not run if it
  // is excluded for other reasons (e.g. another promo starts first; its Feature
  // Engagement conditions are not satisfied).
  //
  // On success, when the FE system is initialized (which might be immediately),
  // `promo_callback` is called with the result of whether the promo was
  // actually shown. Since `promo_callback` could be called any time, make sure
  // that you will not experience any race conditions or UAFs if the calling
  // object goes out of scope.
  //
  // If your promo is not likely to be shown at browser startup, prefer using
  // MaybeShowFeaturePromo() - which always runs synchronously - instead.
  virtual bool MaybeShowStartupFeaturePromo(
      user_education::FeaturePromoParams params) = 0;

  // Aborts the in-product help promo for `iph_feature` if it is showing or
  // cancels a pending startup promo. Aborting a promo means it was not fully
  // shown or interacted with, and may allow the promo to show again. Returns
  // whether a showing or pending promo was canceled.
  virtual bool AbortFeaturePromo(const base::Feature& iph_feature) = 0;

  // Closes the bubble for a feature promo but continues the promo; returns a
  // handle that can be used to end the promo when it is destructed. The handle
  // will be valid (i.e. have a true boolean value) if the promo was showing,
  // invalid otherwise.
  virtual user_education::FeaturePromoHandle CloseFeaturePromoAndContinue(
      const base::Feature& iph_feature) = 0;

  // Records that the user has engaged the specific `feature` associated with an
  // IPH promo; this information is used to determine whether to show the promo
  // in the future. Also specifies which `action` should be taken regarding
  // existing queued or showing promos associated with `feature`.
  //
  // Returns whether a promo was closed as a result.
  virtual bool NotifyFeaturePromoFeatureUsed(
      const base::Feature& feature,
      FeaturePromoFeatureUsedAction action) = 0;

  // Records that the user has performed an action that is specified in in a
  // FeaturePromoSpecification by calling the `SetAdditionalConditions()`
  // method; this information may be used to determine whether and when to show
  // the promo in the future.
  virtual void NotifyAdditionalConditionEvent(const char* event_name) = 0;

  // Returns whether a "New" Badge should be shown on the entry point for
  // `feature`; the badge must be registered for the feature in
  // browser_user_education_service.cc. Call exactly once per time the surface
  // containing the badge will be shown to the user.
  virtual user_education::DisplayNewBadge MaybeShowNewBadgeFor(
      const base::Feature& feature) = 0;

  // Records that the user has engaged the specific `feature` associated with a
  // "New" Badge; this information is used to determine whether to show the
  // badge in the future.
  //
  // You can also call `UserEducationService::MaybeNotifyNewBadgeFeatureUsed()`
  // if you only have access ot a `BrowserContext` or `Profile`.
  virtual void NotifyNewBadgeFeatureUsed(const base::Feature& feature) = 0;

  // Returns the interface associated with the browser containing `contents` in
  // its tabstrip, or null if `contents` is not a tab in any known browser.
  //
  // For WebUI embedded in a specific browser window or secondary UI of a
  // browser window, instead just use the appropriate BrowserWindow[Interface]
  // for that window.
  static BrowserUserEducationInterface* MaybeGetForWebContentsInTab(
      content::WebContents* contents);

 protected:
  virtual user_education::FeaturePromoController*
  GetFeaturePromoControllerImpl() = 0;
};

#endif  // CHROME_BROWSER_UI_USER_EDUCATION_BROWSER_USER_EDUCATION_INTERFACE_H_
