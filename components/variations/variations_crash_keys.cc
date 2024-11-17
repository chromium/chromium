// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_crash_keys.h"

#include <set>
#include <string>

#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/metrics/field_trial_list_including_low_anonymity.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequence_checker.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "build/buildflag.h"
#include "build/chromeos_buildflags.h"
#include "components/crash/core/common/crash_key.h"
#include "components/variations/active_field_trials.h"
#include "components/variations/buildflags.h"
#include "components/variations/synthetic_trials.h"
#include "components/variations/variations_switches.h"

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/task/thread_pool.h"
#include "components/variations/variations_crash_keys_chromeos.h"
#endif

namespace variations {

namespace {

// Size of the "num-experiments" crash key in bytes. 1024*6 bytes should be able
// to hold about 341 entries, given each entry is 18 bytes long (due to being
// of the form "8e7abfb0-c16397b7,").
#if BUILDFLAG(LARGE_VARIATION_KEY_SIZE)
constexpr size_t kVariationsKeySize = 1024 * 8;
constexpr char kVariationKeySizeHistogram[] =
    "Variations.Limits.VariationKeySize.Large";
#else
constexpr size_t kVariationsKeySize = 1024 * 6;
constexpr char kVariationKeySizeHistogram[] =
    "Variations.Limits.VariationKeySize.Default";
#endif
constexpr size_t kVariationsKeySizeNumBuckets = 16;

// Crash key reporting the number of experiments. 8 is the size of the crash key
// in bytes, which is used to hold an int as a string.
crash_reporter::CrashKeyString<8> g_num_variations_crash_key(
    kNumExperimentsKey);

// Crash key reporting the variations state.
crash_reporter::CrashKeyString<kVariationsKeySize> g_variations_crash_key(
    kExperimentListKey);

crash_reporter::CrashKeyString<64> g_variations_seed_version_crash_key(
    kVariationsSeedVersionKey);

}  // namespace

class VariationsCrashKeys final : public base::FieldTrialList::Observer {
 public:
  VariationsCrashKeys();

  VariationsCrashKeys(const VariationsCrashKeys&) = delete;
  VariationsCrashKeys& operator=(const VariationsCrashKeys&) = delete;

  ~VariationsCrashKeys() override;

  // base::FieldTrialList::Observer:
  void OnFieldTrialGroupFinalized(const base::FieldTrial& trial,
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
  // updating crash keys. Returns true if it was successfully added. Returns
  // false otherwise (i.e., the trial was already added previously).
  bool AppendFieldTrial(const std::string& trial_name,
                        const std::string& group_name,
                        bool is_overridden);

  // Updates crash keys based on internal state.
  void UpdateCrashKeys();

  void AppendFieldTrialAndUpdateCrashKeys(const std::string& trial_name,
                                          const std::string& group_name,
                                          bool is_overridden);

  // List of active trials, used to prevent duplicates from being appended to
  // |variations_string_|.
  std::set<std::string> active_trials_;

  // Task runner corresponding to the UI thread, used to reschedule synchronous
  // observer calls that happen on a different thread.
  scoped_refptr<base::SequencedTaskRunner> ui_thread_task_runner_;

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  // Task runner corresponding to a background thread, used for tasks that may
  // block.
  scoped_refptr<base::SequencedTaskRunner> background_thread_task_runner_;
#endif  // IS_CHROMEOS_ASH || BUILDFLAG(IS_CHROMEOS_LACROS)

  // A serialized string containing the variations state.
  std::string variations_string_;

  // A serialized string containing the synthetic trials state.
  std::string synthetic_trials_string_;

  // Number of entries in |synthetic_trials_string_|.
  size_t num_synthetic_trials_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);
};

VariationsCrashKeys::VariationsCrashKeys() {
  // Set |ui_thread_task_runner_| *before* observering field trials. Otherwise,
  // it would be possible for a field trial to be activated on a different
  // thread, calling OnFieldTrialGroupFinalized(), and accessing
  // |ui_thread_task_runner_| before it is set.
  ui_thread_task_runner_ = base::SequencedTaskRunner::GetCurrentDefault();
  // Observe field trials before filling the crash key with the currently
  // active field trials. Otherwise, there could be a race condition where a
  // trial is activated on a different thread before we started observing.
  // Similarly, it is possible a trial is added twice if it is activated on
  // a different thread after starting to observe, but before the call to
  // GetActiveFieldTrialGroups() below. However, this is addressed with the use
  // of |active_trials_|.
  // TODO(crbug.com/40266142): This would not be necessary to do assuming this
  // is called while Chrome is still in single-threaded mode. While this is true
  // for the browser process, child processes call this relatively late (and
  // possibly other platforms as well). Remove |active_trials_| when this is
  // fixed.
  base::FieldTrialListIncludingLowAnonymity::AddObserver(this);

  base::FieldTrial::ActiveGroups active_groups;
  base::FieldTrialListIncludingLowAnonymity::GetActiveFieldTrialGroups(
      &active_groups);
  for (const auto& entry : active_groups) {
    AppendFieldTrial(entry.trial_name, entry.group_name, entry.is_overridden);
  }
#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  background_thread_task_runner_ = base::ThreadPool::CreateSequencedTaskRunner(
      {base::TaskPriority::BEST_EFFORT, base::MayBlock()});
#endif  // IS_CHROMEOS_ASH || BUILDFLAG(IS_CHROMEOS_LACROS)

