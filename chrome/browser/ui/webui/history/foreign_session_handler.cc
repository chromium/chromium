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

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/i18n/time_formatting.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sessions/session_restore.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/session_sync_service_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/side_panel/tabs_from_other_devices/tabs_from_other_devices_side_panel_metrics.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/browser/ui/webui/side_panel/tabs_from_other_devices/tabs_from_other_devices_side_panel_ui.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_device_info/device_info_tracker.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_sessions/synced_session.h"
#include "content/public/browser/url_data_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/base/l10n/time_format.h"
#include "ui/base/mojom/window_open_disposition.mojom.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/base/window_open_disposition_utils.h"

namespace browser_sync {

namespace {

// Maximum number of sessions we're going to display on the NTP
const size_t kMaxSessionsToShow = 10;

// Strings used to set the direction of the HTML document based on locale.
const char kRTLHtmlTextDirection[] = "rtl";
const char kLTRHtmlTextDirection[] = "ltr";

// Helper method to create Mojom objects from Session objects.
history::mojom::ForeignSessionTabPtr SessionTabToMojom(
    const ::sessions::SessionTab& tab) {
  if (tab.navigations.empty()) {
    return nullptr;
  }

  int selected_index = std::min(tab.current_navigation_index,
                                static_cast<int>(tab.navigations.size() - 1));
  const ::sessions::SerializedNavigationEntry& current_navigation =
      tab.navigations.at(selected_index);
  GURL tab_url = current_navigation.virtual_url();
  if (!tab_url.is_valid() || tab_url == chrome::ChromeUINewTabURLAsGURL()) {
    return nullptr;
  }

  auto tab_mojom = history::mojom::ForeignSessionTab::New();
  bool using_url_as_the_title = false;
  tab_mojom->title = base::UTF16ToUTF8(current_navigation.title());
  if (tab_mojom->title.empty()) {
    using_url_as_the_title = true;
    tab_mojom->title = tab_url.spec();
  }

  tab_mojom->url = tab_url;
  tab_mojom->remote_icon_url_for_uma = current_navigation.favicon_url().spec();
  tab_mojom->timestamp =
      tab.timestamp.ToDeltaSinceWindowsEpoch().InMicrosecondsF();
  tab_mojom->timestamp_display_str = base::UTF16ToUTF8(
      ForeignSessionHandler::FormatSessionTime(tab.timestamp));
  tab_mojom->session_id = tab.tab_id.id();

  // Set the "direction" attribute of the title so that in RTL locales, a LTR
  // title is rendered left-to-right and truncated from the right. For example,
  // the title of http://msdn.microsoft.com/en-us/default.aspx is "MSDN:
  // Microsoft developer network". In RTL locales, if the "direction" of this
  // title is not specified, it takes Chrome UI's directionality. So the title
  // will be truncated as "soft developer network". Setting the "direction"
  // attribute as "ltr" renders the truncated title as "MSDN: Microsoft D...".
  // As another example, the title of http://yahoo.com is "Yahoo!". In RTL
  // locales, the title will be rendered as "!Yahoo" if its "direction"
  // attribute is not set to "ltr".
  if (using_url_as_the_title) {
    tab_mojom->direction = kLTRHtmlTextDirection;
  } else {
    tab_mojom->direction =
        base::i18n::IsRTL() && base::i18n::StringContainsStrongRTLChars(
                                   current_navigation.title())
            ? kRTLHtmlTextDirection
            : kLTRHtmlTextDirection;
  }
  return tab_mojom;
}

// Helper method to create Mojom objects from SessionWindow objects.
history::mojom::ForeignSessionWindowPtr SessionWindowToMojom(
    const ::sessions::SessionWindow& window) {
  if (window.tabs.empty()) {
    return nullptr;
  }

  std::vector<history::mojom::ForeignSessionTabPtr> tabs;
  // Calculate the last |modification_time| for all entries within a window.
  base::Time modification_time = window.timestamp;
  for (const std::unique_ptr<sessions::SessionTab>& tab : window.tabs) {
    history::mojom::ForeignSessionTabPtr tab_mojom = SessionTabToMojom(*tab);
    if (tab_mojom) {
      modification_time = std::max(modification_time, tab->timestamp);
      tab_mojom->window_id = window.window_id.id();
      tabs.push_back(std::move(tab_mojom));
    }
  }
  if (tabs.empty()) {
    return nullptr;
  }

  auto window_mojom = history::mojom::ForeignSessionWindow::New();
  window_mojom->timestamp =
      modification_time.ToDeltaSinceWindowsEpoch().InMicrosecondsF();
  window_mojom->session_id = window.window_id.id();
  window_mojom->tabs = std::move(tabs);
  return window_mojom;
}

std::string GetDeviceNameSuffixFromSyncUserAgent(
    const std::string& user_agent) {
  if (user_agent.find("channel(canary)") != std::string::npos) {
    return " (Canary)";
  }
  if (user_agent.find("channel(dev)") != std::string::npos) {
    return " (Dev)";
  }
  if (user_agent.find("channel(beta)") != std::string::npos) {
    return " (Beta)";
  }
  if (user_agent.find("-devel") != std::string::npos) {
    return " (developer build)";
  }
  return "";
}

void FilterStableChannelSessions(
    const syncer::DeviceInfoTracker& device_info_tracker,
    std::vector<raw_ptr<const sync_sessions::SyncedSession,
                        VectorExperimental>>& sessions) {
  std::erase_if(sessions, [&](const sync_sessions::SyncedSession* session) {
    const syncer::DeviceInfo* device_info =
        device_info_tracker.GetDeviceInfo(session->GetSessionTag());
    return device_info && device_info->sync_user_agent().find(
                              "channel(stable)") != std::string::npos;
  });
}

}  // namespace

ForeignSessionHandler::ForeignSessionHandler(
    mojo::PendingReceiver<history::mojom::ForeignSessionPageHandler>
        pending_page_handler,
    Profile* profile,
    content::WebContents* web_contents,
    RestoreForeignSessionTabCallback restore_tab_callback,
    RestoreForeignSessionWindowsCallback restore_windows_callback,
    TabsFromOtherDevicesSidePanelUI* side_panel_ui)
    : profile_(profile),
      web_contents_(web_contents),
      receiver_(this, std::move(pending_page_handler)),
      side_panel_ui_(side_panel_ui),
      restore_tab_callback_(std::move(restore_tab_callback)),
      restore_windows_callback_(std::move(restore_windows_callback)) {
  CHECK(restore_tab_callback_);
  CHECK(restore_windows_callback_);

  sync_sessions::SessionSyncService* service =
      SessionSyncServiceFactory::GetInstance()->GetForProfile(profile_);

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

ForeignSessionHandler::~ForeignSessionHandler() = default;

// static
void ForeignSessionHandler::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kNtpCollapsedForeignSessions);
}

