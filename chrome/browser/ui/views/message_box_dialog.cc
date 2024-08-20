// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/message_box_dialog.h"

#include <utility>

#include "base/compiler_specific.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/task/current_thread.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/simple_message_box.h"
#include "chrome/browser/ui/simple_message_box_internal.h"
#include "chrome/grit/generated_resources.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/startup_metric_utils/browser/startup_metric_utils.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/base/mojom/ui_base_types.mojom-shared.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/display/screen.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/message_box_view.h"
#include "ui/views/views_delegate.h"
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
}
#endif

chrome::MessageBoxResult ShowSync(gfx::NativeWindow parent,
                                  std::u16string_view title,
                                  std::u16string_view message,
                                  chrome::MessageBoxType type,
                                  std::u16string_view yes_text,
                                  std::u16string_view no_text,
                                  std::u16string_view checkbox_text) {
  static bool g_message_box_is_showing_sync = false;
  // To avoid showing another MessageBoxDialog when one is already pending.
  // Otherwise, this might lead to a stack overflow due to infinite runloops.
  if (g_message_box_is_showing_sync)
    return chrome::MESSAGE_BOX_RESULT_NO;

  base::AutoReset<bool> is_showing(&g_message_box_is_showing_sync, true);
  chrome::MessageBoxResult result = chrome::MESSAGE_BOX_RESULT_NO;
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

bool CanUseNativeMessageBox() {
  // Only Windows and macOS have native message box.
  return BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN);
}

bool CanUseViewsMessageBox() {
  // The views toolkit could still be uninitialized during browser startup.
  if (!views::ViewsDelegate::GetInstance()) {
    return false;
  }

  // Views dialogs cannot be shown outside the UI thread.
  if (!base::CurrentUIThread::IsSet()) {
    return false;
  }

  // Views uses icon and text from the resource bundle and therefore can't
  // be used if the bundle is not initialized.
  if (!ui::ResourceBundle::HasSharedInstance()) {
    return false;
  }

  // aura::WindowTreeHost observes display::Screen for device scale change.
  if (!display::Screen::GetScreen()) {
    return false;
  }

  return true;
}

