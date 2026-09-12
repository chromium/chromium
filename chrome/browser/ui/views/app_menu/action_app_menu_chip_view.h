// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_APP_MENU_CHIP_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_APP_MENU_CHIP_VIEW_H_

#include <string>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"

namespace views {
class Label;
class MenuItemView;
}  // namespace views

// A status chip view that can be attached to a menu row item.
class ActionAppMenuChipView : public views::BoxLayoutView {
  METADATA_HEADER(ActionAppMenuChipView, views::BoxLayoutView)

 public:
  explicit ActionAppMenuChipView(const std::u16string& chip_text);
  ActionAppMenuChipView(const ActionAppMenuChipView&) = delete;
  ActionAppMenuChipView& operator=(const ActionAppMenuChipView&) = delete;
  ~ActionAppMenuChipView() override;

  // Attaches a status chip to menu_item
  static void AttachTo(views::MenuItemView* menu_item,
                       const std::u16string& chip_text);

  views::Label* chip_label_for_testing() const { return chip_label_; }

 private:
  void UpdateColors(bool is_selected);

  raw_ptr<views::Label> chip_label_ = nullptr;
  base::CallbackListSubscription selected_changed_subscription_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_ACTION_APP_MENU_CHIP_VIEW_H_
