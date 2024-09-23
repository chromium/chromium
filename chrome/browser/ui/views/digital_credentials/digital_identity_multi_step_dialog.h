// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_MULTI_STEP_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_MULTI_STEP_DIALOG_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace content {
class WebContents;
}

// Wraps views::BubbleDialogDelegate where contents can be updated in order to
// support having multiple steps in dialog.
class DigitalIdentityMultiStepDialog {
 public:
  class TestApi {
   public:
    explicit TestApi(DigitalIdentityMultiStepDialog* dialog);
    ~TestApi();

    views::Widget* get_widget() { return dialog_->dialog_.get(); }

    views::BubbleDialogDelegate* get_widget_delegate() {
      return dialog_->GetWidgetDelegate();
    }

   private:
    const raw_ptr<DigitalIdentityMultiStepDialog> dialog_;
  };

  explicit DigitalIdentityMultiStepDialog(
      base::WeakPtr<content::WebContents> web_contents);
  ~DigitalIdentityMultiStepDialog();

  // Tries to show dialog. Updates the dialog contents if the dialog is already
  // showing. Runs `cancel_callback` if dialog could not be shown (and the
  // dialog is not already showing).
  void TryShow(
      const std::optional<ui::DialogModel::Button::Params>& accept_button,
      base::OnceClosure accept_callback,
      const ui::DialogModel::Button::Params& cancel_button,
      base::OnceClosure cancel_callback,
      const std::u16string& dialog_title,
      const std::u16string& body_text,
      std::unique_ptr<views::View> custom_body_field);

 private:
  // The wrapped views::BubbleDialogDelegate.
  class Delegate : public views::BubbleDialogDelegate {
   public:
    Delegate();
    ~Delegate() override;

    void Update(
        const std::optional<ui::DialogModel::Button::Params>& accept_button,
        base::OnceClosure accept_callback,
        const ui::DialogModel::Button::Params& cancel_button,
        base::OnceClosure cancel_callback,
        const std::u16string& dialog_title,
        const std::u16string& body_text,
        std::unique_ptr<views::View> custom_body_field);

    views::Widget::ClosedReason get_closed_reason() { return closed_reason_; }

   private:
    bool OnDialogAccepted();
    bool OnDialogCanceled();
    void OnDialogClosed();

    void ResetCallbacks();

    // Owned by the parent view.
    raw_ptr<views::View> contents_view_;

    base::OnceClosure accept_callback_;
    base::OnceClosure cancel_callback_;

    views::Widget::ClosedReason closed_reason_ =
        views::Widget::ClosedReason::kUnspecified;

    base::WeakPtrFactory<Delegate> weak_ptr_factory_{this};
  };

  DigitalIdentityMultiStepDialog::Delegate* GetWidgetDelegate();

  // The web contents the dialog is modal to.
  base::WeakPtr<content::WebContents> web_contents_;

  base::WeakPtr<views::Widget> dialog_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_DIGITAL_CREDENTIALS_DIGITAL_IDENTITY_MULTI_STEP_DIALOG_H_
