// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/promos/ios_promo_trigger_service.h"

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/desktop_to_mobile_promos/pref_names.h"
#include "components/desktop_to_mobile_promos/promos_types.h"
#include "components/prefs/pref_service.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "components/sync_device_info/fake_device_info_tracker.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using desktop_to_mobile_promos::PromoType;

std::unique_ptr<KeyedService> BuildFakeDeviceInfoSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::FakeDeviceInfoSyncService>();
}

std::unique_ptr<syncer::DeviceInfo> CreateDeviceInfo(
    const std::string& guid,
    syncer::DeviceInfo::OsType os_type,
    syncer::DeviceInfo::FormFactor form_factor,
    base::Time last_updated) {
  return std::make_unique<syncer::DeviceInfo>(
      guid, "name", "chrome_version", "user_agent",
      sync_pb::SyncEnums_DeviceType_TYPE_LINUX, os_type, form_factor,
      "scoped_id", "manufacturer", "model", "full_hardware_class",
      /*last_updated_timestamp=*/last_updated,
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
      /*auto_sign_out_last_signin_timestamp=*/std::nullopt);
}

}  // namespace

class IOSPromoTriggerServiceTest : public testing::Test {
 public:
  IOSPromoTriggerServiceTest() = default;
  ~IOSPromoTriggerServiceTest() override = default;

  void SetUp() override {
    TestingProfile::Builder builder;
    profile_ = builder.Build();

    DeviceInfoSyncServiceFactory::GetInstance()->SetTestingFactory(
        profile_.get(), base::BindRepeating(&BuildFakeDeviceInfoSyncService));

    service_ = std::make_unique<IOSPromoTriggerService>(profile_.get());

    syncer::FakeDeviceInfoSyncService* fake_device_service =
        static_cast<syncer::FakeDeviceInfoSyncService*>(
            DeviceInfoSyncServiceFactory::GetForProfile(profile_.get()));

    fake_device_info_tracker_ = static_cast<syncer::FakeDeviceInfoTracker*>(
        fake_device_service->GetDeviceInfoTracker());
  }

