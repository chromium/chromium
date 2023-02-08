// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/issue.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace media_router {

namespace {

constexpr char kMediaSinkId[] = "sinkId1";

IssueInfo CreateWarningIssueInfo() {
  IssueInfo issue("title", IssueInfo::Severity::WARNING, kMediaSinkId);
  issue.message = "message";
  return issue;
}

IssueInfo CreateNotificationRouteIssueInfoWithMessage() {
  IssueInfo issue("title", IssueInfo::Severity::NOTIFICATION, kMediaSinkId);
  issue.message = "message";
  issue.route_id = "routeid";
  return issue;
}

IssueInfo CreateNotificationRouteIssueInfo() {
  IssueInfo issue("title", IssueInfo::Severity::NOTIFICATION, kMediaSinkId);
  issue.route_id = "routeid";
  return issue;
}

}  // namespace

TEST(IssueInfoUnitTest, CreateIssueInfo) {
  IssueInfo issue1 = CreateWarningIssueInfo();

  EXPECT_EQ("title", issue1.title);
  EXPECT_EQ("message", issue1.message);
  EXPECT_EQ(IssueInfo::Severity::WARNING, issue1.severity);
  EXPECT_EQ("", issue1.route_id);

  IssueInfo issue2 = CreateNotificationRouteIssueInfoWithMessage();

  EXPECT_EQ("title", issue2.title);
  EXPECT_EQ("message", issue2.message);
  EXPECT_EQ(IssueInfo::Severity::NOTIFICATION, issue2.severity);
  EXPECT_EQ("routeid", issue2.route_id);

  IssueInfo issue3 = CreateNotificationRouteIssueInfo();

  EXPECT_EQ("title", issue3.title);
  EXPECT_EQ("", issue3.message);
  EXPECT_EQ(IssueInfo::Severity::NOTIFICATION, issue3.severity);
  EXPECT_EQ("routeid", issue3.route_id);
}

}  // namespace media_router
