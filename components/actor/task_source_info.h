// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACTOR_TASK_SOURCE_INFO_H_
#define COMPONENTS_ACTOR_TASK_SOURCE_INFO_H_

#include <optional>
#include <string>

namespace actor {

// This object is used when creating an ActorTask to identify the source of the
// task. This is identifying the client type that created the task and, if the
// client supports it, an opaque identifier identifying the creating object in
// the client.
struct TaskSourceInfo {
  using SourceDefinedId = std::string;

  // These values will be persisted so can be added to but existing values must
  // not be renumbered.
  enum class Client {
    kUnknown = 0,
    // Used by internal tests.
    kTest = 1,
    kExperimentalActor = 2,
    kGlic = 3,
  };

  TaskSourceInfo(Client type, std::optional<SourceDefinedId> id);
  ~TaskSourceInfo();
  TaskSourceInfo(const TaskSourceInfo&);
  TaskSourceInfo& operator=(const TaskSourceInfo&);
  TaskSourceInfo(TaskSourceInfo&&);
  TaskSourceInfo& operator=(TaskSourceInfo&&);

  Client type;
  std::optional<SourceDefinedId> id;

  // Equality could mean different things in different cases. For clients that
  // support SourceDefinedId callers probably expect the ids to match indicating
  // a task-level equality but for clients without id equality is used only to
  // compare clients. To avoid potential confusion callers should just compare
  // subobjects which makes it clearer what is intended.
  bool operator==(const TaskSourceInfo& other) const = delete;
  bool operator!=(const TaskSourceInfo& other) const = delete;
};

}  // namespace actor

#endif  // COMPONENTS_ACTOR_TASK_SOURCE_INFO_H_