 protected:
  syncer::DeviceInfo* AddDevice(const std::string& guid,
                                syncer::DeviceInfo::OsType os_type,
                                syncer::DeviceInfo::FormFactor form_factor,
                                base::Time last_updated) {
    auto device = CreateDeviceInfo(guid, os_type, form_factor, last_updated);
    syncer::DeviceInfo* device_ptr = device.get();
    fake_device_info_tracker_->Add(std::move(device));
    return device_ptr;
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<IOSPromoTriggerService> service_;
  raw_ptr<syncer::FakeDeviceInfoTracker> fake_device_info_tracker_;
};

TEST_F(IOSPromoTriggerServiceTest, NotifiesCallback) {
  int call_count = 0;
  std::optional<PromoType> promo_type;

  base::CallbackListSubscription subscription =
      service_->RegisterPromoCallback(base::BindRepeating(
          [](int* call_count, std::optional<PromoType>* promo_type,
             PromoType type) {
            (*call_count)++;
            *promo_type = type;
          },
          &call_count, &promo_type));

  EXPECT_EQ(call_count, 0);
  EXPECT_FALSE(promo_type.has_value());

  service_->NotifyPromoShouldBeShown(PromoType::kPassword);

  EXPECT_EQ(call_count, 1);
  ASSERT_TRUE(promo_type.has_value());
  EXPECT_EQ(promo_type.value(), PromoType::kPassword);
}

TEST_F(IOSPromoTriggerServiceTest,
       CallbackIsRemovedWhenSubscriptionIsDestroyed) {
  int call_count = 0;
  std::optional<PromoType> promo_type;

  {
    base::CallbackListSubscription subscription =
        service_->RegisterPromoCallback(base::BindRepeating(
            [](int* call_count, std::optional<PromoType>* promo_type,
               PromoType type) {
              (*call_count)++;
              *promo_type = type;
            },
            &call_count, &promo_type));
  }

  service_->NotifyPromoShouldBeShown(PromoType::kPassword);

  EXPECT_EQ(call_count, 0);
}

TEST_F(IOSPromoTriggerServiceTest, GetIOSDeviceToRemind_NoDevices) {
  EXPECT_EQ(nullptr, service_->GetIOSDeviceToRemind());
}

TEST_F(IOSPromoTriggerServiceTest, GetIOSDeviceToRemind_NoIOSDevices) {
  AddDevice("guid1", syncer::DeviceInfo::OsType::kAndroid,
            syncer::DeviceInfo::FormFactor::kPhone, base::Time::Now());
  AddDevice("guid2", syncer::DeviceInfo::OsType::kWindows,
            syncer::DeviceInfo::FormFactor::kDesktop, base::Time::Now());

  EXPECT_EQ(nullptr, service_->GetIOSDeviceToRemind());
}

TEST_F(IOSPromoTriggerServiceTest, GetIOSDeviceToRemind_SingleiPhone) {
  const syncer::DeviceInfo* iphone =
      AddDevice("guid1", syncer::DeviceInfo::OsType::kIOS,
                syncer::DeviceInfo::FormFactor::kPhone, base::Time::Now());

  EXPECT_EQ(iphone, service_->GetIOSDeviceToRemind());
}

TEST_F(IOSPromoTriggerServiceTest, GetIOSDeviceToRemind_SingleiPad) {
  const syncer::DeviceInfo* ipad =
      AddDevice("guid1", syncer::DeviceInfo::OsType::kIOS,
                syncer::DeviceInfo::FormFactor::kTablet, base::Time::Now());

  EXPECT_EQ(ipad, service_->GetIOSDeviceToRemind());
}

TEST_F(IOSPromoTriggerServiceTest,
       GetIOSDeviceToRemind_MultipleiPhonesMostRecent) {
  AddDevice("guid1", syncer::DeviceInfo::OsType::kIOS,
            syncer::DeviceInfo::FormFactor::kPhone,
            base::Time::Now() - base::Minutes(10));
  const syncer::DeviceInfo* iphone2 =
      AddDevice("guid2", syncer::DeviceInfo::OsType::kIOS,
                syncer::DeviceInfo::FormFactor::kPhone, base::Time::Now());

  EXPECT_EQ(iphone2, service_->GetIOSDeviceToRemind());
}

TEST_F(IOSPromoTriggerServiceTest,
       GetIOSDeviceToRemind_MultipleiPadsMostRecent) {
  AddDevice("guid1", syncer::DeviceInfo::OsType::kIOS,
            syncer::DeviceInfo::FormFactor::kTablet,
            base::Time::Now() - base::Minutes(10));
  const syncer::DeviceInfo* ipad2 =
      AddDevice("guid2", syncer::DeviceInfo::OsType::kIOS,
                syncer::DeviceInfo::FormFactor::kTablet, base::Time::Now());

  EXPECT_EQ(ipad2, service_->GetIOSDeviceToRemind());
}

TEST_F(IOSPromoTriggerServiceTest,
       GetIOSDeviceToRemind_iPhonePreferredOveriPad) {
  const syncer::DeviceInfo* old_iphone =
      AddDevice("guid1", syncer::DeviceInfo::OsType::kIOS,
                syncer::DeviceInfo::FormFactor::kPhone,
                base::Time::Now() - base::Days(1));
  AddDevice("guid2", syncer::DeviceInfo::OsType::kIOS,
            syncer::DeviceInfo::FormFactor::kTablet, base::Time::Now());

  EXPECT_EQ(old_iphone, service_->GetIOSDeviceToRemind());
}

TEST_F(IOSPromoTriggerServiceTest,
       GetIOSDeviceToRemind_iPhonePreferredOveriPadMostRecent) {
  const syncer::DeviceInfo* recent_iphone =
      AddDevice("guid1", syncer::DeviceInfo::OsType::kIOS,
                syncer::DeviceInfo::FormFactor::kPhone, base::Time::Now());
  AddDevice("guid2", syncer::DeviceInfo::OsType::kIOS,
            syncer::DeviceInfo::FormFactor::kTablet,
            base::Time::Now() - base::Days(1));

  EXPECT_EQ(recent_iphone, service_->GetIOSDeviceToRemind());
}

TEST_F(IOSPromoTriggerServiceTest, SetReminderForIOSDevice) {
  service_->SetReminderForIOSDevice(PromoType::kPassword, "test_guid");

  const base::Value::Dict& promo_reminder_data =
      profile_->GetPrefs()->GetDict(prefs::kIOSPromoReminder);
  ASSERT_TRUE(promo_reminder_data.FindInt(prefs::kIOSPromoReminderPromoType));
  EXPECT_EQ(
      promo_reminder_data.FindInt(prefs::kIOSPromoReminderPromoType).value(),
      static_cast<int>(PromoType::kPassword));
  ASSERT_TRUE(
      promo_reminder_data.FindString(prefs::kIOSPromoReminderDeviceGUID));
  EXPECT_EQ(*promo_reminder_data.FindString(prefs::kIOSPromoReminderDeviceGUID),
            "test_guid");
}
