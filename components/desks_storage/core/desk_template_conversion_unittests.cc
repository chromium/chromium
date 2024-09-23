// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/desks_storage/core/desk_template_conversion.h"

#include <string>
#include <string_view>

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
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/mojom/window_show_state.mojom.h"

namespace desks_storage {

namespace {

constexpr char kTestUuidBrowser[] = "040b6112-67f2-4d3c-8ba8-53a117272eba";
constexpr int kBrowserWindowId = 1555;
constexpr char kBrowserUrl1[] = "https://example.com/";
constexpr char kBrowserUrl2[] = "https://example.com/2";
constexpr char kBrowserTemplateName[] = "BrowserTest";
constexpr char kOverrideUrl[] = "https://example.com/";
constexpr uint64_t kTestLacrosProfileId = 12345;

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

TEST_F(DeskTemplateConversionTest, ParseAdminTemplatePolicy) {
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      std::string_view(desk_test_util::kAdminTemplatePolicy));

  EXPECT_TRUE(parsed_json.has_value());
  EXPECT_TRUE(parsed_json->is_list());

  base::Value::List& parsed_list = parsed_json->GetList();
  EXPECT_EQ(parsed_list.size(), 2UL);

  EXPECT_TRUE(parsed_list[0].is_dict());
  base::Value::Dict& value_dict_zero = parsed_list[0].GetDict();

  EXPECT_TRUE(parsed_list[1].is_dict());
  base::Value::Dict& value_dict_one = parsed_list[1].GetDict();

  std::vector<std::unique_ptr<ash::DeskTemplate>>
      templates_derived_from_policy =
          desk_template_conversion::ParseAdminTemplatesFromPolicyValue(
              parsed_json.value());

  EXPECT_EQ(templates_derived_from_policy.size(), 2UL);

  // Assert Desk Template zero is correct.
  const auto* desk_template_zero = templates_derived_from_policy[0].get();

  EXPECT_TRUE(desk_template_zero != nullptr);

  EXPECT_EQ(value_dict_zero, desk_template_zero->policy_definition());
  EXPECT_EQ(desk_template_conversion::ProtoTimeToTime(13320917261678808),
            desk_template_zero->created_time());
  EXPECT_EQ(desk_template_conversion::ProtoTimeToTime(13320917261678808),
            desk_template_zero->GetLastUpdatedTime());
  EXPECT_EQ(u"App Launch Automation 1", desk_template_zero->template_name());
  EXPECT_EQ(
      base::Uuid::ParseCaseInsensitive("27ea906b-a7d3-40b1-8c36-76d332d7f184"),
      desk_template_zero->uuid());

  const auto* restore_data_zero = desk_template_zero->desk_restore_data();
  const auto browser_restore_data_zero =
      restore_data_zero->app_id_to_launch_list().find(
          app_constants::kChromeAppId);

  EXPECT_TRUE(restore_data_zero != nullptr);
  EXPECT_EQ(restore_data_zero->app_id_to_launch_list().size(), 1UL);
  EXPECT_NE(browser_restore_data_zero,
            restore_data_zero->app_id_to_launch_list().end());

  const auto browser_restore_data_zero_window_zero_it =
      browser_restore_data_zero->second.find(3000);
  ASSERT_NE(browser_restore_data_zero_window_zero_it,
            browser_restore_data_zero->second.end());
  EXPECT_THAT(
      browser_restore_data_zero_window_zero_it->second->browser_extra_info.urls,
      testing::ElementsAre(GURL("https://www.chromium.org/")));

  const auto browser_restore_data_zero_window_one_it =
      browser_restore_data_zero->second.find(30001);
  ASSERT_NE(browser_restore_data_zero_window_one_it,
            browser_restore_data_zero->second.end());
  EXPECT_THAT(
      browser_restore_data_zero_window_one_it->second->browser_extra_info.urls,
      testing::ElementsAre(GURL("chrome://version/"),
                           GURL("https://dev.chromium.org/")));

  // Assert Desk Template one is correct.
  const auto* desk_template_one = templates_derived_from_policy[1].get();

  EXPECT_TRUE(desk_template_one != nullptr);

