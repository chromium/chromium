// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_HISTORY_FOREIGN_SESSION_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_HISTORY_FOREIGN_SESSION_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/sessions/session_service.h"
#include "components/sync_sessions/open_tabs_ui_delegate.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/base/window_open_disposition.h"
#include "ui/webui/resources/cr_components/history/foreign_sessions.mojom.h"

namespace sessions {
struct SessionTab;
}

namespace content {
class WebContents;
}  // namespace content

namespace user_prefs {
class PrefRegistrySyncable;
}

class TabsFromOtherDevicesSidePanelUI;

namespace browser_sync {

// Keep in sync with //chrome/browser/resources/history/constants.js.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class SyncedTabsHistogram {
  INITIALIZED = 0,
  SHOW_MENU_DEPRECATED = 1,
  LINK_CLICKED = 2,
  LINK_RIGHT_CLICKED = 3,
  SESSION_NAME_RIGHT_CLICKED_DEPRECATED = 4,
  SHOW_SESSION_MENU = 5,
  COLLAPSE_SESSION = 6,
  EXPAND_SESSION = 7,
  OPEN_ALL = 8,
  HAS_FOREIGN_DATA = 9,
  HIDE_FOR_NOW = 10,
  OPENED_LINK_VIA_CONTEXT_MENU = 11,
  LIMIT = 12  // Should always be the last one.
};

class ForeignSessionHandler : public history::mojom::ForeignSessionPageHandler {
 public:
  using RestoreForeignSessionTabCallback =
      base::RepeatingCallback<void(content::WebContents* source_web_contents,
                                   const ::sessions::SessionTab& tab,
                                   WindowOpenDisposition disposition)>;

  using RestoreForeignSessionWindowsCallback = base::RepeatingCallback<void(
      Profile* profile,
      const std::vector<const ::sessions::SessionWindow*>& windows)>;

  ForeignSessionHandler(
      mojo::PendingReceiver<history::mojom::ForeignSessionPageHandler>
          pending_page_handler,
      Profile* profile,
      content::WebContents* web_contents,
      RestoreForeignSessionTabCallback restore_tab_callback,
      RestoreForeignSessionWindowsCallback restore_windows_callback,
      TabsFromOtherDevicesSidePanelUI* side_panel_ui);

  ForeignSessionHandler(const ForeignSessionHandler&) = delete;
  ForeignSessionHandler& operator=(const ForeignSessionHandler&) = delete;

  ~ForeignSessionHandler() override;

  // history::mojom::ForeignSessionPageHandler implementation.
  void SetPage(mojo::PendingRemote<history::mojom::ForeignSessionPage>
                   pending_page) override;
  void GetForeignSessions(GetForeignSessionsCallback callback) override;
  void OpenForeignSessionAllTabs(const std::string& session_tag) override;
  void OpenForeignSessionTab(const std::string& session_tag,
                             int32_t tab_id,
                             ui::mojom::ClickModifiersPtr modifiers) override;
  void DeleteForeignSession(const std::string& session_tag) override;
  void SetForeignSessionCollapsed(const std::string& session_tag,
                                  bool collapsed) override;

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Returns a pointer to the current session model associator or nullptr.
  static sync_sessions::OpenTabsUIDelegate* GetOpenTabsUIDelegate(
      Profile* profile);

  // Returns a string used to show the user when a session or tab was last
  // modified (e.g. "2 hours ago").
  static std::u16string FormatSessionTime(const base::Time& time);

 private:
  void OnForeignSessionUpdated();

  std::vector<history::mojom::ForeignSessionPtr> GetForeignSessionsInternal();

  raw_ptr<Profile> profile_;

  // The WebContents that hosts the WebUI. If opened in a regular tab
  // (chrome://history/syncedTabs), this corresponds to that tab. If opened in
  // the side panel, this corresponds to the side panel's contents.
  raw_ptr<content::WebContents> web_contents_;

  // Interface to send information to the web ui page.
  mojo::Remote<history::mojom::ForeignSessionPage> page_;
  // Allows handling received messages from the web ui page.
  mojo::Receiver<history::mojom::ForeignSessionPageHandler> receiver_;

  // Non-null if the handler is being used by the side panel UI.
  raw_ptr<TabsFromOtherDevicesSidePanelUI> side_panel_ui_;

  RestoreForeignSessionTabCallback restore_tab_callback_;
  RestoreForeignSessionWindowsCallback restore_windows_callback_;

  base::CallbackListSubscription foreign_session_updated_subscription_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_UI_WEBUI_HISTORY_FOREIGN_SESSION_HANDLER_H_
