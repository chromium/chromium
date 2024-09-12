// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/login/login_screen_extension_ui/window.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/utility/wm_util.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/login/login_screen_extension_ui/create_options.h"
#include "chrome/browser/ui/ash/login/login_screen_extension_ui/dialog_delegate.h"
#include "chrome/browser/ui/ash/login/login_screen_extension_ui/login_web_view.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace login_screen_extension_ui {

WindowFactory::WindowFactory() = default;
WindowFactory::~WindowFactory() = default;

std::unique_ptr<Window> WindowFactory::Create(CreateOptions* create_options) {
  return std::make_unique<Window>(create_options);
}

Window::Window(CreateOptions* create_options)
    : dialog_delegate_(new DialogDelegate(create_options)),
      dialog_view_(
          new LoginWebView(ProfileHelper::GetSigninProfile(),
                           dialog_delegate_,
                           std::make_unique<ChromeWebContentsHandler>())) {
  dialog_widget_ = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::NATIVE_WIDGET_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW);
  params.delegate = dialog_view_.get();
  ash_util::SetupWidgetInitParamsForContainer(
      &params, kShellWindowId_LockScreenContainer);
  dialog_widget_->Init(std::move(params));
  dialog_widget_->set_movement_disabled(true);
  dialog_delegate_->set_native_window(dialog_widget_->GetNativeWindow());
  dialog_widget_->Show();
}

Window::~Window() {
  dialog_delegate_->set_can_close(true);
  dialog_widget_->Close();
}

DialogDelegate* Window::GetDialogDelegateForTesting() {
  return dialog_delegate_;
}

views::Widget* Window::GetDialogWidgetForTesting() {
  return dialog_widget_;
}

}  // namespace login_screen_extension_ui
}  // namespace ash
