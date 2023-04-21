// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/desk_template_conversion.h"

#include <string>

#include "ash/public/cpp/desk_template.h"
#include "base/json/json_reader.h"
#include "base/json/values_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/account_id/account_id.h"
#include "components/app_constants/constants.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/app_restore_data.h"
#include "components/app_restore/window_info.h"
#include "components/desks_storage/core/desk_test_util.h"
#include "components/desks_storage/core/saved_desk_builder.h"
#include "components/desks_storage/core/saved_desk_test_util.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_registry_cache_wrapper.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/features.h"
#include "components/tab_groups/tab_group_info.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace desks_storage {

namespace {

constexpr char kTestUuidBrowser[] = "040b6112-67f2-4d3c-8ba8-53a117272eba";
constexpr int kBrowserWindowId = 1555;
constexpr char kBrowserUrl1[] = "https://example.com/";
constexpr char kBrowserUrl2[] = "https://example.com/2";
constexpr char kBrowserTemplateName[] = "BrowserTest";

tab_groups::TabGroupInfo MakeSampleTabGroup() {
  return tab_groups::TabGroupInfo(
      {1, 2}, tab_groups::TabGroupVisualData(
                  u"sample_tab_group", tab_groups::TabGroupColorId::kGrey));
}

apps::AppRegistryCache* GetAppsCache(AccountId& account_id) {
  return apps::AppRegistryCacheWrapper::Get().GetAppRegistryCache(account_id);
}

}  // namespace

class DeskTemplateConversionTest : public testing::Test {
 public:
  DeskTemplateConversionTest(const DeskTemplateConversionTest&) = delete;
  DeskTemplateConversionTest& operator=(const DeskTemplateConversionTest&) =
      delete;

 protected:
  DeskTemplateConversionTest()
      : account_id_(AccountId::FromUserEmail("test@gmail.com")),
        cache_(std::make_unique<apps::AppRegistryCache>()) {}

  void SetUp() override {
    desk_test_util::PopulateAppRegistryCache(account_id_, cache_.get());
  }

  AccountId account_id_;

 private:
  std::unique_ptr<apps::AppRegistryCache> cache_;
};

TEST_F(DeskTemplateConversionTest, ParseBrowserTemplate) {
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      base::StringPiece(desk_test_util::kValidPolicyTemplateBrowser));

  EXPECT_TRUE(parsed_json.has_value());
  EXPECT_TRUE(parsed_json->is_dict());

  std::unique_ptr<ash::DeskTemplate> dt =
      desk_template_conversion::ParseDeskTemplateFromSource(
          *parsed_json, ash::DeskTemplateSource::kPolicy);

  EXPECT_TRUE(dt != nullptr);
  EXPECT_EQ(dt->uuid(), base::Uuid::ParseCaseInsensitive(kTestUuidBrowser));
  EXPECT_EQ(dt->created_time(),
            desk_template_conversion::ProtoTimeToTime(1633535632));
  EXPECT_EQ(dt->template_name(),
            base::ASCIIToUTF16(std::string(kBrowserTemplateName)));

  const app_restore::RestoreData* rd = dt->desk_restore_data();

  EXPECT_TRUE(rd != nullptr);
  EXPECT_EQ(rd->app_id_to_launch_list().size(), 1UL);
  EXPECT_NE(rd->app_id_to_launch_list().find(app_constants::kChromeAppId),
            rd->app_id_to_launch_list().end());

  const app_restore::AppRestoreData* ard =
      rd->GetAppRestoreData(app_constants::kChromeAppId, 0);
  EXPECT_TRUE(ard != nullptr);
  EXPECT_TRUE(ard->display_id.has_value());
  EXPECT_EQ(ard->display_id.value(), 100L);
  std::unique_ptr<app_restore::AppLaunchInfo> ali =
      ard->GetAppLaunchInfo(app_constants::kChromeAppId, 0);
  std::unique_ptr<app_restore::WindowInfo> wi = ard->GetWindowInfo();
  EXPECT_TRUE(ali != nullptr);
  EXPECT_TRUE(wi != nullptr);
  EXPECT_TRUE(ali->window_id.has_value());
  EXPECT_EQ(ali->window_id.value(), 0);
  EXPECT_TRUE(ali->display_id.has_value());
  EXPECT_EQ(ali->display_id.value(), 100L);
  EXPECT_TRUE(ali->active_tab_index.has_value());
  EXPECT_EQ(ali->active_tab_index.value(), 1);
  EXPECT_TRUE(ali->first_non_pinned_tab_index.has_value());
  EXPECT_EQ(ali->first_non_pinned_tab_index.value(), 1);
  EXPECT_FALSE(ali->urls.empty());
  EXPECT_EQ(ali->urls[0].spec(), kBrowserUrl1);
  EXPECT_EQ(ali->urls[1].spec(), kBrowserUrl2);
  EXPECT_FALSE(ali->tab_group_infos.empty());
  EXPECT_EQ(ali->tab_group_infos[0], MakeSampleTabGroup());
  EXPECT_TRUE(wi->window_state_type.has_value());
  EXPECT_EQ(wi->window_state_type.value(), chromeos::WindowStateType::kNormal);
  EXPECT_TRUE(wi->current_bounds.has_value());
  EXPECT_EQ(wi->current_bounds.value().x(), 0);
  EXPECT_EQ(wi->current_bounds.value().y(), 1);
  EXPECT_EQ(wi->current_bounds.value().height(), 121);
  EXPECT_EQ(wi->current_bounds.value().width(), 120);
}

