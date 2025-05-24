// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/contextual_search/core/browser/contextual_search_field_trial.h"

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "components/contextual_search/core/browser/public.h"

namespace {

const char kFalseValue[] = "false";
const char kAnyNonEmptyValue[] = "1";
const char kContextualSearchResolverUrl[] = "contextual-search-resolver-url";
const char kContextualSearchSurroundingSizeParamName[] = "surrounding_size";
const char kContextualSearchSampleSurroundingSizeParamName[] =
    "sample_surrounding_size";
const char kContextualSearchDecodeMentionsDisabledParamName[] =
    "disable_decode_mentions";

// The default size of the content surrounding the selection to gather, allowing
// room for other parameters.
const int kContextualSearchDefaultContentSize = 1536;

}  // namespace

// static
const int
    ContextualSearchFieldTrial::kContextualSearchDefaultSampleSurroundingSize =
        400;

BASE_FEATURE(kContextualSearchWithCredentialsForDebug,
             "ContextualSearchWithCredentialsForDebug",
             base::FEATURE_DISABLED_BY_DEFAULT);

ContextualSearchFieldTrial::ContextualSearchFieldTrial()
    : is_resolver_url_prefix_cached_(false),
      is_surrounding_size_cached_(false),
      surrounding_size_(0),
      is_sample_surrounding_size_cached_(false),
      sample_surrounding_size_(0),
      is_decode_mentions_disabled_cached_(false),
      is_decode_mentions_disabled_(false),
      is_contextual_cards_version_cached_(false),
      contextual_cards_version_(0) {}

ContextualSearchFieldTrial::~ContextualSearchFieldTrial() = default;

std::string ContextualSearchFieldTrial::GetResolverURLPrefix() {
  if (!is_resolver_url_prefix_cached_) {
    is_resolver_url_prefix_cached_ = true;
    resolver_url_prefix_ = GetSwitch(kContextualSearchResolverUrl);
    if (resolver_url_prefix_.empty())
      resolver_url_prefix_ = GetParam(kContextualSearchResolverUrl);
  }
  return resolver_url_prefix_;
}

int ContextualSearchFieldTrial::GetResolveSurroundingSize() {
  return GetIntParamValueOrDefault(kContextualSearchSurroundingSizeParamName,
                                   kContextualSearchDefaultContentSize,
                                   &is_surrounding_size_cached_,
                                   &surrounding_size_);
}

int ContextualSearchFieldTrial::GetSampleSurroundingSize() {
  return GetIntParamValueOrDefault(
      kContextualSearchSampleSurroundingSizeParamName,
      kContextualSearchDefaultSampleSurroundingSize,
      &is_sample_surrounding_size_cached_, &sample_surrounding_size_);
}

bool ContextualSearchFieldTrial::IsDecodeMentionsDisabled() {
  return GetBooleanParam(kContextualSearchDecodeMentionsDisabledParamName,
                         &is_decode_mentions_disabled_cached_,
                         &is_decode_mentions_disabled_);
}

int ContextualSearchFieldTrial::GetContextualCardsVersion() {
  return GetIntParamValueOrDefault(
      contextual_search::kContextualCardsVersionParamName, 0,
      &is_contextual_cards_version_cached_, &contextual_cards_version_);
}

bool ContextualSearchFieldTrial::GetBooleanParam(const std::string& name,
                                                 bool* is_value_cached,
                                                 bool* cached_value) {
  if (!*is_value_cached) {
    *is_value_cached = true;
    std::string string_value = GetSwitch(name);
    // A switch with an empty value is true.
    bool has_switch = HasSwitch(name);
    if (has_switch && string_value.empty())
      string_value = kAnyNonEmptyValue;
    if (!has_switch)
      string_value = GetParam(name);
    *cached_value = !string_value.empty() && string_value != kFalseValue;
  }
  return *cached_value;
}

int ContextualSearchFieldTrial::GetIntParamValueOrDefault(
    const std::string& name,
    const int default_value,
    bool* is_value_cached,
    int* cached_value) {
  if (!*is_value_cached) {
    *is_value_cached = true;
    std::string param_string = GetSwitch(name);
    if (param_string.empty())
      param_string = GetParam(name);
    int param_int;
    if (!param_string.empty() && base::StringToInt(param_string, &param_int))
      *cached_value = param_int;
    else
      *cached_value = default_value;
  }
  return *cached_value;
}

bool ContextualSearchFieldTrial::HasSwitch(const std::string& name) {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(name);
}

std::string ContextualSearchFieldTrial::GetSwitch(const std::string& name) {
  if (!HasSwitch(name))
    return std::string();
  else
    return base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(name);
}

std::string ContextualSearchFieldTrial::GetParam(const std::string& name) {
  return base::GetFieldTrialParamValue(
      contextual_search::kContextualSearchFieldTrialName, name);
}
