// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/foreign_session_handler.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/webui/web_ui_util.h"

namespace browser_sync {

namespace {

// Maximum number of sessions we're going to display on the NTP
const size_t kMaxSessionsToShow = 10;

// Converts the DeviceType enum value to a string. This is used
// in the NTP handler for foreign sessions for matching session
// types to an icon style.
std::string DeviceTypeToString(sync_pb::SyncEnums::DeviceType device_type) {
  switch (device_type) {
    case sync_pb::SyncEnums::TYPE_UNSET:
      break;
    case sync_pb::SyncEnums::TYPE_WIN:
      return "win";
    case sync_pb::SyncEnums::TYPE_MAC:
      return "macosx";
    case sync_pb::SyncEnums::TYPE_LINUX:
      return "linux";
    case sync_pb::SyncEnums::TYPE_CROS:
      return "chromeos";
    case sync_pb::SyncEnums::TYPE_OTHER:
      return "other";
    case sync_pb::SyncEnums::TYPE_PHONE:
      return "phone";
    case sync_pb::SyncEnums::TYPE_TABLET:
      return "tablet";
  }
  return std::string();
}

// Helper method to create JSON compatible objects from Session objects.
std::unique_ptr<base::DictionaryValue> SessionTabToValue(
    const ::sessions::SessionTab& tab) {
  if (tab.navigations.empty())
    return nullptr;

  int selected_index = std::min(tab.current_navigation_index,
                                static_cast<int>(tab.navigations.size() - 1));
  const ::sessions::SerializedNavigationEntry& current_navigation =
      tab.navigations.at(selected_index);
  GURL tab_url = current_navigation.virtual_url();
  if (!tab_url.is_valid() ||
      tab_url.spec() == chrome::kChromeUINewTabURL) {
    return nullptr;
  }

  std::unique_ptr<base::DictionaryValue> dictionary(
      new base::DictionaryValue());
  NewTabUI::SetUrlTitleAndDirection(dictionary.get(),
                                    current_navigation.title(), tab_url);
  dictionary->SetString("type", "tab");
  dictionary->SetDouble("timestamp",
                        static_cast<double>(tab.timestamp.ToInternalValue()));
  // TODO(jeremycho): This should probably be renamed to tabId to avoid
  // confusion with the ID corresponding to a session.  Investigate all the
  // places (C++ and JS) where this is being used.  (http://crbug.com/154865).
  dictionary->SetInteger("sessionId", tab.tab_id.id());
  return dictionary;
}

// Helper for initializing a boilerplate SessionWindow JSON compatible object.
std::unique_ptr<base::DictionaryValue> BuildWindowData(
    base::Time modification_time,
    SessionID window_id) {
  std::unique_ptr<base::DictionaryValue> dictionary(
      new base::DictionaryValue());
  // The items which are to be written into |dictionary| are also described in
  // chrome/browser/resources/ntp4/other_sessions.js in @typedef for WindowData.
  // Please update it whenever you add or remove any keys here.
  dictionary->SetString("type", "window");
  dictionary->SetDouble("timestamp", modification_time.ToInternalValue());

  dictionary->SetInteger("sessionId", window_id.id());
  return dictionary;
}

// Helper method to create JSON compatible objects from SessionWindow objects.
std::unique_ptr<base::DictionaryValue> SessionWindowToValue(
    const ::sessions::SessionWindow& window) {
  if (window.tabs.empty())
    return nullptr;
  std::unique_ptr<base::ListValue> tab_values(new base::ListValue());
  // Calculate the last |modification_time| for all entries within a window.
  base::Time modification_time = window.timestamp;
  for (const std::unique_ptr<sessions::SessionTab>& tab : window.tabs) {
    std::unique_ptr<base::DictionaryValue> tab_value(
        SessionTabToValue(*tab.get()));
    if (tab_value.get()) {
      modification_time = std::max(modification_time,
                                   tab->timestamp);
      tab_values->Append(std::move(tab_value));
    }
  }
  if (tab_values->GetSize() == 0)
    return nullptr;
  std::unique_ptr<base::DictionaryValue> dictionary(
      BuildWindowData(window.timestamp, window.window_id));
  dictionary->Set("tabs", std::move(tab_values));
  return dictionary;
}

}  // namespace

ForeignSessionHandler::ForeignSessionHandler() : scoped_observer_(this) {
  load_attempt_time_ = base::TimeTicks::Now();
}

ForeignSessionHandler::~ForeignSessionHandler() {}

// static
void ForeignSessionHandler::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kNtpCollapsedForeignSessions);
}

