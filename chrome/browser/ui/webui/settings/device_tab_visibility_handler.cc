// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/device_tab_visibility_handler.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_sessions/synced_session.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/l10n_util.h"

namespace settings {

namespace {

// Name of the WebUI event fired when the list of foreign sessions or their
// visibility state changes.
constexpr char kDeviceTabVisibilityListChangedEvent[] =
    "device-tab-visibility-list-changed";

// Maps `form_factor` to the device icon category expected by the Settings
// WebUI (`"phone"`, `"tablet"`, or `"desktop"`). Non-mobile form factors fall
// back to `"desktop"` because the subpage only renders phone, tablet, and
// desktop icons.
std::string_view GetFormFactorString(
    syncer::DeviceInfo::FormFactor form_factor) {
  switch (form_factor) {
    case syncer::DeviceInfo::FormFactor::kPhone:
      return "phone";
    case syncer::DeviceInfo::FormFactor::kTablet:
      return "tablet";
    case syncer::DeviceInfo::FormFactor::kDesktop:
    case syncer::DeviceInfo::FormFactor::kAutomotive:
    case syncer::DeviceInfo::FormFactor::kWearable:
    case syncer::DeviceInfo::FormFactor::kTv:
    case syncer::DeviceInfo::FormFactor::kUnknown:
      return "desktop";
  }
}

// Formats `modified_time` into a localized relative activity label (e.g.,
// "Active now", "Active 5m ago", "Active 3h ago", or "Updated 2 days ago").
// TODO(crbug.com/570417844): Extract a shared relative device activity
// formatter instead of mirroring
// `send_tab_to_self::TargetDeviceInfo::GetLastActiveTimeForDisplay()`.
std::string FormatLastActiveTime(base::Time modified_time) {
  const base::TimeDelta time_elapsed = base::Time::Now() - modified_time;
  if (time_elapsed < base::Minutes(1)) {
    return l10n_util::GetStringUTF8(IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_NOW);
  }
  if (time_elapsed < base::Hours(1)) {
    return l10n_util::GetPluralStringFUTF8(
        IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_MINUTES, time_elapsed.InMinutes());
  }
  if (time_elapsed < base::Days(1)) {
    return l10n_util::GetPluralStringFUTF8(
        IDS_SEND_TAB_TO_SELF_DEVICE_ACTIVE_HOURS, time_elapsed.InHours());
  }
  return l10n_util::GetPluralStringFUTF8(
      IDS_SEND_TAB_TO_SELF_DEVICE_LAST_UPDATE_DAYS, time_elapsed.InDays());
}

// Returns true if the foreign session identified by `session_tag` is visible
// in the local device tab list.
bool IsSessionVisible(const PrefService& prefs, std::string_view session_tag) {
  // Sessions are visible by default unless recorded in
  // `prefs::kDeviceTabVisibilityHiddenSessions`.
  const base::DictValue& hidden_sessions =
      prefs.GetDict(prefs::kDeviceTabVisibilityHiddenSessions);
  return !hidden_sessions.contains(session_tag);
}

// Serializes `session` and its local `visible` state into the
// `DeviceTabVisibilityEntry` dictionary structure consumed by the WebUI.
base::DictValue BuildSessionDictionary(
    const sync_sessions::SyncedSession& session,
    bool visible) {
  return base::DictValue()
      .Set("tag", session.GetSessionTag())
      .Set("name", session.GetSessionName())
      .Set("lastActiveTime", FormatLastActiveTime(session.GetModifiedTime()))
      .Set("formFactor", GetFormFactorString(session.GetDeviceFormFactor()))
      .Set("visible", visible);
}

}  // namespace

DeviceTabVisibilityHandler::DeviceTabVisibilityHandler(Profile* profile)
    : profile_(CHECK_DEREF(profile)) {
  pref_change_registrar_.Init(profile_->GetPrefs());
}

DeviceTabVisibilityHandler::~DeviceTabVisibilityHandler() = default;

void DeviceTabVisibilityHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getDeviceTabVisibilityList",
      base::BindRepeating(
          &DeviceTabVisibilityHandler::HandleGetDeviceTabVisibilityList,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setDeviceTabVisibility",
      base::BindRepeating(
          &DeviceTabVisibilityHandler::HandleSetDeviceTabVisibility,
          base::Unretained(this)));
}

