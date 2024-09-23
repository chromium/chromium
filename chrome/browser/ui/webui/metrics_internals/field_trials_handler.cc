// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/metrics_internals/field_trials_handler.h"

#include <string_view>

#include "base/functional/bind.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/variations/field_trial_internals_utils.h"
#include "components/variations/hashing.h"
#include "components/variations/service/variations_service.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace {
using variations::HashNameAsHexString;
using TrialGroup = std::pair<std::string, std::string>;

// Returns a `Group` from components/metrics/debug/browser_proxy.ts.
base::Value::Dict ToGroupValue(
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

  auto result = base::Value::Dict()
                    .Set("hash", group_hash)
                    .Set("forceEnabled", force_enabled)
                    .Set("enabled", currently_enabled);
  if (show_names) {
    result.Set("name", group_name);
  }

  return result;
}

// Returns a `Trial` from components/metrics/debug/browser_proxy.ts.
base::Value::Dict ToTrialValue(
    bool show_names,
    const base::flat_map<std::string, std::string>& overrides,
    const variations::StudyGroupNames& study) {
  base::Value::Dict result =
      base::Value::Dict().Set("hash", HashNameAsHexString(study.name));
  if (show_names) {
    result.Set("name", study.name);
  }
  base::Value::List groups_value;
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

FieldTrialsHandler::FieldTrialsHandler(Profile* profile) : profile_(profile) {}
FieldTrialsHandler::~FieldTrialsHandler() = default;

void FieldTrialsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "fetchTrialState",
      base::BindRepeating(&FieldTrialsHandler::HandleFetchState,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setTrialEnrollState",
      base::BindRepeating(&FieldTrialsHandler::HandleSetEnrollState,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "restart", base::BindRepeating(&FieldTrialsHandler::HandleRestart,
                                     base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "lookupTrialOrGroupName",
      base::BindRepeating(&FieldTrialsHandler::HandleLookupTrialOrGroupName,
                          base::Unretained(this)));
}

void FieldTrialsHandler::InitializeFieldTrials() {
  if (initialized_field_trials_) {
    return;
  }
  initialized_field_trials_ = true;

  bool always_show_names =
#if defined(OFFICIAL_BUILD)
      false;
#else
      true;
#endif

  show_names_ = always_show_names ||
                gaia::IsGoogleInternalAccountEmail(
                    IdentityManagerFactory::GetForProfile(profile_)
                        ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
                        .email);

  studies_ =
      g_browser_process->variations_service()->GetStudiesAvailableToForce();

  overrides_ = RefreshAndGetFieldTrialOverrides(
      studies_, *g_browser_process->local_state(), restart_required_);
}

base::Value::Dict FieldTrialsHandler::GetFieldTrialStateValue() {
  base::Value::List trials;
  for (const auto& study : studies_) {
    trials.Append(ToTrialValue(show_names_, overrides_, study));
  }
  return base::Value::Dict()
      .Set("trials", std::move(trials))
      .Set("restartRequired", restart_required_);
}

void FieldTrialsHandler::HandleFetchState(const base::Value::List& args) {
  if (args.size() != 1) {
    DLOG(ERROR) << "Wrong number of args: " << args.size();
    return;
  }
  AllowJavascript();
  InitializeFieldTrials();

  ResolveJavascriptCallback(args[0], GetFieldTrialStateValue());
}

void FieldTrialsHandler::HandleSetEnrollState(const base::Value::List& args) {
  if (args.size() != 4) {
    DLOG(ERROR) << "Wrong number of args: " << args.size();
    return;
  }
  auto trial_hash = args[1].GetString();
  auto group_hash = args[2].GetString();
  bool enabled = args[3].GetBool();
  ResolveJavascriptCallback(args[0],
                            SetOverride({trial_hash, group_hash}, enabled));
}

bool FieldTrialsHandler::SetOverride(const ExperimentOverride& override,
                                     bool enabled) {
  TrialGroup group = FindExperimentFromHashes(studies_, override.trial_hash,
                                              override.group_hash);
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
    TrialGroup names = FindExperimentFromHashes(studies_, override_hashes.first,
                                                override_hashes.second);
    CHECK(!names.first.empty())
        << "Didn't find experiment: " << override_hashes.first << "."
        << override_hashes.second;
    states.push_back(std::move(names));
  }

  restart_required_ = variations::SetTemporaryTrialOverrides(
                          *g_browser_process->local_state(), states) ||
                      restart_required_;
  return true;
}

void FieldTrialsHandler::HandleRestart(const base::Value::List& args) {
  chrome::AttemptRestart();
}

void FieldTrialsHandler::HandleLookupTrialOrGroupName(
    const base::Value::List& args) {
  if (args.size() != 2) {
    DLOG(ERROR) << "Wrong number of arguments";
    return;
  }

  base::Value::Dict name_hashes;
  std::vector<std::string> names = {args[1].GetString()};
  // Note: the user may have typed in a single study or group name, or a study
  // and group name with a separator. Frequently we use '.' or '-' as a
  // separator, but these are allowed in study/group names. If a user types in
  // "One-Two", we search for all names: ["One-Two", "One", "Two"].
  for (const TrialGroup& study_and_group : ParseGroup(names[0])) {
    names.push_back(study_and_group.first);
    names.push_back(study_and_group.second);
  }
  for (std::string& name : names) {
    for (const auto& study : studies_) {
      if (study.name == name) {
        name_hashes.Set(HashNameAsHexString(name), name);
        for (const std::string& group : study.groups) {
          name_hashes.Set(HashNameAsHexString(group), group);
        }
        break;
      }

      for (const std::string& group_name : study.groups) {
        if (name == group_name) {
          name_hashes.Set(HashNameAsHexString(name), name);
          break;
        }
      }
    }
  }

  ResolveJavascriptCallback(args[0], name_hashes);
}
