// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/history/foreign_session_handler.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/time_formatting.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_sessions/session_sync_service.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/base/window_open_disposition_utils.h"

namespace browser_sync {

namespace {

// Maximum number of sessions we're going to display on the NTP
const size_t kMaxSessionsToShow = 10;

// Helper method to create JSON compatible objects from Session objects.
std::optional<base::Value::Dict> SessionTabToValue(
    const ::sessions::SessionTab& tab) {
  if (tab.navigations.empty())
    return std::nullopt;

  int selected_index = std::min(tab.current_navigation_index,
                                static_cast<int>(tab.navigations.size() - 1));
  const ::sessions::SerializedNavigationEntry& current_navigation =
      tab.navigations.at(selected_index);
  GURL tab_url = current_navigation.virtual_url();
  if (!tab_url.is_valid() || tab_url.spec() == chrome::kChromeUINewTabURL)
    return std::nullopt;

  base::Value::Dict dictionary;
  NewTabUI::SetUrlTitleAndDirection(&dictionary, current_navigation.title(),
                                    tab_url);
  dictionary.Set("remoteIconUrlForUma",
                 current_navigation.favicon_url().spec());
  dictionary.Set("type", "tab");
  dictionary.Set("timestamp",
                 static_cast<double>(tab.timestamp.ToInternalValue()));
  // TODO(jeremycho): This should probably be renamed to tabId to avoid
  // confusion with the ID corresponding to a session.  Investigate all the
  // places (C++ and JS) where this is being used.  (http://crbug.com/154865).
  dictionary.Set("sessionId", tab.tab_id.id());
  return dictionary;
}

// Helper for initializing a boilerplate SessionWindow JSON compatible object.
base::Value::Dict BuildWindowData(base::Time modification_time,
                                  SessionID window_id) {
  base::Value::Dict dictionary;
  dictionary.Set("type", "window");
  dictionary.Set("timestamp",
                 static_cast<double>(modification_time.ToInternalValue()));

  dictionary.Set("sessionId", window_id.id());
  return dictionary;
}

// Helper method to create JSON compatible objects from SessionWindow objects.
std::optional<base::Value::Dict> SessionWindowToValue(
    const ::sessions::SessionWindow& window) {
  if (window.tabs.empty())
    return std::nullopt;

  base::Value::List tab_values;
  // Calculate the last |modification_time| for all entries within a window.
  base::Time modification_time = window.timestamp;
  for (const std::unique_ptr<sessions::SessionTab>& tab : window.tabs) {
    auto tab_value = SessionTabToValue(*tab.get());
    if (tab_value.has_value()) {
      modification_time = std::max(modification_time, tab->timestamp);
      tab_values.Append(std::move(*tab_value));
    }
  }
  if (tab_values.empty())
    return std::nullopt;

  base::Value::Dict dictionary =
      BuildWindowData(window.timestamp, window.window_id);
  dictionary.Set("tabs", std::move(tab_values));
  return dictionary;
}

}  // namespace

ForeignSessionHandler::ForeignSessionHandler() = default;

ForeignSessionHandler::~ForeignSessionHandler() = default;

// static
void ForeignSessionHandler::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kNtpCollapsedForeignSessions);
}

// static
void ForeignSessionHandler::OpenForeignSessionTab(
    content::WebUI* web_ui,
    const std::string& session_string_value,
    SessionID tab_id,
    const WindowOpenDisposition& disposition) {
  sync_sessions::OpenTabsUIDelegate* open_tabs = GetOpenTabsUIDelegate(web_ui);
  if (!open_tabs)
    return;

  const ::sessions::SessionTab* tab;
  if (!open_tabs->GetForeignTab(session_string_value, tab_id, &tab)) {
    LOG(ERROR) << "Failed to load foreign tab.";
    return;
  }
  if (tab->navigations.empty()) {
    LOG(ERROR) << "Foreign tab no longer has valid navigations.";
    return;
  }
  SessionRestore::RestoreForeignSessionTab(web_ui->GetWebContents(), *tab,
                                           disposition);
}

// static
void ForeignSessionHandler::OpenForeignSessionWindows(
    content::WebUI* web_ui,
    const std::string& session_string_value) {
  sync_sessions::OpenTabsUIDelegate* open_tabs = GetOpenTabsUIDelegate(web_ui);
  if (!open_tabs)
    return;

  // Note: we don't own the ForeignSessions themselves.
  std::vector<const ::sessions::SessionWindow*> windows =
      open_tabs->GetForeignSession(session_string_value);
  if (windows.empty()) {
    LOG(ERROR) << "ForeignSessionHandler failed to get session data from"
                  "OpenTabsUIDelegate.";
    return;
  }

  SessionRestore::RestoreForeignSessionWindows(Profile::FromWebUI(web_ui),
                                               windows.begin(), windows.end());
}

