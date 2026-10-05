// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ACTOR_TRANSPORT_ACTUATION_DELEGATE_H_
#define COMPONENTS_ACTOR_TRANSPORT_ACTUATION_DELEGATE_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "components/actor/core/task_id.h"

namespace optimization_guide::proto {
class Actions;
class ActionsResult;
class BrowserStartTask;
class BrowserStartTaskResult;
}  // namespace optimization_guide::proto

namespace actor {

// Delegate interface for executing actuation tasks and actions on the browser.
class ActuationDelegate {
 public:
  virtual ~ActuationDelegate();

  using StartTaskCallback = base::OnceCallback<void(
      optimization_guide::proto::BrowserStartTaskResult)>;
  virtual void StartTask(
      const std::string& session_id,
      const optimization_guide::proto::BrowserStartTask& request,
      StartTaskCallback callback) = 0;

  using StopTaskCallback = base::OnceCallback<void(bool /*success*/)>;
  virtual void StopTask(TaskId task_id, StopTaskCallback callback) = 0;

  using ActCallback =
      base::OnceCallback<void(optimization_guide::proto::ActionsResult)>;
  virtual void Act(const optimization_guide::proto::Actions& actions,
                   ActCallback callback) = 0;
};

}  // namespace actor

#endif  // COMPONENTS_ACTOR_TRANSPORT_ACTUATION_DELEGATE_H_
