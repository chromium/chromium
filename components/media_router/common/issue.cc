// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/issue.h"

#include "base/atomic_sequence_num.h"
#include "base/check.h"

namespace media_router {

namespace {
// ID generator for Issue.
base::AtomicSequenceNumber g_next_issue_id;
}  // namespace

IssueInfo::IssueInfo() = default;
IssueInfo::IssueInfo(const IssueInfo&) = default;
IssueInfo& IssueInfo::operator=(const IssueInfo&) = default;
IssueInfo::IssueInfo(IssueInfo&&) = default;
IssueInfo& IssueInfo::operator=(IssueInfo&&) = default;
IssueInfo::~IssueInfo() = default;

IssueInfo::IssueInfo(const std::string& title,
                     Severity severity,
                     MediaSink::Id sink_id)
    : title(title), severity(severity), sink_id(sink_id) {}

bool IssueInfo::operator==(const IssueInfo& other) const {
  return title == other.title && severity == other.severity &&
         message == other.message && route_id == other.route_id &&
         sink_id == other.sink_id;
}

Issue Issue::CreatePermissionRejectedIssue() {
  return Issue(/*is_permission_rejected_issue*/ true);
}
Issue Issue::CreateIssueWithIssueInfo(IssueInfo info) {
  return Issue(info);
}

Issue::Issue(bool is_permission_rejected_issue)
    : id_(g_next_issue_id.GetNext()),
      is_permission_rejected_issue_(is_permission_rejected_issue) {
  info_.severity = IssueInfo::Severity::WARNING;
}
Issue::Issue(IssueInfo info)
    : id_(g_next_issue_id.GetNext()), info_(std::move(info)) {}

Issue::Issue(const Issue&) = default;
Issue& Issue::operator=(const Issue&) = default;
Issue::Issue(Issue&&) = default;
Issue& Issue::operator=(Issue&&) = default;
Issue::~Issue() = default;

}  // namespace media_router