// static
void ForeignSessionHandler::OpenForeignSessionTab(
    content::WebUI* web_ui,
    const std::string& session_string_value,
    int window_num,
    SessionID tab_id,
    const WindowOpenDisposition& disposition) {
  sync_sessions::OpenTabsUIDelegate* open_tabs = GetOpenTabsUIDelegate(web_ui);
  if (!open_tabs)
    return;

  // We don't actually care about |window_num|, this is just a sanity check.
  DCHECK_LE(0, window_num);
  const ::sessions::SessionTab* tab;
  if (!open_tabs->GetForeignTab(session_string_value, tab_id, &tab)) {
    LOG(ERROR) << "Failed to load foreign tab.";
    return;
  }
  if (tab->navigations.empty()) {
    LOG(ERROR) << "Foreign tab no longer has valid navigations.";
    return;
  }
  SessionRestore::RestoreForeignSessionTab(
      web_ui->GetWebContents(), *tab, disposition);
}

// static
void ForeignSessionHandler::OpenForeignSessionWindows(
    content::WebUI* web_ui,
    const std::string& session_string_value,
    int window_num) {
  sync_sessions::OpenTabsUIDelegate* open_tabs = GetOpenTabsUIDelegate(web_ui);
  if (!open_tabs)
    return;

  std::vector<const ::sessions::SessionWindow*> windows;
  // Note: we don't own the ForeignSessions themselves.
  if (!open_tabs->GetForeignSession(session_string_value, &windows)) {
    LOG(ERROR) << "ForeignSessionHandler failed to get session data from"
        "OpenTabsUIDelegate.";
    return;
  }
  std::vector<const ::sessions::SessionWindow*>::const_iterator iter_begin =
      windows.begin() + (window_num < 0 ? 0 : window_num);
  auto iter_end =
      window_num < 0
          ? std::vector<const ::sessions::SessionWindow*>::const_iterator(
                windows.end())
          : iter_begin + 1;
  SessionRestore::RestoreForeignSessionWindows(Profile::FromWebUI(web_ui),
                                               iter_begin, iter_end);
}

// static
sync_sessions::OpenTabsUIDelegate* ForeignSessionHandler::GetOpenTabsUIDelegate(
    content::WebUI* web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  browser_sync::ProfileSyncService* service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);

  // Only return the delegate if it exists and it is done syncing sessions.
  if (service && service->IsSyncFeatureActive())
    return service->GetOpenTabsUIDelegate();

  return NULL;
}

