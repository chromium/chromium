// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/phonehub/phone_model_test_util.h"

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"

namespace ash {
namespace phonehub {

const char16_t kFakeMobileProviderName[] = u"Fake Mobile Provider";

const PhoneStatusModel::MobileConnectionMetadata&
CreateFakeMobileConnectionMetadata() {
  static const base::NoDestructor<PhoneStatusModel::MobileConnectionMetadata>
      fake_mobile_connection_metadata{
          {PhoneStatusModel::SignalStrength::kFourBars,
           kFakeMobileProviderName}};
  return *fake_mobile_connection_metadata;
}

const PhoneStatusModel& CreateFakePhoneStatusModel() {
  static const base::NoDestructor<PhoneStatusModel> fake_phone_status_model{
      PhoneStatusModel::MobileStatus::kSimWithReception,
      CreateFakeMobileConnectionMetadata(),
      PhoneStatusModel::ChargingState::kNotCharging,
      PhoneStatusModel::BatterySaverState::kOff,
      /*battery_percentage=*/100u};
  return *fake_phone_status_model;
}

const char kFakeBrowserTabUrl1[] = "https://www.example.com/tab1";
const char16_t kFakeBrowserTabName1[] = u"Tab 1";
const base::Time kFakeBrowserTabLastAccessedTimestamp1 =
    base::Time::FromSecondsSinceUnixEpoch(4);

const char kFakeBrowserTabUrl2[] = "https://www.example.com/tab2";
const char16_t kFakeBrowserTabName2[] = u"Tab 2";
const base::Time kFakeBrowserTabLastAccessedTimestamp2 =
    base::Time::FromSecondsSinceUnixEpoch(3);

const BrowserTabsModel::BrowserTabMetadata& CreateFakeBrowserTabMetadata() {
  static const base::NoDestructor<BrowserTabsModel::BrowserTabMetadata>
      fake_browser_tab_metadata{GURL(kFakeBrowserTabUrl1), kFakeBrowserTabName1,
                                kFakeBrowserTabLastAccessedTimestamp1,
                                gfx::Image()};
  return *fake_browser_tab_metadata;
}

const BrowserTabsModel& CreateFakeBrowserTabsModel() {
  static const base::NoDestructor<BrowserTabsModel::BrowserTabMetadata>
      second_browser_tab_metadata{
          GURL(kFakeBrowserTabUrl2), kFakeBrowserTabName2,
          kFakeBrowserTabLastAccessedTimestamp2, gfx::Image()};

  static const base::NoDestructor<
      std::vector<BrowserTabsModel::BrowserTabMetadata>>
      most_recent_tabs(
          {CreateFakeBrowserTabMetadata(), *second_browser_tab_metadata});

  static const base::NoDestructor<BrowserTabsModel> fake_browser_tabs_model{
      /*is_tab_sync_enabled=*/true, *most_recent_tabs};

  return *fake_browser_tabs_model;
}

const char16_t kFakeAppVisibleName[] = u"Fake App";
const char kFakeAppPackageName[] = "com.fakeapp";
const int64_t kFakeAppId = 1234567890;
const int64_t kFakeInlineReplyId = 1337;
const int64_t kUserId = 1;
const char16_t kFakeNotificationTitle[] = u"Fake Title";
const char16_t kFakeNotificationText[] = u"Fake Text";
const base::flat_map<Notification::ActionType, int64_t> kFakeActionIdMap = {
    {Notification::ActionType::kInlineReply, kFakeInlineReplyId}};

const Notification::AppMetadata& CreateFakeAppMetadata() {
  static const base::NoDestructor<Notification::AppMetadata> fake_app_metadata{
      kFakeAppVisibleName,
      kFakeAppPackageName,
      gfx::Image(),
      /*icon_color=*/std::nullopt,
      /*icon_is_monochrome=*/true,
      kUserId,
      phonehub::proto::AppStreamabilityStatus::STREAMABLE};
  return *fake_app_metadata;
}

const Notification& CreateFakeNotification() {
  static const base::NoDestructor<Notification> fake_notification{
      kFakeAppId,
      CreateFakeAppMetadata(),
      base::Time(),
      Notification::Importance::kDefault,
      Notification::Category::kConversation,
      kFakeActionIdMap,
      Notification::InteractionBehavior::kNone,
      kFakeNotificationTitle,
      kFakeNotificationText};
  return *fake_notification;
}

}  // namespace phonehub
}  // namespace ash
