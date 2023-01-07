// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/simple_message_box.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/simple_message_box_internal.h"
#include "chrome/browser/ui/views/message_box_dialog.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/screen.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/window.h"  // nogncheck
#endif

#if BUILDFLAG(IS_WIN)
#include "ui/base/win/message_box_win.h"
#include "ui/views/win/hwnd_util.h"
#endif

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/ui/cocoa/simple_message_box_cocoa.h"
#endif

namespace {
#if BUILDFLAG(IS_WIN)
UINT GetMessageBoxFlagsFromType(chrome::MessageBoxType type) {
  UINT flags = MB_SETFOREGROUND;
  switch (type) {
    case chrome::MESSAGE_BOX_TYPE_WARNING:
      return flags | MB_OK | MB_ICONWARNING;
    case chrome::MESSAGE_BOX_TYPE_QUESTION:
      return flags | MB_YESNO | MB_ICONQUESTION;
  }
  NOTREACHED();
  return flags | MB_OK | MB_ICONWARNING;
}
#endif

// static
chrome::MessageBoxResult ShowSync(gfx::NativeWindow parent,
                                  const std::u16string& title,
                                  const std::u16string& message,
                                  chrome::MessageBoxType type,
                                  const std::u16string& yes_text,
                                  const std::u16string& no_text,
                                  const std::u16string& checkbox_text) {
  static bool g_message_box_is_showing_sync = false;
  // To avoid showing another MessageBoxDialog when one is already pending.
  // Otherwise, this might lead to a stack overflow due to infinite runloops.
  if (g_message_box_is_showing_sync)
    return chrome::MESSAGE_BOX_RESULT_NO;

  base::AutoReset<bool> is_showing(&g_message_box_is_showing_sync, true);
  chrome::MessageBoxResult result = chrome::MESSAGE_BOX_RESULT_NO;
  // TODO(pkotwicz): Exit message loop when the dialog is closed by some other
  // means than |Cancel| or |Accept|. crbug.com/404385
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  MessageBoxDialog::Show(
      parent, title, message, type, yes_text, no_text, checkbox_text,
      base::BindOnce(
          [](base::RunLoop* run_loop, chrome::MessageBoxResult* out_result,
             chrome::MessageBoxResult messagebox_result) {
            *out_result = messagebox_result;
            run_loop->Quit();
          },
          &run_loop, &result));
  run_loop.Run();
  return result;
}
}  // namespace

////////////////////////////////////////////////////////////////////////////////
// MessageBoxDialog, public:

