// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_USER_NOTES_USER_NOTES_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_USER_NOTES_USER_NOTES_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/webui/side_panel/user_notes/user_notes.mojom.h"
#include "components/power_bookmarks/common/power_bookmark_observer.h"
#include "components/power_bookmarks/core/power_bookmark_service.h"
#include "components/prefs/pref_change_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace bookmarks {
class BookmarkModel;
}

namespace power_bookmarks {
class PowerBookmarkService;
}

class Browser;
class UserNotesSidePanelUI;
class Profile;

class UserNotesPageHandler : public side_panel::mojom::UserNotesPageHandler,
                             public power_bookmarks::PowerBookmarkObserver,
                             public TabStripModelObserver,
                             public content::WebContentsObserver {
 public:
  explicit UserNotesPageHandler(
      mojo::PendingReceiver<side_panel::mojom::UserNotesPageHandler> receiver,
      mojo::PendingRemote<side_panel::mojom::UserNotesPage> page,
      Profile* profile,
      Browser* browser,
      bool start_creation_flow,
      UserNotesSidePanelUI* user_notes_ui);
  UserNotesPageHandler(const UserNotesPageHandler&) = delete;
  UserNotesPageHandler& operator=(const UserNotesPageHandler&) = delete;
  ~UserNotesPageHandler() override;

  // side_panel::mojom::UserNotesPageHandler:
  void ShowUI() override;
  void GetNoteOverviews(const std::string& user_input,
                        GetNoteOverviewsCallback callback) override;
  void GetNotesForCurrentTab(GetNotesForCurrentTabCallback callback) override;
  void NewNoteFinished(const std::string& text,
                       NewNoteFinishedCallback callback) override;
  void UpdateNote(const std::string& guid,
                  const std::string& text,
                  UpdateNoteCallback callback) override;
  void DeleteNote(const std::string& guid,
                  DeleteNoteCallback callback) override;
  void DeleteNotesForUrl(const ::GURL& url,
                         DeleteNotesForUrlCallback callback) override;
  void NoteOverviewSelected(
      const ::GURL& url,
      ui::mojom::ClickModifiersPtr click_modifiers) override;
  void SetSortOrder(bool sort_by_newest) override;
  void HasNotesInAnyPages(HasNotesInAnyPagesCallback callback) override;
  void OpenInNewTab(const ::GURL& url) override;
  void OpenInNewWindow(const ::GURL& url) override;
  void OpenInIncognitoWindow(const ::GURL& url) override;

  void OnSortByNewestPrefChanged();

  void StartNoteCreation(bool wait_for_tab_change);

  void SetCurrentTabUrlForTesting(GURL url) { current_tab_url_ = url; }

  GURL GetCurrentTabUrlForTesting() { return current_tab_url_; }

 private:
  // power_bookmarks::PowerBookmarkObserver:
  void OnPowersChanged() override;

  // TabStripModelObserver:
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // WebContentsObserver:
  void PrimaryPageChanged(content::Page& page) override;

  void UpdateCurrentTabUrl();
  void OpenUrl(const ::GURL& url, WindowOpenDisposition open_location);

  mojo::Receiver<side_panel::mojom::UserNotesPageHandler> receiver_;
  mojo::Remote<side_panel::mojom::UserNotesPage> page_;
  const raw_ptr<Profile> profile_;
  PrefChangeRegistrar pref_change_registrar_;
  const raw_ptr<power_bookmarks::PowerBookmarkService> service_;
  const raw_ptr<Browser> browser_;

  raw_ptr<UserNotesSidePanelUI> user_notes_ui_ = nullptr;

  // Use a week pointer here because BookmarkModel may outlive the callback in
  // `GetNoteOverviews`.
  base::WeakPtr<bookmarks::BookmarkModel> bookmark_model_;

  bool start_creation_after_tab_change_ = false;
  GURL current_tab_url_;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_USER_NOTES_USER_NOTES_PAGE_HANDLER_H_
