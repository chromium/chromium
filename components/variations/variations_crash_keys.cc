// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_crash_keys.h"

#include <string>

#include "base/debug/leak_annotations.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "components/crash/core/common/crash_key.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/synthetic_trials.h"

namespace variations {

namespace {

// Size of the "num-experiments" crash key in bytes. 4096 bytes should be able
// to hold about 227 entries, given each entry is 18 bytes long (due to being
// of the form "8e7abfb0-c16397b7,").
constexpr size_t kVariationsKeySize = 4096;

// Crash key reporting the number of experiments. 8 is the size of the crash key
// in bytes, which is used to hold an int as a string.
crash_reporter::CrashKeyString<8> g_num_variations_crash_key("num-experiments");

// Crash key reporting the variations state.
crash_reporter::CrashKeyString<kVariationsKeySize> g_variations_crash_key(
    "variations");

std::string ActiveGroupToString(const ActiveGroupId& active_group) {
  return base::StringPrintf("%x-%x,", active_group.name, active_group.group);
}

class VariationsCrashKeys final : public base::FieldTrialList::Observer {
 public:
  VariationsCrashKeys();
  ~VariationsCrashKeys() override;

  // base::FieldTrialList::Observer:
  void OnFieldTrialGroupFinalized(const std::string& trial_name,
                                  const std::string& group_name) override;

  // Notifies the object that the list of synthetic field trial groups has
  // changed. Note: This matches the SyntheticTrialObserver interface, but this
  // object isn't a direct observer, so doesn't implement it.
  void OnSyntheticTrialsChanged(const std::vector<SyntheticTrialGroup>& groups);

 private:
  // Adds an entry for the specified field trial to internal state, without
  // updating crash keys.
  void AppendFieldTrial(const std::string& trial_name,
                        const std::string& group_name);

  // Updates crash keys based on internal state.
  void UpdateCrashKeys();

  // Task runner corresponding to the UI thread, used to reschedule synchronous
  // observer calls that happen on a different thread.
  scoped_refptr<base::SequencedTaskRunner> ui_thread_task_runner_;

  // A serialized string containing the variations state.
  std::string variations_string_;

  // Number of entries in |variations_string_|.
  size_t num_variations_ = 0;

  // A serialized string containing the synthetic trials state.
  std::string synthetic_trials_string_;

  // Number of entries in |synthetic_trials_string_|.
  size_t num_synthetic_trials_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(VariationsCrashKeys);
};

VariationsCrashKeys::VariationsCrashKeys() {
  base::FieldTrial::ActiveGroups active_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  for (const auto& entry : active_groups) {
    AppendFieldTrial(entry.trial_name, entry.group_name);
  }
  UpdateCrashKeys();

  ui_thread_task_runner_ = base::SequencedTaskRunnerHandle::Get();
  base::FieldTrialList::SetSynchronousObserver(this);
}

VariationsCrashKeys::~VariationsCrashKeys() {
  base::FieldTrialList::RemoveSynchronousObserver(this);
  g_num_variations_crash_key.Clear();
  g_variations_crash_key.Clear();
}

void VariationsCrashKeys::OnFieldTrialGroupFinalized(
    const std::string& trial_name,
    const std::string& group_name) {
  // If this is called on a different thread, post it back to the UI thread.
  // Note: This is safe to do because in production, this object is never
  // deleted and if this is called, it means the constructor has already run,
  // which is the only place that |ui_thread_task_runner_| is set.
  if (!ui_thread_task_runner_->RunsTasksInCurrentSequence()) {
    ui_thread_task_runner_->PostTask(
        FROM_HERE,
        BindOnce(&VariationsCrashKeys::OnFieldTrialGroupFinalized,
                 // base::Unretained() is safe here because this object is
                 // never deleted in production.
                 base::Unretained(this), trial_name, group_name));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  AppendFieldTrial(trial_name, group_name);
  UpdateCrashKeys();
}

void VariationsCrashKeys::AppendFieldTrial(const std::string& trial_name,
                                           const std::string& group_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  auto active_group_id = MakeActiveGroupId(trial_name, group_name);
  auto variation = ActiveGroupToString(active_group_id);

  variations_string_ += variation;
  ++num_variations_;
}

void VariationsCrashKeys::UpdateCrashKeys() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  g_num_variations_crash_key.Set(
      base::NumberToString(num_variations_ + num_synthetic_trials_));

  std::string combined_string;
  combined_string.reserve(variations_string_.size() +
                          synthetic_trials_string_.size());
  combined_string.append(variations_string_);
  combined_string.append(synthetic_trials_string_);

  if (combined_string.size() > kVariationsKeySize) {
    // If size exceeded, truncate to the last full entry.
    int comma_index = combined_string.substr(0, kVariationsKeySize).rfind(',');
    combined_string.resize(comma_index + 1);
    // NOTREACHED() will let us know of the problem and adjust the limit.
    NOTREACHED();
    return;
  }

  g_variations_crash_key.Set(combined_string);
}

void VariationsCrashKeys::OnSyntheticTrialsChanged(
    const std::vector<SyntheticTrialGroup>& synthetic_trials) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Note: This part is inefficient as each time synthetic trials change, this
  // code recomputes all their name hashes. However, given that there should
  // not be too many synthetic trials, this is not too big of an issue.
  synthetic_trials_string_.clear();
  for (const auto& synthetic_trial : synthetic_trials) {
    synthetic_trials_string_ += ActiveGroupToString(synthetic_trial.id);
  }
  num_synthetic_trials_ = synthetic_trials.size();

  UpdateCrashKeys();
}

// Singletone crash key manager. Allocated once at process start up and
// intentionally leaked since it needs to live for the duration of the process
// there's no benefit in cleaning it up at exit.
VariationsCrashKeys* g_variations_crash_keys = nullptr;

}  // namespace

void InitCrashKeys() {
  DCHECK(!g_variations_crash_keys);
  g_variations_crash_keys = new VariationsCrashKeys();
  ANNOTATE_LEAKING_OBJECT_PTR(g_variations_crash_keys);
}

void UpdateCrashKeysWithSyntheticTrials(
    const std::vector<SyntheticTrialGroup>& synthetic_trials) {
  DCHECK(g_variations_crash_keys);
  g_variations_crash_keys->OnSyntheticTrialsChanged(synthetic_trials);
}

void ClearCrashKeysInstanceForTesting() {
  DCHECK(g_variations_crash_keys);
  delete g_variations_crash_keys;
  g_variations_crash_keys = nullptr;
}

}  // namespace variations
