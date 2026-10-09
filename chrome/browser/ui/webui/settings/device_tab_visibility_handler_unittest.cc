// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/device_tab_visibility_handler.h"

#include <memory>
#include <string>
#include <string_view>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/test/values_test_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_sessions/fake_open_tabs_ui_delegate.h"
#include "components/sync_sessions/mock_session_sync_service.h"
#include "components/sync_sessions/synced_session.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace settings {

namespace {

using ::base::test::DictionaryHasValue;
using ::base::test::IsJson;
using ::l10n_util::GetPluralStringFUTF8;
using ::l10n_util::GetStringUTF8;
using ::sync_sessions::FakeOpenTabsUIDelegate;
using ::sync_sessions::MockSessionSyncService;
using ::sync_sessions::SyncedSession;
using ::syncer::DeviceInfo;
using ::testing::_;
using ::testing::ElementsAre;
using ::testing::ExplainMatchResult;
using ::testing::IsEmpty;
using ::testing::NiceMock;
using ::testing::Return;
using FormFactor = ::syncer::DeviceInfo::FormFactor;

constexpr char kCallbackId[] = "device-tab-visibility-callback-id";
constexpr char kGetListMessage[] = "getDeviceTabVisibilityList";
constexpr char kSetVisibilityMessage[] = "setDeviceTabVisibility";
constexpr char kListChangedEvent[] = "device-tab-visibility-list-changed";
constexpr char kWebUIResponse[] = "cr.webUIResponse";
constexpr char kWebUIListenerCallback[] = "cr.webUIListenerCallback";

constexpr char kTagKey[] = "tag";
constexpr char kNameKey[] = "name";
constexpr char kFormFactorKey[] = "formFactor";
constexpr char kLastActiveTimeKey[] = "lastActiveTime";
constexpr char kVisibleKey[] = "visible";

constexpr char kSession1Tag[] = "session_1";
constexpr char kSession2Tag[] = "session_2";
constexpr char kPhoneTag[] = "phone_tag";
constexpr char kPhoneName[] = "Pixel 9 Pro Fold";
constexpr char kTabletTag[] = "tablet_tag";
constexpr char kTabletName[] = "iPad Air";
constexpr char kDesktopTag[] = "desktop_tag";
constexpr char kDesktopName[] = "MacBook Pro";
constexpr char kUnknownTag[] = "unknown_tag";
constexpr char kUnknownName[] = "Linux Workstation";

constexpr char kPhoneFormFactor[] = "phone";
constexpr char kTabletFormFactor[] = "tablet";
constexpr char kDesktopFormFactor[] = "desktop";

constexpr int kTabletActiveMinutes = 5;
constexpr int kDesktopActiveHours = 3;
constexpr int kUnknownActiveDays = 2;

MATCHER_P5(IsDeviceEntry,
           tag,
           name,
           form_factor,
           last_active_time,
           visible,
           "") {
  return ExplainMatchResult(
      IsJson(base::DictValue()
                 .Set(kTagKey, tag)
                 .Set(kNameKey, name)
                 .Set(kFormFactorKey, form_factor)
                 .Set(kLastActiveTimeKey, last_active_time)
                 .Set(kVisibleKey, visible)),
      arg, result_listener);
}

MATCHER_P(IsWebUIListResponse, list_matcher, "") {
  return arg->function_name() == kWebUIResponse && arg->arg1() &&
         *arg->arg1() == base::Value(kCallbackId) && arg->arg2() &&
         *arg->arg2() == base::Value(true) && arg->arg3() &&
         arg->arg3()->is_list() &&
         ExplainMatchResult(list_matcher, arg->arg3()->GetList(),
                            result_listener);
}

MATCHER_P(IsWebUIBoolResponse, expected_value, "") {
  return arg->function_name() == kWebUIResponse && arg->arg1() &&
         *arg->arg1() == base::Value(kCallbackId) && arg->arg2() &&
         *arg->arg2() == base::Value(true) && arg->arg3() &&
         *arg->arg3() == base::Value(expected_value);
}

MATCHER_P(IsListChangedEvent, list_matcher, "") {
  return arg->function_name() == kWebUIListenerCallback && arg->arg1() &&
         *arg->arg1() == base::Value(kListChangedEvent) && arg->arg2() &&
         arg->arg2()->is_list() &&
         ExplainMatchResult(list_matcher, arg->arg2()->GetList(),
                            result_listener);
}

std::unique_ptr<SyncedSession> CreateSyncedSession(const std::string& tag,
                                                   const std::string& name,
                                                   base::Time modified_time,
                                                   FormFactor form_factor) {
  auto session = std::make_unique<SyncedSession>();
  session->SetSessionTag(tag);
  session->SetSessionName(name);
  session->SetModifiedTime(modified_time);
  session->SetDeviceTypeAndFormFactor(DeviceInfo::DeviceType::kUnset,
                                      form_factor);
  return session;
}

bool IsSessionHiddenInPrefs(const PrefService& prefs, std::string_view tag) {
  return prefs.GetDict(prefs::kDeviceTabVisibilityHiddenSessions).contains(tag);
}

void SetSessionHidden(PrefService* prefs, std::string_view tag, bool hidden) {
  ScopedDictPrefUpdate update(prefs, prefs::kDeviceTabVisibilityHiddenSessions);
  if (hidden) {
    update->Set(tag, true);
  } else {
    update->Remove(tag);
  }
}

}  // namespace

