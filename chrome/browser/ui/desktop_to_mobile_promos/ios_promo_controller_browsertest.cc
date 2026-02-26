// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/desktop_to_mobile_promos/ios_promo_controller.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/desktop_to_mobile_promos/ios_promo_trigger_service.h"
#include "chrome/browser/ui/desktop_to_mobile_promos/ios_promo_trigger_service_factory.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/user_education/mock_browser_user_education_interface.h"
#include "components/desktop_to_mobile_promos/features.h"
#include "components/desktop_to_mobile_promos/promos_types.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "components/sync_preferences/features.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#include "ui/views/test/widget_activation_waiter.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/widget/widget.h"

using desktop_to_mobile_promos::PromoType;
using testing::_;
using testing::NiceMock;

namespace {

std::unique_ptr<KeyedService> BuildFakeDeviceInfoSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::FakeDeviceInfoSyncService>();
}

std::unique_ptr<KeyedService> BuildTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

std::unique_ptr<syncer::DeviceInfo> CreateDeviceInfo(
    const std::string& guid,
    syncer::DeviceInfo::OsType os_type,
    syncer::DeviceInfo::FormFactor form_factor,
    bool desktop_to_ios_promo_receiving_enabled) {
  return std::make_unique<syncer::DeviceInfo>(
      guid, "name", "chrome_version", "user_agent",
      sync_pb::SyncEnums_DeviceType_TYPE_PHONE, os_type, form_factor,
      "scoped_id", "manufacturer", "model", "full_hardware_class",
      /*last_updated_timestamp=*/base::Time::Now(),
      /*pulse_interval=*/base::Days(1),
      /*send_tab_to_self_receiving_enabled=*/
      false,
      /*send_tab_to_self_receiving_type=*/
      sync_pb::
          SyncEnums_SendTabReceivingType_SEND_TAB_RECEIVING_TYPE_CHROME_OR_UNSPECIFIED,
      /*sharing_info=*/std::nullopt,
      /*paask_info=*/std::nullopt,
      /*fcm_registration_token=*/std::string(),
      /*interested_data_types=*/syncer::DataTypeSet::All(),
      /*auto_sign_out_last_signin_timestamp=*/std::nullopt,
      desktop_to_ios_promo_receiving_enabled);
}

}  // namespace

// Tests for IOSPromoController.
class IOSPromoControllerBrowserTest : public InProcessBrowserTest {
 public:
  IOSPromoControllerBrowserTest() {
    feature_list_.InitWithFeatures(
        {kMobilePromoOnDesktopWithReminder,
         feature_engagement::kIPHiOSPasswordPromoDesktopFeature,
         sync_preferences::features::kEnableCrossDevicePrefTracker},
        {});
  }
  ~IOSPromoControllerBrowserTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    // Override the creation of BrowserUserEducationInterface to use the mock.
    user_ed_override_ =
        BrowserWindowFeatures::GetUserDataFactoryForTesting()
            .AddOverrideForTesting(
                base::BindRepeating([](BrowserWindowInterface& window) {
                  return std::make_unique<
                      NiceMock<MockBrowserUserEducationInterface>>(&window);
                }));

    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&IOSPromoControllerBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    DeviceInfoSyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildFakeDeviceInfoSyncService));
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildTestSyncService));
  }

  syncer::FakeDeviceInfoTracker* device_info_tracker() {
    return static_cast<syncer::FakeDeviceInfoSyncService*>(
               DeviceInfoSyncServiceFactory::GetForProfile(
                   browser()->profile()))
        ->GetDeviceInfoTracker();
  }

  MockBrowserUserEducationInterface* mock_user_education_interface() {
    return static_cast<MockBrowserUserEducationInterface*>(
        BrowserUserEducationInterface::From(browser()));
  }

  IOSPromoTriggerService* promo_service() {
    return IOSPromoTriggerServiceFactory::GetForProfile(browser()->profile());
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  ui::UserDataFactory::ScopedOverride user_ed_override_;
  base::CallbackListSubscription create_services_subscription_;
};

// Verifies that the promo is shown when the "desktop to iOS promo" feature is
// enabled on the user's iOS device.
IN_PROC_BROWSER_TEST_F(IOSPromoControllerBrowserTest,
                       ShowPromo_ReceivingEnabled) {
  // Add a device with receiving enabled.
  device_info_tracker()->Add(
      CreateDeviceInfo("guid1", syncer::DeviceInfo::OsType::kIOS,
                       syncer::DeviceInfo::FormFactor::kPhone, true));

  views::Widget* widget =
      BrowserView::GetBrowserViewForBrowser(browser())->GetWidget();
  views::test::WidgetTest::SimulateNativeActivate(widget);
  views::test::WaitForWidgetActive(widget, true);

  EXPECT_CALL(*mock_user_education_interface(), MaybeShowFeaturePromo(_))
      .Times(1);

  // Trigger the promo.
  promo_service()->NotifyPromoShouldBeShown(PromoType::kPassword);
}

// Verifies that the promo is not shown when the "desktop to iOS promo" feature
// is not enabled on the user's iOS device.
IN_PROC_BROWSER_TEST_F(IOSPromoControllerBrowserTest,
                       ShowPromo_ReceivingDisabled) {
  // Add a device with receiving disabled.
  device_info_tracker()->Add(
      CreateDeviceInfo("guid1", syncer::DeviceInfo::OsType::kIOS,
                       syncer::DeviceInfo::FormFactor::kPhone, false));

  views::Widget* widget =
      BrowserView::GetBrowserViewForBrowser(browser())->GetWidget();
  views::test::WidgetTest::SimulateNativeActivate(widget);
  views::test::WaitForWidgetActive(widget, true);

  EXPECT_CALL(*mock_user_education_interface(), MaybeShowFeaturePromo(_))
      .Times(0);

  // Trigger the promo.
  promo_service()->NotifyPromoShouldBeShown(PromoType::kPassword);
}
