// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/desktop_to_mobile_promos/ios_promo_controller.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/desktop_to_mobile_promos/promos_pref_names.h"
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
#include "components/prefs/pref_service.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "components/sync_device_info/test_device_info_builder.h"
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
    bool desktop_to_ios_promo_receiving_enabled,
    const MobilePromoOnDesktopPromoTypeSet&
        desktop_to_ios_promo_receiving_types = {}) {
  auto device_info =
      syncer::TestDeviceInfoBuilder(syncer::DeviceInfo::OsType::kIOS)
          .WithDesktopToIosPromoReceivingEnabled(
              desktop_to_ios_promo_receiving_enabled)
          .WithDesktopToIosPromoReceivingTypes(
              desktop_to_ios_promo_receiving_types)
          .Build();
  return device_info;
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
      CreateDeviceInfo(/*desktop_to_ios_promo_receiving_enabled=*/true,
                       {MobilePromoOnDesktopPromoType::kAllPromos}));

  views::Widget* widget =
      BrowserView::GetBrowserViewForBrowser(browser())->GetWidget();
  views::test::WidgetTest::SimulateNativeActivate(widget);
  views::test::WaitForWidgetActive(widget, true);

  EXPECT_CALL(*mock_user_education_interface(), MaybeShowFeaturePromo(_))
      .WillOnce(testing::Return(true));

  // Trigger the promo.
  promo_service()->NotifyPromoShouldBeShown(PromoType::kPassword);
}

// Verifies that the promo is not shown when the "desktop to iOS promo" feature
// is not enabled on the user's iOS device.
IN_PROC_BROWSER_TEST_F(IOSPromoControllerBrowserTest,
                       ShowPromo_ReceivingDisabled) {
  // Add a device with receiving disabled.
  device_info_tracker()->Add(
      CreateDeviceInfo(/*desktop_to_ios_promo_receiving_enabled=*/false));

  views::Widget* widget =
      BrowserView::GetBrowserViewForBrowser(browser())->GetWidget();
  views::test::WidgetTest::SimulateNativeActivate(widget);
  views::test::WaitForWidgetActive(widget, true);

  EXPECT_CALL(*mock_user_education_interface(), MaybeShowFeaturePromo(_))
      .Times(0);

  // Trigger the promo.
  promo_service()->NotifyPromoShouldBeShown(PromoType::kPassword);
}

// Verifies that the promo is not shown when the promo is in the cooldown
// period.
IN_PROC_BROWSER_TEST_F(IOSPromoControllerBrowserTest,
                       ShowPromo_BlockedByCooldown) {
  // Add a device with receiving enabled.
  device_info_tracker()->Add(
      CreateDeviceInfo(/*desktop_to_ios_promo_receiving_enabled=*/true,
                       {MobilePromoOnDesktopPromoType::kAllPromos}));

  // Set the last impression timestamp to now.
  browser()->profile()->GetPrefs()->SetTime(
      promos_prefs::kDesktopToiOSPasswordPromoLastImpressionTimestamp,
      base::Time::Now());

  views::Widget* widget =
      BrowserView::GetBrowserViewForBrowser(browser())->GetWidget();
  views::test::WidgetTest::SimulateNativeActivate(widget);
  views::test::WaitForWidgetActive(widget, true);

  EXPECT_CALL(*mock_user_education_interface(), MaybeShowFeaturePromo(_))
      .Times(0);

  // Trigger the promo.
  promo_service()->NotifyPromoShouldBeShown(PromoType::kPassword);
}

// Verifies that the promo is not shown when the user has seen too many
// impressions.
IN_PROC_BROWSER_TEST_F(IOSPromoControllerBrowserTest,
                       ShowPromo_BlockedByImpressionLimit) {
  // Add a device with receiving enabled.
  device_info_tracker()->Add(
      CreateDeviceInfo(/*desktop_to_ios_promo_receiving_enabled=*/true,
                       {MobilePromoOnDesktopPromoType::kAllPromos}));

  // Set the impression count to the maximum.
  browser()->profile()->GetPrefs()->SetInteger(
      promos_prefs::kDesktopToiOSPasswordPromoImpressionsCounter, 10);

  views::Widget* widget =
      BrowserView::GetBrowserViewForBrowser(browser())->GetWidget();
  views::test::WidgetTest::SimulateNativeActivate(widget);
  views::test::WaitForWidgetActive(widget, true);

  EXPECT_CALL(*mock_user_education_interface(), MaybeShowFeaturePromo(_))
      .Times(0);

  // Trigger the promo.
  promo_service()->NotifyPromoShouldBeShown(PromoType::kPassword);
}