// static
sync_sessions::OpenTabsUIDelegate* ForeignSessionHandler::GetOpenTabsUIDelegate(
    Profile* profile) {
  sync_sessions::SessionSyncService* service =
      SessionSyncServiceFactory::GetInstance()->GetForProfile(profile);
  return service ? service->GetOpenTabsUIDelegate() : nullptr;
}

void ForeignSessionHandler::SetPage(
    mojo::PendingRemote<history::mojom::ForeignSessionPage> pending_page) {
  page_.Bind(std::move(pending_page));

  if (side_panel_ui_) {
    base::WeakPtr<TopChromeWebUIController::Embedder> embedder =
        side_panel_ui_->embedder();
    if (embedder) {
      embedder->ShowUI();
    }
  }
}

void ForeignSessionHandler::GetForeignSessions(
    GetForeignSessionsCallback callback) {
  std::move(callback).Run(GetForeignSessionsInternal());
}

void ForeignSessionHandler::OpenForeignSessionAllTabs(
    const std::string& session_tag) {
  // This is not used by the side panel. If it becomes used in the future, the
  // metrics should be updated to cover this case.
  CHECK(!side_panel_ui_);

  sync_sessions::OpenTabsUIDelegate* open_tabs =
      GetOpenTabsUIDelegate(profile_);
  if (!open_tabs) {
    return;
  }

  // Note: we don't own the ForeignSessions themselves.
  std::vector<const ::sessions::SessionWindow*> windows =
      open_tabs->GetForeignSession(session_tag);
  if (windows.empty()) {
    DVLOG(1) << "ForeignSessionHandler failed to get session data from"
                "OpenTabsUIDelegate.";
    return;
  }

  restore_windows_callback_.Run(profile_, windows);
}

void ForeignSessionHandler::OpenForeignSessionTab(
    const std::string& session_tag,
    int32_t tab_id_value,
    ui::mojom::ClickModifiersPtr modifiers) {
  sync_sessions::OpenTabsUIDelegate* open_tabs =
      GetOpenTabsUIDelegate(profile_);
  if (!open_tabs) {
    return;
  }

  SessionID tab_id = SessionID::FromSerializedValue(tab_id_value);
  if (!tab_id.is_valid()) {
    DVLOG(1) << "Failed to deserialize tab ID.";
    return;
  }

  const ::sessions::SessionTab* tab;
  if (!open_tabs->GetForeignTab(session_tag, tab_id, &tab)) {
    DVLOG(1) << "Failed to load foreign tab.";
    return;
  }
  if (tab->navigations.empty()) {
    DVLOG(1) << "Foreign tab no longer has valid navigations.";
    return;
  }

  WindowOpenDisposition disposition = ui::DispositionFromClick(
      modifiers->middle_button, modifiers->alt_key, modifiers->ctrl_key,
      modifiers->meta_key, modifiers->shift_key);

  // If this is in the side panel, `web_contents_` refers to the content of the
  // side panel, *not* the main tab where the foreign tab should be restored.
  content::WebContents* web_contents =
      side_panel_ui_ ? side_panel_ui_->browser_window_interface()
                           ->GetTabStripModel()
                           ->GetActiveWebContents()
                     : web_contents_.get();
  restore_tab_callback_.Run(web_contents, *tab, disposition);

  if (side_panel_ui_ && side_panel_ui_->metrics_recorder()) {
    side_panel_ui_->metrics_recorder()->RecordTabOpened();
  }
}

