// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/javascript_tab_modal_dialog_view_views.h"

#include "base/functional/callback.h"
#include "chrome/browser/ui/javascript_dialogs/javascript_tab_modal_dialog_manager_delegate_desktop.h"
#include "chrome/browser/ui/views/title_origin_label.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/javascript_dialog_manager.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/fill_layout.h"

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
  if (!message_text_.empty()) {
    GetWidget()->GetRootView()->GetViewAccessibility().SetDescription(
        message_text_);
  }
  // On some platforms, the platform accessibility API automatically
  // calculates the name of the native window based on the child RootView.
  // We override that calculation here so that we can present both the
  // title (e.g. "url.com says") and the message text on platforms where
  // the accessible description is ignored.
  GetViewAccessibility().OverrideNativeWindowTitle(l10n_util::GetStringFUTF16(
      IDS_CONCAT_TWO_STRINGS_WITH_COMMA, GetWindowTitle(), message_text_));
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
  SetModalType(ui::mojom::ModalType::kChild);
  SetDefaultButton(static_cast<int>(ui::mojom::DialogButton::kOk));
  const bool is_alert = dialog_type == content::JAVASCRIPT_DIALOG_TYPE_ALERT;
  SetButtons(
      // Alerts only have an OK button, no Cancel, because there is no choice
      // being made.
      is_alert ? static_cast<int>(ui::mojom::DialogButton::kOk)
               : static_cast<int>(ui::mojom::DialogButton::kOk) |
                     static_cast<int>(ui::mojom::DialogButton::kCancel));

  SetAcceptCallback(base::BindOnce(
      [](JavaScriptTabModalDialogViewViews* dialog) {
        // Remove the force-close callback to indicate that we were closed as a
        // result of user action.
        dialog->dialog_force_closed_callback_ = base::OnceClosure();
        if (dialog->dialog_callback_)
          std::move(dialog->dialog_callback_)
              .Run(true, dialog->message_box_view_->GetInputText());
      },
      base::Unretained(this)));
  SetCancelCallback(base::BindOnce(
      [](JavaScriptTabModalDialogViewViews* dialog) {
        // Remove the force-close callback to indicate that we were closed as a
        // result of user action.
        dialog->dialog_force_closed_callback_ = base::OnceClosure();
        if (dialog->dialog_callback_)
          std::move(dialog->dialog_callback_).Run(false, std::u16string());
      },
      base::Unretained(this)));
  RegisterWindowWillCloseCallback(base::BindOnce(
      [](JavaScriptTabModalDialogViewViews* dialog) {
        // If the force-close callback still exists at this point we're not
        // closed due to a user action (would've been caught in Accept/Cancel).
        if (dialog->dialog_force_closed_callback_)
          std::move(dialog->dialog_force_closed_callback_).Run();
      },
      base::Unretained(this)));

  message_box_view_ = new views::MessageBoxView(
      message_text, /* detect_directionality = */ true);
  if (dialog_type == content::JAVASCRIPT_DIALOG_TYPE_PROMPT)
    message_box_view_->SetPromptField(default_prompt_text);

  SetLayoutManager(std::make_unique<views::FillLayout>());
  AddChildView(message_box_view_.get());

  constrained_window::ShowWebModalDialogViews(this, parent_web_contents);
}

// static
JavaScriptTabModalDialogViewViews*
JavaScriptTabModalDialogViewViews::CreateAlertDialogForTesting(
    Browser* browser,
    std::u16string title,
    std::u16string message) {
  return new JavaScriptTabModalDialogViewViews(
      browser->tab_strip_model()->GetActiveWebContents(),
      browser->tab_strip_model()->GetActiveWebContents(), title,
      content::JAVASCRIPT_DIALOG_TYPE_ALERT, message, std::u16string(),
      base::NullCallback(), base::NullCallback());
}

BEGIN_METADATA(JavaScriptTabModalDialogViewViews)
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
