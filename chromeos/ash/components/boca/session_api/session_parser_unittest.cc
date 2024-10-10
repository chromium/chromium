// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/boca/session_api/session_parser.h"

#include "base/memory/values_equivalent.h"
#include "base/values.h"
#include "chromeos/ash/components/boca/proto/session.pb.h"
#include "chromeos/ash/components/boca/session_api/constants.h"
#include "google_apis/common/base_requests.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::boca {
namespace {

// Unit test cases for proto2json conversion is covered in
// create_session_request_unittest.cc, not duplicating here.
constexpr char kFullSessionResponse[] = R"(
  {
  "startTime":{
    "seconds": 1723773909
  },
  "sessionId": "111",
  "duration": {
    "seconds": 120
  },
  "studentStatuses": {
    "2": {
      "state": "ADDED"
    },
    "3": {
      "state": "ACTIVE",
       "devices":
        {
          "kDummyDeviceId":
         {
            "info": {"device_id":"kDummyDeviceId"},
            "state":"ACTIVE",
            "activity": {
              "activeTab": {
                "title": "google"
                }
              }
         }
        }
    }
  },
  "roster": {
    "studentGroups": [{
      "students": [
        {
          "email": "cat@gmail.com",
          "fullName": "cat",
          "gaiaId": "2",
          "photoUrl": "data:image/123"
        },
        {
          "email": "dog@gmail.com",
          "fullName": "dog",
          "gaiaId": "3",
          "photoUrl": "data:image/123"
        }
      ],
      "title": "main"
    }]
  },
  "sessionState": "ACTIVE",
  "studentGroupConfigs": {
    "main": {
      "captionsConfig": {
        "captionsEnabled": true,
        "translationsEnabled": true
      },
      "onTaskConfig": {
        "activeBundle": {
          "contentConfigs": [
            {
              "faviconUrl": "data:image/123",
              "lockedNavigationOptions": {
                "navigationType": "OPEN_NAVIGATION"
              },
              "title": "google",
              "url": "https://google.com"
            },
            {
              "faviconUrl": "data:image/123",
              "lockedNavigationOptions": {
                "navigationType": "BLOCK_NAVIGATION"
              },
              "title": "youtube",
              "url": "https://youtube.com"
            }
          ],
          "locked": true
        }
      }
    }
  },
  "teacher":
  {
    "email": "teacher@gmail.com",
    "fullName": "teacher",
    "gaiaId": "1",
      "photoUrl": "data:image/123"
        }
        }
       )";

constexpr char kPartialResponse[] = R"(
     {
    "sessionId": "111",
    "duration": {
        "seconds": 120
    },
    "studentStatuses": {},
    "roster": {
        "studentGroups": []
    },
    "sessionState": "ACTIVE",
    "studentGroupConfigs": {
        "main": {
            "captionsConfig": {},
            "onTaskConfig": {
                "activeBundle": {
                    "contentConfigs": []
                }
            }
        }
    },
    "teacher": {
        "gaiaId": "1"
    }
    }
    )";

class SessionParserTest : public testing::Test {
 protected:
  SessionParserTest() = default;
  void SetUp() override {
    session_dict_full = google_apis::ParseJson(kFullSessionResponse);
    session_dict_partial = google_apis::ParseJson(kPartialResponse);
    session_full = std::make_unique<::boca::Session>();
    session_partial = std::make_unique<::boca::Session>();
  }
  std::unique_ptr<::boca::Session> session_full;
  std::unique_ptr<::boca::Session> session_partial;
  std::unique_ptr<base::Value> session_dict_full;
  std::unique_ptr<base::Value> session_dict_partial;
};

TEST_F(SessionParserTest, TestParseTeacherProtoFromJson) {
  ParseTeacherProtoFromJson(session_dict_full->GetIfDict(), session_full.get());
  EXPECT_EQ("teacher@gmail.com", session_full->teacher().email());
  EXPECT_EQ("teacher", session_full->teacher().full_name());
  EXPECT_EQ("1", session_full->teacher().gaia_id());
  EXPECT_EQ("data:image/123", session_full->teacher().photo_url());

  ParseTeacherProtoFromJson(session_dict_partial->GetIfDict(),
                            session_partial.get());
  EXPECT_EQ("1", session_partial->teacher().gaia_id());
}