// static
chrome::MessageBoxResult MessageBoxDialog::Show(
    gfx::NativeWindow parent,
    const std::u16string& title,
    const std::u16string& message,
    chrome::MessageBoxType type,
    const std::u16string& yes_text,
    const std::u16string& no_text,
    const std::u16string& checkbox_text,
    MessageBoxDialog::MessageBoxResultCallback callback) {
  if (!callback)
    return ShowSync(parent, title, message, type, yes_text, no_text,
                    checkbox_text);

  startup_metric_utils::SetNonBrowserUIDisplayed();
  if (chrome::internal::g_should_skip_message_box_for_test) {
    std::move(callback).Run(chrome::MESSAGE_BOX_RESULT_YES);
    return chrome::MESSAGE_BOX_RESULT_DEFERRED;
  }

// Views dialogs cannot be shown outside the UI thread message loop or if the
// ResourceBundle is not initialized yet.
// Fallback to logging with a default response or a Windows MessageBox.
#if BUILDFLAG(IS_WIN)
  if (!base::CurrentUIThread::IsSet() ||
      !base::RunLoop::IsRunningOnCurrentThread() ||
      !ui::ResourceBundle::HasSharedInstance()) {
    LOG_IF(ERROR, !checkbox_text.empty()) << "Dialog checkbox won't be shown";
    int result = ui::MessageBox(
        views::HWNDForNativeWindow(parent), base::AsWString(message),
        base::AsWString(title), GetMessageBoxFlagsFromType(type));
    std::move(callback).Run((result == IDYES || result == IDOK)
                                ? chrome::MESSAGE_BOX_RESULT_YES
                                : chrome::MESSAGE_BOX_RESULT_NO);
    return chrome::MESSAGE_BOX_RESULT_DEFERRED;
  }
#elif BUILDFLAG(IS_MAC)
  if (!base::CurrentUIThread::IsSet() ||
      !base::RunLoop::IsRunningOnCurrentThread() ||
      !ui::ResourceBundle::HasSharedInstance()) {
    // Even though this function could return a value synchronously here in
    // principle, in practice call sites do not expect any behavior other than a
    // return of DEFERRED and an invocation of the callback.
    std::move(callback).Run(
        chrome::ShowMessageBoxCocoa(message, type, checkbox_text));
    return chrome::MESSAGE_BOX_RESULT_DEFERRED;
  }
#else
  if (!base::CurrentUIThread::IsSet() ||
      !ui::ResourceBundle::HasSharedInstance() ||
      !display::Screen::GetScreen()) {
    LOG(ERROR) << "Unable to show a dialog outside the UI thread message loop: "
               << title << " - " << message;
    std::move(callback).Run(chrome::MESSAGE_BOX_RESULT_NO);
    return chrome::MESSAGE_BOX_RESULT_DEFERRED;
  }
#endif

  bool is_system_modal = !parent;

#if BUILDFLAG(IS_MAC)
  // Mac does not support system modals, so never ask MessageBoxDialog to
  // be system modal.
  is_system_modal = false;
#endif

  MessageBoxDialog* dialog = new MessageBoxDialog(
      title, message, type, yes_text, no_text, checkbox_text, is_system_modal);
  views::Widget* widget =
      constrained_window::CreateBrowserModalDialogViews(dialog, parent);

#if BUILDFLAG(IS_MAC)
  // Mac does not support system modal dialogs. If there is no parent window to
  // attach to, move the dialog's widget on top so other windows do not obscure
  // it.
  if (!parent)
    widget->SetZOrderLevel(ui::ZOrderLevel::kFloatingWindow);
#endif

  widget->Show();
  dialog->Run(std::move(callback));
  return chrome::MESSAGE_BOX_RESULT_DEFERRED;
}

void MessageBoxDialog::OnDialogAccepted() {
  if (!message_box_view_->HasVisibleCheckBox() ||
      message_box_view_->IsCheckBoxSelected()) {
    Done(chrome::MESSAGE_BOX_RESULT_YES);
  } else {
    Done(chrome::MESSAGE_BOX_RESULT_NO);
  }
}

std::u16string MessageBoxDialog::GetWindowTitle() const {
  return window_title_;
}

views::View* MessageBoxDialog::GetContentsView() {
  return message_box_view_;
}

bool MessageBoxDialog::ShouldShowCloseButton() const {
  return true;
}

void MessageBoxDialog::OnWidgetActivationChanged(views::Widget* widget,
                                                 bool active) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (GetWidget()->GetNativeWindow()->GetProperty(
          chromeos::kIsShowingInOverviewKey)) {
    // Prevent this from closing while starting overview mode for better UX.
    // See crbug.com/972015.
    return;
  }
#endif

  if (!active)
    GetWidget()->Close();
}

////////////////////////////////////////////////////////////////////////////////
// MessageBoxDialog, private:

MessageBoxDialog::MessageBoxDialog(const std::u16string& title,
                                   const std::u16string& message,
                                   chrome::MessageBoxType type,
                                   const std::u16string& yes_text,
                                   const std::u16string& no_text,
                                   const std::u16string& checkbox_text,
                                   bool is_system_modal)
    : window_title_(title),
      type_(type),
      message_box_view_(new views::MessageBoxView(message)) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  SetModalType(is_system_modal ? ui::MODAL_TYPE_SYSTEM : ui::MODAL_TYPE_WINDOW);
#else
  DCHECK(!is_system_modal);
  SetModalType(ui::MODAL_TYPE_WINDOW);
