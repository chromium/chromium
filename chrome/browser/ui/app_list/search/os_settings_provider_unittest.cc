// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/search/os_settings_provider.h"

#include <memory>
#include "chrome/browser/ui/app_list/search/test/test_search_controller.h"
#include "chrome/browser/ui/webui/settings/ash/fake_hierarchy.h"
#include "chrome/browser/ui/webui/settings/ash/fake_os_settings_sections.h"
#include "chrome/browser/ui/webui/settings/ash/search/search_handler.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list::test {

class OsSettingsProviderTest : public testing::Test {
 public:
  OsSettingsProviderTest()
      : local_search_service_proxy_(
            std::make_unique<
                ash::local_search_service::LocalSearchServiceProxy>(
                /*for_testing=*/true)),
        search_tag_registry_(local_search_service_proxy_.get()),
        fake_hierarchy_(&fake_sections_),
        handler_(&search_tag_registry_,
                 &fake_sections_,
                 &fake_hierarchy_,
                 local_search_service_proxy_.get()) {}
  OsSettingsProviderTest(const OsSettingsProviderTest&) = delete;
  OsSettingsProviderTest& operator=(const OsSettingsProviderTest&) = delete;
  ~OsSettingsProviderTest() override = default;

  void SetUp() override {
    handler_.BindInterface(handler_remote_.BindNewPipeAndPassReceiver());

    search_controller_ = std::make_unique<TestSearchController>();
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("name");
    provider_ = std::make_unique<OsSettingsProvider>(profile_, &handler_,
                                                     &fake_hierarchy_, nullptr);
    provider_->set_controller(search_controller_.get());
    task_environment_.RunUntilIdle();
  }

  void TearDown() override {
    provider_.reset();
    search_controller_.reset();
    profile_ = nullptr;
    profile_manager_->DeleteTestingProfile("name");
  }

  const SearchProvider::Results& results() { return provider_->results(); }

  // Starts a search and waits for the query to be sent
  void StartSearch(const std::u16string& query) {
    provider_->Start(query);
    base::RunLoop().RunUntilIdle();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<ash::local_search_service::LocalSearchServiceProxy>
      local_search_service_proxy_;
  std::unique_ptr<TestSearchController> search_controller_;
  ash::settings::SearchTagRegistry search_tag_registry_;
  ash::settings::FakeOsSettingsSections fake_sections_;
  ash::settings::FakeHierarchy fake_hierarchy_;
  ash::settings::SearchHandler handler_;
  mojo::Remote<ash::settings::mojom::SearchHandler> handler_remote_;

 private:
  std::unique_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* profile_;
  std::unique_ptr<OsSettingsProvider> provider_;
};

TEST_F(OsSettingsProviderTest, Basic) {
  StartSearch(u"");
  EXPECT_TRUE(results().empty());
}
}  // namespace app_list::test
