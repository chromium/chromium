// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_COMBINED_SELECTOR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_COMBINED_SELECTOR_VIEW_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "components/password_manager/core/browser/password_form.h"
#include "ui/views/window/dialog_delegate.h"

namespace views {
class Label;
class RadioButton;
class Widget;
}  // namespace views

class PasswordCombinedSelectorRadioButtonDelegate {
 public:
  virtual void OnRadioButtonChecked(int index) = 0;
};

class PasswordCombinedSelectorController;

// A view that shows a list of credentials (passwords) together with radio
// buttons when needed.
// TODO(crbug.com/477857535): This class is a slightly modified version of
// CombinedSelectorView. Merge the implementations.
class PasswordCombinedSelectorView
    : public views::DialogDelegate,
      public AccountChooserPrompt,
      public PasswordCombinedSelectorRadioButtonDelegate {
 public:
  static constexpr int kSubtitleLabelId = 1;
  static constexpr int kCredentialListId = 2;
  static constexpr int kCredentialRowId = 3;
  static constexpr int kRowUsernameLabelId = 4;
  static constexpr int kRowDetailLabelId = 5;

  PasswordCombinedSelectorView(PasswordCombinedSelectorController* controller,
                               content::WebContents* web_contents);
  PasswordCombinedSelectorView(const PasswordCombinedSelectorView&) = delete;
  PasswordCombinedSelectorView& operator=(const PasswordCombinedSelectorView&) =
      delete;
  ~PasswordCombinedSelectorView() override;

  // AccountChooserPrompt:
  void ShowAccountChooser() override;
  void ControllerGone() override;

  // PasswordCombinedSelectorRadioButtonDelegate:
  void OnRadioButtonChecked(int index) override;

  // views::DialogDelegate:
  bool Accept() override;
  bool ShouldAllowKeyEventsDuringInputProtection() const override;
  void OnWidgetInitialized() override;

 private:
  std::u16string GetWindowTitle() const override;
  bool ShouldShowCloseButton() const override;
  void WindowClosing() override;
  ui::mojom::ModalType GetModalType() const override;

  void InitWindow();

  raw_ptr<PasswordCombinedSelectorController> controller_;
  raw_ptr<content::WebContents> web_contents_;

  // The currently selected password form.
  raw_ptr<const password_manager::PasswordForm> selected_form_ = nullptr;

  // The following pointers point to views owned by the widget, and will dangle
  // during widget teardown before the delegate is destroyed. They are only
  // used for testing and never dereferenced after the widget is closed.
  raw_ptr<views::View> list_view_ = nullptr;

  std::unique_ptr<views::Widget> widget_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_PASSWORDS_PASSWORD_COMBINED_SELECTOR_VIEW_H_
