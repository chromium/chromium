// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/side_panel/user_note/user_note_ui_coordinator.h"

#include <memory>

#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_user_data.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/browser/ui/views/bubble/bubble_contents_wrapper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel_content_proxy.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_registry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/browser/ui/views/side_panel/side_panel_web_ui_view.h"
#include "chrome/browser/ui/webui/side_panel/user_notes/user_notes_side_panel_ui.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/user_notes/interfaces/user_notes_ui.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/view_class_properties.h"

using SidePanelWebUIViewT_UserNotesSidePanelUI =
    SidePanelWebUIViewT<UserNotesSidePanelUI>;
BEGIN_TEMPLATE_METADATA(SidePanelWebUIViewT_UserNotesSidePanelUI,
                        SidePanelWebUIViewT)
END_METADATA

// static
void UserNoteUICoordinator::CreateForBrowser(Browser* browser) {
  DCHECK(browser);
  if (!FromBrowser(browser)) {
    browser->SetUserData(user_notes::UserNotesUI::UserDataKey(),
                         base::WrapUnique(new UserNoteUICoordinator(browser)));
  }
}

// static
UserNoteUICoordinator* UserNoteUICoordinator::FromBrowser(Browser* browser) {
  DCHECK(browser);
  return static_cast<UserNoteUICoordinator*>(
      browser->GetUserData(user_notes::UserNotesUI::UserDataKey()));
}

// static
UserNoteUICoordinator* UserNoteUICoordinator::GetOrCreateForBrowser(
    Browser* browser) {
  if (auto* data = FromBrowser(browser)) {
    return data;
  }

  CreateForBrowser(browser);
  return FromBrowser(browser);
}

UserNoteUICoordinator::UserNoteUICoordinator(Browser* browser)
    : browser_(browser) {}

UserNoteUICoordinator::~UserNoteUICoordinator() = default;

void UserNoteUICoordinator::SwitchTabsAndStartNoteCreation(int tab_index) {
  if (browser_->tab_strip_model()->active_index() == tab_index) {
    StartNoteCreation();
  } else {
    // If the notes ui already exists we should wait until after the tab change
    // to start the note creation so it happens for the correct tab's ui.
    // TODO(crbug.com/1416974): investigate when the page takes focus after tab
    // change.
    if (notes_ui_) {
      Show();
      notes_ui_->StartNoteCreation(/*wait_for_tab_change=*/true);
    }
    browser_->tab_strip_model()->ActivateTabAt(
        tab_index, TabStripUserGestureDetails(
                       TabStripUserGestureDetails::GestureType::kOther));
    if (!notes_ui_) {
      StartNoteCreation();
    }
  }
}

void UserNoteUICoordinator::StartNoteCreation() {
  Show();
  DCHECK(notes_ui_);
  notes_ui_->StartNoteCreation(/* wait_for_tab_change= */ false);
}

void UserNoteUICoordinator::Show() {
  SidePanelUI::GetSidePanelUIForBrowser(browser_)->Show(
      SidePanelEntry::Id::kUserNote);
}

void UserNoteUICoordinator::CreateAndRegisterEntry(
    SidePanelRegistry* global_registry) {
  global_registry->Register(std::make_unique<SidePanelEntry>(
      SidePanelEntry::Id::kUserNote,
      l10n_util::GetStringUTF16(IDS_USER_NOTE_TITLE),
      ui::ImageModel::FromVectorIcon(kNoteOutlineIcon, ui::kColorIcon,
                                     /*icon_size=*/16),
      base::BindRepeating(&UserNoteUICoordinator::CreateUserNotesWebUIView,
                          base::Unretained(this))));
}

std::unique_ptr<views::View> UserNoteUICoordinator::CreateUserNotesWebUIView() {
  auto wrapper = std::make_unique<BubbleContentsWrapperT<UserNotesSidePanelUI>>(
      GURL(chrome::kChromeUIUserNotesSidePanelURL), browser_->profile(),
      IDS_USER_NOTE_TITLE,
      /*webui_resizes_host=*/false,
      /*esc_closes_ui=*/false);
  auto* raw_wrapper = wrapper.get();
  auto view = std::make_unique<SidePanelWebUIViewT<UserNotesSidePanelUI>>(
      base::RepeatingClosure(), base::RepeatingClosure(), std::move(wrapper));
  view->SetProperty(views::kElementIdentifierKey,
                    kUserNotesSidePanelWebViewElementId);
  // Need to set browser after SidePanelWebUIViewT is constructed since it
  // creates the WebUIController.
  notes_ui_ = raw_wrapper->GetWebUIController()->GetWeakPtr();
  DCHECK(notes_ui_);
  notes_ui_->set_browser(browser_);
  // TODO(corising): Remove this and appropriately update availability based on
  // notes ui readiness.
  view->SetVisible(true);
  SidePanelUtil::GetSidePanelContentProxy(view.get())->SetAvailable(true);
  return view;
}
