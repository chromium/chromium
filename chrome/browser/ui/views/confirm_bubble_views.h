// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CONFIRM_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_CONFIRM_BUBBLE_VIEWS_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/window/dialog_delegate.h"

class ConfirmBubbleModel;

namespace views {
class Label;
}  // namespace views

// A dialog (with the standard Title/[OK]/[Cancel] UI elements), as well as
// a message Label and help (?) button. The dialog ultimately appears like this:
//   +------------------------+
//   | Title                  |
//   | Label                  |
//   | (?)      [OK] [Cancel] |
//   +------------------------+
//
// TODO(msw): Remove this class or merge it with DialogDelegateView.
class ConfirmBubbleViews : public views::DialogDelegateView,
                           public views::ButtonListener {
 public:
  explicit ConfirmBubbleViews(std::unique_ptr<ConfirmBubbleModel> model);

 protected:
  ~ConfirmBubbleViews() override;

  // views::DialogDelegate implementation.
  bool IsDialogButtonEnabled(ui::DialogButton button) const override;
  bool Cancel() override;
  bool Accept() override;

  // views::WidgetDelegate implementation.
  ui::ModalType GetModalType() const override;
  base::string16 GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;

  // views::ButtonListener implementation.
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // views::View implementation.
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;

 private:
  // The model to customize this bubble view.
  std::unique_ptr<ConfirmBubbleModel> model_;

  views::Label* label_;
  views::View* help_button_;

  DISALLOW_COPY_AND_ASSIGN(ConfirmBubbleViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CONFIRM_BUBBLE_VIEWS_H_