void ForeignSessionHandler::RegisterMessages() {
  Profile* profile = Profile::FromWebUI(web_ui());

  browser_sync::ProfileSyncService* service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);

  // NOTE: The ProfileSyncService can be null in tests.
  if (service)
    scoped_observer_.Add(service);

  web_ui()->RegisterMessageCallback(
      "deleteForeignSession",
      base::BindRepeating(&ForeignSessionHandler::HandleDeleteForeignSession,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getForeignSessions",
      base::BindRepeating(&ForeignSessionHandler::HandleGetForeignSessions,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "openForeignSession",
      base::BindRepeating(&ForeignSessionHandler::HandleOpenForeignSession,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setForeignSessionCollapsed",
      base::BindRepeating(
          &ForeignSessionHandler::HandleSetForeignSessionCollapsed,
          base::Unretained(this)));
}

void ForeignSessionHandler::OnSyncConfigurationCompleted(
    syncer::SyncService* sync) {
  HandleGetForeignSessions(nullptr);
}

void ForeignSessionHandler::OnForeignSessionUpdated(syncer::SyncService* sync) {
  HandleGetForeignSessions(nullptr);
}

base::string16 ForeignSessionHandler::FormatSessionTime(
    const base::Time& time) {
  // Return a time like "1 hour ago", "2 days ago", etc.
  base::Time now = base::Time::Now();
  // TimeFormat does not support negative TimeDelta values, so then we use 0.
  return ui::TimeFormat::Simple(
      ui::TimeFormat::FORMAT_ELAPSED, ui::TimeFormat::LENGTH_SHORT,
      now < time ? base::TimeDelta() : now - time);
}

void ForeignSessionHandler::HandleGetForeignSessions(
    const base::ListValue* /*args*/) {
  sync_sessions::OpenTabsUIDelegate* open_tabs =
      GetOpenTabsUIDelegate(web_ui());
  std::vector<const sync_sessions::SyncedSession*> sessions;

  base::ListValue session_list;
  if (open_tabs && open_tabs->GetAllForeignSessions(&sessions)) {
    if (!load_attempt_time_.is_null()) {
      UMA_HISTOGRAM_TIMES("Sync.SessionsRefreshDelay",
                          base::TimeTicks::Now() - load_attempt_time_);
      load_attempt_time_ = base::TimeTicks();
    }

    // Use a pref to keep track of sessions that were collapsed by the user.
    // To prevent the pref from accumulating stale sessions, clear it each time
    // and only add back sessions that are still current.
    DictionaryPrefUpdate pref_update(Profile::FromWebUI(web_ui())->GetPrefs(),
                                     prefs::kNtpCollapsedForeignSessions);
    base::DictionaryValue* current_collapsed_sessions = pref_update.Get();
    std::unique_ptr<base::DictionaryValue> collapsed_sessions(
        current_collapsed_sessions->DeepCopy());
    current_collapsed_sessions->Clear();

    // Note: we don't own the SyncedSessions themselves.
    for (size_t i = 0; i < sessions.size() && i < kMaxSessionsToShow; ++i) {
      const sync_sessions::SyncedSession* session = sessions[i];
      const std::string& session_tag = session->session_tag;
      std::unique_ptr<base::DictionaryValue> session_data(
          new base::DictionaryValue());
      // The items which are to be written into |session_data| are also
      // described in chrome/browser/resources/history/externs.js
      // @typedef for ForeignSession. Please update it whenever you add or
      // remove any keys here.
      session_data->SetString("tag", session_tag);
      session_data->SetString("name", session->session_name);
      session_data->SetString("deviceType",
                              DeviceTypeToString(session->device_type));
      session_data->SetString("modifiedTime",
                              FormatSessionTime(session->modified_time));
      session_data->SetDouble("timestamp", session->modified_time.ToJsTime());

      bool is_collapsed = collapsed_sessions->HasKey(session_tag);
      session_data->SetBoolean("collapsed", is_collapsed);
      if (is_collapsed)
        current_collapsed_sessions->SetBoolean(session_tag, true);

      std::unique_ptr<base::ListValue> window_list(new base::ListValue());

      // Order tabs by visual order within window.
      for (const auto& window_pair : session->windows) {
        std::unique_ptr<base::DictionaryValue> window_data(
            SessionWindowToValue(window_pair.second->wrapped_window));
        if (window_data)
          window_list->Append(std::move(window_data));
      }

      session_data->Set("windows", std::move(window_list));
      session_list.Append(std::move(session_data));
    }
  }
  web_ui()->CallJavascriptFunctionUnsafe("setForeignSessions", session_list);
}

void ForeignSessionHandler::HandleOpenForeignSession(
    const base::ListValue* args) {
  size_t num_args = args->GetSize();
  // Expect either 1 or 8 args. For restoring an entire session, only
  // one argument is required -- the session tag. To restore a tab,
  // the additional args required are the window id, the tab id,
  // and 4 properties of the event object (button, altKey, ctrlKey,
  // metaKey, shiftKey) for determining how to open the tab.
  if (num_args != 8U && num_args != 1U) {
    LOG(ERROR) << "openForeignSession called with " << args->GetSize()
               << " arguments.";
    return;
  }

  // Extract the session tag (always provided).
  std::string session_string_value;
  if (!args->GetString(0, &session_string_value)) {
    LOG(ERROR) << "Failed to extract session tag.";
    return;
  }

  // Extract window number.
  std::string window_num_str;
  int window_num = -1;
  if (num_args >= 2 && (!args->GetString(1, &window_num_str) ||
      !base::StringToInt(window_num_str, &window_num))) {
    LOG(ERROR) << "Failed to extract window number.";
    return;
  }

  // Extract tab id.
  std::string tab_id_str;
  SessionID::id_type tab_id_value = 0;
  if (num_args >= 3 && (!args->GetString(2, &tab_id_str) ||
                        !base::StringToInt(tab_id_str, &tab_id_value))) {
    LOG(ERROR) << "Failed to extract tab SessionID.";
    return;
  }

  SessionID tab_id = SessionID::FromSerializedValue(tab_id_value);
  if (tab_id.is_valid()) {
    WindowOpenDisposition disposition = webui::GetDispositionFromClick(args, 3);
    OpenForeignSessionTab(
        web_ui(), session_string_value, window_num, tab_id, disposition);
  } else {
    OpenForeignSessionWindows(web_ui(), session_string_value, window_num);
  }
}

void ForeignSessionHandler::HandleDeleteForeignSession(
    const base::ListValue* args) {
  if (args->GetSize() != 1U) {
    LOG(ERROR) << "Wrong number of args to deleteForeignSession";
    return;
  }

  // Get the session tag argument (required).
  std::string session_tag;
  if (!args->GetString(0, &session_tag)) {
    LOG(ERROR) << "Unable to extract session tag";
    return;
  }

  sync_sessions::OpenTabsUIDelegate* open_tabs =
      GetOpenTabsUIDelegate(web_ui());
  if (open_tabs)
    open_tabs->DeleteForeignSession(session_tag);
}

void ForeignSessionHandler::HandleSetForeignSessionCollapsed(
    const base::ListValue* args) {
  if (args->GetSize() != 2U) {
    LOG(ERROR) << "Wrong number of args to setForeignSessionCollapsed";
    return;
  }

  // Get the session tag argument (required).
  std::string session_tag;
  if (!args->GetString(0, &session_tag)) {
    LOG(ERROR) << "Unable to extract session tag";
    return;
  }

  bool is_collapsed;
  if (!args->GetBoolean(1, &is_collapsed)) {
    LOG(ERROR) << "Unable to extract boolean argument";
    return;
  }

  // Store session tags for collapsed sessions in a preference so that the
  // collapsed state persists.
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  DictionaryPrefUpdate update(prefs, prefs::kNtpCollapsedForeignSessions);
  if (is_collapsed)
    update.Get()->SetBoolean(session_tag, true);
  else
    update.Get()->Remove(session_tag, NULL);
}

}  // namespace browser_sync