void ShowNativeMessageBox(gfx::NativeWindow parent,
                          std::u16string_view title,
                          std::u16string_view message,
                          chrome::MessageBoxType type,
                          std::u16string_view yes_text,
                          std::u16string_view no_text,
                          std::u16string_view checkbox_text,
                          MessageBoxDialog::MessageBoxResultCallback callback) {
  CHECK(CanUseNativeMessageBox());
#if BUILDFLAG(IS_WIN)
  LOG_IF(ERROR, !checkbox_text.empty())
      << "Dialog checkbox won't be shown, checkbox text: " << checkbox_text;

  int result = ui::MessageBox(views::HWNDForNativeWindow(parent),
                              base::AsWString(message), base::AsWString(title),
                              GetMessageBoxFlagsFromType(type));
  std::move(callback).Run((result == IDYES || result == IDOK)
                              ? chrome::MESSAGE_BOX_RESULT_YES
                              : chrome::MESSAGE_BOX_RESULT_NO);
#elif BUILDFLAG(IS_MAC)
  // Even though this function could return a value synchronously here in
  // principle, in practice call sites do not expect any behavior other than a
  // return of DEFERRED and an invocation of the callback.
  std::move(callback).Run(
      chrome::ShowMessageBoxCocoa(message, type, checkbox_text));
#else
  NOTREACHED();
#endif
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// MessageBoxDialog, public:

// static
chrome::MessageBoxResult MessageBoxDialog::Show(
    gfx::NativeWindow parent,
    std::u16string_view title,
    std::u16string_view message,
    chrome::MessageBoxType type,
    std::u16string_view yes_text,
    std::u16string_view no_text,
    std::u16string_view checkbox_text,
    MessageBoxDialog::MessageBoxResultCallback callback) {
  if (!callback) {
    return ShowSync(parent, title, message, type, yes_text, no_text,
                    checkbox_text);
  }

  startup_metric_utils::GetBrowser().SetNonBrowserUIDisplayed();
  if (chrome::internal::g_should_skip_message_box_for_test) {
    std::move(callback).Run(chrome::MESSAGE_BOX_RESULT_YES);
    return chrome::MESSAGE_BOX_RESULT_DEFERRED;
  }

  // Use a native message box if views is not available or no parent is given.
  // This typically is used during browser startup and shutdown when there is
  // no browser window to be used as a parent window.
  if (CanUseNativeMessageBox() && (!CanUseViewsMessageBox() || !parent)) {
    ShowNativeMessageBox(parent, title, message, type, yes_text, no_text,
                         checkbox_text, std::move(callback));
    return chrome::MESSAGE_BOX_RESULT_DEFERRED;
  }

  if (!CanUseViewsMessageBox()) {
    CHECK(!CanUseNativeMessageBox());
    LOG(ERROR) << "Unable to show message box: " << title << " - " << message;
    std::move(callback).Run(chrome::MESSAGE_BOX_RESULT_NO);
    return chrome::MESSAGE_BOX_RESULT_DEFERRED;
  }

  MessageBoxDialog* dialog = new MessageBoxDialog(
      title, message, type, yes_text, no_text, checkbox_text);

  // System modals have no parent and are only supported on ChromeOS Ash.
  const bool is_modal = parent || BUILDFLAG(IS_CHROMEOS_ASH);
  views::Widget* widget = nullptr;
  if (is_modal) {
    dialog->SetModalType(parent ? ui::mojom::ModalType::kWindow
                                : ui::mojom::ModalType::kSystem);
    widget = constrained_window::CreateBrowserModalDialogViews(dialog, parent);
  } else {
    widget =
        views::DialogDelegate::CreateDialogWidget(dialog, nullptr, nullptr);
    // Move the dialog's widget on top so other windows do not obscure it.
    widget->SetZOrderLevel(ui::ZOrderLevel::kFloatingWindow);
  }

  widget->Show();
  dialog->Run(std::move(callback));
  return chrome::MESSAGE_BOX_RESULT_DEFERRED;
}

void MessageBoxDialog::OnDialogAccepted() {
  return Done(!message_box_view_->HasVisibleCheckBox() ||
                      message_box_view_->IsCheckBoxSelected()
                  ? chrome::MESSAGE_BOX_RESULT_YES
                  : chrome::MESSAGE_BOX_RESULT_NO);
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

void MessageBoxDialog::OnWidgetDestroying(views::Widget* widget) {
  // If the native window is closing and the callback has not been invoked yet,
  // invoke it now so that the message loop is not stuck waiting for a dialog
  // result.
  if (result_callback_) {
    Done(chrome::MESSAGE_BOX_RESULT_NO);
  }
}

////////////////////////////////////////////////////////////////////////////////
// MessageBoxDialog, private:

MessageBoxDialog::MessageBoxDialog(std::u16string_view title,
                                   std::u16string_view message,
                                   chrome::MessageBoxType type,
                                   std::u16string_view yes_text,
                                   std::u16string_view no_text,
                                   std::u16string_view checkbox_text)
    : window_title_(title),
      type_(type),
      message_box_view_(new views::MessageBoxView(std::u16string(message))) {
  SetButtons(type_ == chrome::MESSAGE_BOX_TYPE_QUESTION
                 ? static_cast<int>(ui::mojom::DialogButton::kOk) |
                       static_cast<int>(ui::mojom::DialogButton::kCancel)
                 : static_cast<int>(ui::mojom::DialogButton::kOk));

  SetAcceptCallback(base::BindOnce(&MessageBoxDialog::OnDialogAccepted,
                                   base::Unretained(this)));
  SetCancelCallback(base::BindOnce(&MessageBoxDialog::Done,
                                   base::Unretained(this),
                                   chrome::MESSAGE_BOX_RESULT_NO));
  SetCloseCallback(base::BindOnce(&MessageBoxDialog::Done,
                                  base::Unretained(this),
                                  chrome::MESSAGE_BOX_RESULT_NO));
  SetOwnedByWidget(true);

  std::u16string ok_text(yes_text);
  if (ok_text.empty()) {
    ok_text =
        type_ == chrome::MESSAGE_BOX_TYPE_QUESTION
            ? l10n_util::GetStringUTF16(IDS_CONFIRM_MESSAGEBOX_YES_BUTTON_LABEL)
            : l10n_util::GetStringUTF16(IDS_OK);
  }
  SetButtonLabel(ui::mojom::DialogButton::kOk, ok_text);

  // Only MESSAGE_BOX_TYPE_QUESTION has a Cancel button.
  if (type_ == chrome::MESSAGE_BOX_TYPE_QUESTION) {
    std::u16string cancel_text(no_text);
    if (cancel_text.empty())
      cancel_text = l10n_util::GetStringUTF16(IDS_CANCEL);
    SetButtonLabel(ui::mojom::DialogButton::kCancel, cancel_text);
  }

  if (!checkbox_text.empty())
    message_box_view_->SetCheckBoxLabel(std::u16string(checkbox_text));
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
