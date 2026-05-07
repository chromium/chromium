// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Experimental API to handle acting and returning new browser state.
[implemented_in="chrome/browser/extensions/api/experimental_actor/experimental_actor_api.h"]
interface ExperimentalActor {
  // Stops a task.
  // |taskId|: id of the task to stop.
  // |Returns|: a closure that is called when the task is stopped.
  [requiredCallback] static Promise<undefined> stopTask(long taskId);

  // Creates a new task. The callback will contain the task ID for the newly
  // created task.
  // |PromiseValue|: taskId
  [requiredCallback] static Promise<long> createTask();

  // Executes one or more actions according to request.
  // |actionsProto|: encoded optimization_guide.proto.Actions
  // |Returns|: encoded optimization_guide.proto.ActionsResult
  // |PromiseValue|: data
  [requiredCallback]
  static Promise<ArrayBuffer> performActions(ArrayBuffer actionsProto);

  // Requests a TabObservation for a given tab.
  // |tabId|: The session tabId to observe.
  // |Returns|: encoded optimization_guide.proto.TabObservation
  // |PromiseValue|: data
  [requiredCallback]
  static Promise<ArrayBuffer> requestTabObservation(long tabId);
};

partial interface Browser {
  static attribute ExperimentalActor experimentalActor;
};
