// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_USER_NOTES_USER_NOTES_SIDE_PANEL_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_USER_NOTES_USER_NOTES_SIDE_PANEL_UI_H_

#include "chrome/browser/ui/webui/side_panel/user_notes/user_notes.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "ui/webui/mojo_bubble_web_ui_controller.h"

class Browser;
class UserNotesPageHandler;

class UserNotesSidePanelUI
    : public ui::MojoBubbleWebUIController,
      public side_panel::mojom::UserNotesPageHandlerFactory {
 public:
  explicit UserNotesSidePanelUI(content::WebUI* web_ui);
  UserNotesSidePanelUI(const UserNotesSidePanelUI&) = delete;
  UserNotesSidePanelUI& operator=(const UserNotesSidePanelUI&) = delete;
  ~UserNotesSidePanelUI() override;

  void BindInterface(
      mojo::PendingReceiver<side_panel::mojom::UserNotesPageHandlerFactory>
          factory);

  void set_browser(Browser* browser) { browser_ = browser; }

 private:
  // user_notes::mojom::UserNotesPageHandlerFactory
  void CreatePageHandler(
      mojo::PendingRemote<side_panel::mojom::UserNotesPage> page,
      mojo::PendingReceiver<side_panel::mojom::UserNotesPageHandler> receiver)
      override;

  mojo::Receiver<side_panel::mojom::UserNotesPageHandlerFactory>
      page_factory_receiver_{this};

  std::unique_ptr<UserNotesPageHandler> user_notes_page_handler_;
  mojo::Receiver<side_panel::mojom::UserNotesPageHandlerFactory>
      user_notes_page_factory_receiver_{this};

  raw_ptr<Browser> browser_ = nullptr;

  WEB_UI_CONTROLLER_TYPE_DECL();
};

#endif  // CHROME_BROWSER_UI_WEBUI_SIDE_PANEL_USER_NOTES_USER_NOTES_SIDE_PANEL_UI_H_