TEST_F(DeskTemplateConversionTest, ParseBrowserTemplateMinimized) {
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      base::StringPiece(desk_test_util::kValidPolicyTemplateBrowserMinimized));

  EXPECT_TRUE(parsed_json.has_value());
  EXPECT_TRUE(parsed_json->is_dict());

  std::unique_ptr<ash::DeskTemplate> dt =
      desk_template_conversion::ParseDeskTemplateFromSource(
          *parsed_json, ash::DeskTemplateSource::kPolicy);

  EXPECT_TRUE(dt != nullptr);
  EXPECT_EQ(dt->uuid(), base::Uuid::ParseCaseInsensitive(kTestUuidBrowser));
  EXPECT_EQ(dt->created_time(),
            desk_template_conversion::ProtoTimeToTime(1633535632));
  EXPECT_EQ(dt->template_name(),
            base::ASCIIToUTF16(std::string(kBrowserTemplateName)));

  const app_restore::RestoreData* rd = dt->desk_restore_data();

  EXPECT_TRUE(rd != nullptr);
  EXPECT_EQ(rd->app_id_to_launch_list().size(), 1UL);
  EXPECT_NE(rd->app_id_to_launch_list().find(app_constants::kChromeAppId),
            rd->app_id_to_launch_list().end());

  const app_restore::AppRestoreData* ard =
      rd->GetAppRestoreData(app_constants::kChromeAppId, 0);
  EXPECT_TRUE(ard != nullptr);
  EXPECT_TRUE(ard->display_id.has_value());
  EXPECT_EQ(ard->display_id.value(), 100L);
  std::unique_ptr<app_restore::AppLaunchInfo> ali =
      ard->GetAppLaunchInfo(app_constants::kChromeAppId, 0);
  std::unique_ptr<app_restore::WindowInfo> wi = ard->GetWindowInfo();
  EXPECT_TRUE(ali != nullptr);
  EXPECT_TRUE(wi != nullptr);
  EXPECT_TRUE(ali->window_id.has_value());
  EXPECT_EQ(ali->window_id.value(), 0);
  EXPECT_TRUE(ali->display_id.has_value());
  EXPECT_EQ(ali->display_id.value(), 100L);
  EXPECT_TRUE(ali->active_tab_index.has_value());
  EXPECT_EQ(ali->active_tab_index.value(), 1);
  EXPECT_TRUE(ali->first_non_pinned_tab_index.has_value());
  EXPECT_EQ(ali->first_non_pinned_tab_index.value(), 1);
  EXPECT_FALSE(ali->urls.empty());
  EXPECT_EQ(ali->urls[0].spec(), kBrowserUrl1);
  EXPECT_EQ(ali->urls[1].spec(), kBrowserUrl2);
  EXPECT_FALSE(ali->tab_group_infos.empty());
  EXPECT_EQ(ali->tab_group_infos[0], MakeSampleTabGroup());
  EXPECT_TRUE(wi->window_state_type.has_value());
  EXPECT_EQ(wi->window_state_type.value(),
            chromeos::WindowStateType::kMinimized);
  EXPECT_TRUE(wi->pre_minimized_show_state_type.has_value());
  EXPECT_EQ(wi->pre_minimized_show_state_type.value(),
            ui::WindowShowState::SHOW_STATE_NORMAL);
  EXPECT_TRUE(wi->current_bounds.has_value());
  EXPECT_EQ(wi->current_bounds.value().x(), 0);
  EXPECT_EQ(wi->current_bounds.value().y(), 1);
  EXPECT_EQ(wi->current_bounds.value().height(), 121);
  EXPECT_EQ(wi->current_bounds.value().width(), 120);
}