  EXPECT_EQ(value_dict_one, desk_template_one->policy_definition());
  EXPECT_EQ(desk_template_conversion::ProtoTimeToTime(13320917271679905),
            desk_template_one->created_time());
  EXPECT_EQ(desk_template_conversion::ProtoTimeToTime(13320917271679905),
            desk_template_one->GetLastUpdatedTime());
  EXPECT_EQ(u"App Launch Automation 2", desk_template_one->template_name());
  EXPECT_EQ(
      base::Uuid::ParseCaseInsensitive("3aa30d88-576e-48ea-ab26-cbdd2cbe43a1"),
      desk_template_one->uuid());

  const auto* restore_data_one = desk_template_one->desk_restore_data();
  const auto browser_restore_data_one =
      restore_data_one->app_id_to_launch_list().find(
          app_constants::kChromeAppId);

  EXPECT_TRUE(restore_data_one != nullptr);
  EXPECT_EQ(restore_data_one->app_id_to_launch_list().size(), 1UL);
  EXPECT_NE(browser_restore_data_one,
            restore_data_one->app_id_to_launch_list().end());

  const auto browser_restore_data_one_window_zero_it =
      browser_restore_data_one->second.find(30001);
  ASSERT_NE(browser_restore_data_one_window_zero_it,
            browser_restore_data_one->second.end());
  EXPECT_THAT(
      browser_restore_data_one_window_zero_it->second->browser_extra_info.urls,
      testing::ElementsAre(GURL("https://www.google.com/"),
                           GURL("https://www.youtube.com/")));
}

TEST_F(DeskTemplateConversionTest, AdminTemplateConvertsCorrectly) {
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      std::string_view(desk_test_util::kAdminTemplatePolicyWithOneTemplate));

  EXPECT_TRUE(parsed_json.has_value());
  EXPECT_TRUE(parsed_json->is_list());

  base::Value::List& parsed_list = parsed_json->GetList();
  EXPECT_EQ(parsed_list.size(), 1UL);

  EXPECT_TRUE(parsed_list[0].is_dict());
  base::Value::Dict& value_dict = parsed_list[0].GetDict();

  std::vector<std::unique_ptr<ash::DeskTemplate>>
      templates_derived_from_policy =
          desk_template_conversion::ParseAdminTemplatesFromPolicyValue(
              parsed_json.value());

  auto serialized_desk =
      desk_template_conversion::SerializeDeskTemplateAsBaseValue(
          templates_derived_from_policy[0].get(), GetAppsCache(account_id_));
  auto recreated_desk =
      desk_template_conversion::ParseDeskTemplateFromBaseValue(
          serialized_desk, ash::DeskTemplateSource::kPolicy);

  EXPECT_TRUE(recreated_desk.has_value());

  const auto* desk_template = recreated_desk.value().get();

  EXPECT_EQ(value_dict, desk_template->policy_definition());
  EXPECT_EQ(desk_template_conversion::ProtoTimeToTime(13320917261678808),
            desk_template->created_time());
  EXPECT_EQ(desk_template_conversion::ProtoTimeToTime(13320917261678808),
            desk_template->GetLastUpdatedTime());
  EXPECT_EQ(u"App Launch Automation 1", desk_template->template_name());
  EXPECT_EQ(
      base::Uuid::ParseCaseInsensitive("27ea906b-a7d3-40b1-8c36-76d332d7f184"),
      desk_template->uuid());

  const auto* restore_data = desk_template->desk_restore_data();
  const auto browser_restore_data =
      restore_data->app_id_to_launch_list().find(app_constants::kChromeAppId);

  EXPECT_TRUE(restore_data != nullptr);
  EXPECT_EQ(restore_data->app_id_to_launch_list().size(), 1UL);
  EXPECT_NE(browser_restore_data, restore_data->app_id_to_launch_list().end());

  const auto browser_restore_data_window_zero_it =
      browser_restore_data->second.find(3000);
  ASSERT_NE(browser_restore_data_window_zero_it,
            browser_restore_data->second.end());
  EXPECT_THAT(
      browser_restore_data_window_zero_it->second->browser_extra_info.urls,
      testing::ElementsAre(GURL("https://www.chromium.org/")));

  const auto browser_restore_data_window_one_it =
      browser_restore_data->second.find(30001);
  ASSERT_NE(browser_restore_data_window_one_it,
            browser_restore_data->second.end());
  EXPECT_THAT(
      browser_restore_data_window_one_it->second->browser_extra_info.urls,
      testing::ElementsAre(GURL("chrome://version/"),
                           GURL("https://dev.chromium.org/")));
}

