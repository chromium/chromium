// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/script_tracker.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "components/autofill_assistant/browser/batch_element_checker.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/script_executor.h"
#include "components/autofill_assistant/browser/trigger_context.h"

namespace autofill_assistant {

namespace {

// Sort scripts by priority. T must be a normal or smart pointer to a script
void SortScripts(std::vector<std::unique_ptr<Script>>* scripts) {
  std::sort(
      scripts->begin(), scripts->end(),
      [](const std::unique_ptr<Script>& a, const std::unique_ptr<Script>& b) {
        // Runnable scripts with lowest priority value are displayed
        // first, Interrupts with lowest priority values are run first.
        // Order of scripts with the same priority is arbitrary. Fallback
        // to ordering by name and path, arbitrarily, for the behavior to
        // be consistent across runs.
        return std::tie(a->priority, a->handle.path) <
               std::tie(b->priority, a->handle.path);
      });
}

// Creates a value containing a vector of a simple type, accepted by base::Value
// constructor, from a container.
template <typename T>
base::Value ToValueArray(const T& v) {
  std::vector<base::Value> values;
  for (const auto& s : v) {
    values.emplace_back(base::Value(s));
  }
  return base::Value(values);
}

}  // namespace

ScriptTracker::ScriptTracker(ScriptExecutorDelegate* delegate,
                             ScriptExecutorUiDelegate* ui_delegate,
                             ScriptTracker::Listener* listener)
    : delegate_(delegate), ui_delegate_(ui_delegate), listener_(listener) {
  DCHECK(delegate_);
  DCHECK(ui_delegate_);
  DCHECK(listener_);
}

ScriptTracker::~ScriptTracker() = default;

void ScriptTracker::SetScripts(std::vector<std::unique_ptr<Script>> scripts) {
  TerminatePendingChecks();

  available_scripts_.clear();
  interrupts_.clear();
  for (auto& script : scripts) {
    if (script->handle.interrupt) {
      interrupts_.emplace_back(std::move(script));
    } else {
      available_scripts_.emplace_back(std::move(script));
    }
  }
  SortScripts(&available_scripts_);
  SortScripts(&interrupts_);
}

void ScriptTracker::CheckScripts() {
  // In case checks are still running, terminate them.
  TerminatePendingChecks();

  DCHECK(pending_runnable_scripts_.empty());

  GURL url = delegate_->GetCurrentURL();
  batch_element_checker_ = std::make_unique<BatchElementChecker>();
  for (const std::unique_ptr<Script>& script : available_scripts_) {
    if (script->handle.direct_action.empty() && !script->handle.autostart)
      continue;

    script->precondition->Check(
        url, batch_element_checker_.get(), *delegate_->GetTriggerContext(),
        base::BindOnce(&ScriptTracker::OnPreconditionCheck,
                       weak_ptr_factory_.GetWeakPtr(), script->handle.path));
  }
  if (batch_element_checker_->empty() && pending_runnable_scripts_.empty() &&
      !available_scripts_.empty()) {
    VLOG(1) << __func__ << ": No runnable scripts for " << url << " out of "
            << available_scripts_.size() << " available.";
    // There are no runnable scripts, even though we haven't checked the DOM
    // yet. Report it all immediately.
    UpdateRunnableScriptsIfNecessary();
    listener_->OnNoRunnableScriptsForPage();
    TerminatePendingChecks();
    return;
  }
  batch_element_checker_->AddAllDoneCallback(base::BindOnce(
      &ScriptTracker::OnCheckDone, weak_ptr_factory_.GetWeakPtr()));
  batch_element_checker_->Run(delegate_->GetWebController());
}

void ScriptTracker::ExecuteScript(const std::string& script_path,
                                  const UserData* user_data,
                                  std::unique_ptr<TriggerContext> context,
                                  ScriptExecutor::RunScriptCallback callback) {
  if (running()) {
#ifdef NDEBUG
    VLOG(1) << "Unexpected call while another script is running.";
#else
    DVLOG(1) << "Do not expect executing the script (" << script_path
             << " when there is a script running.";
#endif

    ScriptExecutor::Result result;
    result.success = false;
    std::move(callback).Run(result);
    return;
  }

  executor_ = std::make_unique<ScriptExecutor>(
      script_path, std::move(context), last_global_payload_,
      last_script_payload_,
      /* listener= */ this, &interrupts_, delegate_, ui_delegate_);
  ScriptExecutor::RunScriptCallback run_script_callback = base::BindOnce(
      &ScriptTracker::OnScriptRun, weak_ptr_factory_.GetWeakPtr(), script_path,
      std::move(callback));
  TerminatePendingChecks();
  executor_->Run(user_data, std::move(run_script_callback));
}

void ScriptTracker::StopScript() {
  executor_.reset();
}

void ScriptTracker::ClearRunnableScripts() {
  runnable_scripts_.clear();
}

base::Value ScriptTracker::GetDebugContext() const {
  base::Value dict(base::Value::Type::DICTIONARY);

  std::string last_global_payload_js = last_global_payload_;
  base::Base64Encode(last_global_payload_js, &last_global_payload_js);
  dict.SetKey("last-global-payload", base::Value(last_global_payload_js));

  std::string last_script_payload_js = last_script_payload_;
  base::Base64Encode(last_script_payload_js, &last_script_payload_js);
  dict.SetKey("last-script-payload", base::Value(last_script_payload_js));

  std::vector<base::Value> available_scripts_js;
  for (const std::unique_ptr<Script>& script : available_scripts_)
    available_scripts_js.push_back(base::Value(script->handle.path));
  dict.SetKey("available-scripts", base::Value(available_scripts_js));

  std::vector<base::Value> runnable_scripts_js;
  for (const auto& entry : runnable_scripts_) {
    base::Value script_js = base::Value(base::Value::Type::DICTIONARY);
    script_js.SetKey("path", base::Value(entry.path));
    script_js.SetKey("autostart", base::Value(entry.autostart));

    base::Value direct_action_js = base::Value(base::Value::Type::DICTIONARY);
    direct_action_js.SetKey("names", ToValueArray(entry.direct_action.names));
    direct_action_js.SetKey(
        "required_arguments",
        ToValueArray(entry.direct_action.required_arguments));
    direct_action_js.SetKey(
        "optional_arguments",
        ToValueArray(entry.direct_action.optional_arguments));
    script_js.SetKey("direct_action", std::move(direct_action_js));

    runnable_scripts_js.push_back(std::move(script_js));
  }
  dict.SetKey("runnable-scripts", base::Value(runnable_scripts_js));

  return dict;
}

void ScriptTracker::OnScriptRun(
    const std::string& script_path,
    ScriptExecutor::RunScriptCallback original_callback,
    const ScriptExecutor::Result& result) {
  executor_.reset();
  std::move(original_callback).Run(result);
}

void ScriptTracker::OnCheckDone() {
  UpdateRunnableScriptsIfNecessary();
  TerminatePendingChecks();
}

void ScriptTracker::UpdateRunnableScriptsIfNecessary() {
  if (!has_reported_scripts_) {
    has_reported_scripts_ = true;
  } else if (!RunnablesHaveChanged()) {
    return;
  }

  // Go through available scripts, to guarantee that runnable_scripts_ follows
  // the order of available_scripts_. As a side effect, any invalid path in
  // runnable_scripts_ is ignored here.
  runnable_scripts_.clear();
  for (const std::unique_ptr<Script>& script : available_scripts_) {
    if (pending_runnable_scripts_.find(script->handle.path) !=
        pending_runnable_scripts_.end()) {
      runnable_scripts_.push_back(script->handle);
    }
  }
  listener_->OnRunnableScriptsChanged(runnable_scripts_);
}

void ScriptTracker::TerminatePendingChecks() {
  batch_element_checker_.reset();
  pending_runnable_scripts_.clear();
}

bool ScriptTracker::RunnablesHaveChanged() {
  if (runnable_scripts_.size() != pending_runnable_scripts_.size())
    return true;

  std::vector<std::string> all_current_paths;
  for (const auto& handle : runnable_scripts_) {
    all_current_paths.emplace_back(handle.path);
  }
  auto current_paths =
      base::flat_set<std::string>(std::move(all_current_paths));
  return pending_runnable_scripts_ != current_paths;
}

void ScriptTracker::OnPreconditionCheck(const std::string& script_path,
                                        bool met_preconditions) {
  if (met_preconditions)
    pending_runnable_scripts_.insert(script_path);

  // Note that if we receive a path for a script not in available_scripts_ - or
  // not any more - it'll be ignored in UpdateRunnableScriptsIfNecessary.
}

void ScriptTracker::OnServerPayloadChanged(const std::string& global_payload,
                                           const std::string& script_payload) {
  last_global_payload_ = global_payload;
  last_script_payload_ = script_payload;
}

void ScriptTracker::OnScriptListChanged(
    std::vector<std::unique_ptr<Script>> scripts) {
  SetScripts(std::move(scripts));
}

}  // namespace autofill_assistant
