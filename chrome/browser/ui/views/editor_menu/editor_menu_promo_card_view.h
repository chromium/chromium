// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_PROMO_CARD_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_PROMO_CARD_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Label;
class MdTextButton;
}

namespace chromeos::editor_menu {

class EditorMenuViewDelegate;
class PreTargetHandler;

// A view which shows a promo card to introduce the Editor Menu feature.
class EditorMenuPromoCardView : public views::View,
                                public views::WidgetObserver {
 public:
  METADATA_HEADER(EditorMenuPromoCardView);

  EditorMenuPromoCardView(const gfx::Rect& anchor_view_bounds,
                          EditorMenuViewDelegate* delegate);

  EditorMenuPromoCardView(const EditorMenuPromoCardView&) = delete;
  EditorMenuPromoCardView& operator=(const EditorMenuPromoCardView&) = delete;

  ~EditorMenuPromoCardView() override;

  static views::UniqueWidgetPtr CreateWidget(
      const gfx::Rect& anchor_view_bounds,
      EditorMenuViewDelegate* delegate);

  // views::View:
  void AddedToWidget() override;
  void RequestFocus() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  void UpdateBounds(const gfx::Rect& anchor_view_bounds);

 private:
  void InitLayout();
  void AddTitle(views::View* main_view);
  void AddDescription(views::View* main_view);
  void AddButtonBar(views::View* main_view);

  void CloseWidgetWithReason(views::Widget::ClosedReason closed_reason);

  void ResetPreTargetHandler();

  std::unique_ptr<PreTargetHandler> pre_target_handler_;

  // `delegate_` outlives `this`.
  raw_ptr<EditorMenuViewDelegate> delegate_ = nullptr;

  raw_ptr<views::Label> title_ = nullptr;
  raw_ptr<views::Label> description_ = nullptr;
  raw_ptr<views::MdTextButton> dismiss_button_ = nullptr;
  raw_ptr<views::MdTextButton> tell_me_more_button_ = nullptr;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  base::WeakPtrFactory<EditorMenuPromoCardView> weak_factory_{this};
};

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_PROMO_CARD_VIEW_H_
