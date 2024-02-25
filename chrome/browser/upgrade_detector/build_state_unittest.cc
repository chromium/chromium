// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/upgrade_detector/build_state.h"

#include <optional>

#include "base/version.h"
#include "chrome/browser/upgrade_detector/mock_build_state_observer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AllOf;
using ::testing::Eq;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::Property;

class BuildStateTest : public ::testing::Test {
 protected:
  BuildStateTest() { build_state_.AddObserver(&mock_observer_); }
  ~BuildStateTest() override { build_state_.RemoveObserver(&mock_observer_); }

  MockBuildStateObserver& mock_observer() { return mock_observer_; }
  BuildState& build_state() { return build_state_; }

 private:
  MockBuildStateObserver mock_observer_;
  BuildState build_state_;
};

// Observers are not notified when there's no update.
TEST_F(BuildStateTest, SetUpdateNoUpdate) {
  EXPECT_EQ(build_state().update_type(), BuildState::UpdateType::kNone);
  build_state().SetUpdate(BuildState::UpdateType::kNone, base::Version(),
                          std::nullopt);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer());
  EXPECT_EQ(build_state().update_type(), BuildState::UpdateType::kNone);
  EXPECT_FALSE(build_state().installed_version().has_value());
  EXPECT_FALSE(build_state().critical_version().has_value());
}

// Observers are notified upon update when the version couldn't be fetched.
TEST_F(BuildStateTest, SetUpdateWithNoVersion) {
  EXPECT_CALL(
      mock_observer(),
      OnUpdate(AllOf(Eq(&build_state()),
                     Property(&BuildState::update_type,
                              Eq(BuildState::UpdateType::kNormalUpdate)),
                     Property(&BuildState::installed_version, IsFalse()),
                     Property(&BuildState::critical_version, IsFalse()))));
  build_state().SetUpdate(BuildState::UpdateType::kNormalUpdate,
                          base::Version(), std::nullopt);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer());
}

// Observers are notified upon update and the version is held.
TEST_F(BuildStateTest, SetUpdateWithVersion) {
  const base::Version expected_version("1.2.3.4");
  EXPECT_CALL(mock_observer(),
              OnUpdate(AllOf(
                  Eq(&build_state()),
                  Property(&BuildState::update_type,
                           Eq(BuildState::UpdateType::kNormalUpdate)),
                  Property(&BuildState::installed_version, IsTrue()),
                  Property(&BuildState::installed_version,
                           Eq(std::optional<base::Version>(expected_version))),
                  Property(&BuildState::critical_version, IsFalse()))));
  build_state().SetUpdate(BuildState::UpdateType::kNormalUpdate,
                          expected_version, std::nullopt);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer());
}

// Observers are notified upon update with a critical version and both versions
// are held.
TEST_F(BuildStateTest, SetUpdateWithCritical) {
  const base::Version expected_version("1.2.3.4");
  const base::Version expected_critical("1.2.3.3");
  EXPECT_CALL(
      mock_observer(),
      OnUpdate(AllOf(
          Eq(&build_state()),
          Property(&BuildState::update_type,
                   Eq(BuildState::UpdateType::kNormalUpdate)),
          Property(&BuildState::installed_version, IsTrue()),
          Property(&BuildState::installed_version,
                   Eq(std::optional<base::Version>(expected_version))),
          Property(&BuildState::critical_version, IsTrue()),
          Property(&BuildState::critical_version,
                   Eq(std::optional<base::Version>(expected_critical))))));
  build_state().SetUpdate(BuildState::UpdateType::kNormalUpdate,
                          expected_version, expected_critical);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer());
}

// Observers are only notified once for duplicate calls.
TEST_F(BuildStateTest, TwoUpdatesOnceNotification) {
  const base::Version expected_version("1.2.3.4");
  EXPECT_CALL(mock_observer(), OnUpdate(&build_state()));
  build_state().SetUpdate(BuildState::UpdateType::kNormalUpdate,
                          expected_version, std::nullopt);
  build_state().SetUpdate(BuildState::UpdateType::kNormalUpdate,
                          expected_version, std::nullopt);
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer());
}
