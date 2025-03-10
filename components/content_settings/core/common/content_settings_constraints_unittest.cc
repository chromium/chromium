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

TEST_F(ContentSettingConstraintsTest, Clone) {
  ContentSettingConstraints constraints;
  constraints.set_lifetime(base::Seconds(1234));
  constraints.set_session_model(mojom::SessionModel::USER_SESSION);
  constraints.set_track_last_visit_for_autoexpiration(true);
  constraints.set_decided_by_related_website_sets(true);
  constraints.set_options(base::Value(true));

  ContentSettingConstraints different = constraints.Clone();

  EXPECT_EQ(constraints, different);
  EXPECT_EQ(constraints.lifetime(), different.lifetime());
  EXPECT_EQ(constraints.session_model(), different.session_model());
  EXPECT_EQ(constraints.track_last_visit_for_autoexpiration(),
            different.track_last_visit_for_autoexpiration());
  EXPECT_EQ(constraints.decided_by_related_website_sets(),
            different.decided_by_related_website_sets());
  EXPECT_EQ(constraints.options(), different.options());

  different.set_lifetime(base::Days(1));
  EXPECT_NE(constraints, different);
}

TEST_F(ContentSettingConstraintsTest, MoveCtor) {
  ContentSettingConstraints constraints;
  constraints.set_lifetime(base::Seconds(1234));
  constraints.set_session_model(mojom::SessionModel::USER_SESSION);
  constraints.set_track_last_visit_for_autoexpiration(true);
  constraints.set_decided_by_related_website_sets(true);
  constraints.set_options(base::Value(true));

  ContentSettingConstraints clone = constraints.Clone();
  ContentSettingConstraints moved = std::move(constraints);
  EXPECT_EQ(clone, moved);

  moved.set_lifetime(base::Days(1));
  EXPECT_NE(clone, moved);
}

}  // namespace content_settings
