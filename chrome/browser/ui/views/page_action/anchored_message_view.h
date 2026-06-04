// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_ANCHORED_MESSAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_ANCHORED_MESSAGE_VIEW_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/page_action/page_action_model.h"
#include "ui/base/identifier/unique_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/menu_button_controller.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/view.h"

namespace ui {
class SimpleMenuModel;
}

namespace views {
class ImageButton;
class ImageView;
class Label;
class MenuButton;
class Widget;
}

namespace page_actions {

class ChipContainerView;
class MultiIconButton;

// AnchoredMessageBubbleView is the view displaying the anchored message for a
// given page action. It is created and destroyed dynamically.
class AnchoredMessageBubbleView : public views::BubbleDialogDelegate,
                                  public views::View {
  METADATA_HEADER(AnchoredMessageBubbleView, views::View)
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kAnchoredMessageBubbleId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kAnchoredMessageIconId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kAnchoredMessageLabelId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kAnchoredMessageChipId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kAnchoredMessageCloseIconId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kAnchoredMessageMenuIconId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kAnchoredMessageExpandButtonId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kAnchoredMessageExpandedContentId);

  // Delegate is the interface for the AnchoredMessageBubbleView to use the
  // callbacks registered in the PageActionView.
  class Delegate {
   public:
    virtual void AnchoredMessageChipClick() = 0;
    virtual void CloseAnchoredMessage() = 0;
    virtual void AnchoredMessageExpanded() = 0;
    virtual void AnchoredMessageCollapsed() = 0;
  };

  AnchoredMessageBubbleView(views::BubbleAnchor parent,
                            const PageActionModelInterface& model,
                            Delegate& delegate);
  AnchoredMessageBubbleView(const AnchoredMessageBubbleView& other) = delete;
  ~AnchoredMessageBubbleView() override;

  // views::BubbleDialogDelegate:
  views::View* GetContentsView() override;
  bool CanActivate() const override;

  // views::View:
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

  void UpdateContent(const PageActionModelInterface& model);

  // views::BubbleDialogDelegate:
  void OnWidgetDestroying(views::Widget* widget) override;

 protected:
  void OnThemeChanged() override;

 private:
  void ChipCallback();
  void MenuButtonPressed();
  void OnMenuClosed();
  void OnExpandButtonPressed();

  void UpdateExpandButtonTooltip();

  raw_ptr<views::View> top_row_ = nullptr;
  raw_ptr<views::View> bottom_container_ = nullptr;
  raw_ptr<views::Label> label_ = nullptr;
  raw_ptr<MultiIconButton> expand_button_ = nullptr;
  raw_ptr<ChipContainerView> chip_container_ = nullptr;
  raw_ptr<views::ImageButton> close_button_ = nullptr;
  raw_ptr<views::MenuButton> menu_button_ = nullptr;
  raw_ptr<views::ImageView> icon_view_ = nullptr;
  std::optional<ui::ImageModel> icon_;
  std::u16string label_text_;
  bool show_close_button_;
  raw_ptr<ui::SimpleMenuModel> menu_model_ = nullptr;
  std::unique_ptr<views::MenuRunner> menu_runner_;
  std::unique_ptr<views::MenuButtonController::PressedLock> pressed_lock_;
  bool expanded_ = false;
  std::optional<std::u16string> expand_button_tooltip_override_;
  std::optional<std::u16string> collapse_button_tooltip_override_;
  const raw_ref<Delegate> delegate_;
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_ANCHORED_MESSAGE_VIEW_H_
