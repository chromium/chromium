// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_ISSUE_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_ISSUE_H_

#include <string>
#include <vector>

#include "components/media_router/common/media_route.h"
#include "components/media_router/common/media_sink.h"

namespace media_router {

// Contains the information relevant to an issue.
struct IssueInfo {
 public:
  // TODO(crbug.com/1366822): Secondary actions are no longer used and can be
  // removed.
  // Possible actions for an issue.
  enum class Action {
    DISMISS,
    // NOTE: If LEARN_MORE is set as a possible action for an issue, then its
    // |help_page_id_| must also be set to a valid value.
    LEARN_MORE,

    // Denotes enum value boundary. New values should be added above.
    NUM_VALUES = LEARN_MORE
  };

  // Severity type of an issue.
  enum class Severity { WARNING, NOTIFICATION };

  static const int kUnknownHelpPageId = 0;

  // Used by Mojo and testing only.
  IssueInfo();

  // |title|: The title for the issue.
  // |default_action|: Default action user can take to resolve the issue.
  // |severity|: The severity of the issue.
  IssueInfo(const std::string& title, Action default_action, Severity severity);
  IssueInfo(const IssueInfo& other);
  ~IssueInfo();

  IssueInfo& operator=(const IssueInfo& other);
  bool operator==(const IssueInfo& other) const;

  // Fields set with values provided to the constructor.
  std::string title;
  Action default_action;
  Severity severity;

  // Description message for the issue.
  std::string message;

  // Options the user can take to resolve the issue in addition to the
  // default action. Can be empty. If non-empty, currently only one secondary
  // action is supported.
  std::vector<Action> secondary_actions;

  // ID of route associated with the Issue, or empty if no route is associated
  // with it.
  MediaRoute::Id route_id;

  // ID of the sink associated with this issue, or empty if no sink is
  // associated with it.
  MediaSink::Id sink_id;

  // ID of help page to link to, if one of the actions is LEARN_MORE.
  // Defaults to |kUnknownHelpPageId|.
  int help_page_id;
};

// An issue that is associated with a globally unique ID. Created by
// IssueManager when an IssueInfo is added to it.
class Issue {
 public:
  using Id = int;
  // ID is generated during construction.
  explicit Issue(const IssueInfo& info);
  Issue(const Issue& other) = default;
  Issue& operator=(const Issue& other) = default;
  ~Issue();

  const Id& id() const { return id_; }
  const IssueInfo& info() const { return info_; }

 private:
  Id id_;
  IssueInfo info_;
};

}  // namespace media_router

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_ISSUE_H_
