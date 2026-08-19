// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_EDUCATION_TEST_MOCK_NEW_BADGE_CONTROLLER_H_
#define COMPONENTS_USER_EDUCATION_TEST_MOCK_NEW_BADGE_CONTROLLER_H_

#include "base/feature.h"
#include "components/user_education/common/new_badge/new_badge_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace user_education::test {

// Mock for NewBadgeController to be used in unit tests.
class MockNewBadgeController : public NewBadgeController {
 public:
  MockNewBadgeController();
  ~MockNewBadgeController() override;

  // NewBadgeController:
  MOCK_METHOD(void, InitData, (), (override));
  MOCK_METHOD(DisplayNewBadge,
              MaybeShowNewBadge,
              (const base::Feature& feature),
              (override));
  MOCK_METHOD(void,
              NotifyFeatureUsed,
              (const base::Feature& feature),
              (override));
  MOCK_METHOD(void,
              NotifyFeatureUsedIfValid,
              (const base::Feature& feature),
              (override));
};

}  // namespace user_education::test

#endif  // COMPONENTS_USER_EDUCATION_TEST_MOCK_NEW_BADGE_CONTROLLER_H_
