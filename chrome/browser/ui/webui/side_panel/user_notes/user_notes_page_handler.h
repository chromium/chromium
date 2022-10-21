// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_USER_NOTES_USER_NOTES_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_USER_NOTES_USER_NOTES_PAGE_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/webui/side_panel/user_notes/user_notes.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class UserNotesSidePanelUI;
class Profile;

class UserNotesPageHandler : public side_panel::mojom::UserNotesPageHandler {
 public:
  explicit UserNotesPageHandler(
      mojo::PendingReceiver<side_panel::mojom::UserNotesPageHandler> receiver,
      Profile* profile,
      UserNotesSidePanelUI* user_notes_ui);
  UserNotesPageHandler(const UserNotesPageHandler&) = delete;
  UserNotesPageHandler& operator=(const UserNotesPageHandler&) = delete;
  ~UserNotesPageHandler() override;

  // side_panel::mojom::UserNotesPageHandler:
  void ShowUI() override;

 private:
  mojo::Receiver<side_panel::mojom::UserNotesPageHandler> receiver_;
  raw_ptr<UserNotesSidePanelUI> user_notes_ui_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_USER_NOTES_USER_NOTES_PAGE_HANDLER_H_
