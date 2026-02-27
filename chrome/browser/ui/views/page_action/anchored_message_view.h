// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_ANCHORED_MESSAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_ANCHORED_MESSAGE_VIEW_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/page_action/page_action_model.h"
#include "ui/base/models/image_model.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

namespace views {
class Widget;
}

namespace page_actions {

// AnchoredMessageBubbleView is the view displaying the anchored message for a
// given page action. It is created and destroyed dynamically.
class AnchoredMessageBubbleView : public views::BubbleDialogDelegate,
                                  public views::View {
  METADATA_HEADER(AnchoredMessageBubbleView, views::View)
 public:
  AnchoredMessageBubbleView(views::BubbleAnchor parent,
                            const PageActionModelInterface& model,
                            base::RepeatingClosure chip_callback,
                            base::RepeatingClosure close_callback);
  AnchoredMessageBubbleView(const AnchoredMessageBubbleView& other) = delete;
  ~AnchoredMessageBubbleView() override;

  // views::BubbleDialogDelegate:
  views::View* GetContentsView() override;
  bool CanActivate() const override;

  // views::View:
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;

 protected:
  void OnThemeChanged() override;

 private:
  void ChipCallback();

  raw_ptr<views::Label> label_ = nullptr;
  raw_ptr<views::View> chip_container_ = nullptr;
  raw_ptr<views::ImageButton> close_button_ = nullptr;
  const std::u16string label_text_;
  const bool show_close_button_;
  base::RepeatingClosure chip_callback_;
  base::RepeatingClosure close_callback_;
};

}  // namespace page_actions

#endif  // CHROME_BROWSER_UI_VIEWS_PAGE_ACTION_ANCHORED_MESSAGE_VIEW_H_
