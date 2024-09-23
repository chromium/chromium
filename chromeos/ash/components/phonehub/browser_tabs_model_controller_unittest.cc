// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/browser_tabs_model_controller.h"

#include "chromeos/ash/components/phonehub/fake_browser_tabs_model_provider.h"
#include "chromeos/ash/components/phonehub/mutable_phone_model.h"
#include "chromeos/ash/components/phonehub/phone_model_test_util.h"
#include "chromeos/ash/services/multidevice_setup/public/cpp/fake_multidevice_setup_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace phonehub {

using multidevice_setup::mojom::Feature;
using multidevice_setup::mojom::FeatureState;

class BrowserTabsModelControllerTest : public testing::Test {
 protected:
  BrowserTabsModelControllerTest() = default;
  ~BrowserTabsModelControllerTest() override = default;

  BrowserTabsModelControllerTest(const BrowserTabsModelControllerTest&) =
      delete;
  BrowserTabsModelControllerTest& operator=(
      const BrowserTabsModelControllerTest&) = delete;

  // testing::Test:
  void SetUp() override {
    phone_model_ = std::make_unique<MutablePhoneModel>();
    controller_ = std::make_unique<BrowserTabsModelController>(
        &fake_multidevice_setup_client_, &fake_browser_tabs_model_provider_,
        phone_model_.get());
  }

  void DisableTaskContinuation() {
    fake_multidevice_setup_client_.SetFeatureState(
        Feature::kPhoneHubTaskContinuation, FeatureState::kDisabledByUser);
  }

  void EnableTaskContinuation() {
    fake_multidevice_setup_client_.SetFeatureState(
        Feature::kPhoneHubTaskContinuation, FeatureState::kEnabledByUser);
  }

  void NotifyBrowserTabsUpdated(
      bool is_sync_enabled,
      const std::vector<BrowserTabsModel::BrowserTabMetadata>&
          browser_tabs_metadata) {
    fake_browser_tabs_model_provider_.NotifyBrowserTabsUpdated(
        is_sync_enabled, browser_tabs_metadata);
  }

  MutablePhoneModel* phone_model() { return phone_model_.get(); }

 private:
  std::unique_ptr<MutablePhoneModel> phone_model_;
  multidevice_setup::FakeMultiDeviceSetupClient fake_multidevice_setup_client_;
  FakeBrowserTabsModelProvider fake_browser_tabs_model_provider_;
  std::unique_ptr<BrowserTabsModelController> controller_;
};

TEST_F(BrowserTabsModelControllerTest, MutablePhoneModelProperlySet) {
  // Test that the MutablePhoneModel is not updated when task continuation
  // disabled.
  DisableTaskContinuation();
  std::vector<BrowserTabsModel::BrowserTabMetadata> metadata;
  metadata.push_back(CreateFakeBrowserTabMetadata());
  NotifyBrowserTabsUpdated(true, metadata);
  EXPECT_FALSE(phone_model()->browser_tabs_model());

  // Test that the MutablePhoneModel is updated when task continuation enabled.
  EnableTaskContinuation();
  EXPECT_TRUE(phone_model()->browser_tabs_model()->is_tab_sync_enabled());
  EXPECT_EQ(phone_model()->browser_tabs_model()->most_recent_tabs().size(), 1U);
}

}  // namespace phonehub
}  // namespace ash
