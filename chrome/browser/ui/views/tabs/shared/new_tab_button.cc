// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/shared/new_tab_button.h"

#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/events/event.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/view_class_properties.h"

namespace shared {

NewTabButton::NewTabButton(BrowserWindowInterface* browser,
                           const int button_size,
                           const int icon_size)
    : action_view_controller_(std::make_unique<views::ActionViewController>()),
      browser_(browser) {
  SetProperty(views::kElementIdentifierKey, kNewTabButtonElementId);

  SetHorizontalAlignment(gfx::HorizontalAlignment::ALIGN_CENTER);

  SetPreferredSize(gfx::Size(button_size, button_size));

  SetIconSize(icon_size);

  CHECK(browser_);
  CHECK(browser_->GetActions());
  actions::ActionItem* action_item = actions::ActionManager::Get().FindAction(
      kActionNewTab, browser_->GetActions()->root_action_item());
  CHECK(action_item);
  action_view_controller_->CreateActionViewRelationship(
      this, action_item->GetAsWeakPtr());
}

NewTabButton::~NewTabButton() = default;

void NewTabButton::OnMouseEvent(ui::MouseEvent* event) {
  if (event->IsOnlyMiddleMouseButton()) {
    if (event->type() == ui::EventType::kMousePressed) {
      chrome::NewTabFromClipboardURL(browser_);
    }
    event->SetHandled();
    return;
  }
  TabStripFlatEdgeButton::OnMouseEvent(event);
}

BEGIN_METADATA(NewTabButton)
END_METADATA

}  // namespace shared
