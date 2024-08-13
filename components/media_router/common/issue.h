// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_ISSUE_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_ISSUE_H_

#include <string>

#include "components/media_router/common/media_route.h"
#include "components/media_router/common/media_sink.h"

namespace media_router {

// Contains the information relevant to an issue.
struct IssueInfo {
 public:
  // Severity type of an issue.
  enum class Severity { WARNING, NOTIFICATION };

  // Used by Mojo and testing only.
  IssueInfo();

  // |title|: The title for the issue.
  // |severity|: The severity of the issue.
  // |sink_id|: ID of the associated MediaSink.
  IssueInfo(const std::string& title, Severity severity, MediaSink::Id sink_id);
  IssueInfo(const IssueInfo&);
  IssueInfo& operator=(const IssueInfo&);
  IssueInfo(IssueInfo&&);
  IssueInfo& operator=(IssueInfo&&);
  ~IssueInfo();

  bool operator==(const IssueInfo& other) const;

  // Fields set with values provided to the constructor.
  std::string title;
  Severity severity{IssueInfo::Severity::NOTIFICATION};

  // Description message for the issue.
  std::string message;

  // ID of route associated with the Issue, or empty if no route is associated
  // with it.
  MediaRoute::Id route_id;

  // ID of the sink associated with this issue, or empty if no sink is
  // associated with it.
  MediaSink::Id sink_id;
};

// An issue that is associated with a globally unique ID. Created by
// IssueManager when an IssueInfo is added to it.
class Issue {
 public:
  using Id = int;

  static Issue CreatePermissionRejectedIssue();
  static Issue CreateIssueWithIssueInfo(IssueInfo info);

  Issue(const Issue&);
  Issue& operator=(const Issue&);
  Issue(Issue&&);
  Issue& operator=(Issue&&);
  ~Issue();

  const Id& id() const { return id_; }
  const IssueInfo& info() const { return info_; }
  bool is_permission_rejected_issue() const {
    return is_permission_rejected_issue_;
  }

 private:
  explicit Issue(bool is_permission_rejected_issue);
  explicit Issue(IssueInfo info);

  // ID is generated during construction.
  Id id_;
  IssueInfo info_;
  bool is_permission_rejected_issue_ = false;
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_ISSUE_H_
