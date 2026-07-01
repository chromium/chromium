// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/app_menu/action_menu_item_view.h"

#include <string>

#include "base/check.h"
#include "base/functional/bind.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/image/image_skia.h"

ActionMenuItemView::ActionMenuItemView(views::MenuItemView* parent,
                                       actions::ActionItem* action_item,
                                       views::MenuItemView::Type type)
    : views::MenuItemView(
          parent,
          action_item ? action_item->GetActionId().value_or(0) : 0,
          type),
      action_item_(action_item) {
  CHECK(action_item_);

  // Initial view attributes synchronization.
  UpdateFromActionItem();

  // Bind the view and its respective action item so that it updates
  // automatically.
  action_changed_subscription_ =
      action_item_->AddActionChangedCallback(base::BindRepeating(
          &ActionMenuItemView::UpdateFromActionItem, base::Unretained(this)));
}

ActionMenuItemView::~ActionMenuItemView() {
  action_item_ = nullptr;
}

// Updates the view with the current state of its corresponding action item.
void ActionMenuItemView::UpdateFromActionItem() {
  if (!action_item_) {
    return;
  }
  SetTitle(std::u16string(action_item_->GetText()));
  if (!action_item_->GetImage().IsEmpty()) {
    SetIcon(action_item_->GetImage());
  }
  SetEnabled(action_item_->GetEnabled());
  SetVisible(action_item_->GetVisible());
}

BEGIN_METADATA(ActionMenuItemView)
END_METADATA
