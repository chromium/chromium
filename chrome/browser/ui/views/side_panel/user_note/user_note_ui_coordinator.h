// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_USER_NOTE_USER_NOTE_UI_COORDINATOR_H_
#define CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_USER_NOTE_USER_NOTE_UI_COORDINATOR_H_

#include <memory>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_user_data.h"
#include "ui/views/view.h"
#include "ui/views/view_observer.h"

class SidePanelRegistry;

class UserNoteUICoordinator : public BrowserUserData<UserNoteUICoordinator> {
 public:
  UserNoteUICoordinator(const UserNoteUICoordinator&) = delete;
  UserNoteUICoordinator& operator=(const UserNoteUICoordinator&) = delete;
  ~UserNoteUICoordinator() override;

  void CreateAndRegisterEntry(SidePanelRegistry* global_registry);

 private:
  friend class BrowserUserData<UserNoteUICoordinator>;

  explicit UserNoteUICoordinator(Browser* browser);

  void CreateSidePanelEntry(SidePanelRegistry* global_registry);
  std::unique_ptr<views::View> CreateUserNotesWebUIView();

  BROWSER_USER_DATA_KEY_DECL();
};

#endif  // CHROME_BROWSER_UI_VIEWS_SIDE_PANEL_USER_NOTE_USER_NOTE_UI_COORDINATOR_H_
