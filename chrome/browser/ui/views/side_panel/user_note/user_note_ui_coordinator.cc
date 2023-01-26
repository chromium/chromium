// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/user_note/user_note_ui_coordinator.h"

#include <memory>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/views/bubble/bubble_contents_wrapper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_content_proxy.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/side_panel/user_notes/user_notes_side_panel_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

UserNoteUICoordinator::UserNoteUICoordinator(Browser* browser)
    : BrowserUserData<UserNoteUICoordinator>(*browser) {}

UserNoteUICoordinator::~UserNoteUICoordinator() = default;

void UserNoteUICoordinator::CreateAndRegisterEntry(
    SidePanelRegistry* global_registry) {
  global_registry->Register(std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kUserNote,
      l10n_util::GetStringUTF16(IDS_USER_NOTE_TITLE),
      ui::ImageModel::FromVectorIcon(kInkHighlighterIcon, ui::kColorIcon),
      base::BindRepeating(&UserNoteUICoordinator::CreateUserNotesWebUIView,
                          base::Unretained(this))));
}

std::unique_ptr<views::View> UserNoteUICoordinator::CreateUserNotesWebUIView() {
  auto wrapper = std::make_unique<BubbleContentsWrapperT<UserNotesSidePanelUI>>(
      GURL(chrome::kChromeUIUserNotesSidePanelURL), GetBrowser().profile(),
      IDS_USER_NOTE_TITLE,
      /*webui_resizes_host=*/false,
      /*esc_closes_ui=*/false);
  auto* raw_wrapper = wrapper.get();
  auto view = std::make_unique<SidePanelWebUIViewT<UserNotesSidePanelUI>>(
      base::RepeatingClosure(), base::RepeatingClosure(), std::move(wrapper));
  // Need to set browser after SidePanelWebUIViewT is constructed since it
  // creates the WebUIController.
  UserNotesSidePanelUI* notes_ui = raw_wrapper->GetWebUIController();
  DCHECK(notes_ui);
  notes_ui->set_browser(&GetBrowser());
  // TODO(corising): Remove this and appropriately update availability based on
  // notes ui readiness.
  view->SetVisible(true);
  SidePanelUtil::GetSidePanelContentProxy(view.get())->SetAvailable(true);
  return view;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(UserNoteUICoordinator);
