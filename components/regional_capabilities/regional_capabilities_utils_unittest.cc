// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/regional_capabilities/regional_capabilities_utils.h"

#include "base/test/gtest_util.h"
#include "components/country_codes/country_codes.h"
#include "components/regional_capabilities/access/country_access_reason.h"
#include "components/regional_capabilities/program_settings.h"
#include "components/regional_capabilities/regional_capabilities_prefs.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/search_engines_data/resources/definitions/prepopulated_engines.h"
#include "third_party/search_engines_data/resources/definitions/regional_settings.h"

namespace regional_capabilities {
namespace {

using ::TemplateURLPrepopulateData::bing;
using ::TemplateURLPrepopulateData::duckduckgo;
using ::TemplateURLPrepopulateData::ecosia;
using ::TemplateURLPrepopulateData::google;
using ::TemplateURLPrepopulateData::PrepopulatedEngine;
using ::TemplateURLPrepopulateData::yahoo;

std::vector<const PrepopulatedEngine*> sample_regional_engines = {
    &google,
    &duckduckgo,
    &bing,
};

std::vector<const PrepopulatedEngine*> other_known_engines = {
    &yahoo,
    &ecosia,
};

TEST(RegionalCapabilitiesUtilsTest, GetAllPrepopulatedEngines) {
  regional_capabilities::ClearPrepopulatedEnginesOverrideForTesting();
  EXPECT_EQ(regional_capabilities::GetAllPrepopulatedEngines().size(),
            TemplateURLPrepopulateData::kAllEngines.size());
}

TEST(RegionalCapabilitiesUtilsTest, GetAllPrepopulatedEngines_Overridden) {
  auto scoped_override =
      regional_capabilities::SetPrepopulatedEnginesOverrideForTesting(
          sample_regional_engines, other_known_engines);
  EXPECT_EQ(regional_capabilities::GetAllPrepopulatedEngines().size(),
            sample_regional_engines.size() + other_known_engines.size());

  for (size_t i = 0; i < other_known_engines.size(); i++) {
    EXPECT_EQ(regional_capabilities::GetAllPrepopulatedEngines()[i],
              other_known_engines[i]);
  }

  for (size_t i = 0; i < sample_regional_engines.size(); i++) {
    int aggregated_index = other_known_engines.size() + i;
    EXPECT_EQ(
        regional_capabilities::GetAllPrepopulatedEngines()[aggregated_index],
        sample_regional_engines[i]);
  }
}

TEST(RegionalCapabilitiesUtilsTest, GetPrepopulatedEngines) {
  sync_preferences::TestingPrefServiceSyncable pref_service;
  regional_capabilities::prefs::RegisterProfilePrefs(pref_service.registry());

  country_codes::CountryId us_country_id("US");
  const auto prepopulated_engines =
      regional_capabilities::GetPrepopulatedEngines(
          us_country_id, pref_service, SearchEngineListType::kTopN);

  EXPECT_EQ(prepopulated_engines.size(),
            TemplateURLPrepopulateData::kRegionalSettings.at(us_country_id)
                ->search_engines.size());
}

TEST(RegionalCapabilitiesUtilsTest, GetPrepopulatedEnginesOverrideForTesting) {
  // No overrides, should fail a CHECK.
  EXPECT_CHECK_DEATH(
      regional_capabilities::GetPrepopulatedEnginesOverrideForTesting());

  {
    auto scoped_override =
        regional_capabilities::SetPrepopulatedEnginesOverrideForTesting(
            sample_regional_engines, other_known_engines);

    const auto& overrides =
        regional_capabilities::GetPrepopulatedEnginesOverrideForTesting();
    EXPECT_EQ(overrides.all_engines.size(),
              sample_regional_engines.size() + other_known_engines.size());
  }

  // Scoped override expires, should fail again.
  EXPECT_CHECK_DEATH(
      regional_capabilities::GetPrepopulatedEnginesOverrideForTesting());
}

}  // namespace
}  // namespace regional_capabilities
