// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_INPUT_METHOD_UNDO_WINDOW_H_
#define CHROME_BROWSER_UI_ASH_INPUT_METHOD_UNDO_WINDOW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/input_method/assistive_window_properties.h"
#include "chrome/browser/ui/ash/input_method/assistive_delegate.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/chromeos/ui_chromeos_export.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/link.h"
#include "ui/views/metadata/view_factory.h"

namespace ui {
namespace ime {

// Pop up UI for users to undo an autocorrected word.
class UI_CHROMEOS_EXPORT UndoWindow : public views::BubbleDialogDelegateView {
  METADATA_HEADER(UndoWindow, views::BubbleDialogDelegateView)

 public:
  explicit UndoWindow(gfx::NativeView parent, AssistiveDelegate* delegate);
  UndoWindow(const UndoWindow&) = delete;
  UndoWindow& operator=(const UndoWindow&) = delete;
  ~UndoWindow() override;

  views::Widget* InitWidget();
  void Hide();
  void Show(bool show_setting_link);

  // Set the position of the undo window at the start of the autocorrected word.
  void SetBounds(const gfx::Rect& word_bounds);

  void SetButtonHighlighted(const AssistiveWindowButton& button,
                            bool highlighted);

  views::Button* GetUndoButtonForTesting();

 protected:
  void OnThemeChanged() override;

 private:
  void UndoButtonPressed();

  raw_ptr<AssistiveDelegate, DanglingUntriaged> delegate_;
  raw_ptr<views::LabelButton> undo_button_;
  raw_ptr<views::ImageButton> learn_more_button_;
};

BEGIN_VIEW_BUILDER(UI_CHROMEOS_EXPORT,
                   UndoWindow,
                   views::BubbleDialogDelegateView)
END_VIEW_BUILDER

}  // namespace ime
}  // namespace ui

DEFINE_VIEW_BUILDER(UI_CHROMEOS_EXPORT, ui::ime::UndoWindow)

#endif  // CHROME_BROWSER_UI_ASH_INPUT_METHOD_UNDO_WINDOW_H_
