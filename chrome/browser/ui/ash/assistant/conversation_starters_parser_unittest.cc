// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/assistant/conversation_starters_parser.h"

#include "ash/public/cpp/assistant/conversation_starter.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Helpers ---------------------------------------------------------------------

void ExpectEqual(const ash::ConversationStarter& a,
                 const ash::ConversationStarter& b) {
  EXPECT_EQ(a.label(), b.label());
  EXPECT_EQ(a.action_url(), b.action_url());
  EXPECT_EQ(a.icon_url(), b.icon_url());
  EXPECT_EQ(a.required_permissions(), b.required_permissions());
}

}  // namespace

// Tests -----------------------------------------------------------------------

using ConversationStartersParserTest = ChromeAshTestBase;

// Verifies handling of an empty response.
TEST_F(ConversationStartersParserTest, HandlesEmptyResponse) {
  const std::vector<ash::ConversationStarter> conversation_starters =
      ConversationStartersParser::Parse(std::string());
  EXPECT_TRUE(conversation_starters.empty());
}

// Verifies handling of a non-safe JSON response.
TEST_F(ConversationStartersParserTest, HandlesJsonResponse) {
  const std::vector<ash::ConversationStarter> conversation_starters =
      ConversationStartersParser::Parse(R"(
        {
          "conversationStarter": [
            {
              "label": "Label",
              "iconUrl": "https://g.co/iconUrl",
              "actionUrl": "https://g.co/actionUrl",
              "requiredPermission": []
            }
          ]
        })");

  EXPECT_TRUE(conversation_starters.empty());
}

// Verifies handling of a safe JSON response that is not recognized.
TEST_F(ConversationStartersParserTest, HandlesUnrecognizedSafeJsonResponse) {
  const std::vector<ash::ConversationStarter> conversation_starters =
      ConversationStartersParser::Parse(R"()]}'
          {
            "unrecognizedConversationStarter": [
              {
                "label": "Label"
              },
              {
                "label": "Label w/ Action, Icon, and Empty Permissions",
                "iconUrl": "https://g.co/iconUrl",
                "actionUrl": "https://g.co/actionUrl",
                "requiredPermission": []
              },
              {
                "label": "Label w/ Action, Icon, and Required Permissions",
                "iconUrl": "https://g.co/iconUrl",
                "actionUrl": "https://g.co/actionUrl",
                "requiredPermission": [
                  "RELATED_INFO"
                ]
              }
            ]
          })");

  EXPECT_TRUE(conversation_starters.empty());
}

// Verifies handling of a safe JSON response that is recognized.
// Note that |label| is required, but |actionUrl|, |iconUrl|, and
// |requiredPermissions| are optional.
TEST_F(ConversationStartersParserTest, HandlesRecognizedSafeJsonResponse) {
  const std::vector<ash::ConversationStarter> conversation_starters =
      ConversationStartersParser::Parse(R"()]}'
          {
            "conversationStarter": [
              {
                "label": "Label"
              },
              {
                "label": "Label w/ Action, Icon, and Empty Permissions",
                "iconUrl": "https://g.co/iconUrl",
                "actionUrl": "https://g.co/actionUrl",
                "requiredPermission": []
              },
              {
                "label": "Label w/ Action, Icon, and Required Permissions",
                "iconUrl": "https://g.co/iconUrl",
                "actionUrl": "https://g.co/actionUrl",
                "requiredPermission": [
                  "RELATED_INFO"
                ]
              }
            ]
          })");

  EXPECT_EQ(3u, conversation_starters.size());

  ExpectEqual(conversation_starters.at(0),
              ash::ConversationStarter("Label", /*action_url=*/absl::nullopt,
                                       /*icon_url=*/absl::nullopt,
                                       /*required_permissions=*/0u));

  ExpectEqual(
      conversation_starters.at(1),
      ash::ConversationStarter("Label w/ Action, Icon, and Empty Permissions",
                               /*action_url=*/GURL("https://g.co/actionUrl"),
                               /*icon_url=*/GURL("https://g.co/iconUrl"),
                               /*required_permissions=*/0u));

  ExpectEqual(conversation_starters.at(2),
              ash::ConversationStarter(
                  "Label w/ Action, Icon, and Required Permissions",
                  /*action_url=*/GURL("https://g.co/actionUrl"),
                  /*icon_url=*/GURL("https://g.co/iconUrl"),
                  /*required_permissions=*/
                  ash::ConversationStarter::Permission::kRelatedInfo));
}

// Verifies that a conversation starter that does not specify a |label| is
// omitted from the collection.
TEST_F(ConversationStartersParserTest, HandlesMissingLabel) {
  const std::vector<ash::ConversationStarter> conversation_starters =
      ConversationStartersParser::Parse(R"()]}'
          {
            "conversationStarter": [
              {
                "iconUrl": "https://g.co/iconUrl",
                "actionUrl": "https://g.co/actionUrl",
                "requiredPermission": []
              }
            ]
          })");

  EXPECT_TRUE(conversation_starters.empty());
}

// Verifies that a conversation starter that specifies a required permission
// that is not recognized is marked as requiring an unknown permission.
TEST_F(ConversationStartersParserTest, HandlesUnknownPermission) {
  const std::vector<ash::ConversationStarter> conversation_starters =
      ConversationStartersParser::Parse(R"()]}'
          {
            "conversationStarter": [
              {
                "label": "Unrecognized Required Permission",
                "requiredPermission": [
                  "RELATED_INFO",
                  "FOO"
                ]
              }
            ]
          })");

  EXPECT_EQ(1u, conversation_starters.size());

  ExpectEqual(conversation_starters.at(0),
              ash::ConversationStarter(
                  /*label=*/"Unrecognized Required Permission",
                  /*action_url=*/absl::nullopt,
                  /*icon_url=*/absl::nullopt,
                  /*required_permissions=*/
                  ash::ConversationStarter::Permission::kRelatedInfo |
                      ash::ConversationStarter::Permission::kUnknown));
}