TEST_F(DeskTemplateConversionTest, ParseBrowserTemplate) {
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      std::string_view(desk_test_util::kValidPolicyTemplateBrowser));

  EXPECT_TRUE(parsed_json.has_value());
  EXPECT_TRUE(parsed_json->is_dict());

  auto dt = desk_template_conversion::ParseDeskTemplateFromBaseValue(
      *parsed_json, ash::DeskTemplateSource::kPolicy);

  EXPECT_TRUE(dt.has_value());
  EXPECT_EQ(dt.value()->uuid(),
            base::Uuid::ParseCaseInsensitive(kTestUuidBrowser));
  EXPECT_EQ(dt.value()->created_time(),
            desk_template_conversion::ProtoTimeToTime(1633535632));
  EXPECT_EQ(dt.value()->template_name(),
            base::ASCIIToUTF16(std::string(kBrowserTemplateName)));

  const app_restore::RestoreData* rd = dt.value()->desk_restore_data();

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

  app_restore::BrowserExtraInfo browser_extra_info = ali->browser_extra_info;
  EXPECT_THAT(browser_extra_info.urls,
              testing::ElementsAre(kBrowserUrl1, kBrowserUrl2));
  EXPECT_THAT(browser_extra_info.active_tab_index, testing::Optional(1));
  EXPECT_THAT(browser_extra_info.first_non_pinned_tab_index,
              testing::Optional(1));
  EXPECT_THAT(browser_extra_info.tab_group_infos,
              testing::ElementsAre(MakeSampleTabGroup()));

  EXPECT_THAT(wi->window_state_type,
              testing::Optional(chromeos::WindowStateType::kNormal));
  EXPECT_THAT(wi->current_bounds, testing::Optional(gfx::Rect(0, 1, 120, 121)));
}

TEST_F(DeskTemplateConversionTest, ParseBrowserTemplateMinimized) {
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      std::string_view(desk_test_util::kValidPolicyTemplateBrowserMinimized));

  ASSERT_TRUE(parsed_json.has_value());
  EXPECT_TRUE(parsed_json->is_dict());

  auto dt = desk_template_conversion::ParseDeskTemplateFromBaseValue(
      *parsed_json, ash::DeskTemplateSource::kPolicy);

  ASSERT_TRUE(dt.has_value());
  EXPECT_EQ(dt.value()->uuid(),
            base::Uuid::ParseCaseInsensitive(kTestUuidBrowser));
  EXPECT_EQ(dt.value()->created_time(),
            desk_template_conversion::ProtoTimeToTime(1633535632));
  EXPECT_EQ(dt.value()->template_name(),
            base::ASCIIToUTF16(std::string(kBrowserTemplateName)));

  const app_restore::RestoreData* rd = dt.value()->desk_restore_data();

  ASSERT_TRUE(rd);
  EXPECT_THAT(rd->app_id_to_launch_list(),
              testing::ElementsAre(
                  testing::Pair(app_constants::kChromeAppId, testing::_)));

  const app_restore::AppRestoreData* ard =
      rd->GetAppRestoreData(app_constants::kChromeAppId, 0);
  ASSERT_TRUE(ard);
  EXPECT_THAT(ard->display_id, testing::Optional(100L));

  std::unique_ptr<app_restore::AppLaunchInfo> ali =
      ard->GetAppLaunchInfo(app_constants::kChromeAppId, 0);
  ASSERT_TRUE(ali);
  EXPECT_THAT(ali->window_id, testing::Optional(0));
  EXPECT_THAT(ali->display_id, testing::Optional(100L));

  EXPECT_THAT(ali->browser_extra_info.urls,
              testing::ElementsAre(kBrowserUrl1, kBrowserUrl2));
  EXPECT_THAT(ali->browser_extra_info.active_tab_index, testing::Optional(1));
  EXPECT_THAT(ali->browser_extra_info.first_non_pinned_tab_index,
              testing::Optional(1));
  EXPECT_THAT(ali->browser_extra_info.tab_group_infos,
              testing::ElementsAre(MakeSampleTabGroup()));

  std::unique_ptr<app_restore::WindowInfo> wi = ard->GetWindowInfo();
  ASSERT_TRUE(wi);
  EXPECT_THAT(wi->window_state_type,
              testing::Optional(chromeos::WindowStateType::kMinimized));
  EXPECT_THAT(wi->pre_minimized_show_state_type,
              testing::Optional(ui::mojom::WindowShowState::kNormal));
  EXPECT_THAT(wi->current_bounds, testing::Optional(gfx::Rect(0, 1, 120, 121)));
}

