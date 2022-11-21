// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/update_client/action_runner.h"

#include <utility>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/task/single_thread_task_runner.h"
#include "base/task/thread_pool.h"
#include "components/update_client/component.h"
#include "components/update_client/task_traits.h"

namespace update_client {

ActionRunner::ActionRunner(const Component& component)
    : component_(component),
      main_task_runner_(base::SingleThreadTaskRunner::GetCurrentDefault()) {}

ActionRunner::~ActionRunner() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void ActionRunner::Run(Callback callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto action_handler = component_->crx_component()->action_handler;
  if (!action_handler) {
    DVLOG(1) << component_->action_run() << " is missing an action handler";
    main_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), false, -1, 0));
    return;
  }

  callback_ = std::move(callback);

  // Resolve an absolute path for the file referred by the run action.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, kTaskTraits,
      base::BindOnce(
          [](const Component* component) {
            base::FilePath crx_path;
            component->crx_component()->installer->GetInstalledFile(
                component->action_run(), &crx_path);
            return crx_path;
          },
          base::Unretained(&*component_)),
      base::BindOnce(&ActionRunner::Handle, base::Unretained(this)));
}

void ActionRunner::Handle(const base::FilePath& crx_path) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto action_handler = component_->crx_component()->action_handler;
  DCHECK(action_handler);

  action_handler->Handle(crx_path, component_->session_id(),
                         std::move(callback_));
}

}  // namespace update_client
