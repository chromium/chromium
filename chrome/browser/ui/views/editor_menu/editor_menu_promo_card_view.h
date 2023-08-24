// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_PROMO_CARD_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_PROMO_CARD_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/widget/unique_widget_ptr.h"

namespace chromeos::editor_menu {

// A view which shows a promo card to introduce the Editor Menu feature.
class EditorMenuPromoCardView : public views::View {
 public:
  METADATA_HEADER(EditorMenuPromoCardView);

  explicit EditorMenuPromoCardView(const gfx::Rect& anchor_view_bounds);

  EditorMenuPromoCardView(const EditorMenuPromoCardView&) = delete;
  EditorMenuPromoCardView& operator=(const EditorMenuPromoCardView&) = delete;

  ~EditorMenuPromoCardView() override;

  static views::UniqueWidgetPtr CreateWidget(
      const gfx::Rect& anchor_view_bounds);

  // views::View:
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  void UpdateBounds(const gfx::Rect& anchor_view_bounds);

 private:
  void InitLayout();
  void InitTextContainer(views::View* main_view);
  void InitButtonBar(views::View* main_view);
};

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_PROMO_CARD_VIEW_H_