TEST_F(DeskTemplateConversionTest, ParseChromePwaTemplate) {
  auto parsed_json =
      base::JSONReader::ReadAndReturnValueWithError(std::string_view(
          desk_test_util::kValidPolicyTemplateChromeAndProgressive));

  EXPECT_TRUE(parsed_json.has_value());
  EXPECT_TRUE(parsed_json->is_dict());

  auto dt = desk_template_conversion::ParseDeskTemplateFromBaseValue(
      *parsed_json, ash::DeskTemplateSource::kPolicy);

  EXPECT_TRUE(dt.has_value());
  EXPECT_EQ(dt.value()->uuid(), base::Uuid::ParseCaseInsensitive(
                                    "7f4b7ff0-970a-41bb-aa91-f6c3e2724207"));
  EXPECT_EQ(dt.value()->created_time(),
            desk_template_conversion::ProtoTimeToTime(1633535632000LL));
  EXPECT_EQ(dt.value()->template_name(), u"ChromeAppTest");

  const app_restore::RestoreData* rd = dt.value()->desk_restore_data();

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
  EXPECT_TRUE(ali_chrome->override_url.has_value());
  EXPECT_EQ(ali_chrome->override_url.value(), kOverrideUrl);
  EXPECT_TRUE(ali_chrome->display_id.has_value());
  EXPECT_EQ(ali_chrome->display_id.value(), 100L);
  EXPECT_FALSE(ali_chrome->browser_extra_info.active_tab_index.has_value());
  EXPECT_TRUE(ali_chrome->browser_extra_info.urls.empty());

  EXPECT_TRUE(ali_pwa != nullptr);
  EXPECT_TRUE(ali_pwa->window_id.has_value());
  EXPECT_EQ(ali_pwa->window_id.value(), 1);
  EXPECT_TRUE(ali_pwa->override_url.has_value());
  EXPECT_EQ(ali_pwa->override_url.value(), kOverrideUrl);
  EXPECT_TRUE(ali_pwa->display_id.has_value());
  EXPECT_EQ(ali_pwa->display_id.value(), 100L);
  EXPECT_FALSE(ali_pwa->browser_extra_info.active_tab_index.has_value());
  EXPECT_TRUE(ali_pwa->browser_extra_info.urls.empty());

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
      base::JSONReader::ReadAndReturnValueWithError(std::string_view("{}"));

  EXPECT_TRUE(parsed_json.has_value());
  EXPECT_TRUE(parsed_json->is_dict());

  auto dt = desk_template_conversion::ParseDeskTemplateFromBaseValue(
      *parsed_json, ash::DeskTemplateSource::kPolicy);
  EXPECT_FALSE(dt.has_value());
  EXPECT_EQ(
      dt.error(),
      desk_template_conversion::SavedDeskParseError::kMissingRequiredFields);
}

TEST_F(DeskTemplateConversionTest, ParsesWithDefaultValueSetToTemplates) {
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      std::string_view(desk_test_util::kPolicyTemplateWithoutType));

  EXPECT_TRUE(parsed_json.has_value());
  EXPECT_TRUE(parsed_json->is_dict());

  auto dt = desk_template_conversion::ParseDeskTemplateFromBaseValue(
      *parsed_json, ash::DeskTemplateSource::kPolicy);
  EXPECT_TRUE(dt.has_value());
  EXPECT_EQ(ash::DeskTemplateType::kTemplate, dt.value()->type());
}

TEST_F(DeskTemplateConversionTest, DeskTemplateFromJsonBrowserTest) {
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      std::string_view(desk_test_util::kValidPolicyTemplateBrowser));

  EXPECT_TRUE(parsed_json.has_value());
  EXPECT_TRUE(parsed_json->is_dict());

  auto desk_template = desk_template_conversion::ParseDeskTemplateFromBaseValue(
      *parsed_json, ash::DeskTemplateSource::kPolicy);

  EXPECT_TRUE(desk_template.has_value());

  base::Value desk_template_value =
      desk_template_conversion::SerializeDeskTemplateAsBaseValue(
          desk_template.value().get(), GetAppsCache(account_id_));
  EXPECT_EQ(*parsed_json, desk_template_value);
}