void ForeignSessionHandler::DeleteForeignSession(
    const std::string& session_tag) {
  sync_sessions::OpenTabsUIDelegate* open_tabs =
      GetOpenTabsUIDelegate(profile_);
  if (open_tabs) {
    open_tabs->DeleteForeignSession(session_tag);
  }
}

void ForeignSessionHandler::SetForeignSessionCollapsed(
    const std::string& session_tag,
    bool collapsed) {
  // Store session tags for collapsed sessions in a preference so that the
  // collapsed state persists.
  PrefService* prefs = profile_->GetPrefs();
  ScopedDictPrefUpdate update(prefs, prefs::kNtpCollapsedForeignSessions);
  if (collapsed) {
    update->Set(session_tag, true);
  } else {
    update->Remove(session_tag);
  }
}

void ForeignSessionHandler::OnForeignSessionUpdated() {
  if (page_) {
    page_->OnForeignSessionsChanged(GetForeignSessionsInternal());
  }
}

// static
std::u16string ForeignSessionHandler::FormatSessionTime(
    const base::Time& time) {
  // Return a time like "1 hour ago", "2 days ago", etc.
  base::Time now = base::Time::Now();
  // TimeFormat does not support negative TimeDelta values, so then we use 0.
  return ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_ELAPSED,
                                ui::TimeFormat::LENGTH_SHORT,
                                now < time ? base::TimeDelta() : now - time);
}

std::vector<history::mojom::ForeignSessionPtr>
ForeignSessionHandler::GetForeignSessionsInternal() {
  sync_sessions::OpenTabsUIDelegate* open_tabs =
      GetOpenTabsUIDelegate(profile_);
  std::vector<raw_ptr<const sync_sessions::SyncedSession, VectorExperimental>>
      sessions;

  std::vector<history::mojom::ForeignSessionPtr> session_list;
  if (open_tabs && open_tabs->GetAllForeignSessions(&sessions)) {
    // Use a pref to keep track of sessions that were collapsed by the user.
    // To prevent the pref from accumulating stale sessions, clear it each time
    // and only add back sessions that are still current.
    ScopedDictPrefUpdate pref_update(profile_->GetPrefs(),
                                     prefs::kNtpCollapsedForeignSessions);
    base::DictValue& current_collapsed_sessions = *pref_update;
    base::DictValue collapsed_sessions = current_collapsed_sessions.Clone();
    current_collapsed_sessions.clear();

    std::map<std::string, int> name_counts;
    const syncer::DeviceInfoTracker* device_info_tracker = nullptr;
    if (side_panel_ui_) {
      syncer::DeviceInfoSyncService* device_info_sync_service =
          DeviceInfoSyncServiceFactory::GetForProfile(profile_);
      if (device_info_sync_service) {
        device_info_tracker = device_info_sync_service->GetDeviceInfoTracker();
      }

      if (base::FeatureList::IsEnabled(
              features::kTabsFromOtherDevicesSidePanelExcludeStableChannel) &&
          device_info_tracker) {
        FilterStableChannelSessions(*device_info_tracker, sessions);
      }

      for (const sync_sessions::SyncedSession* session : sessions) {
        ++name_counts[session->GetSessionName()];
      }
    }

    // Note: we don't own the SyncedSessions themselves.
    for (size_t i = 0; i < sessions.size() && i < kMaxSessionsToShow; ++i) {
      const sync_sessions::SyncedSession* session = sessions[i];
      const std::string& session_tag = session->GetSessionTag();
      auto session_mojom = history::mojom::ForeignSession::New();
      session_mojom->tag = session_tag;
      session_mojom->name = session->GetSessionName();
      if (side_panel_ui_ && name_counts[session_mojom->name] > 1 &&
          device_info_tracker) {
        const syncer::DeviceInfo* device_info =
            device_info_tracker->GetDeviceInfo(session_tag);
        if (device_info) {
          session_mojom->name += GetDeviceNameSuffixFromSyncUserAgent(
              device_info->sync_user_agent());
        }
      }
      session_mojom->modified_time =
          base::UTF16ToUTF8(FormatSessionTime(session->GetModifiedTime()));
      session_mojom->timestamp =
          session->GetModifiedTime().InMillisecondsFSinceUnixEpoch();

      bool is_collapsed = collapsed_sessions.contains(session_tag);
      session_mojom->collapsed = is_collapsed;
      if (is_collapsed) {
        current_collapsed_sessions.Set(session_tag, true);
      }

      std::vector<history::mojom::ForeignSessionWindowPtr> windows;

      // Order tabs by visual order within window.
      for (const auto& window_pair : session->windows) {
        history::mojom::ForeignSessionWindowPtr window_mojom =
            SessionWindowToMojom(window_pair.second->wrapped_window);
        if (window_mojom) {
          windows.push_back(std::move(window_mojom));
        }
      }

      session_mojom->windows = std::move(windows);
      session_list.push_back(std::move(session_mojom));
    }
  }
  return session_list;
}

}  // namespace browser_sync