TEST_F(DeskTemplateConversionTest, ParseChromePwaTemplate) {
  auto parsed_json =
      base::JSONReader::ReadAndReturnValueWithError(base::StringPiece(
          desk_test_util::kValidPolicyTemplateChromeAndProgressive));

  EXPECT_TRUE(parsed_json.has_value());
  EXPECT_TRUE(parsed_json->is_dict());

  std::unique_ptr<ash::DeskTemplate> dt =
      desk_template_conversion::ParseDeskTemplateFromSource(
          *parsed_json, ash::DeskTemplateSource::kPolicy);

  EXPECT_TRUE(dt != nullptr);
  EXPECT_EQ(dt->uuid(), base::Uuid::ParseCaseInsensitive(
                            "7f4b7ff0-970a-41bb-aa91-f6c3e2724207"));
  EXPECT_EQ(dt->created_time(),
            desk_template_conversion::ProtoTimeToTime(1633535632000LL));
  EXPECT_EQ(dt->template_name(), u"ChromeAppTest");

  const app_restore::RestoreData* rd = dt->desk_restore_data();

  EXPECT_TRUE(rd != nullptr);
  EXPECT_EQ(rd->app_id_to_launch_list().size(), 2UL);
  EXPECT_NE(rd->app_id_to_launch_list().find(desk_test_util::kTestChromeAppId1),
            rd->app_id_to_launch_list().end());
  EXPECT_NE(rd->app_id_to_launch_list().find(desk_test_util::kTestPwaAppId1),
            rd->app_id_to_launch_list().end());

  const app_restore::AppRestoreData* ard_chrome =
      rd->GetAppRestoreData(desk_test_util::kTestChromeAppId1, 0);
  const app_restore::AppRestoreData* ard_pwa =
      rd->GetAppRestoreData(desk_test_util::kTestPwaAppId1, 1);
  EXPECT_TRUE(ard_chrome != nullptr);
  EXPECT_TRUE(ard_pwa != nullptr);
  std::unique_ptr<app_restore::AppLaunchInfo> ali_chrome =
      ard_chrome->GetAppLaunchInfo(desk_test_util::kTestChromeAppId1, 0);
  std::unique_ptr<app_restore::AppLaunchInfo> ali_pwa =
      ard_pwa->GetAppLaunchInfo(desk_test_util::kTestPwaAppId1, 1);
  std::unique_ptr<app_restore::WindowInfo> wi_chrome =
      ard_chrome->GetWindowInfo();
  std::unique_ptr<app_restore::WindowInfo> wi_pwa = ard_pwa->GetWindowInfo();

  EXPECT_TRUE(ali_chrome != nullptr);
  EXPECT_TRUE(ali_chrome->window_id.has_value());
  EXPECT_EQ(ali_chrome->window_id.value(), 0);
  EXPECT_TRUE(ali_chrome->display_id.has_value());
  EXPECT_EQ(ali_chrome->display_id.value(), 100L);
  EXPECT_FALSE(ali_chrome->active_tab_index.has_value());
  EXPECT_TRUE(ali_chrome->urls.empty());

  EXPECT_TRUE(ali_pwa != nullptr);

  EXPECT_TRUE(ali_pwa != nullptr);
  EXPECT_TRUE(ali_pwa->window_id.has_value());
  EXPECT_EQ(ali_pwa->window_id.value(), 1);
  EXPECT_TRUE(ali_pwa->display_id.has_value());
  EXPECT_EQ(ali_pwa->display_id.value(), 100L);
  EXPECT_FALSE(ali_pwa->active_tab_index.has_value());
  EXPECT_TRUE(ali_pwa->urls.empty());

  EXPECT_TRUE(wi_chrome != nullptr);
  EXPECT_TRUE(wi_chrome->window_state_type.has_value());
  EXPECT_EQ(wi_chrome->window_state_type.value(),
            chromeos::WindowStateType::kPrimarySnapped);
  EXPECT_TRUE(wi_chrome->current_bounds.has_value());
  EXPECT_EQ(wi_chrome->current_bounds.value().x(), 200);
  EXPECT_EQ(wi_chrome->current_bounds.value().y(), 200);
  EXPECT_EQ(wi_chrome->current_bounds.value().height(), 1000);
  EXPECT_EQ(wi_chrome->current_bounds.value().width(), 1000);

  EXPECT_TRUE(wi_pwa != nullptr);
  EXPECT_TRUE(wi_pwa->window_state_type.has_value());
  EXPECT_EQ(wi_pwa->window_state_type.value(),
            chromeos::WindowStateType::kNormal);
  EXPECT_TRUE(wi_pwa->current_bounds.has_value());
  EXPECT_EQ(wi_pwa->current_bounds.value().x(), 0);
  EXPECT_EQ(wi_pwa->current_bounds.value().y(), 0);
  EXPECT_EQ(wi_pwa->current_bounds.value().height(), 120);
  EXPECT_EQ(wi_pwa->current_bounds.value().width(), 120);
}