class DeviceTabVisibilityHandlerTest : public testing::Test {
 public:
  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        SessionSyncServiceFactory::GetInstance(),
        base::BindRepeating([](content::BrowserContext* /*context*/)
                                -> std::unique_ptr<KeyedService> {
          return std::make_unique<NiceMock<MockSessionSyncService>>();
        }));
    profile_ = builder.Build();

    ON_CALL(mock_session_sync_service(), GetOpenTabsUIDelegate())
        .WillByDefault(Return(&fake_open_tabs_ui_delegate_));
    ON_CALL(mock_session_sync_service(), SubscribeToForeignSessionsChanged(_))
        .WillByDefault([this](const base::RepeatingClosure& cb) {
          return foreign_sessions_changed_callbacks_.Add(cb);
        });

    auto handler = std::make_unique<DeviceTabVisibilityHandler>(profile_.get());
    handler_ = handler.get();
    web_ui_.AddMessageHandler(std::move(handler));
  }

  void SendGetListMessage() {
    base::ListValue args;
    args.Append(kCallbackId);
    web_ui_.HandleReceivedMessage(kGetListMessage, args);
  }

  void SendSetVisibilityMessage(std::string_view tag, bool visible) {
    base::ListValue args;
    args.Append(kCallbackId);
    args.Append(tag);
    args.Append(visible);
    web_ui_.HandleReceivedMessage(kSetVisibilityMessage, args);
  }

  void NotifyForeignSessionsChanged() {
    foreign_sessions_changed_callbacks_.Notify();
  }

  DeviceTabVisibilityHandler* handler() { return handler_; }
  TestingProfile* profile() { return profile_.get(); }
  content::TestWebUI* web_ui() { return &web_ui_; }
  MockSessionSyncService& mock_session_sync_service() {
    return *static_cast<NiceMock<MockSessionSyncService>*>(
        SessionSyncServiceFactory::GetForProfile(profile()));
  }
  FakeOpenTabsUIDelegate& fake_open_tabs_ui_delegate() {
    return fake_open_tabs_ui_delegate_;
  }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  FakeOpenTabsUIDelegate fake_open_tabs_ui_delegate_;
  base::RepeatingClosureList foreign_sessions_changed_callbacks_;
  std::unique_ptr<TestingProfile> profile_;
  content::TestWebUI web_ui_;
  raw_ptr<DeviceTabVisibilityHandler> handler_ = nullptr;
};

// Verifies that `getDeviceTabVisibilityList` resolves with an empty list when
// `OpenTabsUIDelegate` is null or when `GetAllForeignSessions` returns false.
TEST_F(DeviceTabVisibilityHandlerTest, GetListWhenDelegateUnavailable) {
  // Simulate tab sync being disabled (`GetOpenTabsUIDelegate()` returns null).
  ON_CALL(mock_session_sync_service(), GetOpenTabsUIDelegate())
      .WillByDefault(Return(nullptr));
  SendGetListMessage();

  // Restore the delegate with no foreign sessions added, so
  // `GetAllForeignSessions()` returns false.
  ON_CALL(mock_session_sync_service(), GetOpenTabsUIDelegate())
      .WillByDefault(Return(&fake_open_tabs_ui_delegate()));
  SendGetListMessage();

  // Both requests should resolve with an empty device list.
  EXPECT_THAT(web_ui()->call_data(),
              ElementsAre(IsWebUIListResponse(IsEmpty()),
                          IsWebUIListResponse(IsEmpty())));
}

