// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/starter_heuristic_configs/finch_configs.h"
#include "base/no_destructor.h"
#include "components/autofill_assistant/browser/features.h"
#include "components/autofill_assistant/browser/starter_heuristic_configs/finch_starter_heuristic_config.h"

namespace autofill_assistant {

namespace {

// String parameter containing the JSON-encoded parameter dictionary.
const char kUrlHeuristicParametersKey[] = "json_parameters";
// The 5 URL heuristics features that are defined for future use cases.
constexpr base::FeatureParam<std::string> kUrlHeuristicParams1{
    &features::kAutofillAssistantUrlHeuristic1, kUrlHeuristicParametersKey, ""};
constexpr base::FeatureParam<std::string> kUrlHeuristicParams2{
    &features::kAutofillAssistantUrlHeuristic2, kUrlHeuristicParametersKey, ""};
constexpr base::FeatureParam<std::string> kUrlHeuristicParams3{
    &features::kAutofillAssistantUrlHeuristic3, kUrlHeuristicParametersKey, ""};
constexpr base::FeatureParam<std::string> kUrlHeuristicParams4{
    &features::kAutofillAssistantUrlHeuristic4, kUrlHeuristicParametersKey, ""};
constexpr base::FeatureParam<std::string> kUrlHeuristicParams5{
    &features::kAutofillAssistantUrlHeuristic5, kUrlHeuristicParametersKey, ""};
constexpr base::FeatureParam<std::string> kUrlHeuristicParams6{
    &features::kAutofillAssistantUrlHeuristic6, kUrlHeuristicParametersKey, ""};
constexpr base::FeatureParam<std::string> kUrlHeuristicParams7{
    &features::kAutofillAssistantUrlHeuristic7, kUrlHeuristicParametersKey, ""};
constexpr base::FeatureParam<std::string> kUrlHeuristicParams8{
    &features::kAutofillAssistantUrlHeuristic8, kUrlHeuristicParametersKey, ""};
constexpr base::FeatureParam<std::string> kUrlHeuristicParams9{
    &features::kAutofillAssistantUrlHeuristic9, kUrlHeuristicParametersKey, ""};

}  // namespace

namespace finch_configs {

// static
const FinchStarterHeuristicConfig* GetOrCreateUrlHeuristic1() {
  static base::NoDestructor<FinchStarterHeuristicConfig>
      starter_heuristic_config_1(kUrlHeuristicParams1);
  return starter_heuristic_config_1.get();
}

// static
const FinchStarterHeuristicConfig* GetOrCreateUrlHeuristic2() {
  static base::NoDestructor<FinchStarterHeuristicConfig>
      starter_heuristic_config_2(kUrlHeuristicParams2);
  return starter_heuristic_config_2.get();
}

// static
const FinchStarterHeuristicConfig* GetOrCreateUrlHeuristic3() {
  static base::NoDestructor<FinchStarterHeuristicConfig>
      starter_heuristic_config_3(kUrlHeuristicParams3);
  return starter_heuristic_config_3.get();
}

// static
const FinchStarterHeuristicConfig* GetOrCreateUrlHeuristic4() {
  static base::NoDestructor<FinchStarterHeuristicConfig>
      starter_heuristic_config_4(kUrlHeuristicParams4);
  return starter_heuristic_config_4.get();
}

// static
const FinchStarterHeuristicConfig* GetOrCreateUrlHeuristic5() {
  static base::NoDestructor<FinchStarterHeuristicConfig>
      starter_heuristic_config_5(kUrlHeuristicParams5);
  return starter_heuristic_config_5.get();
}

const FinchStarterHeuristicConfig* GetOrCreateUrlHeuristic6() {
  static base::NoDestructor<FinchStarterHeuristicConfig>
      starter_heuristic_config_6(kUrlHeuristicParams6);
  return starter_heuristic_config_6.get();
}

const FinchStarterHeuristicConfig* GetOrCreateUrlHeuristic7() {
  static base::NoDestructor<FinchStarterHeuristicConfig>
      starter_heuristic_config_7(kUrlHeuristicParams7);
  return starter_heuristic_config_7.get();
}

const FinchStarterHeuristicConfig* GetOrCreateUrlHeuristic8() {
  static base::NoDestructor<FinchStarterHeuristicConfig>
      starter_heuristic_config_8(kUrlHeuristicParams8);
  return starter_heuristic_config_8.get();
}

const FinchStarterHeuristicConfig* GetOrCreateUrlHeuristic9() {
  static base::NoDestructor<FinchStarterHeuristicConfig>
      starter_heuristic_config_9(kUrlHeuristicParams9);
  return starter_heuristic_config_9.get();
}

}  // namespace finch_configs
}  // namespace autofill_assistant
