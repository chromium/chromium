// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_crash_keys.h"

#include <string>

#include "base/debug/leak_annotations.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "components/crash/core/common/crash_key.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/buildflags.h"
#include "components/variations/synthetic_trials.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/task/thread_pool.h"
#include "components/variations/variations_crash_keys_chromeos.h"
#endif

namespace variations {

namespace {

// Size of the "num-experiments" crash key in bytes. 4096 bytes should be able
// to hold about 227 entries, given each entry is 18 bytes long (due to being
// of the form "8e7abfb0-c16397b7,").
#if BUILDFLAG(LARGE_VARIATION_KEY_SIZE)
constexpr size_t kVariationsKeySize = 8192;
#else
constexpr size_t kVariationsKeySize = 6144;
#endif

// Crash key reporting the number of experiments. 8 is the size of the crash key
// in bytes, which is used to hold an int as a string.
crash_reporter::CrashKeyString<8> g_num_variations_crash_key(
    kNumExperimentsKey);

// Crash key reporting the variations state.
crash_reporter::CrashKeyString<kVariationsKeySize> g_variations_crash_key(
    kExperimentListKey);

std::string ActiveGroupToString(const ActiveGroupId& active_group) {
  return base::StringPrintf("%x-%x,", active_group.name, active_group.group);
}

class VariationsCrashKeys final : public base::FieldTrialList::Observer {
 public:
  VariationsCrashKeys();

  VariationsCrashKeys(const VariationsCrashKeys&) = delete;
  VariationsCrashKeys& operator=(const VariationsCrashKeys&) = delete;

  ~VariationsCrashKeys() override;

  // base::FieldTrialList::Observer:
  void OnFieldTrialGroupFinalized(const std::string& trial_name,
                                  const std::string& group_name) override;

  // Notifies the object that the list of synthetic field trial groups has
  // changed. Note: This matches the SyntheticTrialObserver interface, but this
  // object isn't a direct observer, so doesn't implement it.
  void OnSyntheticTrialsChanged(const std::vector<SyntheticTrialGroup>& groups);

  // Gets the list of experiments and number of experiments in the format we
  // want to place it in the crash keys.
  ExperimentListInfo GetExperimentListInfo();

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

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Task runner corresponding to a background thread, used for tasks that may
  // block.
  scoped_refptr<base::SequencedTaskRunner> background_thread_task_runner_;
#endif  // IS_CHROMEOS_ASH

  // A serialized string containing the variations state.
  std::string variations_string_;

  // Number of entries in |variations_string_|.
  size_t num_variations_ = 0;

  // A serialized string containing the synthetic trials state.
  std::string synthetic_trials_string_;

  // Number of entries in |synthetic_trials_string_|.
  size_t num_synthetic_trials_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);
};

VariationsCrashKeys::VariationsCrashKeys() {
  base::FieldTrial::ActiveGroups active_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&active_groups);
  for (const auto& entry : active_groups) {
    AppendFieldTrial(entry.trial_name, entry.group_name);
  }
#if BUILDFLAG(IS_CHROMEOS_ASH)
  background_thread_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::BEST_EFFORT, base::MayBlock()});
#endif  // IS_CHROMEOS_ASH

  UpdateCrashKeys();

  ui_thread_task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  base::FieldTrialList::AddObserver(this);
}

VariationsCrashKeys::~VariationsCrashKeys() {
  base::FieldTrialList::RemoveObserver(this);
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

ExperimentListInfo VariationsCrashKeys::GetExperimentListInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ExperimentListInfo result;
  result.num_experiments = num_variations_ + num_synthetic_trials_;
  result.experiment_list.reserve(variations_string_.size() +
                                 synthetic_trials_string_.size());
  result.experiment_list.append(variations_string_);
  result.experiment_list.append(synthetic_trials_string_);
  return result;
}

void VariationsCrashKeys::UpdateCrashKeys() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  ExperimentListInfo info = GetExperimentListInfo();
  g_num_variations_crash_key.Set(base::NumberToString(info.num_experiments));

  if (info.experiment_list.size() > kVariationsKeySize) {
    // If size exceeded, truncate to the last full entry.
    int comma_index =
        info.experiment_list.substr(0, kVariationsKeySize).rfind(',');
    info.experiment_list.resize(comma_index + 1);
    // NOTREACHED() will let us know of the problem and adjust the limit.
    NOTREACHED();
  }

  g_variations_crash_key.Set(info.experiment_list);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  ReportVariationsToChromeOs(background_thread_task_runner_, info);
#endif  // IS_CHROMEOS_ASH
}

void VariationsCrashKeys::OnSyntheticTrialsChanged(
    const std::vector<SyntheticTrialGroup>& synthetic_trials) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // Note: This part is inefficient as each time synthetic trials change, this
  // code recomputes all their name hashes. However, given that there should
  // not be too many synthetic trials, this is not too big of an issue.
  synthetic_trials_string_.clear();
  for (const auto& synthetic_trial : synthetic_trials) {
    synthetic_trials_string_ += ActiveGroupToString(synthetic_trial.id());
  }
  num_synthetic_trials_ = synthetic_trials.size();

  UpdateCrashKeys();
}

// Singletone crash key manager. Allocated once at process start up and
// intentionally leaked since it needs to live for the duration of the process
// there's no benefit in cleaning it up at exit.
VariationsCrashKeys* g_variations_crash_keys = nullptr;

}  // namespace

const char kNumExperimentsKey[] = "num-experiments";
const char kExperimentListKey[] = "variations";

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

ExperimentListInfo GetExperimentListInfo() {
  DCHECK(g_variations_crash_keys);
  return g_variations_crash_keys->GetExperimentListInfo();
}

}  // namespace variations