TEST_F(DeskTemplateConversionTest, EmptyJsonTest) {
  auto parsed_json =
      base::JSONReader::ReadAndReturnValueWithError(base::StringPiece("{}"));

  EXPECT_TRUE(parsed_json.has_value());
  EXPECT_TRUE(parsed_json->is_dict());

  std::unique_ptr<ash::DeskTemplate> dt =
      desk_template_conversion::ParseDeskTemplateFromSource(
          *parsed_json, ash::DeskTemplateSource::kPolicy);
  EXPECT_TRUE(dt == nullptr);
}

TEST_F(DeskTemplateConversionTest, ParsesWithDefaultValueSetToTemplates) {
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      base::StringPiece(desk_test_util::kPolicyTemplateWithoutType));

  EXPECT_TRUE(parsed_json.has_value());
  EXPECT_TRUE(parsed_json->is_dict());

  std::unique_ptr<ash::DeskTemplate> dt =
      desk_template_conversion::ParseDeskTemplateFromSource(
          *parsed_json, ash::DeskTemplateSource::kPolicy);
  EXPECT_TRUE(dt);
  EXPECT_EQ(ash::DeskTemplateType::kTemplate, dt->type());
}

TEST_F(DeskTemplateConversionTest, DeskTemplateFromJsonBrowserTest) {
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      base::StringPiece(desk_test_util::kValidPolicyTemplateBrowser));

  EXPECT_TRUE(parsed_json.has_value());
  EXPECT_TRUE(parsed_json->is_dict());

  std::unique_ptr<ash::DeskTemplate> desk_template =
      desk_template_conversion::ParseDeskTemplateFromSource(
          *parsed_json, ash::DeskTemplateSource::kPolicy);

  base::Value desk_template_value =
      desk_template_conversion::SerializeDeskTemplateAsPolicy(
          desk_template.get(), GetAppsCache(account_id_));
  EXPECT_EQ(*parsed_json, desk_template_value);
}

TEST_F(DeskTemplateConversionTest, ToJsonIgnoreUnsupportedApp) {
  constexpr int32_t kTestWindowId = 1234567;
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      base::StringPiece(desk_test_util::kValidPolicyTemplateBrowser));

  EXPECT_TRUE(parsed_json.has_value());
  EXPECT_TRUE(parsed_json->is_dict());

  std::unique_ptr<ash::DeskTemplate> desk_template =
      desk_template_conversion::ParseDeskTemplateFromSource(
          *parsed_json, ash::DeskTemplateSource::kUser);

  // Adding this unsupported app should not change the serialized JSON content.
  saved_desk_test_util::AddGenericAppWindow(
      kTestWindowId, desk_test_util::kTestUnsupportedAppId,
      desk_template->mutable_desk_restore_data());

  base::Value desk_template_value =
      desk_template_conversion::SerializeDeskTemplateAsPolicy(
          desk_template.get(), GetAppsCache(account_id_));

  EXPECT_EQ(*parsed_json, desk_template_value);
}

