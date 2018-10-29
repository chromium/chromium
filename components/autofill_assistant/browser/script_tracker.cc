// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/script_tracker.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/script_executor.h"

namespace autofill_assistant {

ScriptTracker::ScriptTracker(ScriptExecutorDelegate* delegate,
                             ScriptTracker::Listener* listener)
    : delegate_(delegate),
      listener_(listener),
      weak_ptr_factory_(this) {
  DCHECK(delegate_);
  DCHECK(listener_);
}

ScriptTracker::~ScriptTracker() = default;

void ScriptTracker::SetScripts(std::vector<std::unique_ptr<Script>> scripts) {
  ClearAvailableScripts();
  for (auto& script : scripts) {
    available_scripts_[script.get()] = std::move(script);
  }
}

void ScriptTracker::CheckScripts(const base::TimeDelta& max_duration) {
  if (batch_element_checker_) {
    // It should be possible to just call batch_element_checker_.reset() to give
    // up on all checks. This doesn't work, however, because it ends up running
    // multiple checks in parallel, which fails.
    //
    // StopTrying() tells BatchElementChecker to give up early and call
    // OnCheckDone as soon as possible, which is a safe point for deleting the
    // checker and starting a new check.
    //
    // TODO(crbug.com/806868): Figure out why checks run in parallel don't work
    // and simplify this logic.
    batch_element_checker_->StopTrying();

    // TODO(crbug.com/806868): May stop recheck if there is a script pending to
    // run.
    must_recheck_ = base::BindOnce(&ScriptTracker::CheckScripts,
                                   base::Unretained(this), max_duration);
    return;
  }
  DCHECK(pending_runnable_scripts_.empty());

  batch_element_checker_ =
      delegate_->GetWebController()->CreateBatchElementChecker();
  for (const auto& entry : available_scripts_) {
    Script* script = entry.first;
    script->precondition->Check(
        delegate_->GetWebController()->GetUrl(), batch_element_checker_.get(),
        delegate_->GetParameters(), executed_scripts_,
        base::BindOnce(&ScriptTracker::OnPreconditionCheck,
                       weak_ptr_factory_.GetWeakPtr(), script));
  }
  batch_element_checker_->Run(
      max_duration,
      /* try_done= */
      base::BindRepeating(&ScriptTracker::UpdateRunnableScriptsIfNecessary,
                          base::Unretained(this)),
      /* all_done= */
      base::BindOnce(&ScriptTracker::OnCheckDone, base::Unretained(this)));
  // base::Unretained(this) is safe since this instance owns
  // batch_element_checker_.
}

void ScriptTracker::ExecuteScript(const std::string& script_path,
                                  ScriptExecutor::RunScriptCallback callback) {
  if (running()) {
    DLOG(ERROR) << "Do not expect executing the script (" << script_path
                << " when there is a script running.";
    ScriptExecutor::Result result;
    result.success = false;
    std::move(callback).Run(result);
    return;
  }

  executed_scripts_[script_path] = SCRIPT_STATUS_RUNNING;
  executor_ = std::make_unique<ScriptExecutor>(
      script_path, last_server_payload_, this, delegate_);
  ScriptExecutor::RunScriptCallback run_script_callback = base::BindOnce(
      &ScriptTracker::OnScriptRun, weak_ptr_factory_.GetWeakPtr(), script_path,
      std::move(callback));
  // Postpone running script until finishing the current round of preconditions
  // check.
  if (!batch_element_checker_ && !must_recheck_) {
    executor_->Run(std::move(run_script_callback));
  } else {
    pending_run_script_callback_ = std::move(run_script_callback);
    // Do not recheck and retry when there is a script pending to run. Note
    // that |batch_element_checker_| may take a long time to wait on retrying
    // unsatisfied preconditions check without stop trying.
    must_recheck_.Reset();
    batch_element_checker_->StopTrying();
  }
}

void ScriptTracker::ClearRunnableScripts() {
  runnable_scripts_.clear();
  listener_->OnRunnableScriptsChanged(runnable_scripts_);
}

void ScriptTracker::OnScriptRun(
    const std::string& script_path,
    ScriptExecutor::RunScriptCallback original_callback,
    ScriptExecutor::Result result) {
  DCHECK(!pending_run_script_callback_);
  executor_.reset();
  executed_scripts_[script_path] =
      result.success ? SCRIPT_STATUS_SUCCESS : SCRIPT_STATUS_FAILURE;
  std::move(original_callback).Run(result);
}

void ScriptTracker::UpdateRunnableScriptsIfNecessary() {
  if (!RunnablesHaveChanged())
    return;

  runnable_scripts_.clear();
  std::sort(pending_runnable_scripts_.begin(), pending_runnable_scripts_.end(),
            [](const Script* a, const Script* b) {
              // Runnable scripts with lowest priority value are displayed
              // first. The display order of scripts with the same priority is
              // arbitrary. Fallback to ordering by name, arbitrarily, for the
              // behavior to be consistent.
              return std::tie(a->priority, a->handle.name) <
                     std::tie(b->priority, b->handle.name);
            });
  for (Script* script : pending_runnable_scripts_) {
    runnable_scripts_.push_back(script->handle);
  }

  listener_->OnRunnableScriptsChanged(runnable_scripts_);
}

void ScriptTracker::OnCheckDone() {
  TerminatePendingChecks();
  if (must_recheck_) {
    std::move(must_recheck_).Run();
    return;
  }

  // TODO(crbug.com/806868): Check whether the script is still runnable.
  if (pending_run_script_callback_)
    executor_->Run(std::move(pending_run_script_callback_));
}

void ScriptTracker::TerminatePendingChecks() {
  batch_element_checker_.reset();
  pending_runnable_scripts_.clear();
}

bool ScriptTracker::RunnablesHaveChanged() {
  if (runnable_scripts_.size() != pending_runnable_scripts_.size())
    return true;

  std::set<std::string> pending_paths;
  for (Script* script : pending_runnable_scripts_) {
    pending_paths.insert(script->handle.path);
  }
  std::set<std::string> current_paths;
  for (const auto& handle : runnable_scripts_) {
    current_paths.insert(handle.path);
  }
  return pending_paths != current_paths;
}

void ScriptTracker::OnPreconditionCheck(Script* script,
                                        bool met_preconditions) {
  if (available_scripts_.find(script) == available_scripts_.end()) {
    // Result is not relevant anymore.
    return;
  }
  if (met_preconditions)
    pending_runnable_scripts_.push_back(script);
}

void ScriptTracker::ClearAvailableScripts() {
  available_scripts_.clear();
  // Clearing available_scripts_ has cancelled any pending precondition checks,
  // ending them.
  TerminatePendingChecks();
}

void ScriptTracker::OnServerPayloadChanged(const std::string& server_payload) {
  last_server_payload_ = server_payload;
}

}  // namespace autofill_assistant
