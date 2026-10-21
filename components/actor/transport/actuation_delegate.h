// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACTOR_TRANSPORT_ACTUATION_DELEGATE_H_
#define COMPONENTS_ACTOR_TRANSPORT_ACTUATION_DELEGATE_H_

#include "base/functional/callback_forward.h"
#include "components/actor/core/task_id.h"

namespace chrome::aix::autobrowse::proto {
class PerformActionsRequest;
class PerformActionsResponse;
class StartTaskRequest;
class StartTaskResponse;
class StopTaskRequest;
}  // namespace chrome::aix::autobrowse::proto

namespace actor {

namespace proto = ::chrome::aix::autobrowse::proto;

// Delegate interface for executing actuation tasks and actions on the browser.
class ActuationDelegate {
 public:
  virtual ~ActuationDelegate();

  using StartTaskCallback =
      base::OnceCallback<void(TaskId, proto::StartTaskResponse)>;
  virtual void StartTask(const proto::StartTaskRequest& request,
                         StartTaskCallback callback) = 0;

  using StopTaskCallback = base::OnceCallback<void(bool /*success*/)>;
  virtual void StopTask(TaskId task_id,
                        const proto::StopTaskRequest& request,
                        StopTaskCallback callback) = 0;

  using ActCallback = base::OnceCallback<void(proto::PerformActionsResponse)>;
  virtual void Act(TaskId task_id,
                   const proto::PerformActionsRequest& request,
                   ActCallback callback) = 0;
};

}  // namespace actor

#endif  // COMPONENTS_ACTOR_TRANSPORT_ACTUATION_DELEGATE_H_
