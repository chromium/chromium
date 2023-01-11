// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UPDATE_CLIENT_ACTION_RUNNER_H_
#define COMPONENTS_UPDATE_CLIENT_ACTION_RUNNER_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/threading/thread_checker.h"
#include "components/update_client/update_client.h"

namespace base {
class FilePath;
class SingleThreadTaskRunner;
}  // namespace base

namespace update_client {

class Component;

class ActionRunner {
 public:
  using Callback = ActionHandler::Callback;

  explicit ActionRunner(const Component& component);
  ~ActionRunner();

  void Run(Callback run_complete);

 private:
  void Handle(const base::FilePath& crx_path);

  THREAD_CHECKER(thread_checker_);

  const raw_ref<const Component> component_;

  // Used to post callbacks to the main thread.
  scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  Callback callback_;

  ActionRunner(const ActionRunner&) = delete;
  ActionRunner& operator=(const ActionRunner&) = delete;
};

}  // namespace update_client

#endif  // COMPONENTS_UPDATE_CLIENT_ACTION_RUNNER_H_
