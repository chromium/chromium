// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_INPUT_METHOD_GRAMMAR_SUGGESTION_WINDOW_H_
#define CHROME_BROWSER_UI_ASH_INPUT_METHOD_GRAMMAR_SUGGESTION_WINDOW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/ash/input_method/assistive_delegate.h"
#include "chrome/browser/ui/ash/input_method/completion_suggestion_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/chromeos/ui_chromeos_export.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/metadata/view_factory.h"

namespace ui {
namespace ime {

// Pop up UI for users to undo an autocorrected word.
class UI_CHROMEOS_EXPORT GrammarSuggestionWindow
    : public views::BubbleDialogDelegateView {
  METADATA_HEADER(GrammarSuggestionWindow, views::BubbleDialogDelegateView)

 public:
  explicit GrammarSuggestionWindow(gfx::NativeView parent,
                                   AssistiveDelegate* delegate);
  GrammarSuggestionWindow(const GrammarSuggestionWindow&) = delete;
  GrammarSuggestionWindow& operator=(const GrammarSuggestionWindow&) = delete;
  ~GrammarSuggestionWindow() override;

  views::Widget* InitWidget();
  void Show();
  void Hide();

  void SetSuggestion(const std::u16string& suggestion);

  void SetButtonHighlighted(const AssistiveWindowButton& button,
                            bool highlighted);

  void SetBounds(gfx::Rect bounds);

  CompletionSuggestionView* GetSuggestionButtonForTesting();
  views::Button* GetIgnoreButtonForTesting();

 protected:
  void OnThemeChanged() override;

 private:
  raw_ptr<AssistiveDelegate> delegate_;
  raw_ptr<CompletionSuggestionView> suggestion_button_;
  raw_ptr<views::ImageButton> ignore_button_;

  ButtonId current_highlighted_button_id_ = ButtonId::kNone;

  base::flat_map<views::View*, base::CallbackListSubscription> subscriptions_;
};

BEGIN_VIEW_BUILDER(UI_CHROMEOS_EXPORT,
                   GrammarSuggestionWindow,
                   views::BubbleDialogDelegateView)
END_VIEW_BUILDER

}  // namespace ime
}  // namespace ui

DEFINE_VIEW_BUILDER(UI_CHROMEOS_EXPORT, ui::ime::GrammarSuggestionWindow)

#endif  // CHROME_BROWSER_UI_ASH_INPUT_METHOD_GRAMMAR_SUGGESTION_WINDOW_H_
