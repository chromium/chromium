// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_CHILD_PROCESS_FIELD_TRIAL_SYNCER_H_
#define COMPONENTS_VARIATIONS_CHILD_PROCESS_FIELD_TRIAL_SYNCER_H_

#include <set>
#include <string>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/metrics/field_trial.h"

namespace variations {

// Provides functionality for child processes to sync the "activated" state of
// field trials between the child and browser. Specifically, when a field trial
// is activated in the browser, it also activates it in the child process and
// when a field trial is activated in the child process, it notifies the browser
// process to activate it.
//
// It also updates crash keys in the child process corresponding to the field
// trial state.
class COMPONENT_EXPORT(VARIATIONS) ChildProcessFieldTrialSyncer
    : public base::FieldTrialList::Observer {
 public:
  using FieldTrialActivatedCallback =
      base::RepeatingCallback<void(const std::string& trial_name)>;

  // Creates and returns the global ChildProcessFieldTrialSyncer instance for
  // this process. Immediately invokes |activated_callback| for each currently
  // active field trial. Then, |activated_callback| is invoked as field trial
  // are activated. |activated_callback| may be invoked from any sequence and
  // must therefore be thread-safe. CreateInstance() must not be called
  // concurrently with activating a field trial. The created instance is never
  // destroyed because it would be difficult to ensure that no field trial is
  // activated concurrently with unregistering it as an observer of
  // FieldTrialList (see FieldTrialList::RemoveObserver).
  static ChildProcessFieldTrialSyncer* CreateInstance(
      FieldTrialActivatedCallback activated_callback);

  // Testing variant which allows specifying a set of initially active trials.
  static ChildProcessFieldTrialSyncer* CreateInstanceForTesting(
      const std::set<std::string>& initially_active_trials,
      FieldTrialActivatedCallback activated_callback);

  ChildProcessFieldTrialSyncer(const ChildProcessFieldTrialSyncer&) = delete;
  ChildProcessFieldTrialSyncer& operator=(const ChildProcessFieldTrialSyncer&) =
      delete;

  // Deletes the global ChildProcessFieldTrialSyncer instance.
  static void DeleteInstanceForTesting();

  // Must be invoked when the browser process notifies this child process that a
  // field trial was activated.
  void SetFieldTrialGroupFromBrowser(const std::string& trial_name,
                                     const std::string& group_name);

 private:
  explicit ChildProcessFieldTrialSyncer(
      FieldTrialActivatedCallback activated_callback);
  ~ChildProcessFieldTrialSyncer() override;

  // Initializes field trial state change observation and invokes |callback_|
  // for any field trials that might have already been activated according to
  // `initially_active_trials`.
  void Init(const std::set<std::string>& initially_active_trials);

  // base::FieldTrialList::Observer:
  void OnFieldTrialGroupFinalized(const base::FieldTrial& trial,
                                  const std::string& group_name) override;

  // Callback to invoke when a field trial is activated.
  const FieldTrialActivatedCallback activated_callback_;
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_CHILD_PROCESS_FIELD_TRIAL_SYNCER_H_
