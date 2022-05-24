// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_USER_NOTE_USER_NOTE_UI_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_USER_NOTE_USER_NOTE_UI_COORDINATOR_H_

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_user_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/user_notes/interfaces/user_notes_ui.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/view.h"

namespace user_notes {
class UserNoteInstance;
}

class SidePanelRegistry;

class UserNoteUICoordinator : public user_notes::UserNotesUI,
                              public TabStripModelObserver,
                              public BrowserUserData<UserNoteUICoordinator> {
 public:
  explicit UserNoteUICoordinator(Browser* browser);

  UserNoteUICoordinator(const UserNoteUICoordinator&) = delete;
  UserNoteUICoordinator& operator=(const UserNoteUICoordinator&) = delete;
  ~UserNoteUICoordinator() override;

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kScrollViewElementIdForTesting);

  void CreateAndRegisterEntry(SidePanelRegistry* global_registry);
  void OnNoteDeleted(const base::UnguessableToken& id,
                     UserNoteView* user_note_view);
  void OnNoteCreationDone(const base::UnguessableToken& id,
                          const std::string& note_content);
  void OnNoteCreationCancelled(const base::UnguessableToken& id);

  // UserNoteUI overrides
  void FocusNote(const std::string& guid) override;
  void StartNoteCreation(user_notes::UserNoteInstance* instance) override;
  void Invalidate() override;
  void Show() override;

  // TabStripModelObserver overrides
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

 private:
  friend class BrowserUserData<UserNoteUICoordinator>;

  void CreateSidePanelEntry(SidePanelRegistry* global_registry);
  std::unique_ptr<views::View> CreateUserNotesView();

  Browser* browser_;
  views::View* scroll_contents_view_;

  BROWSER_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_USER_NOTE_USER_NOTE_UI_COORDINATOR_H_