// Verifies that `getDeviceTabVisibilityList` populates form factors, active
// time labels across time ranges (including future clock skew), and visibility
// states from `prefs::kDeviceTabVisibilityHiddenSessions`.
TEST_F(DeviceTabVisibilityHandlerTest, GetListPopulatesDevices) {
  const base::Time now = base::Time::Now();
  // Add sessions spanning each supported form factor and active-time bucket
  // (future/< 1 min, minutes, hours, days). `FormFactor::kUnknown` falls back
  // to `"desktop"`.
  fake_open_tabs_ui_delegate().AddForeignSession(CreateSyncedSession(
      kPhoneTag, kPhoneName, now + base::Seconds(10), FormFactor::kPhone));
  fake_open_tabs_ui_delegate().AddForeignSession(CreateSyncedSession(
      kTabletTag, kTabletName, now - base::Minutes(kTabletActiveMinutes),
      FormFactor::kTablet));
  fake_open_tabs_ui_delegate().AddForeignSession(CreateSyncedSession(
      kDesktopTag, kDesktopName, now - base::Hours(kDesktopActiveHours),
      FormFactor::kDesktop));
  fake_open_tabs_ui_delegate().AddForeignSession(CreateSyncedSession(
      kUnknownTag, kUnknownName, now - base::Days(kUnknownActiveDays),
      FormFactor::kUnknown));
  // Pre-hide the tablet session in prefs to verify initial `visible` state.
  SetSessionHidden(profile()->GetPrefs(), kTabletTag, true);

  SendGetListMessage();
  EXPECT_THAT(
      web_ui()->call_data(),
      ElementsAre(IsWebUIListResponse(ElementsAre(
          IsDeviceEntry(kPhoneTag, kPhoneName, kPhoneFormFactor,
                        GetStringUTF8(IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_NOW),
                        true),
          IsDeviceEntry(
              kTabletTag, kTabletName, kTabletFormFactor,
              GetPluralStringFUTF8(IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_MINUTES,
                                   kTabletActiveMinutes),
              false),
          IsDeviceEntry(
              kDesktopTag, kDesktopName, kDesktopFormFactor,
              GetPluralStringFUTF8(IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_HOURS,
                                   kDesktopActiveHours),
              true),
          IsDeviceEntry(
              kUnknownTag, kUnknownName, kDesktopFormFactor,
              GetPluralStringFUTF8(IDS_SEND_TAB_TO_SELF_DEVICE_LAST_UPDATE_DAYS,
                                   kUnknownActiveDays),
              true)))));
}

// Verifies that `setDeviceTabVisibility` persists hiding and re-enabling a
// device in `prefs::kDeviceTabVisibilityHiddenSessions` and fires the WebUI
// listener via `PrefChangeRegistrar` only when the preference actually changes.
TEST_F(DeviceTabVisibilityHandlerTest, SetVisibilityUpdatesPrefAndNotifies) {
  PrefService* prefs = profile()->GetPrefs();
  fake_open_tabs_ui_delegate().AddForeignSession(CreateSyncedSession(
      kPhoneTag, kPhoneName, base::Time::Now(), FormFactor::kPhone));
  EXPECT_FALSE(IsSessionHiddenInPrefs(*prefs, kPhoneTag));

  // Hide `kPhoneTag` and verify only that session is recorded as hidden.
  SendSetVisibilityMessage(kPhoneTag, false);
  EXPECT_TRUE(IsSessionHiddenInPrefs(*prefs, kPhoneTag));
  EXPECT_FALSE(IsSessionHiddenInPrefs(*prefs, kSession2Tag));

  // Re-enable `kPhoneTag` (and repeat idempotently) and verify the pref entry
  // is removed without firing a redundant listener event on the no-op call.
  SendSetVisibilityMessage(kPhoneTag, true);
  SendSetVisibilityMessage(kPhoneTag, true);
  EXPECT_FALSE(IsSessionHiddenInPrefs(*prefs, kPhoneTag));

  // External updates to `prefs::kDeviceTabVisibilityHiddenSessions` (e.g. from
  // another open Settings tab) also notify the active WebUI listener.
  SetSessionHidden(prefs, kPhoneTag, true);

  EXPECT_THAT(web_ui()->call_data(),
              ElementsAre(IsWebUIBoolResponse(true),
                          IsListChangedEvent(ElementsAre(DictionaryHasValue(
                              kVisibleKey, base::Value(false)))),
                          IsWebUIBoolResponse(true),
                          IsListChangedEvent(ElementsAre(DictionaryHasValue(
                              kVisibleKey, base::Value(true)))),
                          IsWebUIBoolResponse(true),
                          IsListChangedEvent(ElementsAre(DictionaryHasValue(
                              kVisibleKey, base::Value(false))))));
}

// Verifies that foreign session and preference updates fire
// `"device-tab-visibility-list-changed"` while JavaScript is allowed, stop
// firing after `DisallowJavascript()`, and resume cleanly when JavaScript is
// allowed again.
TEST_F(DeviceTabVisibilityHandlerTest, ForeignSessionsChangedNotifiesWebUI) {
  // Sending the initial request calls `AllowJavascript()` and subscribes to
  // foreign session and preference changes.
  SendGetListMessage();
  NotifyForeignSessionsChanged();

  // Once JavaScript is disallowed, subscriptions are reset and subsequent
  // foreign session or preference notifications are ignored.
  handler()->DisallowJavascript();
  NotifyForeignSessionsChanged();
  SetSessionHidden(profile()->GetPrefs(), kSession1Tag, true);

  // Re-allowing JavaScript re-subscribes without re-initializing
  // `PrefChangeRegistrar`.
  SendGetListMessage();

  EXPECT_THAT(
      web_ui()->call_data(),
      ElementsAre(IsWebUIListResponse(IsEmpty()), IsListChangedEvent(IsEmpty()),
                  IsWebUIListResponse(IsEmpty())));
}

}  // namespace settings
