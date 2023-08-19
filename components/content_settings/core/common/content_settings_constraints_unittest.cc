// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/core/common/content_settings_constraints.h"

#include "base/test/task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content_settings {
namespace {}  // namespace

class ContentSettingConstraintsTest : public testing::Test {
 public:
  base::test::TaskEnvironment& env() { return env_; }

 private:
  base::test::TaskEnvironment env_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(ContentSettingConstraintsTest, CopyCtor) {
  ContentSettingConstraints constraints;
  constraints.set_lifetime(base::Seconds(1234));
  constraints.set_session_model(SessionModel::UserSession);
  constraints.set_track_last_visit_for_autoexpiration(true);

  ContentSettingConstraints copy = constraints;
  EXPECT_EQ(constraints, copy);

  ContentSettingConstraints different = constraints;
  different.set_lifetime(base::Days(1));
  EXPECT_NE(constraints, different);

  ContentSettingConstraints old_constraints;
  env().FastForwardBy(base::Seconds(1));
  ContentSettingConstraints new_constraints;
  // The creation time differs, but there's no lifetime, so these are
  // equivalent.
  EXPECT_EQ(old_constraints, new_constraints);

  old_constraints.set_lifetime(base::Days(1));
  new_constraints.set_lifetime(base::Days(1));
  // Now there is a lifetime associated with the constraint, so the different
  // creation time makes a difference.
  EXPECT_NE(old_constraints, new_constraints);
}

TEST_F(ContentSettingConstraintsTest, MoveCtor) {
  ContentSettingConstraints constraints;
  constraints.set_lifetime(base::Seconds(1234));
  constraints.set_session_model(SessionModel::UserSession);
  constraints.set_track_last_visit_for_autoexpiration(true);

  ContentSettingConstraints copy = constraints;
  ContentSettingConstraints moved = std::move(constraints);
  EXPECT_EQ(copy, moved);

  moved.set_lifetime(base::Days(1));
  EXPECT_NE(copy, moved);
}

}  // namespace content_settings