void DeviceTabVisibilityHandler::OnJavascriptAllowed() {
  pref_change_registrar_.Add(
      prefs::kDeviceTabVisibilityHiddenSessions,
      base::BindRepeating(&DeviceTabVisibilityHandler::OnForeignSessionsChanged,
                          base::Unretained(this)));

  sync_sessions::SessionSyncService* service =
      SessionSyncServiceFactory::GetForProfile(&profile_.get());
  if (!service) {
    return;
  }

  // `base::Unretained` is safe because `foreign_session_updated_subscription_`
  // is destroyed when `this` is destroyed or when JavaScript is disallowed.
  foreign_session_updated_subscription_ =
      service->SubscribeToForeignSessionsChanged(base::BindRepeating(
          &DeviceTabVisibilityHandler::OnForeignSessionsChanged,
          base::Unretained(this)));
}

void DeviceTabVisibilityHandler::OnJavascriptDisallowed() {
  // Unsubscribe from preference and foreign session updates while JavaScript is
  // disallowed to avoid firing WebUI events into an inactive page.
  pref_change_registrar_.RemoveAll();
  foreign_session_updated_subscription_ = base::CallbackListSubscription();
}

base::ListValue DeviceTabVisibilityHandler::GetDeviceTabVisibilityList() const {
  sync_sessions::SessionSyncService* service =
      SessionSyncServiceFactory::GetForProfile(&profile_.get());
  if (!service) {
    return {};
  }

  // `GetOpenTabsUIDelegate()` returns `nullptr` when tab sync is disabled or
  // not yet initialized.
  sync_sessions::OpenTabsUIDelegate* open_tabs =
      service->GetOpenTabsUIDelegate();
  if (!open_tabs) {
    return {};
  }

  // TODO(crbug.com/570417844): Also include synced devices that have no open
  // tabs (`GetAllForeignSessions()` only returns sessions with open tabs).
  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      sessions;
  if (!open_tabs->GetAllForeignSessions(&sessions)) {
    return {};
  }

  const PrefService& prefs = CHECK_DEREF(profile_->GetPrefs());
  base::ListValue device_list;
  for (const sync_sessions::SyncedSession* session : sessions) {
    CHECK(session);
    const bool visible = IsSessionVisible(prefs, session->GetSessionTag());
    device_list.Append(BuildSessionDictionary(*session, visible));
  }
  return device_list;
}

void DeviceTabVisibilityHandler::HandleGetDeviceTabVisibilityList(
    const base::ListValue& args) {
  CHECK_EQ(args.size(), 1U);
  const base::Value& callback_id = args[0];
  AllowJavascript();
  ResolveJavascriptCallback(callback_id, GetDeviceTabVisibilityList());
}

void DeviceTabVisibilityHandler::HandleSetDeviceTabVisibility(
    const base::ListValue& args) {
  CHECK_EQ(args.size(), 3U);
  const base::Value& callback_id = args[0];
  const std::string& session_tag = args[1].GetString();
  // TODO(crbug.com/570417844): Verify that `session_tag` corresponds to a
  // known foreign session.
  CHECK(!session_tag.empty());
  const bool visible = args[2].GetBool();
  AllowJavascript();

  // TODO(crbug.com/570416929): Require OS re-authentication when `visible` is
  // true before re-enabling visibility for a hidden device.

  PrefService& prefs = CHECK_DEREF(profile_->GetPrefs());
  if (IsSessionVisible(prefs, session_tag) == visible) {
    ResolveJavascriptCallback(callback_id, base::Value(true));
    return;
  }

  // Only hidden sessions are persisted in the dictionary preference;
  // re-enabling visibility removes the entry to keep the preference sparse.
  // When `update` goes out of scope, `PrefService` notifies
  // `pref_change_registrar_`.
  // TODO(crbug.com/570417844): Clean up stale `session_tag` entries from
  // `prefs::kDeviceTabVisibilityHiddenSessions` when remote sessions expire.
  ScopedDictPrefUpdate update(&prefs,
                              prefs::kDeviceTabVisibilityHiddenSessions);
  if (visible) {
    update->Remove(session_tag);
  } else {
    update->Set(session_tag, true);
  }

  ResolveJavascriptCallback(callback_id, base::Value(true));
}

void DeviceTabVisibilityHandler::OnForeignSessionsChanged() {
  FireWebUIListener(kDeviceTabVisibilityListChangedEvent,
                    GetDeviceTabVisibilityList());
}

}  // namespace settings
