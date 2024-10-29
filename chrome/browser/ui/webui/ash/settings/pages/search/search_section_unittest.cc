// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/search/search_section.h"

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/components/magic_boost/test/fake_magic_boost_state.h"
#include "content/public/test/test_web_ui_data_source.h"

namespace ash::settings {

// Test for the search settings page.
class SearchSectionTest : public ChromeAshTestBase {
 public:
  SearchSectionTest()
      : local_search_service_proxy_(
            std::make_unique<
                ash::local_search_service::LocalSearchServiceProxy>(
                /*for_testing=*/true)),
        search_tag_registry_(local_search_service_proxy_.get()) {}

  ~SearchSectionTest() override = default;

  TestingProfile* profile() { return &profile_; }
  ash::settings::SearchTagRegistry* search_tag_registry() {
    return &search_tag_registry_;
  }
  std::unique_ptr<SearchSection> search_section_;

 protected:
  void SetUp() override { ChromeAshTestBase::SetUp(); }
  void TearDown() override {
    search_section_.reset();
    ChromeAshTestBase::TearDown();
  }

 private:
  std::unique_ptr<ash::local_search_service::LocalSearchServiceProxy>
      local_search_service_proxy_;
  ash::settings::SearchTagRegistry search_tag_registry_;
  TestingProfile profile_;
};

TEST_F(SearchSectionTest,
       SearchSectionDoesNotIncludesLobsterSettingsByDefault) {
  search_section_ =
      std::make_unique<SearchSection>(profile(), search_tag_registry());
  std::unique_ptr<content::TestWebUIDataSource> html_source =
      content::TestWebUIDataSource::Create("test-search-section");

  chromeos::test::FakeMagicBoostState magic_boost_state;

  search_section_->AddLoadTimeData(html_source->GetWebUIDataSource());

  EXPECT_FALSE(html_source->GetLocalizedStrings()
                   ->FindBool("isLobsterSettingsToggleVisible")
                   .value());
}

}  // namespace ash::settings
