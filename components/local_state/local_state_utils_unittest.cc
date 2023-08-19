// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/local_state/local_state_utils.h"

#include "base/json/json_reader.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(LocalStateUtilsTest, UnfilterPrefs) {
  std::map<std::string, std::string> prefs{
      {"valid_pref", "notallowed"},
      {"variations.anything", "allowed"},
      {"user_experience_metrics.anything", "allowed"},
      {"variation", "notallowed"},
      {"user_experience_metric", "notallowed"},
      {"anything", "notallowed"},
  };

  TestingPrefServiceSimple pref_service;

  for (const auto& [key, value] : prefs) {
    pref_service.registry()->RegisterStringPref(key, std::string());
    pref_service.SetString(key, value);
  }
  ASSERT_EQ(pref_service.GetPreferencesValueAndStore().size(), prefs.size());

  auto unfiltered_json_prefs =
      local_state_utils::GetPrefsAsJson(&pref_service, {});
  ASSERT_TRUE(unfiltered_json_prefs.has_value());
  auto unfiltered_prefs = base::JSONReader::ReadAndReturnValueWithError(
      unfiltered_json_prefs.value(),
      base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_EQ(unfiltered_prefs.value().GetDict().size(), prefs.size());
}

TEST(LocalStateUtilsTest, FilterPrefs) {
  std::map<std::string, std::string> prefs{
      {"valid_pref", "notallowed"},
      {"variations.anything", "allowed"},
      {"user_experience_metrics.anything", "allowed"},
      {"variation", "notallowed"},
      {"user_experience_metric", "notallowed"},
      {"anything", "notallowed"},
  };

  TestingPrefServiceSimple pref_service;

  for (const auto& [key, value] : prefs) {
    pref_service.registry()->RegisterStringPref(key, std::string());
    pref_service.SetString(key, value);
  }
  ASSERT_EQ(pref_service.GetPreferencesValueAndStore().size(), prefs.size());

  auto filtered_json_prefs = local_state_utils::GetPrefsAsJson(
      &pref_service, {"variations", "user_experience_metrics"});
  ASSERT_TRUE(filtered_json_prefs.has_value());
  auto filtered_prefs = base::JSONReader::ReadAndReturnValueWithError(
      filtered_json_prefs.value(),
      base::JSONParserOptions::JSON_ALLOW_TRAILING_COMMAS);

  ASSERT_EQ(filtered_prefs.value().GetDict().size(), 2u);
  for (const auto [_, dict_value] : filtered_prefs.value().GetDict()) {
    for (const auto [key, value] : dict_value.GetDict()) {
      if (key == "value") {
        EXPECT_EQ(value.GetString(), "allowed");
      } else if (key == "metadata") {
        EXPECT_TRUE(value.is_list());
      }
    }
  }
}
