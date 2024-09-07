// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_PROMO_CARD_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_PROMO_CARD_VIEW_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/views/editor_menu/utils/pre_target_handler.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class Label;
class MdTextButton;
}  // namespace views

namespace chromeos::editor_menu {

class EditorMenuViewDelegate;

// A view which shows a promo card to introduce the Editor Menu feature.
class EditorMenuPromoCardView : public views::View,
                                public views::WidgetObserver,
                                public PreTargetHandler::Delegate {
  METADATA_HEADER(EditorMenuPromoCardView, views::View)

 public:
  EditorMenuPromoCardView(const gfx::Rect& anchor_view_bounds,
                          EditorMenuViewDelegate* delegate);

  EditorMenuPromoCardView(const EditorMenuPromoCardView&) = delete;
  EditorMenuPromoCardView& operator=(const EditorMenuPromoCardView&) = delete;

  ~EditorMenuPromoCardView() override;

  static std::unique_ptr<views::Widget> CreateWidget(
      const gfx::Rect& anchor_view_bounds,
      EditorMenuViewDelegate* delegate);

  // views::View:
  void AddedToWidget() override;
  void RequestFocus() override;
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;

  // views::WidgetObserver:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  void OnWidgetVisibilityChanged(views::Widget* widget, bool visible) override;

  void UpdateBounds(const gfx::Rect& anchor_view_bounds);

  // chromeos::editor_menu::PreTargetHandler::Delegate:
  views::View* GetRootView() override;
  std::vector<views::View*> GetTraversableViewsByUpDownKeys() override;

  views::Label* title_for_testing() { return title_; }

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
  raw_ptr<views::MdTextButton> try_it_button_ = nullptr;

  bool queued_announcement_ = false;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};

  base::WeakPtrFactory<EditorMenuPromoCardView> weak_factory_{this};
};

}  // namespace chromeos::editor_menu

#endif  // CHROME_BROWSER_UI_VIEWS_EDITOR_MENU_EDITOR_MENU_PROMO_CARD_VIEW_H_
