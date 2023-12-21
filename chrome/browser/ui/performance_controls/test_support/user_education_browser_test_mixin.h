// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TEST_SUPPORT_USER_EDUCATION_BROWSER_TEST_MIXIN_H_
#define CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TEST_SUPPORT_USER_EDUCATION_BROWSER_TEST_MIXIN_H_

#include <type_traits>
#include "chrome/test/base/in_process_browser_test.h"
#include "components/user_education/test/feature_promo_test_util.h"

class BrowserFeaturePromoController;

// Template to be used as a mixin class for memory saver tests extending
// InProcessBrowserTest.
template <typename T,
          typename =
              std::enable_if_t<std::is_base_of_v<InProcessBrowserTest, T>>>
class UserEducationBrowserTestMixin : public T {
 public:
  template <class... Args>
  explicit UserEducationBrowserTestMixin(Args&&... args)
      : T(std::forward<Args>(args)...) {}

  ~UserEducationBrowserTestMixin() override = default;

  UserEducationBrowserTestMixin(const UserEducationBrowserTestMixin&) = delete;
  UserEducationBrowserTestMixin& operator=(
      const UserEducationBrowserTestMixin&) = delete;

  BrowserFeaturePromoController* GetFeaturePromoController() {
    return static_cast<BrowserFeaturePromoController*>(
        T::browser()->window()->GetFeaturePromoController());
  }

  bool WaitForFeatureTrackerInitialization() {
    feature_engagement::Tracker* const tracker =
        GetFeaturePromoController()->feature_engagement_tracker();
    return user_education::test::WaitForFeatureEngagementReady(tracker);
  }
};

#endif  // CHROME_BROWSER_UI_PERFORMANCE_CONTROLS_TEST_SUPPORT_USER_EDUCATION_BROWSER_TEST_MIXIN_H_
