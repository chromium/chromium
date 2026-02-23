// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_FEATURE_PROMO_CONTROLLER_H_
#define COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_FEATURE_PROMO_CONTROLLER_H_

#include <string>

#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "components/user_education/common/user_education_context.h"
#include "components/user_education/common/user_education_data.h"

namespace user_education {

// Describes the status of a feature promo.
enum class FeaturePromoStatus {
  kNotRunning,     // The promo is not running or queued.
  kQueued,         // The promo is queued but not yet shown.
  kBubbleShowing,  // The promo bubble is showing.
  kContinued       // The bubble was closed but the promo is still active.
};

// Enum for client code to specify why a promo should be programmatically ended.
enum class EndFeaturePromoReason {
  // Used to indicate that the user left the flow of the FeaturePromo.
  // For example, this may mean the user ignored a page-specific FeaturePromo
  // by navigating to another page.
  kAbortPromo,

  // Used to indicate that the user interacted with the promoted feature
  // in some meaningful way. For example, if an IPH is anchored to
  // a page action then clicking the page action might indicate that the
  // user engaged with the feature.
  kFeatureEngaged,
};

struct FeaturePromoParams;

// Mostly virtual base class for feature promos; used to mock the interface in
// tests.
class FeaturePromoController {
 public:
  using BubbleCloseCallback = base::OnceClosure;
  using ShowPromoResultCallback =
      base::OnceCallback<void(FeaturePromoResult promo_result)>;

  FeaturePromoController();
  FeaturePromoController(const FeaturePromoController& other) = delete;
  virtual ~FeaturePromoController();
  void operator=(const FeaturePromoController& other) = delete;

  // Queries whether the given promo could be shown at the current moment.
  //
  // In general it is unnecessary to call this method if the intention is to
  // show the promo; just call `MaybeShowPromo()` directly. However, in cases
  // where determining whether to try to show a promo would be prohibitively
  // expensive, this is a slightly less expensive out (but please note that it
  // is not zero cost; a number of prefs and application states do need to be
  // queried).
  //
  // Note that some fields of `params` may be ignored if they are not needed to
  // perform the checks involved.
  virtual FeaturePromoResult CanShowPromo(
      const FeaturePromoParams& params,
      const UserEducationContextPtr& context) const = 0;

  // Starts the promo if possible. If a result callback is specified, it will be
  // called with the result of trying to show the promo. In cases where a promo
  // could be queued, the callback may happen significantly later.
  virtual void MaybeShowPromo(FeaturePromoParams params,
                              UserEducationContextPtr context) = 0;

  // Tries to start the promo at startup, when the Feature Engagement backend
  // may not yet be initialized. Once it is initialized (which could be
  // immediately), attempts to show the promo and calls
  // `params.show_promo_result_callback` with the result. If EndPromo() is
  // called before the promo is shown, the promo is canceled immediately.
  //
  // USAGE NOTE: Startup promos may only be queued once per profile each
  // (though queuing them in a browser that does not support user education
  // doesn't count). Attempting to requeue the same promo at any time will
  // result in an "already queued" failure.
  //
  // A promo may be queued and then not show due to its Feature Engagement
  // conditions not being satisfied. For example, if multiple promos with a
  // session limit of 1 are queued, both may queue successfully, but only one
  // will actually show. If you care about whether the promo is actually shown,
  // set an appropriate `show_promo_result_callback`.
  //
  // Note: Since `show_promo_result_callback` is asynchronous and can
  // theoretically still be pending after the caller's scope disappears, care
  // must be taken to avoid a UAF on callback; the caller should prefer to
  // either not bind transient objects (e.g. only use the callback for things
  // like UMA logging) or use a weak pointer to avoid this situation.
  //
  // Otherwise, this is identical to MaybeShowPromo().
  virtual void MaybeShowStartupPromo(FeaturePromoParams params,
                                     UserEducationContextPtr context) = 0;

  // Gets the current status of the promo associated with `iph_feature`.
  virtual FeaturePromoStatus GetPromoStatus(
      const base::Feature& iph_feature) const = 0;

  // Gets the feature for the current promo.
  virtual const base::Feature* GetCurrentPromoFeature() const = 0;

  // Gets the specification for a feature promo, if a promo is currently
  // showing anchored to the given element identifier.
  //
  // This is used by menus to continue the promo and highlight menu items
  // when the user opens the menu.
  virtual const FeaturePromoSpecification*
  GetCurrentPromoSpecificationForAnchor(
      ui::ElementIdentifier menu_element_id) const = 0;

  // Returns whether a particular promo has previously been dismissed.
  // Useful in cases where determining if a promo should show could be
  // expensive. If `last_close_reason` is set, and the promo has been
  // dismissed, it wil be populated with the most recent close reason.
  // (The value is undefined if this method returns false.)
  //
  // Note that while `params` is a full parameters block, only `feature` and
  // `key` are actually used.
  virtual bool HasPromoBeenDismissed(
      const FeaturePromoParams& params,
      FeaturePromoClosedReason* last_close_reason) const = 0;
  inline bool HasPromoBeenDismissed(const FeaturePromoParams& params) const {
    return HasPromoBeenDismissed(params, nullptr);
  }

