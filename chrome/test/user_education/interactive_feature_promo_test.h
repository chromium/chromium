// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_USER_EDUCATION_INTERACTIVE_FEATURE_PROMO_TEST_H_
#define CHROME_TEST_USER_EDUCATION_INTERACTIVE_FEATURE_PROMO_TEST_H_

#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test_common.h"
#include "chrome/test/user_education/interactive_feature_promo_test_internal.h"
#include "components/user_education/common/feature_promo_result.h"
#include "components/user_education/common/feature_promo_specification.h"

// API class that provides both base browser Kombucha functionality and
// additional logic for testing User Education experiences.
//
// Using this test class:
//  - Enables a select list of IPH to be shown during the test or uses a mock
//    `FeatureEngagementTracker` to sidestep that system entirely.
//  - Disables window activity checking for the browser, making tests more
//    stable in environments where the browser window might lose activation.
//  - Sets up the current session state so that the browser reports as either
//    idle, inside the "grace period", or outside the "grace period", affecting
//    whether and which kinds of IPH can be shown.
//  - Optionally grants control over the clock used by User Education for finer
//    control over session state.
//
// This API is relatively safe for use in `browser_tests` (in addition to
// `interactive_ui_tests`) subject to the same limitations as
// `InteractiveBrowserTest*`. Window activation and mouse input are not
// reliable, and bringing up a dialog or menu that is dismissed on loss of focus
// can cause a test to flake in `browser_tests`.
//
// Prefer to use InteractiveFeaturePromoTest[T] as the base class for your tests
// instead of directly using this class.
class InteractiveFeaturePromoTestApi
    : public InteractiveBrowserTestApi,
      public InteractiveFeaturePromoTestCommon {
 public:
  // Constructs the API with the given preferences for test behavior.
  explicit InteractiveFeaturePromoTestApi(
      TrackerMode tracker_mode = UseMockTracker(),
      ClockMode clock_mode = ClockMode::kUseTestClock,
      InitialSessionState initial_session_state =
          InitialSessionState::kOutsideGracePeriod);
  ~InteractiveFeaturePromoTestApi() override;

  // Gets the mock tracker, if the tracker mode is `UseMockTracker`.
  MockTracker* GetMockTrackerFor(Browser* browser);

  // Registers a test feature for use with a mock tracker.
  //
  // Note that since a promo specification can only be registered in one
  // registry, this method requires the `browser` to be specified.
  void RegisterTestFeature(Browser* browser,
                           user_education::FeaturePromoSpecification spec);

  // Ensures that the feature engagement tracker is initialized and ready.
  //
  // Usage notes:
  //  - Not compatible with `UseMockTracker`.
  //  - Only waits for the browser in the current context.
  //  - Unnecessary in the primary browser when using
  //    `TrackerInitializationMode::kWaitForMainBrowser`.
  [[nodiscard]] MultiStep WaitForFeatureEngagementReady();

  // Returns a test step that advances time.
  // Fails if not in `ClockMode::kUseTestClock` or if `time` is null.
  [[nodiscard]] StepBuilder AdvanceTime(NewTime time);

  // Returns a test step that updates the last active time reported by the idle
  // observer based on the given parameters. If `time` is relative, the new
  // value is based on the current time (either default or test clock). If
  // `time` is null, there is no current active time.
  [[nodiscard]] StepBuilder SetLastActive(NewTime time);

  // --------------------------------------------------------------------------
  // IMPORTANT NOTE: the following methods only work for Views help bubbles.

  struct WebUiHelpBubbleShown {};
  using ShowPromoResult =
      std::variant<WebUiHelpBubbleShown, user_education::FeaturePromoResult>;

  // Possibly tries to show the promo with `params`, which should produce the
  // `show_promo_result`. If `show_promo_result` is not `WebUiHelpBubbleShown`
  // and the result is success, checks that a Views help bubble is open and the
  // correct promo is showing. For WebUI bubbles, only checks that the correct
  // promo is showing.
  //
  // When using a mock `FeatureEngagementTracker` the tracker will be set up to
  // handle the appropriate calls.
  [[nodiscard]] MultiStep MaybeShowPromo(
      user_education::FeaturePromoParams params,
      ShowPromoResult show_promo_result =
          user_education::FeaturePromoResult::Success());

  // Waits for the given Views promo bubble to be shown and verifies that the
  // correct IPH is active.
  [[nodiscard]] MultiStep WaitForPromo(const base::Feature& iph_feature);

  // Checks that the promo `iph_feature` is active.
  [[nodiscard]] StepBuilder CheckPromoIsActive(const base::Feature& iph_feature,
                                               bool active = true);

  // Ends the specified promo via the API, with reason `kAborted`.
  [[nodiscard]] MultiStep AbortPromo(const base::Feature& iph_feature,
                                     bool expected_result = true);

  // These methods press one of the buttons on a feature promo bubble, which
  // must be showing.
  [[nodiscard]] MultiStep PressClosePromoButton();
  [[nodiscard]] MultiStep PressDefaultPromoButton();
  [[nodiscard]] MultiStep PressNonDefaultPromoButton();

  // End Views help bubble-only methods.
  // --------------------------------------------------------------------------

 private:
  internal::InteractiveFeaturePromoTestPrivate& test_impl() {
    return static_cast<internal::InteractiveFeaturePromoTestPrivate&>(
        private_test_impl());
  }
};

// Template for adding `InteractiveFeaturePromoTestApi` to any existing test
// class; the class must derive from `InProcessBrowserTest`. Use only if you
// already have a separate class that you must derive from, otherwise prefer to
// use `InteractiveFeaturePromoTest` directly.
template <typename T>
  requires std::derived_from<T, InProcessBrowserTest>
class InteractiveFeaturePromoTestT : public T,
                                     public InteractiveFeaturePromoTestApi {
 public:
  explicit InteractiveFeaturePromoTestT(
      TrackerMode tracker_mode = UseMockTracker(),
      ClockMode clock_mode = ClockMode::kUseTestClock,
      InitialSessionState initial_session_state =
          InitialSessionState::kOutsideGracePeriod)
      : T(),
        InteractiveFeaturePromoTestApi(std::move(tracker_mode),
                                       clock_mode,
                                       initial_session_state) {}

  template <typename... Args>
  InteractiveFeaturePromoTestT(TrackerMode tracker_mode,
                               ClockMode clock_mode,
                               InitialSessionState initial_session_state,
                               Args&&... args)
      : T(std::forward<Args>(args)...),
        InteractiveFeaturePromoTestApi(std::move(tracker_mode),
                                       clock_mode,
                                       initial_session_state) {}
  ~InteractiveFeaturePromoTestT() override = default;

 protected:
  void SetUpOnMainThread() override {
    T::SetUpOnMainThread();
    private_test_impl().DoTestSetUp();
    if (Browser* browser = T::browser()) {
      SetContextWidget(
          BrowserView::GetBrowserViewForBrowser(browser)->GetWidget());
      static_cast<internal::InteractiveFeaturePromoTestPrivate&>(
          private_test_impl())
          .MaybeWaitForTrackerInitialization(browser);
    }
  }

  void TearDownOnMainThread() override {
    private_test_impl().DoTestTearDown();
    T::TearDownOnMainThread();
  }
};

using InteractiveFeaturePromoTest =
    InteractiveFeaturePromoTestT<InProcessBrowserTest>;

#endif  // CHROME_TEST_USER_EDUCATION_INTERACTIVE_FEATURE_PROMO_TEST_H_
