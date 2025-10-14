// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_USER_EDUCATION_INTERACTIVE_FEATURE_PROMO_TEST_H_
#define CHROME_TEST_USER_EDUCATION_INTERACTIVE_FEATURE_PROMO_TEST_H_

#include <variant>

#include "base/feature_list.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "chrome/test/user_education/interactive_feature_promo_test_common.h"
#include "chrome/test/user_education/interactive_feature_promo_test_internal.h"
#include "components/prefs/pref_service.h"
#include "components/user_education/common/feature_promo/feature_promo_result.h"
#include "components/user_education/common/feature_promo/feature_promo_specification.h"
#include "components/webui/chrome_urls/pref_names.h"
#include "ui/base/interaction/element_identifier.h"

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

  // Changes the controller mode. Call before calling base class `SetUp()`.
  void SetControllerMode(ControllerMode controller_mode);

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
  struct CustomHelpBubbleShown {
    ui::ElementIdentifier expected_id;
  };
  using ShowPromoResult = std::variant<WebUiHelpBubbleShown,
                                       CustomHelpBubbleShown,
                                       user_education::FeaturePromoResult>;

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
  //
  // If this step is done `InAnyContext()` it will verify that the promo appears
  // in at least one browser.
  //
  // IMPORTANT USAGE NOTE: if the browser the promo is shown from (by calling
  // `MaybeShowFeaturePromo()` or using the `MaybeShowPromo()` action in this
  // test API) is different from the window in which it actually appears, you
  // MUST use `InAnyContext()` with this step, as otherwise it assumes that both
  // are in the current/specified context - i.e. the same browser window.
  // Failing to do so will cause either the check for the bubble or the step
  // that verifies the correct promo is showing to fail.
  //
  // NOTE: the current context is potentially undefined after this step if it
  // is run `InAnyContext()`; do not follow this step with `InSameContext()` in
  // that case.
  [[nodiscard]] MultiStep WaitForPromo(const base::Feature& iph_feature);

  // Checks that the promo `iph_feature` is has been requested and is either
  // queued or showing. Does not handle the case where the promo has already
  // been closed.
  //
  // If this step is done `InAnyContext()` it will verify that the promo is
  // requested in at least one browser (or if `active` is false, that it is not
  // requested in any browser.)
  //
  // NOTE: the current context is potentially undefined after this step if it
  // is run `InAnyContext()`; do not follow this step with `InSameContext()` in
  // that case.
  [[nodiscard]] StepBuilder CheckPromoRequested(
      const base::Feature& iph_feature,
      bool requested = true);

  // Same as `CheckPromoRequested()` but ignores queued promos. Usually prefer
  // to use `CheckPromoRequested()`. Note that "active" includes both "bubble
  // visible" and "bubble closed but promo continued".
  [[nodiscard]] StepBuilder CheckPromoActive(const base::Feature& iph_feature,
                                             bool requested = true);

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

  internal::InteractiveFeaturePromoTestPrivate& feature_promo_test_impl() {
    return *test_impl_;
  }

 private:
  // Shared by CheckPromoRequested and some internal actions.
  [[nodiscard]] StepBuilder CheckPromoImpl(const base::Feature& iph_feature,
                                           bool requested,
                                           bool include_queued);

  const raw_ptr<internal::InteractiveFeaturePromoTestPrivate> test_impl_;
};

// Template for adding `InteractiveFeaturePromoTestApi` to any existing test
// class; the class must derive from `InProcessBrowserTest`. Use only if you
// already have a separate class that you must derive from, otherwise prefer to
// use `InteractiveFeaturePromoTest` directly.
template <typename T>
  requires std::derived_from<T, InProcessBrowserTest>
class InteractiveFeaturePromoTestMixin : public T,
                                         public InteractiveFeaturePromoTestApi {
 public:
  explicit InteractiveFeaturePromoTestMixin(
      TrackerMode tracker_mode = UseMockTracker(),
      ClockMode clock_mode = ClockMode::kUseTestClock,
      InitialSessionState initial_session_state =
          InitialSessionState::kOutsideGracePeriod)
      : T(),
        InteractiveFeaturePromoTestApi(std::move(tracker_mode),
                                       clock_mode,
                                       initial_session_state) {}

  template <typename... Args>
  InteractiveFeaturePromoTestMixin(TrackerMode tracker_mode,
                                   ClockMode clock_mode,
                                   InitialSessionState initial_session_state,
                                   Args&&... args)
      : T(std::forward<Args>(args)...),
        InteractiveFeaturePromoTestApi(std::move(tracker_mode),
                                       clock_mode,
                                       initial_session_state) {}
  ~InteractiveFeaturePromoTestMixin() override = default;

 protected:
  void SetUp() override {
    feature_promo_test_impl().CommitControllerMode();
    T::SetUp();
  }

  void TearDown() override {
    T::TearDown();
    feature_promo_test_impl().ResetControllerMode();
  }

  void SetUpOnMainThread() override {
    T::SetUpOnMainThread();
    private_test_impl().DoTestSetUp();
    g_browser_process->local_state()->SetBoolean(
        chrome_urls::kInternalOnlyUisEnabled, true);
    if (Browser* browser = T::browser()) {
      SetContextWidget(
          BrowserView::GetBrowserViewForBrowser(browser)->GetWidget());
      feature_promo_test_impl().MaybeWaitForTrackerInitialization(browser);
    }
  }

  void TearDownOnMainThread() override {
    private_test_impl().DoTestTearDown();
    T::TearDownOnMainThread();
  }
};

using InteractiveFeaturePromoTest =
    InteractiveFeaturePromoTestMixin<InProcessBrowserTest>;

#endif  // CHROME_TEST_USER_EDUCATION_INTERACTIVE_FEATURE_PROMO_TEST_H_
