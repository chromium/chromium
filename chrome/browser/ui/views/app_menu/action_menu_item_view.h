// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_MENU_ITEM_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_MENU_ITEM_VIEW_H_

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "ui/actions/actions.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/menu/menu_item_view.h"

// A custom MenuItemView whose visual attributes are bound dynamically to an
// actions::ActionItem.
class ActionMenuItemView : public views::MenuItemView {
  METADATA_HEADER(ActionMenuItemView, views::MenuItemView)

 public:
  ActionMenuItemView(views::MenuItemView* parent,
                     actions::ActionItem* action_item,
                     views::MenuItemView::Type type);
  ActionMenuItemView(const ActionMenuItemView&) = delete;
  ActionMenuItemView& operator=(const ActionMenuItemView&) = delete;
  ~ActionMenuItemView() override;

  actions::ActionItem* action_item() const { return action_item_; }

 private:
  // Updates attributes according to the ActionItem that corresponds to this
  // view.
  void UpdateFromActionItem();

  raw_ptr<actions::ActionItem> action_item_ = nullptr;
  base::CallbackListSubscription action_changed_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_MENU_ITEM_VIEW_H_
