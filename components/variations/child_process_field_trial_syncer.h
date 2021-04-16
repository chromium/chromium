// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VARIATIONS_FIELD_TRIAL_SYNCER_H_
#define COMPONENTS_VARIATIONS_FIELD_TRIAL_SYNCER_H_

#include <string>

#include "base/macros.h"
#include "base/metrics/field_trial.h"

namespace base {
class CommandLine;
}

namespace variations {

// ChildProcessFieldTrialSyncer is a utility class that's responsible for
// syncing the "activated" state of field trials between browser and child
// processes. Specifically, when a field trial is activated in the browser, it
// also activates it in the child process and when a field trial is activated
// in the child process, it notifies the browser process to activate it.
class ChildProcessFieldTrialSyncer {
 public:
  // ChildProcessFieldTrialSyncer constructor taking an externally owned
  // |observer| param that's responsible for sending IPCs to the browser process
  // when a trial is activated. The |observer| must outlive this object.
  explicit ChildProcessFieldTrialSyncer(
      base::FieldTrialList::Observer* observer);
  ~ChildProcessFieldTrialSyncer();

  // Initializes field trial state change observation and notifies the browser
  // of any field trials that might have already been activated.
  void InitFieldTrialObserving(const base::CommandLine& command_line);

  // Handler for messages from the browser process notifying the child process
  // that a field trial was activated.
  void OnSetFieldTrialGroup(const std::string& trial_name,
                            const std::string& group_name);

 private:
  base::FieldTrialList::Observer* const observer_;

  DISALLOW_COPY_AND_ASSIGN(ChildProcessFieldTrialSyncer);
};

}  // namespace variations

#endif  // COMPONENTS_VARIATIONS_FIELD_TRIAL_SYNCER_H_
