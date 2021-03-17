// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBAUTHN_HOVER_LIST_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBAUTHN_HOVER_LIST_VIEW_H_

#include <map>
#include <memory>
#include <string>

#include "base/optional.h"
#include "chrome/browser/ui/webauthn/hover_list_model.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace gfx {
struct VectorIcon;
}  // namespace gfx

namespace views {
class Separator;
}  // namespace views

class WebAuthnHoverButton;

// View that shows a list of items. Each item is rendered as a HoverButton with
// an icon, name, optional description, and chevron, like so:
//
//  +----------------------------------+
//  | ICON1 | Item 1 name          | > |
//  |       | Item 1 description   | > |
//  +----------------------------------+
//  | ICON2 | Item 2 name          | > |
//  |       | Item 2 description   | > |
//  +----------------------------------+
//  | ICON3 | Item 3 name          | > |
//  |       | Item 3 description   | > |
//  +----------------------------------+
//
class HoverListView : public views::View,
                      public HoverListModel::Observer {
 public:
  METADATA_HEADER(HoverListView);
  explicit HoverListView(std::unique_ptr<HoverListModel> model);
  HoverListView(const HoverListView&) = delete;
  HoverListView& operator=(const HoverListView&) = delete;
  ~HoverListView() override;

 private:
  struct ListItemViews {
    WebAuthnHoverButton* item_view;
    views::Separator* separator_view;
  };

  void AppendListItemView(const gfx::VectorIcon* icon,
                          std::u16string item_text,
                          std::u16string item_description,
                          int item_tag);
  void CreateAndAppendPlaceholderItem();
  void AddListItemView(int item_tag);
  void RemoveListItemView(int item_tag);
  void RemoveListItemView(ListItemViews list_item);
  views::Button& GetTopListItemView() const;
  int GetPreferredViewHeight() const;

  // views::View:
  void RequestFocus() override;

  // HoverListModel::Observer:
  void OnListItemAdded(int item_tag) override;
  void OnListItemRemoved(int removed_item_view_tag) override;
  void OnListItemChanged(int changed_list_item_tag,
                         HoverListModel::ListItemChangeType type) override;

  std::unique_ptr<HoverListModel> model_;
  std::map<int, ListItemViews> tags_to_list_item_views_;
  std::vector<WebAuthnHoverButton*> throbber_views_;
  base::Optional<ListItemViews> placeholder_list_item_view_;
  views::ScrollView* scroll_view_;
  views::View* item_container_;
  // is_two_line_list_, if true, indicates that list items should be sized so
  // that entries with only a single line of text are as tall as entries with
  // two lines.
  const bool is_two_line_list_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBAUTHN_HOVER_LIST_VIEW_H_