TEST_F(DeskTemplateConversionTest, ToJsonIgnoreUnsupportedApp) {
  constexpr int32_t kTestWindowId = 1234567;
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      std::string_view(desk_test_util::kValidPolicyTemplateBrowser));

  EXPECT_TRUE(parsed_json.has_value());
  EXPECT_TRUE(parsed_json->is_dict());

  auto desk_template = desk_template_conversion::ParseDeskTemplateFromBaseValue(
      *parsed_json, ash::DeskTemplateSource::kUser);

  // Adding this unsupported app should not change the serialized JSON content.
  saved_desk_test_util::AddGenericAppWindow(
      kTestWindowId, desk_test_util::kTestUnsupportedAppId,
      desk_template.value()->mutable_desk_restore_data());

  base::Value desk_template_value =
      desk_template_conversion::SerializeDeskTemplateAsBaseValue(
          desk_template.value().get(), GetAppsCache(account_id_));

  EXPECT_EQ(*parsed_json, desk_template_value);
}

TEST_F(DeskTemplateConversionTest, DeskTemplateFromJsonAppTest) {
  auto parsed_json =
      base::JSONReader::ReadAndReturnValueWithError(std::string_view(
          desk_test_util::kValidPolicyTemplateChromeAndProgressive));

  EXPECT_TRUE(parsed_json.has_value());
  EXPECT_TRUE(parsed_json->is_dict());

  auto desk_template = desk_template_conversion::ParseDeskTemplateFromBaseValue(
      *parsed_json, ash::DeskTemplateSource::kPolicy);

  EXPECT_TRUE(desk_template.has_value());

  base::Value desk_template_value =
      desk_template_conversion::SerializeDeskTemplateAsBaseValue(
          desk_template.value().get(), GetAppsCache(account_id_));

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
          .SetLacrosProfileId(kTestLacrosProfileId)
          .AddAppWindow(
              SavedDeskBrowserBuilder()
                  .SetGenericBuilder(SavedDeskGenericAppBuilder().SetWindowId(
                      kBrowserWindowId))
                  .SetLacrosProfileId(kTestLacrosProfileId)
                  .SetUrls({GURL(kBrowserUrl1), GURL(kBrowserUrl2)})
                  .Build())
          .Build();

  base::Value desk_template_value =
      desk_template_conversion::SerializeDeskTemplateAsBaseValue(
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
  expected_browser_app_value.Set("lacros_profile_id",
                                 base::NumberToString(kTestLacrosProfileId));
  expected_browser_app_value.Set("app_id", app_constants::kChromeAppId);

  base::Value::List expected_app_list;
  expected_app_list.Append(std::move(expected_browser_app_value));

  base::Value::Dict expected_desk_value;
  expected_desk_value.Set("apps", std::move(expected_app_list));

  base::Value::Dict expected_value;
  expected_value.Set("auto_launch_on_startup", false);
  expected_value.Set("version", base::Value(1));
  expected_value.Set("uuid", base::Value(kTestUuidBrowser));
  expected_value.Set("name", base::Value(kBrowserTemplateName));
  expected_value.Set("created_time_usec", base::TimeToValue(created_time));
  expected_value.Set("updated_time_usec",
                     base::TimeToValue(desk_template->GetLastUpdatedTime()));
  expected_value.Set("desk_type", base::Value("SAVE_AND_RECALL"));
  expected_value.Set("desk", std::move(expected_desk_value));
  expected_value.Set("lacros_profile_id",
                     base::NumberToString(kTestLacrosProfileId));

  EXPECT_EQ(expected_value, desk_template_value);
}

TEST_F(DeskTemplateConversionTest,
       DeskTemplateFromFloatingWorkspaceJsonAppTest) {
  base::expected<base::Value, base::JSONReader::Error> parsed_json =
      base::JSONReader::ReadAndReturnValueWithError(std::string_view(
          desk_test_util::kValidPolicyTemplateChromeForFloatingWorkspace));

  ASSERT_TRUE(parsed_json.has_value());
  ASSERT_TRUE(parsed_json->is_dict());

  auto desk_template = desk_template_conversion::ParseDeskTemplateFromBaseValue(
      *parsed_json, ash::DeskTemplateSource::kPolicy);

  EXPECT_TRUE(desk_template.has_value());

  base::Value desk_template_value =
      desk_template_conversion::SerializeDeskTemplateAsBaseValue(
          desk_template.value().get(), GetAppsCache(account_id_));

  EXPECT_EQ(*parsed_json, desk_template_value);
}

}  // namespace desks_storage