#endif
  SetButtons(type_ == chrome::MESSAGE_BOX_TYPE_QUESTION
                 ? ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL
                 : ui::DIALOG_BUTTON_OK);

  SetAcceptCallback(base::BindOnce(&MessageBoxDialog::OnDialogAccepted,
                                   base::Unretained(this)));
  SetCancelCallback(base::BindOnce(&MessageBoxDialog::Done,
                                   base::Unretained(this),
                                   chrome::MESSAGE_BOX_RESULT_NO));
  SetCloseCallback(base::BindOnce(&MessageBoxDialog::Done,
                                  base::Unretained(this),
                                  chrome::MESSAGE_BOX_RESULT_NO));
  SetOwnedByWidget(true);

  std::u16string ok_text = yes_text;
  if (ok_text.empty()) {
    ok_text =
        type_ == chrome::MESSAGE_BOX_TYPE_QUESTION
            ? l10n_util::GetStringUTF16(IDS_CONFIRM_MESSAGEBOX_YES_BUTTON_LABEL)
            : l10n_util::GetStringUTF16(IDS_OK);
  }
  SetButtonLabel(ui::DIALOG_BUTTON_OK, ok_text);

  // Only MESSAGE_BOX_TYPE_QUESTION has a Cancel button.
  if (type_ == chrome::MESSAGE_BOX_TYPE_QUESTION) {
    std::u16string cancel_text = no_text;
    if (cancel_text.empty())
      cancel_text = l10n_util::GetStringUTF16(IDS_CANCEL);
    SetButtonLabel(ui::DIALOG_BUTTON_CANCEL, cancel_text);
  }

  if (!checkbox_text.empty())
    message_box_view_->SetCheckBoxLabel(checkbox_text);
}

MessageBoxDialog::~MessageBoxDialog() {
  GetWidget()->RemoveObserver(this);
  CHECK(!IsInObserverList());
}

void MessageBoxDialog::Run(MessageBoxResultCallback result_callback) {
  GetWidget()->AddObserver(this);
  result_callback_ = std::move(result_callback);
}

void MessageBoxDialog::Done(chrome::MessageBoxResult result) {
  CHECK(!result_callback_.is_null());
  std::move(result_callback_).Run(result);
}

views::Widget* MessageBoxDialog::GetWidget() {
  return message_box_view_->GetWidget();
}

const views::Widget* MessageBoxDialog::GetWidget() const {
  return message_box_view_->GetWidget();
}

namespace chrome {

MessageBoxResult ShowWarningMessageBox(gfx::NativeWindow parent,
                                       const std::u16string& title,
                                       const std::u16string& message) {
  return MessageBoxDialog::Show(
      parent, title, message, chrome::MESSAGE_BOX_TYPE_WARNING,
      std::u16string(), std::u16string(), std::u16string());
}

void ShowWarningMessageBoxWithCheckbox(
    gfx::NativeWindow parent,
    const std::u16string& title,
    const std::u16string& message,
    const std::u16string& checkbox_text,
    base::OnceCallback<void(bool checked)> callback) {
  MessageBoxDialog::Show(parent, title, message,
                         chrome::MESSAGE_BOX_TYPE_WARNING, std::u16string(),
                         std::u16string(), checkbox_text,
                         base::BindOnce(
                             [](base::OnceCallback<void(bool checked)> callback,
                                MessageBoxResult message_box_result) {
                               std::move(callback).Run(message_box_result ==
                                                       MESSAGE_BOX_RESULT_YES);
                             },
                             std::move(callback)));
}

MessageBoxResult ShowQuestionMessageBoxSync(gfx::NativeWindow parent,
                                            const std::u16string& title,
                                            const std::u16string& message) {
  return MessageBoxDialog::Show(
      parent, title, message, chrome::MESSAGE_BOX_TYPE_QUESTION,
      std::u16string(), std::u16string(), std::u16string());
}

void ShowQuestionMessageBox(
    gfx::NativeWindow parent,
    const std::u16string& title,
    const std::u16string& message,
    base::OnceCallback<void(MessageBoxResult)> callback) {
  MessageBoxDialog::Show(parent, title, message,
                         chrome::MESSAGE_BOX_TYPE_QUESTION, std::u16string(),
                         std::u16string(), std::u16string(),
                         std::move(callback));
}

MessageBoxResult ShowMessageBoxWithButtonText(gfx::NativeWindow parent,
                                              const std::u16string& title,
                                              const std::u16string& message,
                                              const std::u16string& yes_text,
                                              const std::u16string& no_text) {
  return MessageBoxDialog::Show(parent, title, message,
                                chrome::MESSAGE_BOX_TYPE_QUESTION, yes_text,
                                no_text, std::u16string());
}

}  // namespace chrome
