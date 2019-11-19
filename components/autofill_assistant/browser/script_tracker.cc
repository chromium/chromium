// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/script_tracker.h"

#include <algorithm>
#include <utility>

#include "base/base64.h"
#include "base/bind.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/script_executor.h"
#include "components/autofill_assistant/browser/trigger_context.h"

namespace autofill_assistant {

namespace {

// Sort scripts by priority.
void SortScripts(std::vector<Script*>* scripts) {
  std::sort(scripts->begin(), scripts->end(),
            [](const Script* a, const Script* b) {
              // Runnable scripts with lowest priority value are displayed
              // first, Interrupts with lowest priority values are run first.
              // Order of scripts with the same priority is arbitrary. Fallback
              // to ordering by name and path, arbitrarily, for the behavior to
              // be consistent across runs.
              return std::tie(a->priority, a->handle.chip.text,
                              a->handle.path) <
                     std::tie(b->priority, b->handle.chip.text, a->handle.path);
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
                             ScriptTracker::Listener* listener)
    : delegate_(delegate), listener_(listener) {
  DCHECK(delegate_);
  DCHECK(listener_);
}

ScriptTracker::~ScriptTracker() = default;

void ScriptTracker::SetScripts(std::vector<std::unique_ptr<Script>> scripts) {
  // The set of interrupts must not change while a script is running, as they're
  // passed to ScriptExecutor.
  if (running())
    return;

  TerminatePendingChecks();

  available_scripts_.clear();
  for (auto& script : scripts) {
    available_scripts_[script.get()] = std::move(script);
  }

  interrupts_.clear();
  for (auto& entry : available_scripts_) {
    Script* script = entry.first;
    if (script->handle.interrupt) {
      interrupts_.push_back(script);
    }
  }
  SortScripts(&interrupts_);
}

void ScriptTracker::CheckScripts() {
  // In case checks are still running, terminate them.
  TerminatePendingChecks();

  DCHECK(pending_runnable_scripts_.empty());

  GURL url = delegate_->GetCurrentURL();
  batch_element_checker_ = std::make_unique<BatchElementChecker>();
  for (const auto& entry : available_scripts_) {
    Script* script = entry.first;
    if (script->handle.chip.empty() && script->handle.direct_action.empty() &&
        !script->handle.autostart)
      continue;

    script->precondition->Check(
        url, batch_element_checker_.get(), *delegate_->GetTriggerContext(),
        scripts_state_,
        base::BindOnce(&ScriptTracker::OnPreconditionCheck,
                       weak_ptr_factory_.GetWeakPtr(), script));
  }
  if (batch_element_checker_->empty() && pending_runnable_scripts_.empty() &&
      !available_scripts_.empty()) {
    DVLOG(1) << __func__ << ": No runnable scripts for " << url << " out of "
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
                                  std::unique_ptr<TriggerContext> context,
                                  ScriptExecutor::RunScriptCallback callback) {
  if (running()) {
    DVLOG(1) << "Do not expect executing the script (" << script_path
             << " when there is a script running.";
    ScriptExecutor::Result result;
    result.success = false;
    std::move(callback).Run(result);
    return;
  }

  executor_ = std::make_unique<ScriptExecutor>(
      script_path, std::move(context), last_global_payload_,
      last_script_payload_,
      /* listener= */ this, &scripts_state_, &interrupts_, delegate_);
  ScriptExecutor::RunScriptCallback run_script_callback = base::BindOnce(
      &ScriptTracker::OnScriptRun, weak_ptr_factory_.GetWeakPtr(), script_path,
      std::move(callback));
  TerminatePendingChecks();
  executor_->Run(std::move(run_script_callback));
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

  std::vector<base::Value> scripts_state_js;
  for (const auto& entry : scripts_state_) {
    base::Value script_js = base::Value(base::Value::Type::DICTIONARY);
    script_js.SetKey(entry.first, base::Value(entry.second));
    scripts_state_js.push_back(std::move(script_js));
  }
  dict.SetKey("executed-scripts", base::Value(scripts_state_js));

  std::vector<base::Value> available_scripts_js;
  for (const auto& entry : available_scripts_)
    available_scripts_js.push_back(base::Value(entry.second->handle.path));
  dict.SetKey("available-scripts", base::Value(available_scripts_js));

  std::vector<base::Value> runnable_scripts_js;
  for (const auto& entry : runnable_scripts_) {
    base::Value script_js = base::Value(base::Value::Type::DICTIONARY);
    script_js.SetKey("name", base::Value(entry.chip.text));
    script_js.SetKey("path", base::Value(entry.path));
    script_js.SetKey("initial_prompt", base::Value(entry.initial_prompt));
    script_js.SetKey("autostart", base::Value(entry.autostart));
    script_js.SetKey("chip_type", base::Value(entry.chip.type));

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
  MaybeSwapInScripts();
  std::move(original_callback).Run(result);
}

void ScriptTracker::MaybeSwapInScripts() {
  if (scripts_update_) {
    SetScripts(std::move(*scripts_update_));
    scripts_update_.reset();
  }
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

  runnable_scripts_.clear();
  SortScripts(&pending_runnable_scripts_);
  for (Script* script : pending_runnable_scripts_) {
    runnable_scripts_.push_back(script->handle);
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

void ScriptTracker::OnServerPayloadChanged(const std::string& global_payload,
                                           const std::string& script_payload) {
  last_global_payload_ = global_payload;
  last_script_payload_ = script_payload;
}

void ScriptTracker::OnScriptListChanged(
    std::vector<std::unique_ptr<Script>> scripts) {
  scripts_update_.reset(
      new std::vector<std::unique_ptr<Script>>(std::move(scripts)));
}

}  // namespace autofill_assistant
