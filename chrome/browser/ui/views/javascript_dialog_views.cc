// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/javascript_dialog_views.h"

#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/front_eliding_title_label.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"

JavaScriptDialogViews::~JavaScriptDialogViews() = default;

// static
base::WeakPtr<JavaScriptDialog> JavaScriptDialog::CreateNewDialog(
    content::WebContents* parent_web_contents,
    content::WebContents* alerting_web_contents,
    const base::string16& title,
    content::JavaScriptDialogType dialog_type,
    const base::string16& message_text,
    const base::string16& default_prompt_text,
    content::JavaScriptDialogManager::DialogClosedCallback dialog_callback,
    base::OnceClosure dialog_force_closed_callback) {
  return (new JavaScriptDialogViews(
              parent_web_contents, alerting_web_contents, title, dialog_type,
              message_text, default_prompt_text, std::move(dialog_callback),
              std::move(dialog_force_closed_callback)))
      ->weak_factory_.GetWeakPtr();
}

void JavaScriptDialogViews::CloseDialogWithoutCallback() {
  dialog_callback_.Reset();
  dialog_force_closed_callback_.Reset();
  GetWidget()->Close();
}

base::string16 JavaScriptDialogViews::GetUserInput() {
  return message_box_view_->GetInputText();
}

base::string16 JavaScriptDialogViews::GetWindowTitle() const {
  return title_;
}

bool JavaScriptDialogViews::Cancel() {
  if (dialog_callback_)
    std::move(dialog_callback_).Run(false, base::string16());
  return true;
}

bool JavaScriptDialogViews::Accept() {
  if (dialog_callback_)
    std::move(dialog_callback_).Run(true, message_box_view_->GetInputText());
  return true;
}

bool JavaScriptDialogViews::Close() {
  if (dialog_force_closed_callback_)
    std::move(dialog_force_closed_callback_).Run();
  return true;
}

bool JavaScriptDialogViews::ShouldShowCloseButton() const {
  return false;
}

views::View* JavaScriptDialogViews::GetInitiallyFocusedView() {
  auto* text_box = message_box_view_->text_box();
  return text_box ? text_box : views::DialogDelegate::GetInitiallyFocusedView();
}

ui::ModalType JavaScriptDialogViews::GetModalType() const {
  return ui::MODAL_TYPE_CHILD;
}

void JavaScriptDialogViews::AddedToWidget() {
  auto* bubble_frame_view = static_cast<views::BubbleFrameView*>(
      GetWidget()->non_client_view()->frame_view());
  bubble_frame_view->SetTitleView(
      CreateFrontElidingTitleLabel(GetWindowTitle()));
}

JavaScriptDialogViews::JavaScriptDialogViews(
    content::WebContents* parent_web_contents,
    content::WebContents* alerting_web_contents,
    const base::string16& title,
    content::JavaScriptDialogType dialog_type,
    const base::string16& message_text,
    const base::string16& default_prompt_text,
    content::JavaScriptDialogManager::DialogClosedCallback dialog_callback,
    base::OnceClosure dialog_force_closed_callback)
    : title_(title),
      message_text_(message_text),
      default_prompt_text_(default_prompt_text),
      dialog_callback_(std::move(dialog_callback)),
      dialog_force_closed_callback_(std::move(dialog_force_closed_callback)) {
  DialogDelegate::set_default_button(ui::DIALOG_BUTTON_OK);
  const bool is_alert = dialog_type == content::JAVASCRIPT_DIALOG_TYPE_ALERT;
  DialogDelegate::set_buttons(
      // Alerts only have an OK button, no Cancel, because there is no choice
      // being made.
      is_alert ? ui::DIALOG_BUTTON_OK
               : (ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL));

  int options = views::MessageBoxView::DETECT_DIRECTIONALITY;
  if (dialog_type == content::JAVASCRIPT_DIALOG_TYPE_PROMPT)
    options |= views::MessageBoxView::HAS_PROMPT_FIELD;

  views::MessageBoxView::InitParams params(message_text);
  params.options = options;
  params.default_prompt = default_prompt_text;
  message_box_view_ = new views::MessageBoxView(params);
  DCHECK(message_box_view_);

  SetLayoutManager(std::make_unique<views::FillLayout>());
  AddChildView(message_box_view_);

  constrained_window::ShowWebModalDialogViews(this, parent_web_contents);
  chrome::RecordDialogCreation(chrome::DialogIdentifier::JAVA_SCRIPT);
}
