// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_USER_NOTE_USER_NOTE_UI_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_USER_NOTE_USER_NOTE_UI_COORDINATOR_H_

#include <memory>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_user_data.h"
#include "components/user_notes/interfaces/user_notes_ui.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

class SidePanelRegistry;
class UserNotesSidePanelUI;

class UserNoteUICoordinator : public user_notes::UserNotesUI {
 public:
  // Creates a UserNoteUICoordinator and attaches it to the specified Browser
  // using the user data key of UserNotesUI. If an instance is already attached,
  // does nothing.
  static void CreateForBrowser(Browser* browser);

  // Retrieves the UserNoteUICoordinator instance that was attached to the
  // specified Browser (via CreateForBrowser above) and returns it. If no
  // instance of the type was attached, returns nullptr.
  static UserNoteUICoordinator* FromBrowser(Browser* browser);

  static UserNoteUICoordinator* GetOrCreateForBrowser(Browser* browser);

  UserNoteUICoordinator(const UserNoteUICoordinator&) = delete;
  UserNoteUICoordinator& operator=(const UserNoteUICoordinator&) = delete;
  ~UserNoteUICoordinator() override;

  // UserNoteUI:
  void SwitchTabsAndStartNoteCreation(int tab_index) override;
  void StartNoteCreation() override;
  void Show() override;

  void CreateAndRegisterEntry(SidePanelRegistry* global_registry);

 private:
  explicit UserNoteUICoordinator(Browser* browser);

  void CreateSidePanelEntry(SidePanelRegistry* global_registry);
  std::unique_ptr<views::View> CreateUserNotesWebUIView();

  raw_ptr<Browser> browser_;
  // A weak reference to the last-created UI object for this browser.
  base::WeakPtr<UserNotesSidePanelUI> notes_ui_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_USER_NOTE_USER_NOTE_UI_COORDINATOR_H_