// Verifies that the promo is not shown when the "desktop to iOS promo" feature
// is enabled on the user's iOS device, but not for the specific promo type.
IN_PROC_BROWSER_TEST_F(IOSPromoControllerBrowserTest,
                       ShowPromo_ReceivingEnabled_WrongType) {
  // Add a device with receiving enabled, but for a different promo type (Lens).
  // Note: An iOS client with only specific promos enabled (and not kAllPromos)
  // will populate the legacy boolean as false.
  device_info_tracker()->Add(
      CreateDeviceInfo(/*desktop_to_ios_promo_receiving_enabled=*/false,
                       {MobilePromoOnDesktopPromoType::kLensPromo}));

  views::Widget* widget =
      BrowserView::GetBrowserViewForBrowser(browser())->GetWidget();
  views::test::WidgetTest::SimulateNativeActivate(widget);
  views::test::WaitForWidgetActive(widget, true);

  EXPECT_CALL(*mock_user_education_interface(), MaybeShowFeaturePromo(_))
      .Times(0);

  // Trigger the promo. Password maps to something else, not Lens.
  promo_service()->NotifyPromoShouldBeShown(PromoType::kPassword);
}

// Verifies that the promo is not shown when the "desktop to iOS promo" feature
// legacy boolean is true (due to a different legacy promo being enabled), but
// the specific promo type requested is not enabled on the user's iOS device.
IN_PROC_BROWSER_TEST_F(IOSPromoControllerBrowserTest,
                       ShowPromo_ReceivingEnabled_LegacyBooleanTrue_WrongType) {
  // Add a device where legacy receiving is true (because ESBPromo is enabled),
  // but AutofillPromo (Password) is not in the types list.
  device_info_tracker()->Add(
      CreateDeviceInfo(/*desktop_to_ios_promo_receiving_enabled=*/true,
                       {MobilePromoOnDesktopPromoType::kESBPromo}));

  views::Widget* widget =
      BrowserView::GetBrowserViewForBrowser(browser())->GetWidget();
  views::test::WidgetTest::SimulateNativeActivate(widget);
  views::test::WaitForWidgetActive(widget, true);

  EXPECT_CALL(*mock_user_education_interface(), MaybeShowFeaturePromo(_))
      .Times(0);

  // Trigger the promo.
  promo_service()->NotifyPromoShouldBeShown(PromoType::kPassword);
}

// Verifies that the promo is shown when the "desktop to iOS promo" feature
// is enabled on an older iOS device that only populates the legacy boolean.
IN_PROC_BROWSER_TEST_F(IOSPromoControllerBrowserTest,
                       ShowPromo_ReceivingEnabled_LegacyClient) {
  // Add a device with legacy receiving enabled, and an empty types list.
  device_info_tracker()->Add(
      CreateDeviceInfo(/*desktop_to_ios_promo_receiving_enabled=*/true, {}));

  views::Widget* widget =
      BrowserView::GetBrowserViewForBrowser(browser())->GetWidget();
  views::test::WidgetTest::SimulateNativeActivate(widget);
  views::test::WaitForWidgetActive(widget, true);

  EXPECT_CALL(*mock_user_education_interface(), MaybeShowFeaturePromo(_))
      .Times(1);

  // Trigger the promo.
  promo_service()->NotifyPromoShouldBeShown(PromoType::kPassword);
}

// Verifies that the promo is shown when the specific promo type is enabled,
// even if kAllPromos is not present.
IN_PROC_BROWSER_TEST_F(IOSPromoControllerBrowserTest,
                       ShowPromo_ReceivingEnabled_SpecificType) {
  // Add a device with legacy receiving enabled (because AutofillPromo is
  // enabled), and the specific type (kAutofillPromo) is in the types list.
  device_info_tracker()->Add(
      CreateDeviceInfo(/*desktop_to_ios_promo_receiving_enabled=*/true,
                       {MobilePromoOnDesktopPromoType::kAutofillPromo}));

  views::Widget* widget =
      BrowserView::GetBrowserViewForBrowser(browser())->GetWidget();
  views::test::WidgetTest::SimulateNativeActivate(widget);
  views::test::WaitForWidgetActive(widget, true);

  EXPECT_CALL(*mock_user_education_interface(), MaybeShowFeaturePromo(_))
      .Times(1);

  // Trigger the promo.
  promo_service()->NotifyPromoShouldBeShown(PromoType::kPassword);
}