TEST_F(SessionParserTest, TestParseRosterProtoFromJson) {
  ParseRosterProtoFromJson(session_dict_full->GetIfDict(), session_full.get());
  ASSERT_EQ(2, session_full->roster().student_groups()[0].students().size());
  EXPECT_EQ(kMainStudentGroupName,
            session_full->roster().student_groups()[0].title());

  EXPECT_EQ("cat@gmail.com",
            session_full->roster().student_groups()[0].students()[0].email());
  EXPECT_EQ(
      "cat",
      session_full->roster().student_groups()[0].students()[0].full_name());
  EXPECT_EQ("2",
            session_full->roster().student_groups()[0].students()[0].gaia_id());
  EXPECT_EQ(
      "data:image/123",
      session_full->roster().student_groups()[0].students()[0].photo_url());

  EXPECT_EQ("dog@gmail.com",
            session_full->roster().student_groups()[0].students()[1].email());
  EXPECT_EQ(
      "dog",
      session_full->roster().student_groups()[0].students()[1].full_name());
  EXPECT_EQ("3",
            session_full->roster().student_groups()[0].students()[1].gaia_id());
  EXPECT_EQ(
      "data:image/123",
      session_full->roster().student_groups()[0].students()[1].photo_url());

  ParseRosterProtoFromJson(session_dict_partial->GetIfDict(),
                           session_partial.get());
  EXPECT_TRUE(session_partial->roster().student_groups().empty());
}

TEST_F(SessionParserTest, TestParseSessionConfigProtoFromJson) {
  // For producer.
  ParseSessionConfigProtoFromJson(session_dict_full->GetIfDict(),
                                  session_full.get());
  ASSERT_EQ(1u, session_full->student_group_configs().size());
  EXPECT_TRUE(session_full->student_group_configs()
                  .at(kMainStudentGroupName)
                  .captions_config()
                  .captions_enabled());
  EXPECT_TRUE(session_full->student_group_configs()
                  .at(kMainStudentGroupName)
                  .captions_config()
                  .translations_enabled());

  EXPECT_TRUE(session_full->student_group_configs()
                  .at(kMainStudentGroupName)
                  .on_task_config()
                  .active_bundle()
                  .locked());

  auto content_config = std::move(session_full->student_group_configs()
                                      .at(kMainStudentGroupName)
                                      .on_task_config()
                                      .active_bundle()
                                      .content_configs());
  ASSERT_EQ(2, content_config.size());

  EXPECT_EQ("data:image/123", content_config[0].favicon_url());
  EXPECT_EQ("google", content_config[0].title());
  EXPECT_EQ("https://google.com", content_config[0].url());
  EXPECT_EQ(::boca::LockedNavigationOptions::OPEN_NAVIGATION,
            content_config[0].locked_navigation_options().navigation_type());

  EXPECT_EQ("data:image/123", content_config[1].favicon_url());
  EXPECT_EQ("youtube", content_config[1].title());
  EXPECT_EQ("https://youtube.com", content_config[1].url());
  EXPECT_EQ(::boca::LockedNavigationOptions::BLOCK_NAVIGATION,
            content_config[1].locked_navigation_options().navigation_type());

  ParseSessionConfigProtoFromJson(session_dict_partial->GetIfDict(),
                                  session_partial.get());
  ASSERT_EQ(1u, session_partial->student_group_configs().size());

  auto content_config_1 = std::move(session_partial->student_group_configs()
                                        .at(kMainStudentGroupName)
                                        .on_task_config()
                                        .active_bundle()
                                        .content_configs());
  ASSERT_EQ(0, content_config_1.size());

  EXPECT_FALSE(session_partial->student_group_configs()
                   .at(kMainStudentGroupName)
                   .captions_config()
                   .captions_enabled());
  EXPECT_FALSE(session_partial->student_group_configs()
                   .at(kMainStudentGroupName)
                   .captions_config()
                   .translations_enabled());

  EXPECT_FALSE(session_partial->student_group_configs()
                   .at(kMainStudentGroupName)
                   .on_task_config()
                   .active_bundle()
                   .locked());
}

TEST_F(SessionParserTest, TestParseStudentStatusProtoFromJson) {
  // Student status depends on roster info.
  ParseRosterProtoFromJson(session_dict_full->GetIfDict(), session_full.get());
  ParseStudentStatusProtoFromJson(session_dict_full->GetIfDict(),
                                  session_full.get());
  ASSERT_EQ(2u, session_full->student_statuses().size());
  EXPECT_EQ(::boca::StudentStatus::ADDED,
            session_full->student_statuses().at("2").state());
  EXPECT_EQ(::boca::StudentStatus::ACTIVE,
            session_full->student_statuses().at("3").state());

  EXPECT_EQ("google", session_full->student_statuses()
                          .at("3")
                          .devices()
                          .at("kDummyDeviceId")
                          .activity()
                          .active_tab()
                          .title());
  ParseStudentStatusProtoFromJson(session_dict_partial->GetIfDict(),
                                  session_partial.get());
  EXPECT_EQ(0u, session_partial->student_statuses().size());
}
}  // namespace
}  // namespace ash::boca