TEST_F(DeskTemplateConversionTest, DeskTemplateFromJsonAppTest) {
  auto parsed_json =
      base::JSONReader::ReadAndReturnValueWithError(base::StringPiece(
          desk_test_util::kValidPolicyTemplateChromeAndProgressive));

  EXPECT_TRUE(parsed_json.has_value());
  EXPECT_TRUE(parsed_json->is_dict());

  std::unique_ptr<ash::DeskTemplate> desk_template =
      desk_template_conversion::ParseDeskTemplateFromSource(
          *parsed_json, ash::DeskTemplateSource::kPolicy);

  base::Value desk_template_value =
      desk_template_conversion::SerializeDeskTemplateAsPolicy(
          desk_template.get(), GetAppsCache(account_id_));

  EXPECT_EQ(*parsed_json, desk_template_value);
}

TEST_F(DeskTemplateConversionTest, EnsureLacrosBrowserWindowsSavedProperly) {
  base::Time created_time = base::Time::Now();
  std::unique_ptr<ash::DeskTemplate> desk_template =
      SavedDeskBuilder()
          .SetUuid(kTestUuidBrowser)
          .SetName(kBrowserTemplateName)
          .SetType(ash::DeskTemplateType::kSaveAndRecall)
          .SetCreatedTime(created_time)
          .AddAppWindow(
              SavedDeskBrowserBuilder()
                  .SetGenericBuilder(SavedDeskGenericAppBuilder().SetWindowId(
                      kBrowserWindowId))
                  .SetUrls({GURL(kBrowserUrl1), GURL(kBrowserUrl2)})
                  .Build())
          .Build();

  base::Value desk_template_value =
      desk_template_conversion::SerializeDeskTemplateAsPolicy(
          desk_template.get(), GetAppsCache(account_id_));

  base::Value::Dict expected_browser_tab1;
  expected_browser_tab1.Set("url", base::Value(kBrowserUrl1));
  base::Value::Dict expected_browser_tab2;
  expected_browser_tab2.Set("url", base::Value(kBrowserUrl2));
  base::Value::List expected_tab_list;
  expected_tab_list.Append(std::move(expected_browser_tab1));
  expected_tab_list.Append(std::move(expected_browser_tab2));

  base::Value::Dict expected_browser_app_value;
  expected_browser_app_value.Set("app_type", base::Value("BROWSER"));
  expected_browser_app_value.Set("event_flag", base::Value(0));
  expected_browser_app_value.Set("window_id", base::Value(kBrowserWindowId));
  expected_browser_app_value.Set("tabs", std::move(expected_tab_list));

  base::Value::List expected_app_list;
  expected_app_list.Append(std::move(expected_browser_app_value));

  base::Value::Dict expected_desk_value;
  expected_desk_value.Set("apps", std::move(expected_app_list));

  base::Value::Dict expected_value;
  expected_value.Set("version", base::Value(1));
  expected_value.Set("uuid", base::Value(kTestUuidBrowser));
  expected_value.Set("name", base::Value(kBrowserTemplateName));
  expected_value.Set("created_time_usec", base::TimeToValue(created_time));
  expected_value.Set("updated_time_usec",
                     base::TimeToValue(desk_template->GetLastUpdatedTime()));
  expected_value.Set("desk_type", base::Value("SAVE_AND_RECALL"));
  expected_value.Set("desk", std::move(expected_desk_value));

  EXPECT_EQ(expected_value, desk_template_value);
}

TEST_F(DeskTemplateConversionTest,
       DeskTemplateFromFloatingWorkspaceJsonAppTest) {
  base::expected<base::Value, base::JSONReader::Error> parsed_json =
      base::JSONReader::ReadAndReturnValueWithError(base::StringPiece(
          desk_test_util::kValidPolicyTemplateChromeForFloatingWorkspace));

  ASSERT_TRUE(parsed_json.has_value());
  ASSERT_TRUE(parsed_json->is_dict());

  std::unique_ptr<ash::DeskTemplate> desk_template =
      desk_template_conversion::ParseDeskTemplateFromSource(
          *parsed_json, ash::DeskTemplateSource::kPolicy);

  base::Value desk_template_value =
      desk_template_conversion::SerializeDeskTemplateAsPolicy(
          desk_template.get(), GetAppsCache(account_id_));

  EXPECT_EQ(*parsed_json, desk_template_value);
}

}  // namespace desks_storage
