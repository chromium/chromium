// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/search_engine_choice/icon_utils.h"

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "chrome/browser/browser_process.h"
#include "components/country_codes/country_codes.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/regional_capabilities/regional_capabilities_test_utils.h"
#include "components/search_engines/search_engines_test_environment.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "testing/gtest/include/gtest/gtest.h"

class IconUtilsTest : public ::testing::Test {
 public:
  ~IconUtilsTest() override = default;
  PrefService* pref_service() {
    return &search_engines_test_environment_.pref_service();
  }
  search_engines::SearchEngineChoiceService* search_engine_choice_service() {
    return &search_engines_test_environment_.search_engine_choice_service();
  }
  TemplateURLPrepopulateData::Resolver& prepopulate_data_resolver() {
    return search_engines_test_environment_.prepopulate_data_resolver();
  }

 private:
  search_engines::SearchEnginesTestEnvironment search_engines_test_environment_;
};

TEST_F(IconUtilsTest, GetSearchEngineGeneratedIconPath) {
  // Make sure the country is not forced.
  ASSERT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSearchEngineChoiceCountry));

  for (int country_id : regional_capabilities::kEeaChoiceCountriesIds) {
    search_engine_choice_service()->ClearCountryIdCacheForTesting();
    pref_service()->SetInteger(country_codes::kCountryIDAtInstall, country_id);
    std::vector<std::unique_ptr<TemplateURLData>> urls =
        prepopulate_data_resolver().GetPrepopulatedEngines();
    for (const std::unique_ptr<TemplateURLData>& url : urls) {
      EXPECT_FALSE(GetSearchEngineGeneratedIconPath(url->keyword()).empty())
          << "Missing icon for " << url->keyword() << ". Try re-running "
          << "`tools/search_engine_choice/generate_search_engine_icons.py`.";
    }
  }
}
