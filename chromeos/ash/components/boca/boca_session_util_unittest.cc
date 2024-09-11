// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/boca_session_util.h"

#include "chromeos/ash/components/boca/proto/roster.pb.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::boca {
namespace {

class BocaSessionUtilTest : public testing::Test {
 protected:
  BocaSessionUtilTest() = default;
};

TEST_F(BocaSessionUtilTest, TestGetSessionConfigWithNullInputShouldNotCrash) {
  ASSERT_EQ(::boca::SessionConfig().SerializeAsString(),
            GetSessionConfigSafe(nullptr).SerializeAsString());
  ::boca::Session session;
  ASSERT_EQ(::boca::SessionConfig().SerializeAsString(),
            GetSessionConfigSafe(&session).SerializeAsString());
  session.mutable_student_group_configs()->emplace("test",
                                                   ::boca::SessionConfig());
  ASSERT_EQ(::boca::SessionConfig().SerializeAsString(),
            GetSessionConfigSafe(&session).SerializeAsString());

  auto session_config = ::boca::SessionConfig();
  session_config.mutable_captions_config()->set_captions_enabled(true);
  session.mutable_student_group_configs()->emplace(kMainStudentGroupName,
                                                   session_config);

  ASSERT_EQ(session_config.SerializeAsString(),
            GetSessionConfigSafe(&session).SerializeAsString());
}

TEST_F(BocaSessionUtilTest, TestGetStudentGroupsWithNullInputShouldNotCrash) {
  ASSERT_EQ(0, GetStudentGroupsSafe(nullptr).size());

  ::boca::Session session;
  ASSERT_EQ(0, GetStudentGroupsSafe(&session).size());

  auto* student_groups =
      session.mutable_roster()->mutable_student_groups()->Add();
  student_groups->set_title("main");
  auto* student = student_groups->mutable_students()->Add();
  student->set_email("test");
  ASSERT_EQ(1, GetStudentGroupsSafe(&session).size());
}

TEST_F(BocaSessionUtilTest, TestGetRosterWithNullInputShouldNotCrash) {
  ASSERT_EQ(::boca::Roster().SerializeAsString(),
            GetRosterSafe(nullptr).SerializeAsString());

  ::boca::Session session;
  session.mutable_roster()->set_roster_id("123");
  ASSERT_EQ("123", GetRosterSafe(&session).roster_id());
}
}  // namespace
}  // namespace ash::boca
