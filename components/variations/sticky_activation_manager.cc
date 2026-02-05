// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/sticky_activation_manager.h"

#include <string>

#include "base/debug/dump_without_crashing.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/field_trial_list_including_low_anonymity.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/task/sequenced_task_runner.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/variations/pref_names.h"

namespace variations {
namespace {

BASE_FEATURE(kVariationsStickyPersistence, base::FEATURE_ENABLED_BY_DEFAULT);

// The type of persistence to use after updating the pref.
enum class PersistenceType {
  // No persistence, just update the pref.
  kSetOnly = 0,
  // Update the pref and commit the write.
  kSetAndCommit = 1,
  // Update the pref and schedule the write.
  kSetAndSchedule = 2,
};
constexpr base::FeatureParam<PersistenceType>::Option kPersistenceTypes[] = {
    // Note: kSetOnly is not listed here, it's used as the fallback.
    {PersistenceType::kSetAndCommit, "commit"},
    {PersistenceType::kSetAndSchedule, "schedule"},
};
BASE_FEATURE_ENUM_PARAM(PersistenceType,
                        kVariationsStickyPersistenceModeParam,
                        &kVariationsStickyPersistence,
                        "persistence_type",
                        PersistenceType::kSetOnly,
                        &kPersistenceTypes);

// Used as the group name for studies that we know have STICKY_AFTER_QUERY
// activation, but haven't been made active yet.
//
// Note: We intentionally use the same character as the separator for the pref,
// since a) that character is already reserved and can't appear naturally in
// these strings and b) to guarantee it's not something we'd load or save to the
// pref, as doing so would make it invalid.
const char kInactiveStickyTrialSentinel[] = "/";

// Parses the sticky studies pref value, which is expected to be of the format
// "Study1/Group1/Study2/Group2" and returns as a map from trial names to
// groups names.
StickyActivationManager::TrialNameToGroupNameMap ParsePref(
    const std::string& pref_value) {
  StickyActivationManager::TrialNameToGroupNameMap result;

  // Note: Even though base::FieldTrial::ParseFieldTrialsString() provides more
  // features than we need, by using it we benefit from the validation it does.
  std::vector<base::FieldTrial::State> entries;
  if (!base::FieldTrial::ParseFieldTrialsString(
          pref_value, /*override_trials=*/false, entries)) {
    // This is not a CHECK() since the pref value is external, but we still want
    // to monitor the occurrence of invalid prefs in case there is a a code
    // issue, so dump without crashing to signal the issue.
    base::debug::DumpWithoutCrashing();
    return result;
  }
  for (const auto& entry : entries) {
    result[std::string(entry.trial_name)] = std::string(entry.group_name);
  }
  return result;
}

// Encodes `trials` as a string pref value of the format
// "Study1/Group1/Study2/Group2".
std::string EncodePref(
    const StickyActivationManager::TrialNameToGroupNameMap& trials) {
  std::string pref_value;
  for (const auto& [key, value] : trials) {
    if (value == kInactiveStickyTrialSentinel) {
      continue;
    }
    if (!pref_value.empty()) {
      base::StrAppend(&pref_value, {"/"});
    }
    base::StrAppend(&pref_value, {key, "/", value});
  }
  return pref_value;
}

}  // namespace

// An observer that forwards notifications to the StickyActivationManager on the
// UI thread. This is a RefCountedThreadSafe object so that it can be safely
// passed between threads.
class StickyActivationManager::Observer
    : public base::FieldTrialList::Observer,
      public base::RefCountedThreadSafe<StickyActivationManager::Observer> {
 public:
  Observer(scoped_refptr<base::SequencedTaskRunner> task_runner,
           base::WeakPtr<StickyActivationManager> manager)
      : task_runner_(std::move(task_runner)), manager_(manager) {}

  Observer(const Observer&) = delete;
  Observer& operator=(const Observer&) = delete;

 private:
  friend class base::RefCountedThreadSafe<Observer>;
  ~Observer() override = default;

  // base::FieldTrialList::Observer:
  void OnFieldTrialGroupFinalized(const base::FieldTrial& trial,
                                  const std::string& group_name) override {
    // This may be called on any thread. If it's called on the UI thread, we can
    // run the task directly.
    if (task_runner_->RunsTasksInCurrentSequence()) {
      // The manager may be null if it was destroyed, since the observer is
      // ref-counted and may outlive the manager.
      if (manager_) {
        manager_->OnFieldTrialGroupFinalized(
            base::PassKey<StickyActivationManager>(), trial.trial_name(),
            group_name);
      }
      return;
    }

    // Otherwise, post a task to the UI thread.
    task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&StickyActivationManager::OnFieldTrialGroupFinalized,
                       manager_, base::PassKey<StickyActivationManager>(),
                       trial.trial_name(), group_name));
  }

  scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::WeakPtr<StickyActivationManager> manager_;
};

