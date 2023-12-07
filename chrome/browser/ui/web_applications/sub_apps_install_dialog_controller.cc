// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/web_applications/sub_apps_install_dialog_controller.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/web_applications/web_app_dialogs.h"
#include "chrome/browser/ui/web_applications/web_app_ui_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/webapps/common/web_app_id.h"
#include "ui/views/widget/widget.h"

namespace {

std::optional<web_app::SubAppsInstallDialogController::DialogActionForTesting>
    g_dialog_override_for_testing;

}  // namespace.

namespace web_app {

// static
[[nodiscard]] base::AutoReset<
    std::optional<SubAppsInstallDialogController::DialogActionForTesting>>
SubAppsInstallDialogController::SetAutomaticActionForTesting(
    DialogActionForTesting auto_accept) {
  return base::AutoReset<std::optional<DialogActionForTesting>>(
      &g_dialog_override_for_testing, auto_accept);
}

SubAppsInstallDialogController::SubAppsInstallDialogController() = default;

void SubAppsInstallDialogController::Init(
    base::OnceCallback<void(bool)> callback,
    const std::vector<std::unique_ptr<WebAppInstallInfo>>& sub_apps,
    const std::string& parent_app_name,
    const webapps::AppId& parent_app_id,
    Profile* profile,
    gfx::NativeWindow window) {
  if (g_dialog_override_for_testing) {
    switch (g_dialog_override_for_testing.value()) {
      case DialogActionForTesting::kAccept:
        std::move(callback).Run(true);
        break;
      case DialogActionForTesting::kCancel:
        std::move(callback).Run(false);
        break;
    }
    return;
  }

  callback_ = std::move(callback);

  widget_ = CreateSubAppsInstallDialogWidget(
      base::UTF8ToUTF16(parent_app_name), sub_apps,
      base::BindRepeating(OpenAppSettingsForParentApp, parent_app_id, profile),
      window);
  widget_observation_.Observe(widget_);
  widget_->Show();
}

void SubAppsInstallDialogController::OnWidgetDestroying(views::Widget* widget) {
  widget_observation_.Reset();
  widget_ = nullptr;
  switch (widget->closed_reason()) {
    case views::Widget::ClosedReason::kAcceptButtonClicked:
      std::move(callback_).Run(true);
      break;
    case views::Widget::ClosedReason::kUnspecified:
    case views::Widget::ClosedReason::kEscKeyPressed:
    case views::Widget::ClosedReason::kCloseButtonClicked:
    case views::Widget::ClosedReason::kLostFocus:
    case views::Widget::ClosedReason::kCancelButtonClicked:
      std::move(callback_).Run(false);
      break;
  }
}

views::Widget* SubAppsInstallDialogController::GetWidgetForTesting() {
  return widget_;
}

SubAppsInstallDialogController::~SubAppsInstallDialogController() {
  if (widget_) {
    widget_observation_.Reset();
    widget_->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  }
}

}  // namespace web_app
