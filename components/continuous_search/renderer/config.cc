// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/continuous_search/renderer/config.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/no_destructor.h"

namespace continuous_search {

namespace {

Config& GetConfigInternal() {
  static base::NoDestructor<Config> s_config;
  return *s_config;
}

}  // namespace

BASE_FEATURE(kRelatedSearchesExtraction,
             "RelatedSearchesExtraction",
             base::FEATURE_ENABLED_BY_DEFAULT);

Config::Config() {
  std::string id_value = base::GetFieldTrialParamValueByFeature(
      kRelatedSearchesExtraction, "related_searches_id");
  if (!id_value.empty()) {
    related_searches_id = id_value;
  }

  std::string anchor_value = base::GetFieldTrialParamValueByFeature(
      kRelatedSearchesExtraction, "related_searches_anchor_classname");
  if (!anchor_value.empty()) {
    related_searches_anchor_classname = anchor_value;
  }

  std::string title_value = base::GetFieldTrialParamValueByFeature(
      kRelatedSearchesExtraction, "related_searches_title_classname");
  if (!title_value.empty()) {
    related_searches_title_classname = title_value;
  }
}

Config::Config(const Config& other) = default;
Config::~Config() = default;

const Config& GetConfig() {
  return GetConfigInternal();
}

void SetConfigForTesting(const Config& config) {
  GetConfigInternal() = config;
}

}  // namespace continuous_search