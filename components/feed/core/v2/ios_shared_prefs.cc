// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/ios_shared_prefs.h"

#include "components/feed/core/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace feed {
namespace prefs {

namespace {
const char kNameKey[] = "name";
const char kIdKey[] = "id";
}  // namespace

void SetLastFetchHadNoticeCard(PrefService& pref_service, bool value) {
  pref_service.SetBoolean(feed::prefs::kLastFetchHadNoticeCard, value);
}

bool GetLastFetchHadNoticeCard(const PrefService& pref_service) {
  return pref_service.GetBoolean(feed::prefs::kLastFetchHadNoticeCard);
}

void SetHasReachedClickAndViewActionsUploadConditions(PrefService& pref_service,
                                                      bool value) {
  pref_service.SetBoolean(
      feed::prefs::kHasReachedClickAndViewActionsUploadConditions, value);
}

bool GetHasReachedClickAndViewActionsUploadConditions(
    const PrefService& pref_service) {
  return pref_service.GetBoolean(
      feed::prefs::kHasReachedClickAndViewActionsUploadConditions);
}

void IncrementNoticeCardViewsCount(PrefService& pref_service) {
  int count = pref_service.GetInteger(feed::prefs::kNoticeCardViewsCount);
  pref_service.SetInteger(feed::prefs::kNoticeCardViewsCount, count + 1);
}

int GetNoticeCardViewsCount(const PrefService& pref_service) {
  return pref_service.GetInteger(feed::prefs::kNoticeCardViewsCount);
}

void IncrementNoticeCardClicksCount(PrefService& pref_service) {
  int count = pref_service.GetInteger(feed::prefs::kNoticeCardClicksCount);
  pref_service.SetInteger(feed::prefs::kNoticeCardClicksCount, count + 1);
}

int GetNoticeCardClicksCount(const PrefService& pref_service) {
  return pref_service.GetInteger(feed::prefs::kNoticeCardClicksCount);
}

void SetExperiments(const Experiments& experiments, PrefService& pref_service) {
  base::Value::Dict dict;
  for (const auto& exp : experiments) {
    base::Value::List list;
    for (auto elem : exp.second) {
      base::Value::Dict group_dict;
      group_dict.Set(kNameKey, elem.name);
      group_dict.Set(kIdKey, elem.experiment_id);
      list.Append(std::move(group_dict));
    }
    dict.Set(exp.first, std::move(list));
  }
  pref_service.SetDict(kExperimentsV3, std::move(dict));
}

Experiments GetExperiments(PrefService& pref_service) {
  const auto& dict = pref_service.GetDict(kExperimentsV3);
  Experiments experiments;
  for (auto kv : dict) {
    std::vector<ExperimentGroup> vect;
    for (const auto& v : kv.second.GetList()) {
      ExperimentGroup group;
      auto* group_dict = v.GetIfDict();
      if (group_dict) {
        const std::string* name = group_dict->FindString(kNameKey);
        if (!name) {
          continue;
        }
        group.name = *name;
        group.experiment_id = group_dict->FindInt(kIdKey).value_or(0);
      }
      vect.push_back(group);
    }
    experiments[kv.first] = vect;
  }
  return experiments;
}

void MigrateObsoleteFeedExperimentPref_Jun_2024(PrefService* prefs) {
  const base::Value* val =
      prefs->GetUserPrefValue(prefs::kExperimentsV2Deprecated);
  const base::Value::Dict* old = val ? val->GetIfDict() : nullptr;
  if (old) {
    Experiments experiments;
    for (const auto kv : *old) {
      std::vector<ExperimentGroup> vect;
      for (const auto& v : kv.second.GetList()) {
        ExperimentGroup group;
        group.name = v.GetString();
        group.experiment_id = 0;
        vect.push_back(group);
      }
      experiments[kv.first] = vect;
    }
    SetExperiments(experiments, *prefs);
  }
  prefs->ClearPref(prefs::kExperimentsV2Deprecated);
}

}  // namespace prefs
}  // namespace feed