// static
sync_sessions::OpenTabsUIDelegate* ForeignSessionHandler::GetOpenTabsUIDelegate(
    content::WebUI* web_ui) {
  Profile* profile = Profile::FromWebUI(web_ui);
  sync_sessions::SessionSyncService* service =
      SessionSyncServiceFactory::GetInstance()->GetForProfile(profile);
  return service ? service->GetOpenTabsUIDelegate() : nullptr;
}

void ForeignSessionHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "deleteForeignSession",
      base::BindRepeating(&ForeignSessionHandler::HandleDeleteForeignSession,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getForeignSessions",
      base::BindRepeating(&ForeignSessionHandler::HandleGetForeignSessions,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "openForeignSessionAllTabs",
      base::BindRepeating(
          &ForeignSessionHandler::HandleOpenForeignSessionAllTabs,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "openForeignSessionTab",
      base::BindRepeating(&ForeignSessionHandler::HandleOpenForeignSessionTab,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setForeignSessionCollapsed",
      base::BindRepeating(
          &ForeignSessionHandler::HandleSetForeignSessionCollapsed,
          base::Unretained(this)));
}

void ForeignSessionHandler::OnJavascriptAllowed() {
  // This can happen if the page is refreshed.
  if (!initial_session_list_)
    initial_session_list_ = GetForeignSessions();

  Profile* profile = Profile::FromWebUI(web_ui());

  sync_sessions::SessionSyncService* service =
      SessionSyncServiceFactory::GetInstance()->GetForProfile(profile);

  // NOTE: The SessionSyncService can be null in tests.
  if (service) {
    // base::Unretained() is safe below because the subscription itself is a
    // class member field and handles destruction well.
    foreign_session_updated_subscription_ =
        service->SubscribeToForeignSessionsChanged(
            base::BindRepeating(&ForeignSessionHandler::OnForeignSessionUpdated,
                                base::Unretained(this)));
  }
}

void ForeignSessionHandler::OnJavascriptDisallowed() {
  // Avoid notifying Javascript listeners due to foreign session changes, which
  // is now disallowed and would otherwise run into CHECK failures in
  // OnForeignSessionUpdated().
  foreign_session_updated_subscription_ = base::CallbackListSubscription();
}

void ForeignSessionHandler::OnForeignSessionUpdated() {
  FireWebUIListener("foreign-sessions-changed",
                    std::move(GetForeignSessions()));
}

void ForeignSessionHandler::InitializeForeignSessions() {
  initial_session_list_ = GetForeignSessions();
}

std::u16string ForeignSessionHandler::FormatSessionTime(
    const base::Time& time) {
  // Return a time like "1 hour ago", "2 days ago", etc.
  base::Time now = base::Time::Now();
  // TimeFormat does not support negative TimeDelta values, so then we use 0.
  return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_ELAPSED,
                                ui::TimeFormat::LENGTH_SHORT,
                                now < time ? base::TimeDelta() : now - time);
}

void ForeignSessionHandler::HandleGetForeignSessions(
    const base::Value::List& args) {
  AllowJavascript();
  CHECK(initial_session_list_);
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(callback_id, *initial_session_list_);

  // Clear the initial list so that it will be reset in AllowJavascript if the
  // page is refreshed.
  initial_session_list_ = std::nullopt;
}

base::Value::List ForeignSessionHandler::GetForeignSessions() {
  sync_sessions::OpenTabsUIDelegate* open_tabs =
      GetOpenTabsUIDelegate(web_ui());
  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      sessions;

  base::Value::List session_list;
  if (open_tabs && open_tabs->GetAllForeignSessions(&sessions)) {
    // Use a pref to keep track of sessions that were collapsed by the user.
    // To prevent the pref from accumulating stale sessions, clear it each time
    // and only add back sessions that are still current.
    ScopedDictPrefUpdate pref_update(Profile::FromWebUI(web_ui())->GetPrefs(),
                                     prefs::kNtpCollapsedForeignSessions);
    base::Value::Dict& current_collapsed_sessions = pref_update.Get();
    base::Value::Dict collapsed_sessions = current_collapsed_sessions.Clone();
    current_collapsed_sessions.clear();

    // Note: we don't own the SyncedSessions themselves.
    for (size_t i = 0; i < sessions.size() && i < kMaxSessionsToShow; ++i) {
      const sync_sessions::SyncedSession* session = sessions[i];
      const std::string& session_tag = session->GetSessionTag();
      base::Value::Dict session_data;
      // The items which are to be written into |session_data| are also
      // described in chrome/browser/resources/history/externs.js
      // @typedef for ForeignSession. Please update it whenever you add or
      // remove any keys here.
      session_data.Set("tag", session_tag);
      session_data.Set("name", session->GetSessionName());
      session_data.Set("modifiedTime",
                       FormatSessionTime(session->GetModifiedTime()));
      session_data.Set(
          "timestamp",
          session->GetModifiedTime().InMillisecondsFSinceUnixEpoch());

      bool is_collapsed = collapsed_sessions.Find(session_tag);
      session_data.Set("collapsed", is_collapsed);
      if (is_collapsed)
        current_collapsed_sessions.Set(session_tag, true);

      base::Value::List window_list;

      // Order tabs by visual order within window.
      for (const auto& window_pair : session->windows) {
        auto window_data =
            SessionWindowToValue(window_pair.second->wrapped_window);
        if (window_data.has_value()) {
          window_list.Append(std::move(*window_data));
        }
      }

      session_data.Set("windows", std::move(window_list));
      session_list.Append(std::move(session_data));
    }
  }
  return session_list;
}

void ForeignSessionHandler::HandleOpenForeignSessionAllTabs(
    const base::Value::List& args) {
  CHECK_EQ(args.size(), 1U);

  // Extract the session tag (always provided).
  if (!args[0].is_string()) {
    LOG(ERROR) << "Failed to extract session tag.";
    return;
  }
  const std::string& session_string_value = args[0].GetString();
  OpenForeignSessionWindows(web_ui(), session_string_value);
}

void ForeignSessionHandler::HandleOpenForeignSessionTab(
    const base::Value::List& args) {
  CHECK_EQ(args.size(), 7U);

  // Extract the session tag (always provided).
  if (!args[0].is_string()) {
    LOG(ERROR) << "Failed to extract session tag.";
    return;
  }
  const std::string& session_string_value = args[0].GetString();

  // Extract tab id.
  SessionID::id_type tab_id_value = 0;
  if (!args[1].is_string() ||
      !base::StringToInt(args[1].GetString(), &tab_id_value)) {
    LOG(ERROR) << "Failed to extract tab SessionID.";
    return;
  }

  SessionID tab_id = SessionID::FromSerializedValue(tab_id_value);
  if (!tab_id.is_valid()) {
    LOG(ERROR) << "Failed to deserialize tab ID.";
    return;
  }

  WindowOpenDisposition disposition = webui::GetDispositionFromClick(args, 2);
  OpenForeignSessionTab(web_ui(), session_string_value, tab_id, disposition);
}

void ForeignSessionHandler::HandleDeleteForeignSession(
    const base::Value::List& args) {
  if (args.size() != 1U) {
    LOG(ERROR) << "Wrong number of args to deleteForeignSession";
    return;
  }

  // Get the session tag argument (required).
  if (!args[0].is_string()) {
    LOG(ERROR) << "Unable to extract session tag";
    return;
  }
  const std::string& session_tag = args[0].GetString();

  sync_sessions::OpenTabsUIDelegate* open_tabs =
      GetOpenTabsUIDelegate(web_ui());
  if (open_tabs)
    open_tabs->DeleteForeignSession(session_tag);
}

void ForeignSessionHandler::HandleSetForeignSessionCollapsed(
    const base::Value::List& args) {
  if (args.size() != 2U) {
    LOG(ERROR) << "Wrong number of args to setForeignSessionCollapsed";
    return;
  }

  // Get the session tag argument (required).
  if (!args[0].is_string()) {
    LOG(ERROR) << "Unable to extract session tag";
    return;
  }
  const std::string& session_tag = args[0].GetString();

  if (!args[1].is_bool()) {
    LOG(ERROR) << "Unable to extract boolean argument";
    return;
  }
  const bool is_collapsed = args[1].GetBool();

  // Store session tags for collapsed sessions in a preference so that the
  // collapsed state persists.
  PrefService* prefs = Profile::FromWebUI(web_ui())->GetPrefs();
  ScopedDictPrefUpdate update(prefs, prefs::kNtpCollapsedForeignSessions);
  if (is_collapsed)
    update->Set(session_tag, true);
  else
    update->Remove(session_tag);
}

}  // namespace browser_sync
