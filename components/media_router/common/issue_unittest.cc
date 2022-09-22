// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/issue.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace media_router {

namespace {

IssueInfo CreateWarningIssueInfo(IssueInfo::Action action_type) {
  IssueInfo issue("title", action_type, IssueInfo::Severity::WARNING);
  issue.message = "message";
  issue.help_page_id = 12345;
  return issue;
}

IssueInfo CreateNotificationRouteIssueInfoWithMessage(
    IssueInfo::Action action_type) {
  IssueInfo issue("title", action_type, IssueInfo::Severity::NOTIFICATION);
  issue.message = "message";
  issue.help_page_id = 12345;
  issue.route_id = "routeid";
  return issue;
}

IssueInfo CreateNotificationRouteIssueInfo(IssueInfo::Action action_type) {
  IssueInfo issue("title", action_type, IssueInfo::Severity::NOTIFICATION);
  issue.help_page_id = 12345;
  issue.route_id = "routeid";
  return issue;
}

}  // namespace

// Tests Issues without any secondary actions.
TEST(IssueInfoUnitTest, CustomIssueConstructionWithNoSecondaryActions) {
  IssueInfo issue1 = CreateWarningIssueInfo(IssueInfo::Action::DISMISS);

  EXPECT_EQ("title", issue1.title);
  EXPECT_EQ("message", issue1.message);
  EXPECT_EQ(IssueInfo::Action::DISMISS, issue1.default_action);
  EXPECT_TRUE(issue1.secondary_actions.empty());
  EXPECT_EQ(IssueInfo::Severity::WARNING, issue1.severity);
  EXPECT_EQ("", issue1.route_id);
  EXPECT_EQ(12345, issue1.help_page_id);

  IssueInfo issue2 =
      CreateNotificationRouteIssueInfoWithMessage(IssueInfo::Action::DISMISS);

  EXPECT_EQ("title", issue2.title);
  EXPECT_EQ("message", issue2.message);
  EXPECT_EQ(IssueInfo::Action::DISMISS, issue1.default_action);
  EXPECT_TRUE(issue2.secondary_actions.empty());
  EXPECT_EQ(IssueInfo::Severity::NOTIFICATION, issue2.severity);
  EXPECT_EQ("routeid", issue2.route_id);
  EXPECT_EQ(12345, issue2.help_page_id);

  IssueInfo issue3 =
      CreateNotificationRouteIssueInfo(IssueInfo::Action::DISMISS);

  EXPECT_EQ("title", issue3.title);
  EXPECT_EQ("", issue3.message);
  EXPECT_EQ(IssueInfo::Action::DISMISS, issue1.default_action);
  EXPECT_TRUE(issue3.secondary_actions.empty());
  EXPECT_EQ(IssueInfo::Severity::NOTIFICATION, issue3.severity);
  EXPECT_EQ("routeid", issue3.route_id);
  EXPECT_EQ(12345, issue3.help_page_id);
}

// Tests Issues with secondary actions.
TEST(IssueInfoUnitTest, CustomIssueConstructionWithSecondaryActions) {
  std::vector<IssueInfo::Action> secondary_actions;
  secondary_actions.push_back(IssueInfo::Action::DISMISS);

  IssueInfo issue1 = CreateWarningIssueInfo(IssueInfo::Action::LEARN_MORE);
  issue1.secondary_actions = secondary_actions;

  EXPECT_EQ("title", issue1.title);
  EXPECT_EQ("message", issue1.message);
  EXPECT_EQ(IssueInfo::Action::LEARN_MORE, issue1.default_action);
  EXPECT_FALSE(issue1.secondary_actions.empty());
  EXPECT_EQ(1u, issue1.secondary_actions.size());
  EXPECT_EQ(IssueInfo::Severity::WARNING, issue1.severity);
  EXPECT_EQ("", issue1.route_id);

  IssueInfo issue2 = CreateNotificationRouteIssueInfoWithMessage(
      IssueInfo::Action::LEARN_MORE);
  issue2.secondary_actions = secondary_actions;

  EXPECT_EQ("title", issue2.title);
  EXPECT_EQ("message", issue2.message);
  EXPECT_EQ(IssueInfo::Action::LEARN_MORE, issue2.default_action);
  EXPECT_FALSE(issue2.secondary_actions.empty());
  EXPECT_EQ(1u, issue2.secondary_actions.size());
  EXPECT_EQ(IssueInfo::Severity::NOTIFICATION, issue2.severity);
  EXPECT_EQ("routeid", issue2.route_id);

  IssueInfo issue3 =
      CreateNotificationRouteIssueInfo(IssueInfo::Action::LEARN_MORE);
  issue3.secondary_actions = secondary_actions;

  EXPECT_EQ("title", issue3.title);
  EXPECT_EQ("", issue3.message);
  EXPECT_EQ(IssueInfo::Action::LEARN_MORE, issue3.default_action);
  EXPECT_FALSE(issue3.secondary_actions.empty());
  EXPECT_EQ(1u, issue3.secondary_actions.size());
  EXPECT_EQ(IssueInfo::Severity::NOTIFICATION, issue3.severity);
  EXPECT_EQ("routeid", issue3.route_id);
}

}  // namespace media_router
