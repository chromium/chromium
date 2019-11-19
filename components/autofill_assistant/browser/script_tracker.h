// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_TRACKER_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_TRACKER_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/autofill_assistant/browser/script.h"
#include "components/autofill_assistant/browser/script_executor.h"
#include "components/autofill_assistant/browser/service.pb.h"

namespace autofill_assistant {
class ScriptExecutorDelegate;
class ScriptTrackerTest;

// The script tracker keeps track of which scripts are available, which are
// running, which have run, which are runnable whose preconditions are met.
//
// User of this class is responsible for retrieving and passing scripts to the
// tracker and letting the tracker know about changes to the DOM.
class ScriptTracker : public ScriptExecutor::Listener {
 public:
  // Listens to changes on the ScriptTracker state.
  class Listener {
   public:
    virtual ~Listener() = default;

    // Called when the set of runnable scripts have changed. |runnable_scripts|
    // are the new runnable scripts. Runnable scripts are ordered by priority.
    //
    // The result of the first check is always reported, even if the set of
    // scripts that were found is empty.
    virtual void OnRunnableScriptsChanged(
        const std::vector<ScriptHandle>& runnable_scripts) = 0;

    // Called when there are no more runnable scripts anymore and there cannot
    // be any without navigating to another page.
    //
    // This is not called if a DOM change could make some scripts runnable.
    //
    // This is only called when there are scripts. That is, SetScripts was last
    // passed a non-empty vector.
    virtual void OnNoRunnableScriptsForPage() = 0;
  };

  // |delegate| and |listener| should outlive this object and should not be
  // nullptr.
  ScriptTracker(ScriptExecutorDelegate* delegate,
                ScriptTracker::Listener* listener);

  ~ScriptTracker() override;

  // Updates the set of available |scripts|. This interrupts any pending checks,
  // but don't start a new one.'
  void SetScripts(std::vector<std::unique_ptr<Script>> scripts);

  // Run the preconditions on the current set of scripts, and possibly update
  // the set of runnable scripts.
  //
  // Calling CheckScripts() while a check is in progress cancels the previously
  // running check and starts a new one right away.
  void CheckScripts();

  // Runs a script and reports, when the script has ended, whether the run was
  // successful. Fails immediately if a script is already running.
  //
  // Scripts that are already executed won't be considered runnable anymore.
  // Call CheckScripts to refresh the set of runnable script after script
  // execution.
  //
  // The given context allows specifying additional parameters and experiments,
  // on top of what's available in the context returned by
  // ScriptExecutorDelegate.
  void ExecuteScript(const std::string& path,
                     std::unique_ptr<TriggerContext> context,
                     ScriptExecutor::RunScriptCallback callback);

  // Stops a script, if one is running.
  void StopScript();

  // Clears the set of scripts that could be run.
  //
  // Calling this function will clean the bottom bar.
  void ClearRunnableScripts();

  // Checks whether a script is currently running. There can be at most one
  // script running at a time.
  bool running() const { return executor_ != nullptr; }

  // Returns a dictionary describing the current execution context, which
  // is intended to be serialized as JSON string. The execution context is
  // useful when analyzing feedback forms and for debugging in general.
  base::Value GetDebugContext() const;

 private:
  typedef std::map<Script*, std::unique_ptr<Script>> AvailableScriptMap;

  friend class ScriptTrackerTest;

  void OnScriptRun(const std::string& script_path,
                   ScriptExecutor::RunScriptCallback original_callback,
                   const ScriptExecutor::Result& result);

  // Updates the list of available scripts if there is a pending update from
  // when a script was still being executed.
  void MaybeSwapInScripts();
  void OnCheckDone();
  void UpdateRunnableScriptsIfNecessary();

  // Stops running pending checks and cleans up any state used by pending
  // checks. This can safely be called at any time, including when no checks are
  // running.
  void TerminatePendingChecks();

  // Returns true if |runnable_| should be updated.
  bool RunnablesHaveChanged();
  void OnPreconditionCheck(Script* script, bool met_preconditions);

  // Overrides ScriptExecutor::Listener.
  void OnServerPayloadChanged(const std::string& global_payload,
                              const std::string& script_payload) override;
  void OnScriptListChanged(
      std::vector<std::unique_ptr<Script>> scripts) override;

  ScriptExecutorDelegate* const delegate_;
  ScriptTracker::Listener* const listener_;

  // If true, a set of script has already been reported to
  // Listener::OnRunnableScriptsChanged.
  bool has_reported_scripts_ = false;

  // Paths and names of scripts known to be runnable (they pass the
  // preconditions).
  //
  // NOTE 1: Set of runnable scripts can survive a SetScripts; as
  // long as the new set of runnable script has the same path, it won't be seen
  // as a change to the set of runnable, even if the pointers have changed.
  // NOTE 2: Set of runnable scripts should be in sync with what is displayed on
  // the bottom bar.
  std::vector<ScriptHandle> runnable_scripts_;

  // Sets of available scripts. SetScripts resets this and interrupts
  // any pending check.
  AvailableScriptMap available_scripts_;

  // A subset of available_scripts that are interrupts.
  std::vector<Script*> interrupts_;

  // List of scripts that have been executed and their corresponding statuses.
  std::map<std::string, ScriptStatusProto> scripts_state_;

  std::unique_ptr<BatchElementChecker> batch_element_checker_;

  // Scripts found to be runnable so far, in the current check, represented by
  // |batch_element_checker_|.
  std::vector<Script*> pending_runnable_scripts_;

  // If a script is currently running, this is the script's executor. Otherwise,
  // this is nullptr.
  std::unique_ptr<ScriptExecutor> executor_;

  std::string last_global_payload_;
  std::string last_script_payload_;

  // List of scripts to replace the currently available scripts. The replacement
  // only occurse when |scripts_update| is not nullptr.
  std::unique_ptr<std::vector<std::unique_ptr<Script>>> scripts_update_;

  base::WeakPtrFactory<ScriptTracker> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(ScriptTracker);
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_SCRIPT_TRACKER_H_
