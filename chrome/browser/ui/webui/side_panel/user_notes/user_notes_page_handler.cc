// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/user_notes/user_notes_page_handler.h"

#include "base/memory/ptr_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/side_panel/user_notes/user_notes_side_panel_ui.h"

UserNotesPageHandler::UserNotesPageHandler(
    mojo::PendingReceiver<side_panel::mojom::UserNotesPageHandler> receiver,
    Profile* profile,
    UserNotesSidePanelUI* user_notes_ui)
    : receiver_(this, std::move(receiver)), user_notes_ui_(user_notes_ui) {}

UserNotesPageHandler::~UserNotesPageHandler() = default;

void UserNotesPageHandler::ShowUI() {
  auto embedder = user_notes_ui_->embedder();
  if (embedder) {
    embedder->ShowUI();
  }
}
