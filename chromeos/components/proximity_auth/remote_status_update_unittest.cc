// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/proximity_auth/remote_status_update.h"

#include "base/json/json_reader.h"
#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace proximity_auth {
namespace {

// Parses the |json| into a RemoteStatusUpdate instance.
std::unique_ptr<RemoteStatusUpdate> ParseJson(const std::string& json) {
  std::unique_ptr<base::DictionaryValue> as_dictionary =
      base::DictionaryValue::From(base::JSONReader::ReadDeprecated(json));
  return RemoteStatusUpdate::Deserialize(*as_dictionary);
}

}  // namespace

// Verify that all valid values can be parsed.
TEST(ProximityAuthRemoteStatusUpdateTest, Deserialize_Valid_UserPresent) {
  const char kValidJson[] =
      R"({
        "type": "status_update",
        "user_presence": "present",
        "secure_screen_lock": "enabled",
        "trust_agent": "enabled"
      })";
  std::unique_ptr<RemoteStatusUpdate> parsed_update = ParseJson(kValidJson);
  ASSERT_TRUE(parsed_update);
  EXPECT_EQ(USER_PRESENT, parsed_update->user_presence);
  EXPECT_EQ(SECURE_SCREEN_LOCK_ENABLED,
            parsed_update->secure_screen_lock_state);
  EXPECT_EQ(TRUST_AGENT_ENABLED, parsed_update->trust_agent_state);
}

TEST(ProximityAuthRemoteStatusUpdateTest, Deserialize_Valid_UserAbsent) {
  const char kValidJson[] =
      R"({
        "type": "status_update",
        "user_presence": "absent",
        "secure_screen_lock": "disabled",
        "trust_agent": "disabled"
      })";
  std::unique_ptr<RemoteStatusUpdate> parsed_update = ParseJson(kValidJson);
  ASSERT_TRUE(parsed_update);
  EXPECT_EQ(USER_ABSENT, parsed_update->user_presence);
  EXPECT_EQ(SECURE_SCREEN_LOCK_DISABLED,
            parsed_update->secure_screen_lock_state);
  EXPECT_EQ(TRUST_AGENT_DISABLED, parsed_update->trust_agent_state);
}

TEST(ProximityAuthRemoteStatusUpdateTest, Deserialize_Valid_UserSecondary) {
  const char kValidJson[] =
      R"({
        "type": "status_update",
        "user_presence": "secondary",
        "secure_screen_lock": "disabled",
        "trust_agent": "disabled"
      })";
  std::unique_ptr<RemoteStatusUpdate> parsed_update = ParseJson(kValidJson);
  ASSERT_TRUE(parsed_update);
  EXPECT_EQ(USER_PRESENCE_SECONDARY, parsed_update->user_presence);
  EXPECT_EQ(SECURE_SCREEN_LOCK_DISABLED,
            parsed_update->secure_screen_lock_state);
  EXPECT_EQ(TRUST_AGENT_DISABLED, parsed_update->trust_agent_state);
}

TEST(ProximityAuthRemoteStatusUpdateTest, Deserialize_Valid_UserBackground) {
  const char kValidJson[] =
      R"({
        "type": "status_update",
        "user_presence": "background",
        "secure_screen_lock": "disabled",
        "trust_agent": "disabled"
      })";
  std::unique_ptr<RemoteStatusUpdate> parsed_update = ParseJson(kValidJson);
  ASSERT_TRUE(parsed_update);
  EXPECT_EQ(USER_PRESENCE_BACKGROUND, parsed_update->user_presence);
  EXPECT_EQ(SECURE_SCREEN_LOCK_DISABLED,
            parsed_update->secure_screen_lock_state);
  EXPECT_EQ(TRUST_AGENT_DISABLED, parsed_update->trust_agent_state);
}

TEST(ProximityAuthRemoteStatusUpdateTest, Deserialize_Valid_Unknown) {
  const char kValidJson[] =
      R"({
        "type": "status_update",
        "user_presence": "unknown",
        "secure_screen_lock": "unknown",
        "trust_agent": "unsupported"
      })";
  std::unique_ptr<RemoteStatusUpdate> parsed_update = ParseJson(kValidJson);
  ASSERT_TRUE(parsed_update);
  EXPECT_EQ(USER_PRESENCE_UNKNOWN, parsed_update->user_presence);
  EXPECT_EQ(SECURE_SCREEN_LOCK_STATE_UNKNOWN,
            parsed_update->secure_screen_lock_state);
  EXPECT_EQ(TRUST_AGENT_UNSUPPORTED, parsed_update->trust_agent_state);
}