  UpdateCrashKeys();
}

VariationsCrashKeys::~VariationsCrashKeys() {
  base::FieldTrialListIncludingLowAnonymity::RemoveObserver(this);
  g_num_variations_crash_key.Clear();
  g_variations_crash_key.Clear();
  g_variations_seed_version_crash_key.Clear();
}

void VariationsCrashKeys::OnFieldTrialGroupFinalized(
    const base::FieldTrial& trial,
    const std::string& group_name) {
  // If this is called on a different thread, post it back to the UI thread.
  // Note: This is safe to do because in production, this object is never
  // deleted and if this is called, it means the constructor has already run,
  // which is the only place that |ui_thread_task_runner_| is set.
  if (!ui_thread_task_runner_->RunsTasksInCurrentSequence()) {
    ui_thread_task_runner_->PostTask(
        FROM_HERE,
        BindOnce(&VariationsCrashKeys::AppendFieldTrialAndUpdateCrashKeys,
                 // base::Unretained() is safe here because this object is
                 // never deleted in production.
                 base::Unretained(this), trial.trial_name(), group_name,
                 trial.IsOverridden()));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  AppendFieldTrialAndUpdateCrashKeys(trial.trial_name(), group_name,
                                     trial.IsOverridden());
}

bool VariationsCrashKeys::AppendFieldTrial(const std::string& trial_name,
                                           const std::string& group_name,
                                           bool is_overridden) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!active_trials_.insert(trial_name).second) {
    return false;
  }

  auto active_group_id =
      MakeActiveGroupId(trial_name, group_name, is_overridden);
  auto variation = ActiveGroupToString(active_group_id);

  variations_string_ += variation;

  return true;
}

void VariationsCrashKeys::AppendFieldTrialAndUpdateCrashKeys(
    const std::string& trial_name,
    const std::string& group_name,
    bool is_overridden) {
  if (AppendFieldTrial(trial_name, group_name, is_overridden)) {
    UpdateCrashKeys();
  }
}

ExperimentListInfo VariationsCrashKeys::GetExperimentListInfo() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ExperimentListInfo result;
  result.num_experiments = active_trials_.size() + num_synthetic_trials_;
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

  const size_t count_of_kbs = info.experiment_list.size() / 1024;
  UMA_HISTOGRAM_EXACT_LINEAR(kVariationKeySizeHistogram, count_of_kbs,
                             kVariationsKeySizeNumBuckets);
  if (info.experiment_list.size() > kVariationsKeySize) {
    // If size exceeded, truncate to the last full entry.
    int comma_index =
        info.experiment_list.substr(0, kVariationsKeySize).rfind(',');
    info.experiment_list.resize(comma_index + 1);
  }

  g_variations_crash_key.Set(info.experiment_list);

  // If we're in the child process, set the variations seed version from the
  // command line, which is passed from the browser process. In the browser
  // process, SetVariationsSeedVersionCrashKey() gets called on startup.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(variations::switches::kVariationsSeedVersion)) {
    SetVariationsSeedVersionCrashKey(command_line->GetSwitchValueASCII(
        variations::switches::kVariationsSeedVersion));
  }

#if BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS)
  ReportVariationsToChromeOs(background_thread_task_runner_, info);
#endif  // IS_CHROMEOS_ASH || BUILDFLAG(IS_CHROMEOS_LACROS)
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

const char kNumExperimentsKey[] = "num-experiments";
const char kExperimentListKey[] = "variations";
const char kVariationsSeedVersionKey[] = "variations-seed-version";

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

void SetVariationsSeedVersionCrashKey(const std::string& seed_version) {
  g_variations_seed_version_crash_key.Set(seed_version);
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

std::string ActiveGroupToString(const ActiveGroupId& active_group) {
  return base::StringPrintf("%x-%x,", active_group.name, active_group.group);
}

}  // namespace variations
