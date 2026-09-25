// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_MINOR_TEXT_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_MINOR_TEXT_VIEW_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"

namespace views {
class Label;
class MenuItemView;
}  // namespace views

// A secondary text view that can be attached to a menu row item (such as the
// upgrade notification row) without using MenuItemView's accelerator column.
class AppMenuMinorTextView : public views::BoxLayoutView {
  METADATA_HEADER(AppMenuMinorTextView, views::BoxLayoutView)

 public:
  explicit AppMenuMinorTextView(const std::u16string& minor_text);
  AppMenuMinorTextView(const AppMenuMinorTextView&) = delete;
  AppMenuMinorTextView& operator=(const AppMenuMinorTextView&) = delete;
  ~AppMenuMinorTextView() override;

  // Attaches a minor text view to `menu_item`.
  static void AttachTo(views::MenuItemView* menu_item,
                       const std::u16string& minor_text);

  views::Label* label_for_testing() const { return label_; }

 private:
  raw_ptr<views::Label> label_ = nullptr;
};

#endif  // CHROME_BROWSER_UI_VIEWS_APP_MENU_APP_MENU_MINOR_TEXT_VIEW_H_
