// Copyright 2015 The Chromium Authors. All rights reserved.
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

IssueInfo CreateFatalRouteIssueInfoWithMessage(IssueInfo::Action action_type) {
  IssueInfo issue("title", action_type, IssueInfo::Severity::FATAL);
  issue.message = "message";
  issue.route_id = "routeid";
  issue.help_page_id = 12345;
  return issue;
}

IssueInfo CreateFatalRouteIssueInfo(IssueInfo::Action action_type) {
  IssueInfo issue("title", action_type, IssueInfo::Severity::FATAL);
  issue.route_id = "routeid";
  issue.help_page_id = 12345;
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
  EXPECT_FALSE(issue1.is_blocking);
  EXPECT_EQ(12345, issue1.help_page_id);

  IssueInfo issue2 =
      CreateFatalRouteIssueInfoWithMessage(IssueInfo::Action::DISMISS);

  EXPECT_EQ("title", issue2.title);
  EXPECT_EQ("message", issue2.message);
  EXPECT_EQ(IssueInfo::Action::DISMISS, issue1.default_action);
  EXPECT_TRUE(issue2.secondary_actions.empty());
  EXPECT_EQ(IssueInfo::Severity::FATAL, issue2.severity);
  EXPECT_EQ("routeid", issue2.route_id);
  EXPECT_TRUE(issue2.is_blocking);
  EXPECT_EQ(12345, issue2.help_page_id);

  IssueInfo issue3 = CreateFatalRouteIssueInfo(IssueInfo::Action::DISMISS);

  EXPECT_EQ("title", issue3.title);
  EXPECT_EQ("", issue3.message);
  EXPECT_EQ(IssueInfo::Action::DISMISS, issue1.default_action);
  EXPECT_TRUE(issue3.secondary_actions.empty());
  EXPECT_EQ(IssueInfo::Severity::FATAL, issue3.severity);
  EXPECT_EQ("routeid", issue3.route_id);
  EXPECT_TRUE(issue3.is_blocking);
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
  EXPECT_FALSE(issue1.is_blocking);

  IssueInfo issue2 =
      CreateFatalRouteIssueInfoWithMessage(IssueInfo::Action::LEARN_MORE);
  issue2.secondary_actions = secondary_actions;

  EXPECT_EQ("title", issue2.title);
  EXPECT_EQ("message", issue2.message);
  EXPECT_EQ(IssueInfo::Action::LEARN_MORE, issue2.default_action);
  EXPECT_FALSE(issue2.secondary_actions.empty());
  EXPECT_EQ(1u, issue2.secondary_actions.size());
  EXPECT_EQ(IssueInfo::Severity::FATAL, issue2.severity);
  EXPECT_EQ("routeid", issue2.route_id);
  EXPECT_TRUE(issue2.is_blocking);

  IssueInfo issue3 = CreateFatalRouteIssueInfo(IssueInfo::Action::LEARN_MORE);
  issue3.secondary_actions = secondary_actions;

  EXPECT_EQ("title", issue3.title);
  EXPECT_EQ("", issue3.message);
  EXPECT_EQ(IssueInfo::Action::LEARN_MORE, issue3.default_action);
  EXPECT_FALSE(issue3.secondary_actions.empty());
  EXPECT_EQ(1u, issue3.secondary_actions.size());
  EXPECT_EQ(IssueInfo::Severity::FATAL, issue3.severity);
  EXPECT_EQ("routeid", issue3.route_id);
  EXPECT_TRUE(issue3.is_blocking);
}

}  // namespace media_router
