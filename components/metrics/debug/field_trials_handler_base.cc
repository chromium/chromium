// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/debug/field_trials_handler_base.h"

#include <string_view>

#include "base/functional/bind.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "components/variations/field_trial_internals_utils.h"
#include "components/variations/hashing.h"
#include "components/variations/service/variations_service.h"

namespace metrics {

namespace {
using variations::HashNameAsHexString;
using TrialGroup = std::pair<std::string, std::string>;

// Returns a `Group` from components/metrics/debug/browser_proxy.ts.
base::DictValue ToGroupValue(
    bool show_names,
    const base::flat_map<std::string, std::string>& overrides,
    std::string_view study_name,
    std::string_view group_name) {
  std::string group_hash = HashNameAsHexString(group_name);

  base::FieldTrial* found_trial = base::FieldTrialList::Find(study_name);
  std::string selected_group;
  if (found_trial) {
    selected_group = found_trial->GetGroupNameWithoutActivation();
  }
  bool currently_enabled = group_name == selected_group;

  std::string trial_hash = HashNameAsHexString(study_name);
  auto iter = overrides.find(trial_hash);
  bool force_enabled = (iter != overrides.end() && iter->second == group_hash);

  auto result = base::DictValue()
                    .Set("hash", group_hash)
                    .Set("forceEnabled", force_enabled)
                    .Set("enabled", currently_enabled);
  if (show_names) {
    result.Set("name", group_name);
  }

  return result;
}

// Returns a `Trial` from components/metrics/debug/browser_proxy.ts.
base::DictValue ToTrialValue(
    bool show_names,
    const base::flat_map<std::string, std::string>& overrides,
    const variations::StudyGroupNames& study) {
  base::DictValue result =
      base::DictValue().Set("hash", HashNameAsHexString(study.name));
  if (show_names) {
    result.Set("name", study.name);
  }
  base::ListValue groups_value;
  for (const auto& group : study.groups) {
    groups_value.Append(ToGroupValue(show_names, overrides, study.name, group));
  }
  result.Set("groups", std::move(groups_value));
  return result;
}

TrialGroup FindExperimentFromHashes(
    const std::vector<variations::StudyGroupNames>& studies,
    std::string_view study_hash,
    std::string_view experiment_hash) {
  for (const auto& study : studies) {
    if (HashNameAsHexString(study.name) == study_hash) {
      for (const std::string& group_name : study.groups) {
        if (HashNameAsHexString(group_name) == experiment_hash) {
          return {study.name, group_name};
        }
      }
    }
  }
  return {};
}

// Returns all possible intrepretations of `name` as a Trial and Group name.
// All of "Trial/Group", "Trial.Group", "Trial:Group", "Trial-Group" are
// allowed.
std::vector<TrialGroup> ParseGroup(std::string_view name) {
  std::vector<TrialGroup> groups;
  for (const char separator : {'/', '.', ':', '-'}) {
    std::vector<std::string> parts =
        base::SplitString(name, std::string(1, separator),
                          base::WhitespaceHandling::TRIM_WHITESPACE,
                          base::SplitResult::SPLIT_WANT_ALL);
    if (parts.size() != 2) {
      continue;
    }
    groups.emplace_back(parts[0], parts[1]);
  }
  return groups;
}

}  // namespace

FieldTrialsHandlerBase::FieldTrialsHandlerBase(
    Delegate* delegate,
    variations::VariationsService* variations_service,
    PrefService* local_state)
    : delegate_(delegate),
      variations_service_(variations_service),
      local_state_(local_state) {}

FieldTrialsHandlerBase::~FieldTrialsHandlerBase() = default;

void FieldTrialsHandlerBase::InitializeFieldTrials(
    base::OnceCallback<void(base::ValueView)> done_callback,
    bool show_names) {
  show_names_ = show_names;
  if (studies_.has_value()) {
    std::move(done_callback).Run(GetFieldTrialStateValue());
    return;
  }

  if (variations_service_) {
    variations_service_->GetStudiesAvailableToForce(base::BindOnce(
        &FieldTrialsHandlerBase::RefreshFieldTrialOverrides,
        weak_ptr_factory_.GetWeakPtr(), std::move(done_callback)));
  } else {
    // VariationsService is not available, run callback with empty results.
    base::DictValue result;
    result.Set("trials", base::ListValue());
    result.Set("restartRequired", false);
    std::move(done_callback).Run(std::move(result));
  }
}

void FieldTrialsHandlerBase::RefreshFieldTrialOverrides(
    base::OnceCallback<void(base::ValueView)> done_callback,
    std::vector<variations::StudyGroupNames> studies) {
  studies_ = std::move(studies);
  if (local_state_) {
    overrides_ = RefreshAndGetFieldTrialOverrides(
        studies_.value(), *local_state_, restart_required_);
  }
  std::move(done_callback).Run(GetFieldTrialStateValue());
}

base::DictValue FieldTrialsHandlerBase::GetFieldTrialStateValue() {
  CHECK(studies_.has_value()) << "Field trials not initialized.";
  base::ListValue trials;
  for (const auto& study : studies_.value()) {
    trials.Append(ToTrialValue(show_names_, overrides_, study));
  }
  return base::DictValue()
      .Set("trials", std::move(trials))
      .Set("restartRequired", restart_required_);
}

void FieldTrialsHandlerBase::HandleFetchState(const base::Value& callback_id,
                                              bool show_names) {
  base::OnceCallback<void(base::ValueView)> resolve_js_callback =
      base::BindOnce(&FieldTrialsHandlerBase::ResolveJsCallbackHelper,
                     weak_ptr_factory_.GetWeakPtr(), callback_id.Clone());

  InitializeFieldTrials(std::move(resolve_js_callback), show_names);
}

void FieldTrialsHandlerBase::ResolveJsCallbackHelper(base::Value callback_id,
                                                     base::ValueView result) {
  delegate_->ResolvePageCallback(callback_id, result);
}

void FieldTrialsHandlerBase::HandleSetEnrollState(
    const base::Value& callback_id,
    std::string_view trial_hash,
    std::string_view group_hash,
    bool enabled) {
  delegate_->ResolvePageCallback(
      callback_id,
      base::Value(SetOverride(
          {std::string(trial_hash), std::string(group_hash)}, enabled)));
}

bool FieldTrialsHandlerBase::SetOverride(const ExperimentOverride& override,
                                         bool enabled) {
  CHECK(studies_.has_value()) << "Field trials not initialized.";
  TrialGroup group = FindExperimentFromHashes(
      studies_.value(), override.trial_hash, override.group_hash);
  if (group.first.empty()) {
    return false;
  }

  if (enabled) {
    overrides_[override.trial_hash] = override.group_hash;
  } else {
    overrides_.erase(override.trial_hash);
  }

  std::vector<TrialGroup> states;
  for (const TrialGroup& override_hashes : overrides_) {
    TrialGroup names = FindExperimentFromHashes(
        studies_.value(), override_hashes.first, override_hashes.second);
    CHECK(!names.first.empty())
        << "Didn't find experiment: " << override_hashes.first << "."
        << override_hashes.second;
    states.push_back(std::move(names));
  }

  if (local_state_) {
    restart_required_ =
        variations::SetTemporaryTrialOverrides(*local_state_, states) ||
        restart_required_;
  }
  return true;
}

void FieldTrialsHandlerBase::HandleLookupTrialOrGroupName(
    const base::Value& callback_id,
    std::string_view name) {
  CHECK(studies_.has_value()) << "Field trials not initialized.";
  base::DictValue name_hashes;
  std::vector<std::string> names = {std::string(name)};
  // Note: the user may have typed in a single study or group name, or a study
  // and group name with a separator. Frequently we use '.' or '-' as a
  // separator, but these are allowed in study/group names. If a user types in
  // "One-Two", we search for all names: ["One-Two", "One", "Two"].
  for (const TrialGroup& study_and_group : ParseGroup(names[0])) {
    names.push_back(study_and_group.first);
    names.push_back(study_and_group.second);
  }
  for (std::string& n : names) {
    for (const auto& study : studies_.value()) {
      if (study.name == n) {
        name_hashes.Set(HashNameAsHexString(n), n);
        for (const std::string& g : study.groups) {
          name_hashes.Set(HashNameAsHexString(g), g);
        }
        break;
      }

      for (const std::string& group_name : study.groups) {
        if (n == group_name) {
          name_hashes.Set(HashNameAsHexString(n), n);
          break;
        }
      }
    }
  }

  delegate_->ResolvePageCallback(callback_id, name_hashes);
}

}  // namespace metrics
