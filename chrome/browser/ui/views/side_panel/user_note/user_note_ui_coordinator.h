// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_USER_NOTE_USER_NOTE_UI_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_USER_NOTE_USER_NOTE_UI_COORDINATOR_H_

#include <memory>

#include "base/scoped_observation.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_user_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "components/user_notes/interfaces/user_notes_ui.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace user_notes {
class UserNoteInstance;
}

class SidePanelRegistry;
class UserNoteView;

namespace user_notes {
class UserNoteInstance;
}

namespace views {
class ScrollView;
}

namespace base {
class UnguessableToken;
}

class UserNoteUICoordinator : public user_notes::UserNotesUI,
                              public TabStripModelObserver,
                              public views::ViewObserver,
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
  void OnNoteCreationCancelled(const base::UnguessableToken& id,
                               UserNoteView* user_note_view);
  void OnNoteUpdated(const base::UnguessableToken& id,
                     const std::string& note_content);

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

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;

 private:
  friend class BrowserUserData<UserNoteUICoordinator>;

  void CreateSidePanelEntry(SidePanelRegistry* global_registry);
  void ScrollToNote();
  std::unique_ptr<views::View> CreateUserNotesView();

  raw_ptr<Browser> browser_;
  raw_ptr<views::ScrollView> scroll_view_ = nullptr;
  base::ScopedObservation<views::View, views::ViewObserver>
      scoped_view_observer_{this};
  base::UnguessableToken scroll_to_note_id_ = base::UnguessableToken::Null();

  BROWSER_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_USER_NOTE_USER_NOTE_UI_COORDINATOR_H_
