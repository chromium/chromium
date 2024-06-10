// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/search_engine_choice/icon_utils.h"

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "components/country_codes/country_codes.h"
#include "components/search_engines/eea_countries_ids.h"
#include "components/search_engines/search_engine_choice/search_engine_choice_service.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/search_engines/template_url_prepopulate_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"

class IconUtilsTest : public ::testing::Test {
 public:
  IconUtilsTest() {
    TemplateURLService::RegisterProfilePrefs(pref_service_.registry());
    TemplateURLPrepopulateData::RegisterProfilePrefs(pref_service_.registry());
    search_engine_choice_service_ =
        std::make_unique<search_engines::SearchEngineChoiceService>(
            pref_service_, g_browser_process->local_state());
  }

  ~IconUtilsTest() override = default;
  PrefService* pref_service() { return &pref_service_; }
  search_engines::SearchEngineChoiceService* search_engine_choice_service() {
    return search_engine_choice_service_.get();
  }

 private:
  base::test::ScopedFeatureList feature_list_{
      switches::kSearchEngineChoiceTrigger};
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  std::unique_ptr<search_engines::SearchEngineChoiceService>
      search_engine_choice_service_;
};

TEST_F(IconUtilsTest, GetSearchEngineGeneratedIconPath) {
  // Make sure the country is not forced.
  ASSERT_FALSE(base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSearchEngineChoiceCountry));

  for (int country_id : search_engines::kEeaChoiceCountriesIds) {
    pref_service()->SetInteger(country_codes::kCountryIDAtInstall, country_id);
    std::vector<std::unique_ptr<TemplateURLData>> urls =
        TemplateURLPrepopulateData::GetPrepopulatedEngines(
            pref_service(), search_engine_choice_service());
    for (const std::unique_ptr<TemplateURLData>& url : urls) {
      EXPECT_FALSE(GetSearchEngineGeneratedIconPath(url->keyword()).empty())
          << "Missing icon for " << url->keyword() << ". Try re-running "
          << "`tools/search_engine_choice/generate_search_engine_icons.py`.";
    }
  }
}
