// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/settings/pages/multidevice/multidevice_section.h"

#include <string_view>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/ash/eche_app/eche_app_manager_factory.h"
#include "chrome/browser/ui/webui/ash/settings/search/search_tag_registry.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy.h"
#include "chromeos/ash/components/multidevice/logging/logging.h"
#include "chromeos/ash/components/phonehub/fake_phone_hub_manager.h"
#include "chromeos/ash/components/phonehub/pref_names.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::settings {

class MockWebUIDataSource : public content::WebUIDataSource {
 public:
  MockWebUIDataSource() = default;
  ~MockWebUIDataSource() override = default;
  MockWebUIDataSource(const MockWebUIDataSource&) = delete;
  MockWebUIDataSource& operator=(const MockWebUIDataSource&) = delete;

  // WebUIDataSource:
  MOCK_METHOD(void,
              AddBoolean,
              (std::string_view name, bool value),
              (override));
  void AddString(std::string_view name, std::u16string_view value) override {}
  void AddString(std::string_view name, std::string_view value) override {}
  void AddLocalizedString(std::string_view name, int ids) override {}
  void AddLocalizedStrings(
      base::span<const webui::LocalizedString> strings) override {}
  void AddLocalizedStrings(
      const base::Value::Dict& localized_strings) override {}
  void AddInteger(std::string_view name, int32_t value) override {}
  void AddDouble(std::string_view name, double value) override {}
  void UseStringsJs() override {}
  void AddResourcePath(std::string_view path, int resource_id) override {}
  void AddResourcePaths(base::span<const webui::ResourcePath> paths) override {}
  void SetDefaultResource(int resource_id) override {}
  void SetRequestFilter(const WebUIDataSource::ShouldHandleRequestCallback&
                            should_handle_request_callback,
                        const WebUIDataSource::HandleRequestCallback&
                            handle_request_callback) override {}
  void OverrideContentSecurityPolicy(network::mojom::CSPDirectiveName directive,
                                     const std::string& value) override {}
  void OverrideCrossOriginOpenerPolicy(const std::string& value) override {}
  void OverrideCrossOriginEmbedderPolicy(const std::string& value) override {}
  void OverrideCrossOriginResourcePolicy(const std::string& value) override {}
  void DisableTrustedTypesCSP() override {}
  void DisableDenyXFrameOptions() override {}
  void EnableReplaceI18nInJS() override {}
  std::string GetSource() override { return ""; }
  std::string GetScheme() override { return ""; }
  void AddFrameAncestor(const GURL& frame_ancestor) override {}
  void SetSupportedScheme(std::string_view scheme) override {}
};

class MultiDeviceSectionTest : public testing::Test {
 protected:
  MultiDeviceSectionTest()
      : profile_manager_(TestingBrowserProcess::GetGlobal()) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kEcheSWA},
        /*disabled_features=*/{});
  }
  MultiDeviceSectionTest(const MultiDeviceSectionTest&) = delete;
  MultiDeviceSectionTest& operator=(const MultiDeviceSectionTest&) = delete;
  ~MultiDeviceSectionTest() override = default;

  // testing::Test:
  void SetUp() override {
    ASSERT_TRUE(profile_manager_.SetUp());
    pref_service_.registry()->RegisterBooleanPref(prefs::kEnableAutoScreenLock,
                                                  false);
    pref_service_.registry()->RegisterIntegerPref(
        phonehub::prefs::kScreenLockStatus,
        static_cast<int>(phonehub::ScreenLockManager::LockStatus::kLockedOn));
    mock_web_ui_data_source_ = std::make_unique<MockWebUIDataSource>();
    service_proxy_ =
        std::make_unique<local_search_service::LocalSearchServiceProxy>(
            /*for_testing=*/true);
    search_tag_registry_ =
        std::make_unique<SearchTagRegistry>(service_proxy_.get());
    TestingProfile* profile =
        profile_manager_.CreateTestingProfile("TestingProfile");
    fake_multidevice_setup_client_ =
        std::make_unique<multidevice_setup::FakeMultiDeviceSetupClient>();
    fake_phone_hub_manager_ = std::make_unique<phonehub::FakePhoneHubManager>();
    multi_device_section_ = std::make_unique<MultiDeviceSection>(
        profile, search_tag_registry_.get(),
        fake_multidevice_setup_client_.get(), fake_phone_hub_manager_.get(),
        &pref_service_,
        eche_app::EcheAppManagerFactory::GetForProfile(profile));
  }

  void VerifyOnEnableScreenLockChangedIsCalled() {
    EXPECT_CALL(*mock_web_ui_data_source_, AddBoolean(testing::_, testing::_))
        .Times(testing::AnyNumber());
    multi_device_section_->AddLoadTimeData(mock_web_ui_data_source_.get());

    EXPECT_CALL(
        *mock_web_ui_data_source_,
        AddBoolean(testing::Eq("isChromeosScreenLockEnabled"), testing::_))
        .Times(1);
    pref_service_.SetBoolean(prefs::kEnableAutoScreenLock, true);
  }

  void VerifyOnScreenLockStatusChangedIsCalled() {
    EXPECT_CALL(*mock_web_ui_data_source_, AddBoolean(testing::_, testing::_))
        .Times(testing::AnyNumber());
    multi_device_section_->AddLoadTimeData(mock_web_ui_data_source_.get());

    EXPECT_CALL(*mock_web_ui_data_source_,
                AddBoolean(testing::Eq("isPhoneScreenLockEnabled"), testing::_))
        .Times(1);
    pref_service_.SetInteger(
        phonehub::prefs::kScreenLockStatus,
        static_cast<int>(phonehub::ScreenLockManager::LockStatus::kLockedOn));
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  TestingProfileManager profile_manager_;
  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<MockWebUIDataSource> mock_web_ui_data_source_;
  std::unique_ptr<local_search_service::LocalSearchServiceProxy> service_proxy_;
  std::unique_ptr<SearchTagRegistry> search_tag_registry_;
  std::unique_ptr<multidevice_setup::FakeMultiDeviceSetupClient>
      fake_multidevice_setup_client_;
  std::unique_ptr<phonehub::FakePhoneHubManager> fake_phone_hub_manager_;
  std::unique_ptr<MultiDeviceSection> multi_device_section_;
};

TEST_F(MultiDeviceSectionTest, OnEnableScreenLockChanged) {
  VerifyOnEnableScreenLockChangedIsCalled();
}

TEST_F(MultiDeviceSectionTest, OnScreenLockStatusChanged) {
  VerifyOnScreenLockStatusChangedIsCalled();
}

}  // namespace ash::settings
