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
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "components/sync_device_info/device_info.h"
#include "components/sync_device_info/remote_device_info.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "components/sync_sessions/session_sync_prefs.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_sessions/synced_session.h"
#include "content/public/browser/web_ui.h"

namespace settings {

namespace {

using FormFactor = syncer::DeviceInfo::FormFactor;
using sync_sessions::OpenTabsUIDelegate;
using sync_sessions::SessionSyncPrefs;
using sync_sessions::SessionSyncService;
using sync_sessions::SessionVisibility;
using sync_sessions::SyncedSession;
using syncer::RemoteDeviceInfo;

// Name of the WebUI event fired when the list of foreign sessions or their
// visibility state changes.
constexpr char kDeviceTabVisibilityListChangedEvent[] =
    "device-tab-visibility-list-changed";

// Maps `form_factor` to the device icon category expected by the Settings
// WebUI (`"phone"`, `"tablet"`, or `"desktop"`). Non-mobile form factors fall
// back to `"desktop"` because the subpage only renders phone, tablet, and
// desktop icons.
std::string_view GetFormFactorString(FormFactor form_factor) {
  switch (form_factor) {
    case FormFactor::kPhone:
      return "phone";
    case FormFactor::kTablet:
      return "tablet";
    case FormFactor::kDesktop:
    case FormFactor::kAutomotive:
    case FormFactor::kWearable:
    case FormFactor::kTv:
    case FormFactor::kUnknown:
      return "desktop";
  }
}

// Serializes `session` and its local `visible` state into the
// `DeviceTabVisibilityEntry` dictionary structure consumed by the WebUI.
base::DictValue BuildSessionDictionary(const SyncedSession& session,
                                       bool visible) {
  return base::DictValue()
      .Set("tag", session.GetSessionTag())
      .Set("name", session.GetSessionName())
      .Set("lastActiveTime",
           base::UTF16ToUTF8(RemoteDeviceInfo::GetLastActiveTimeForDisplay(
               session.GetModifiedTime(),
               /*has_high_precision_timestamp=*/true)))
      .Set("formFactor", GetFormFactorString(session.GetDeviceFormFactor()))
      .Set("visible", visible);
}

}  // namespace

DeviceTabVisibilityHandler::DeviceTabVisibilityHandler(Profile* profile)
    : profile_(CHECK_DEREF(profile)),
      session_sync_prefs_(profile_->GetPrefs()) {}

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
  // `base::Unretained` is safe because the subscriptions are destroyed when
  // `this` is destroyed or when JavaScript is disallowed.
  session_visibility_changed_subscription_ =
      session_sync_prefs_.SubscribeToSessionVisibilityChanged(
          base::BindRepeating(
              &DeviceTabVisibilityHandler::OnForeignSessionsChanged,
              base::Unretained(this)));

  SessionSyncService* service =
      SessionSyncServiceFactory::GetForProfile(&profile_.get());
  if (!service) {
    return;
  }

  foreign_session_updated_subscription_ =
      service->SubscribeToForeignSessionsChanged(base::BindRepeating(
          &DeviceTabVisibilityHandler::OnForeignSessionsChanged,
          base::Unretained(this)));
}

void DeviceTabVisibilityHandler::OnJavascriptDisallowed() {
  // Unsubscribe from preference and foreign session updates while JavaScript is
  // disallowed to avoid firing WebUI events into an inactive page.
  session_visibility_changed_subscription_ = base::CallbackListSubscription();
  foreign_session_updated_subscription_ = base::CallbackListSubscription();
}

base::ListValue DeviceTabVisibilityHandler::GetDeviceTabVisibilityList() const {
  SessionSyncService* service =
      SessionSyncServiceFactory::GetForProfile(&profile_.get());
  if (!service) {
    return {};
  }

  // `GetOpenTabsUIDelegate()` returns `nullptr` when tab sync is disabled or
  // not yet initialized.
  OpenTabsUIDelegate* open_tabs = service->GetOpenTabsUIDelegate();
  if (!open_tabs) {
    return {};
  }

  // TODO(crbug.com/572717502): Also include synced devices that have no open
  // tabs (`GetAllForeignSessions()` only returns sessions with open tabs).
  std::vector<raw_ptr<const SyncedSession, VectorExperimental>> sessions;
  if (!open_tabs->GetAllForeignSessions(&sessions)) {
    return {};
  }

  base::ListValue device_list;
  for (const SyncedSession* session : sessions) {
    CHECK(session);
    const bool visible =
        session_sync_prefs_.GetSessionVisibility(session->GetSessionTag()) ==
        SessionVisibility::kVisible;
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
  CHECK_EQ(args.size(), 2U);
  const std::string& session_tag = args[0].GetString();
  // TODO(crbug.com/570417844): Verify that `session_tag` corresponds to a
  // known foreign session.
  CHECK(!session_tag.empty());
  const bool visible = args[1].GetBool();
  AllowJavascript();

  session_sync_prefs_.SetSessionVisibility(
      session_tag,
      visible ? SessionVisibility::kVisible : SessionVisibility::kHidden);
}

void DeviceTabVisibilityHandler::OnForeignSessionsChanged() {
  FireWebUIListener(kDeviceTabVisibilityListChangedEvent,
                    GetDeviceTabVisibilityList());
}

}  // namespace settings
