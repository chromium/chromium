// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_USER_EDUCATION_MOCK_BROWSER_USER_EDUCATION_INTERFACE_H_
#define CHROME_TEST_USER_EDUCATION_MOCK_BROWSER_USER_EDUCATION_INTERFACE_H_

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockBrowserUserEducationInterface : public BrowserUserEducationInterface {
 public:
  explicit MockBrowserUserEducationInterface(BrowserWindowInterface*);
  ~MockBrowserUserEducationInterface() override;

  MOCK_METHOD(bool,
              IsFeaturePromoQueued,
              (const base::Feature&),
              (const override));
  MOCK_METHOD(bool,
              IsFeaturePromoActive,
              (const base::Feature&),
              (const override));
  MOCK_METHOD(user_education::FeaturePromoResult,
              CanShowFeaturePromo,
              (const base::Feature&),
              (const override));
  MOCK_METHOD(void,
              MaybeShowFeaturePromo,
              (user_education::FeaturePromoParams),
              (override));
  MOCK_METHOD(void,
              MaybeShowStartupFeaturePromo,
              (user_education::FeaturePromoParams),
              (override));
  MOCK_METHOD(bool, AbortFeaturePromo, (const base::Feature&), (override));
  MOCK_METHOD(user_education::FeaturePromoHandle,
              CloseFeaturePromoAndContinue,
              (const base::Feature&),
              (override));
  MOCK_METHOD(bool,
              NotifyFeaturePromoFeatureUsed,
              (const base::Feature&, FeaturePromoFeatureUsedAction),
              (override));
  MOCK_METHOD(void, NotifyAdditionalConditionEvent, (const char*), (override));
  MOCK_METHOD(user_education::DisplayNewBadge,
              MaybeShowNewBadgeFor,
              (const base::Feature&),
              (override));
  MOCK_METHOD(void,
              NotifyNewBadgeFeatureUsed,
              (const base::Feature&),
              (override));
  MOCK_METHOD(const user_education::UserEducationContextPtr&,
              GetUserEducationContextImpl,
              (),
              (const override));
};

#endif  // CHROME_TEST_USER_EDUCATION_MOCK_BROWSER_USER_EDUCATION_INTERFACE_H_