  // Returns whether the promo for `iph_feature` matches kBubbleShowing or any
  // of `additional_status`.
  template <typename... Args>
  bool IsPromoActive(const base::Feature& iph_feature,
                     Args... additional_status) const {
    const FeaturePromoStatus actual = GetPromoStatus(iph_feature);
    const std::initializer_list<FeaturePromoStatus> list{additional_status...};
    DCHECK(!std::ranges::contains(list, FeaturePromoStatus::kNotRunning));
    return actual == FeaturePromoStatus::kBubbleShowing ||
           std::ranges::contains(list, actual);
  }

  // Starts a promo with the settings for skipping any logging or filtering
  // provided by the implementation for MaybeShowPromo.
  virtual void MaybeShowPromoForDemoPage(FeaturePromoParams params,
                                         UserEducationContextPtr context) = 0;

  // For systems where there are rendering issues of e.g. displaying the
  // omnibox and a bubble in the same region on the screen, dismisses a non-
  // critical promo bubble which overlaps a given screen region. Returns true
  // if a bubble is closed as a result.
  virtual bool DismissNonCriticalBubbleInRegion(
      const gfx::Rect& screen_bounds) = 0;

  // Ends or cancels the current promo if it is queued. Returns true if a promo
  // was successfully canceled or a bubble closed.
  //
  // Has no effect for promos closed with CloseBubbleAndContinuePromo(); discard
  // or release the FeaturePromoHandle to end those promos.
  virtual bool EndPromo(const base::Feature& iph_feature,
                        EndFeaturePromoReason end_promo_reason) = 0;

  // Closes the promo for `iph_feature` - which must be showing - but continues
  // the promo via the return value. Dispose or release the resulting handle to
  // actually end the promo.
  //
  // Useful when a promo chains into some other user action and you don't want
  // other promos to be able to show until after the operation is finished.
  virtual FeaturePromoHandle CloseBubbleAndContinuePromo(
      const base::Feature& iph_feature) = 0;

  // Returns a weak pointer to this object.
  virtual base::WeakPtr<FeaturePromoController> GetAsWeakPtr() = 0;

#if !BUILDFLAG(IS_ANDROID)
  // If `feature` has a registered promo, notifies the tracker that the feature
  // has been used.
  virtual void NotifyFeatureUsedIfValid(const base::Feature& feature) = 0;
#endif

  // Posts `result` to `callback` on a fresh call stack. Requires a functioning
  // message pump.
  static void PostShowPromoResult(const base::Feature& feature,
                                  ShowPromoResultCallback callback,
                                  FeaturePromoResult result);

  // Add the ability to have a global callback for promo results for tests.
  using TestResultCallback =
      base::RepeatingCallback<void(const base::Feature&, FeaturePromoResult)>;
  static base::CallbackListSubscription AddResultCallbackForTesting(
      TestResultCallback callback);

 protected:
  friend class FeaturePromoHandle;

  // Called when FeaturePromoHandle is destroyed to finish the promo.
  virtual void FinishContinuedPromo(const base::Feature& iph_feature) = 0;

  // Records when and why an IPH was not shown.
  virtual void RecordPromoNotShown(
      const char* feature_name,
      FeaturePromoResult::Failure failure) const = 0;
};

// Params for showing a promo; you can pass a single feature or add additional
// params as necessary. Replaces the old parameter list as it was (a) long and
// unwieldy, and (b) violated the prohibition on optional parameters in virtual
// methods.
struct FeaturePromoParams {
  // NOLINTNEXTLINE(google-explicit-constructor)
  FeaturePromoParams(const base::Feature& iph_feature,
                     const std::string& key = std::string());
  FeaturePromoParams(FeaturePromoParams&& other) noexcept;
  FeaturePromoParams& operator=(FeaturePromoParams&& other) noexcept;
  ~FeaturePromoParams();

  // The feature for the IPH to show. Must be an IPH feature defined in
  // components/feature_engagement/public/feature_list.cc and registered with
  // |FeaturePromoRegistry|.
  //
  // Note that this is different than the feature that the IPH is showing for.
  raw_ref<const base::Feature> feature;

  // The key required for keyed promos. Should be left empty for all other
  // (i.e. non-keyed) promos.
  std::string key;

  // Will be called when the promo actually shows or fails to show. For queued
  // promos, will be called when the promo is shown. For non-queued promos, will
  // be posted immediately with the result of the request (arrives on a fresh
  // message loop call stack).
  FeaturePromoController::ShowPromoResultCallback show_promo_result_callback;

  // If a bubble was shown and `close_callback` is provided, it will be called
  // when the bubble closes. The callback must remain valid as long as the
  // bubble shows.
  FeaturePromoController::BubbleCloseCallback close_callback;

  // If the body text is parameterized, pass parameters here.
  FeaturePromoSpecification::FormatParameters body_params =
      FeaturePromoSpecification::NoSubstitution();

  // If the accessible text is parameterized, pass parameters here.
  FeaturePromoSpecification::FormatParameters screen_reader_params =
      FeaturePromoSpecification::NoSubstitution();

  // If the title text is parameterized, pass parameters here.
  FeaturePromoSpecification::FormatParameters title_params =
      FeaturePromoSpecification::NoSubstitution();
};

std::ostream& operator<<(std::ostream& os, FeaturePromoStatus status);

}  // namespace user_education

#endif  // COMPONENTS_USER_EDUCATION_COMMON_FEATURE_PROMO_FEATURE_PROMO_CONTROLLER_H_