TEST(ProximityAuthRemoteStatusUpdateTest, Deserialize_MissingUserPresence) {
  const char kJson[] =
      R"({
        "type": "status_update",
        "secure_screen_lock": "enabled",
        "trust_agent": "enabled"
      })";
  std::unique_ptr<RemoteStatusUpdate> parsed_update = ParseJson(kJson);
  EXPECT_FALSE(parsed_update);
}

TEST(ProximityAuthRemoteStatusUpdateTest, Deserialize_MissingSecureScreenLock) {
  const char kJson[] =
      R"({
        "type": "status_update",
        "user_presence": "present",
        "trust_agent": "enabled"
      })";
  std::unique_ptr<RemoteStatusUpdate> parsed_update = ParseJson(kJson);
  EXPECT_FALSE(parsed_update);
}

TEST(ProximityAuthRemoteStatusUpdateTest, Deserialize_MissingTrustAgent) {
  const char kJson[] =
      R"({
        "type": "status_update",
        "user_presence": "present",
        "secure_screen_lock": "enabled"
      })";
  std::unique_ptr<RemoteStatusUpdate> parsed_update = ParseJson(kJson);
  EXPECT_FALSE(parsed_update);
}

TEST(ProximityAuthRemoteStatusUpdateTest, Deserialize_InvalidType) {
  const char kJson[] =
      R"({
        "type": "garbage",
        "user_presence": "present",
        "secure_screen_lock": "enabled",
        "trust_agent": "enabled"
      })";
  std::unique_ptr<RemoteStatusUpdate> parsed_update = ParseJson(kJson);
  EXPECT_FALSE(parsed_update);
}

TEST(ProximityAuthRemoteStatusUpdateTest, Deserialize_InvalidPresence) {
  const char kJson[] =
      R"({
        "type": "status_update",
        "user_presence": "garbage",
        "secure_screen_lock": "enabled",
        "trust_agent": "enabled"
      })";
  std::unique_ptr<RemoteStatusUpdate> parsed_update = ParseJson(kJson);
  EXPECT_FALSE(parsed_update);
}

TEST(ProximityAuthRemoteStatusUpdateTest, Deserialize_InvalidLock) {
  const char kJson[] =
      R"({
        "type": "status_update",
        "user_presence": "present",
        "secure_screen_lock": "garbage",
        "trust_agent": "enabled"
      })";
  std::unique_ptr<RemoteStatusUpdate> parsed_update = ParseJson(kJson);
  EXPECT_FALSE(parsed_update);
}

TEST(ProximityAuthRemoteStatusUpdateTest, Deserialize_InvalidAgent) {
  const char kJson[] =
      R"({
        "type": "status_update",
        "user_presence": "present",
        "secure_screen_lock": "enabled",
        "trust_agent": "garbage"
      })";
  std::unique_ptr<RemoteStatusUpdate> parsed_update = ParseJson(kJson);
  EXPECT_FALSE(parsed_update);
}

// Verify that extra fields do not prevent parsing. This provides
// forward-compatibility.
TEST(ProximityAuthRemoteStatusUpdateTest,
     Deserialize_ValidStatusWithExtraFields) {
  const char kJson[] =
      R"({
        "type": "status_update",
        "user_presence": "present",
        "secure_screen_lock": "enabled",
        "trust_agent": "enabled",
        "secret_sauce": "chipotle"
      })";
  std::unique_ptr<RemoteStatusUpdate> parsed_update = ParseJson(kJson);
  ASSERT_TRUE(parsed_update);
  EXPECT_EQ(USER_PRESENT, parsed_update->user_presence);
  EXPECT_EQ(SECURE_SCREEN_LOCK_ENABLED,
            parsed_update->secure_screen_lock_state);
  EXPECT_EQ(TRUST_AGENT_ENABLED, parsed_update->trust_agent_state);
}

}  // namespace proximity_auth