StickyActivationManager::StickyActivationManager(PrefService* local_state)
    : local_state_(local_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (local_state) {
    loaded_sticky_trials_ =
        ParsePref(local_state_->GetString(prefs::kVariationsStickyStudies));
  }
}

StickyActivationManager::~StickyActivationManager() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (monitoring_started_) {
    base::FieldTrialListIncludingLowAnonymity::RemoveObserver(observer_.get());
  }
}

// static
void StickyActivationManager::RegisterPrefs(PrefRegistrySimple& registry) {
  registry.RegisterStringPref(prefs::kVariationsStickyStudies, "",
                              PrefRegistry::LOSSY_PREF);
}

void StickyActivationManager::StartMonitoring() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!monitoring_started_);
  monitoring_started_ = true;

  // Clear the loaded sticky trials, since these are no longer needed. The
  // entries that were activated have been copied over to
  // `active_sticky_trials_`.
  loaded_sticky_trials_.clear();

  observer_ = base::MakeRefCounted<Observer>(
      base::SequencedTaskRunner::GetCurrentDefault(),
      weak_factory_.GetWeakPtr());
  base::FieldTrialListIncludingLowAnonymity::AddObserver(observer_.get());

  UpdatePref();
}

bool StickyActivationManager::ShouldActivate(const std::string& trial_name,
                                             const std::string& group_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(!monitoring_started_);

  auto it = loaded_sticky_trials_.find(trial_name);
  if (it != loaded_sticky_trials_.end() && it->second == group_name) {
    active_sticky_trials_[trial_name] = group_name;
    return true;
  }
  // Otherwise, we know this is a sticky trial and it's not active yet, so
  // reserve a slot for it so we can tell it's a sticky trial when we observe
  // its activation.
  active_sticky_trials_[trial_name] = kInactiveStickyTrialSentinel;
  return false;
}

void StickyActivationManager::OnFieldTrialGroupFinalized(
    base::PassKey<StickyActivationManager> pass_key,
    const std::string& trial_name,
    const std::string& group_name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(monitoring_started_);

  // Check whether the trial is present in `active_sticky_trials_`, which is how
  // we track which trials have the STICKY_AFTER_QUERY activation type.
  auto it = active_sticky_trials_.find(trial_name);
  if (it != active_sticky_trials_.end()) {
    // We don't expect to be notified of the same trial twice, so the entry for
    // this trial should be the sentinel.
    //
    // Note: We DCHECK() instead of CHECK() here because this code being hit
    // relies both on a client-side coding bug, but also a specific server-side
    // payload that would exercise this (i.e. existence of STICKY_AFTER_QUERY
    // studies). We don't want a case where the client-side bug is introduced
    // but the server-side payload not exercising this to make it to Stable and
    // then start crashing lots of users, so use a DCHECK.
    DCHECK_EQ(it->second, kInactiveStickyTrialSentinel);

    it->second = group_name;

    // Record a metric for when the study is activated for the first time.
    // Note: This is not recorded when the study is activated on a subsequent
    // sessions due to being sticky, because StartMonitoring() is only called
    // following startup activations of persisted sticky studies.
    base::UmaHistogramSparse(
        "Variations.StickyAfterQuery.Activation",
        static_cast<int>(base::HashFieldTrialName(trial_name)));
    UpdatePref();
  }
}

void StickyActivationManager::UpdatePref() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CHECK(monitoring_started_);

  // TODO: crbug.com/435630455 - Instead of updating the pref each time,
  // schedule an update so that we can batch multiple updates together.
  if (!local_state_) {
    return;
  }

  std::string pref_value = EncodePref(active_sticky_trials_);
  if (pref_value == local_state_->GetString(prefs::kVariationsStickyStudies)) {
    return;
  }
  local_state_->SetString(prefs::kVariationsStickyStudies, pref_value);

  // If the feature list is not yet initialized, we can't use it to determine
  // the persistence mode. This is expected when monitoring starts and for any
  // features checked by variations code before the feature list is set.
  if (!base::FeatureList::GetInstance()) {
    return;
  }
  switch (kVariationsStickyPersistenceModeParam.Get()) {
    case PersistenceType::kSetOnly:
      break;
    case PersistenceType::kSetAndCommit:
      local_state_->CommitPendingWrite();
      break;
    case PersistenceType::kSetAndSchedule:
      local_state_->SchedulePendingLossyWrites();
      break;
  }
}

}  // namespace variations
