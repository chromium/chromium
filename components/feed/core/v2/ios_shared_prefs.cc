// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/ios_shared_prefs.h"

#include "components/feed/core/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace feed {
namespace prefs {

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
      list.Append(elem);
    }
    dict.Set(exp.first, std::move(list));
  }
  pref_service.SetDict(kExperimentsV2, std::move(dict));
}

Experiments GetExperiments(PrefService& pref_service) {
  const auto& dict = pref_service.GetDict(kExperimentsV2);
  Experiments experiments;
  for (auto kv : dict) {
    std::vector<std::string> vect;
    for (const auto& v : kv.second.GetList()) {
      vect.push_back(v.GetString());
    }
    experiments[kv.first] = vect;
  }
  return experiments;
}

}  // namespace prefs
}  // namespace feed
