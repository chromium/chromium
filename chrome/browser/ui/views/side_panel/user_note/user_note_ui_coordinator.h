// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_USER_NOTE_USER_NOTE_UI_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_USER_NOTE_USER_NOTE_UI_COORDINATOR_H_

#include <memory>

#include "base/scoped_observation.h"
#include "base/unguessable_token.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_observer.h"
#include "chrome/browser/ui/views/side_panel/side_panel_view_state_observer.h"
#include "components/user_notes/interfaces/user_notes_ui.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

namespace user_notes {
class UserNoteInstance;
}

class SidePanelRegistry;
class UserNoteView;
class BrowserView;
class TabStripModel;

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
                              public SidePanelViewStateObserver,
                              public SidePanelEntryObserver {
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

  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kScrollViewElementIdForTesting);

  void CreateAndRegisterEntry(SidePanelRegistry* global_registry);
  void OnNoteDeleted(const base::UnguessableToken& id,
                     UserNoteView* user_note_view);
  void OnNoteCreationDone(const base::UnguessableToken& id,
                          const std::u16string& note_content);
  void OnNoteCreationCancelled(const base::UnguessableToken& id,
                               UserNoteView* user_note_view);
  void OnNoteUpdated(const base::UnguessableToken& id,
                     const std::u16string& note_content);
  void OnNoteSelected(const base::UnguessableToken& id);

  // UserNoteUI overrides
  void FocusNote(const base::UnguessableToken& guid) override;
  void StartNoteCreation(user_notes::UserNoteInstance* instance) override;
  void InvalidateIfVisible() override;
  void Show() override;

  // TabStripModelObserver overrides
  void OnTabStripModelChanged(
      TabStripModel* tab_strip_model,
      const TabStripModelChange& change,
      const TabStripSelectionChange& selection) override;

  // views::ViewObserver:
  void OnViewBoundsChanged(views::View* observed_view) override;

  // SidePanelViewStateObserver
  void OnSidePanelDidClose() override;

  // SidePanelEntryObserver:
  void OnEntryShown(SidePanelEntry* entry) override;
  void OnEntryHidden(SidePanelEntry* entry) override;

 private:
  explicit UserNoteUICoordinator(Browser* browser);

  FRIEND_TEST_ALL_PREFIXES(UserNoteUICoordinatorTest,
                           CleanScrollViewOnSidePanelCloseWithoutNotes);
  FRIEND_TEST_ALL_PREFIXES(UserNoteUICoordinatorTest,
                           CleanScrollViewOnSidePanelCloseWithNotes);

  void CreateSidePanelEntry(SidePanelRegistry* global_registry);
  void ScrollToNote();
  std::unique_ptr<views::View> CreateUserNotesView();
  std::unique_ptr<views::View> CreateUserNotesWebUIView();
  void Invalidate();

  raw_ptr<Browser> browser_;
  raw_ptr<views::ScrollView> scroll_view_ = nullptr;
  base::ScopedObservation<views::View, views::ViewObserver>
      scoped_view_observer_{this};
  base::UnguessableToken scroll_to_note_id_ = base::UnguessableToken::Null();
  raw_ptr<BrowserView> browser_view_ = nullptr;
  bool is_tab_strip_model_observed_ = false;
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_USER_NOTE_USER_NOTE_UI_COORDINATOR_H_
