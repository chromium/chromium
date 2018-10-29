// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_HOVER_LIST_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_HOVER_LIST_VIEW_H_

#include <map>
#include <memory>

#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/browser/ui/webauthn/hover_list_model.h"
#include "ui/views/view.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace views {
class Separator;
}  // namespace views

class HoverButton;

// View that shows a list of items. Each item is rendered as a HoverButton with
// an icon, name, and chevron, like so:
//
//  +----------------------------------+
//  | ICON1 | Item 1 name          | > |
//  +----------------------------------+
//  | ICON2 | Item 2 name          | > |
//  +----------------------------------+
//  | ICON3 | Item 3 name          | > |
//  +----------------------------------+
//
class HoverListView : public views::View,
                      public views::ButtonListener,
                      public HoverListModel::Observer {
 public:
  explicit HoverListView(std::unique_ptr<HoverListModel> model);
  ~HoverListView() override;

 private:
  struct ListItemViews {
    HoverButton* item_view;
    views::Separator* separator_view;
  };

  void AppendListItemView(const gfx::VectorIcon& icon,
                          base::string16 item_text,
                          int item_tag);
  void CreateAndAppendPlaceholderItem();
  void AddListItemView(int item_tag);
  void RemoveListItemView(int item_tag);
  void RemoveListItemView(ListItemViews list_item);

  // views::View:
  void RequestFocus() override;

  // HoverListModel::Observer:
  void OnListItemAdded(int item_tag) override;
  void OnListItemRemoved(int removed_item_view_tag) override;
  void OnListItemChanged(int changed_list_item_tag,
                         HoverListModel::ListItemChangeType type) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  std::unique_ptr<HoverListModel> model_;
  views::Button* first_list_item_view_ = nullptr;
  std::map<int, ListItemViews> tags_to_list_item_views_;
  base::Optional<ListItemViews> placeholder_list_item_view_;

  DISALLOW_COPY_AND_ASSIGN(HoverListView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_HOVER_LIST_VIEW_H_
