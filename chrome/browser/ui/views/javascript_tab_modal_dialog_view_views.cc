// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/javascript_tab_modal_dialog_view_views.h"

#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/javascript_dialogs/javascript_tab_modal_dialog_manager_delegate_desktop.h"
#include "chrome/browser/ui/views/title_origin_label.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/metadata/metadata_impl_macros.h"

JavaScriptTabModalDialogViewViews::~JavaScriptTabModalDialogViewViews() =
    default;

void JavaScriptTabModalDialogViewViews::CloseDialogWithoutCallback() {
  dialog_callback_.Reset();
  dialog_force_closed_callback_.Reset();
  GetWidget()->Close();
}

std::u16string JavaScriptTabModalDialogViewViews::GetUserInput() {
  return message_box_view_->GetInputText();
}

std::u16string JavaScriptTabModalDialogViewViews::GetWindowTitle() const {
  return title_;
}

bool JavaScriptTabModalDialogViewViews::ShouldShowCloseButton() const {
  return false;
}

views::View* JavaScriptTabModalDialogViewViews::GetInitiallyFocusedView() {
  auto* text_box = message_box_view_->GetVisiblePromptField();
  return text_box ? text_box : views::DialogDelegate::GetInitiallyFocusedView();
}

void JavaScriptTabModalDialogViewViews::AddedToWidget() {
  auto* bubble_frame_view = static_cast<views::BubbleFrameView*>(
      GetWidget()->non_client_view()->frame_view());
  bubble_frame_view->SetTitleView(CreateTitleOriginLabel(GetWindowTitle()));
}

JavaScriptTabModalDialogViewViews::JavaScriptTabModalDialogViewViews(
    content::WebContents* parent_web_contents,
    content::WebContents* alerting_web_contents,
    const std::u16string& title,
    content::JavaScriptDialogType dialog_type,
    const std::u16string& message_text,
    const std::u16string& default_prompt_text,
    content::JavaScriptDialogManager::DialogClosedCallback dialog_callback,
    base::OnceClosure dialog_force_closed_callback)
    : title_(title),
      message_text_(message_text),
      default_prompt_text_(default_prompt_text),
      dialog_callback_(std::move(dialog_callback)),
      dialog_force_closed_callback_(std::move(dialog_force_closed_callback)) {
  SetModalType(ui::MODAL_TYPE_CHILD);
  SetDefaultButton(ui::DIALOG_BUTTON_OK);
  const bool is_alert = dialog_type == content::JAVASCRIPT_DIALOG_TYPE_ALERT;
  SetButtons(
      // Alerts only have an OK button, no Cancel, because there is no choice
      // being made.
      is_alert ? ui::DIALOG_BUTTON_OK
               : (ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL));

  SetAcceptCallback(base::BindOnce(
      [](JavaScriptTabModalDialogViewViews* dialog) {
        if (dialog->dialog_callback_)
          std::move(dialog->dialog_callback_)
              .Run(true, dialog->message_box_view_->GetInputText());
      },
      base::Unretained(this)));
  SetCancelCallback(base::BindOnce(
      [](JavaScriptTabModalDialogViewViews* dialog) {
        if (dialog->dialog_callback_)
          std::move(dialog->dialog_callback_).Run(false, std::u16string());
      },
      base::Unretained(this)));
  SetCloseCallback(base::BindOnce(
      [](JavaScriptTabModalDialogViewViews* dialog) {
        if (dialog->dialog_force_closed_callback_)
          std::move(dialog->dialog_force_closed_callback_).Run();
      },
      base::Unretained(this)));

  message_box_view_ = new views::MessageBoxView(
      message_text, /* detect_directionality = */ true);
  if (dialog_type == content::JAVASCRIPT_DIALOG_TYPE_PROMPT)
    message_box_view_->SetPromptField(default_prompt_text);

  SetLayoutManager(std::make_unique<views::FillLayout>());
  AddChildView(message_box_view_);

  constrained_window::ShowWebModalDialogViews(this, parent_web_contents);
  chrome::RecordDialogCreation(chrome::DialogIdentifier::JAVA_SCRIPT);
}

BEGIN_METADATA(JavaScriptTabModalDialogViewViews, views::DialogDelegateView)
END_METADATA

// Creates a new JS dialog. Note the two callbacks; |dialog_callback| is for
// user responses, while |dialog_force_closed_callback| is for when Views
// forces the dialog closed without a user reply.
base::WeakPtr<javascript_dialogs::TabModalDialogView>
JavaScriptTabModalDialogManagerDelegateDesktop::CreateNewDialog(
    content::WebContents* alerting_web_contents,
    const std::u16string& title,
    content::JavaScriptDialogType dialog_type,
    const std::u16string& message_text,
    const std::u16string& default_prompt_text,
    content::JavaScriptDialogManager::DialogClosedCallback dialog_callback,
    base::OnceClosure dialog_force_closed_callback) {
  return (new JavaScriptTabModalDialogViewViews(
              web_contents_, alerting_web_contents, title, dialog_type,
              message_text, default_prompt_text, std::move(dialog_callback),
              std::move(dialog_force_closed_callback)))
      ->weak_factory_.GetWeakPtr();
}
