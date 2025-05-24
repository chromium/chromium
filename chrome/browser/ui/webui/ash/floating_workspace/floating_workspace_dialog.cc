// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/floating_workspace/floating_workspace_dialog.h"

#include <string>

#include "chrome/browser/ash/floating_workspace/floating_workspace_metrics_util.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_service.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_service_factory.h"
#include "chrome/browser/ui/ash/login/oobe_dialog_size_utils.h"
#include "chrome/browser/ui/webui/ash/floating_workspace/floating_workspace_handler.h"
#include "chrome/browser/ui/webui/ash/floating_workspace/floating_workspace_ui.h"
#include "chrome/browser/ui/webui/ash/system_web_dialog/system_web_dialog_delegate.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_types.h"
#include "url/gurl.h"

namespace ash {

namespace {
FloatingWorkspaceDialog* g_dialog = nullptr;
}  // namespace

FloatingWorkspaceDialog::FloatingWorkspaceDialog()
    : SystemWebDialogDelegate(GURL(chrome::kChromeUIFloatingWorkspaceDialogURL),
                              /*title=*/std::u16string()) {
  CHECK(!g_dialog);
  g_dialog = this;
  set_dialog_modal_type(ui::mojom::ModalType::kSystem);
}

FloatingWorkspaceDialog::~FloatingWorkspaceDialog() {
  CHECK_EQ(this, g_dialog);
  g_dialog = nullptr;
}

FloatingWorkspaceDialogHandler* FloatingWorkspaceDialog::GetHandler() {
  if (!g_dialog) {
    return nullptr;
  }
  auto* web_ui = g_dialog->webui();
  if (!web_ui) {
    return nullptr;
  }
  auto* controller = web_ui->GetController();
  if (!controller) {
    return nullptr;
  }
  return static_cast<FloatingWorkspaceUI*>(controller)->GetMainHandler();
}

void FloatingWorkspaceDialog::GetDialogSize(gfx::Size* size) const {
  *size = CalculateOobeDialogSizeForPrimaryDisplay();
}

void FloatingWorkspaceDialog::OnDialogClosed(const std::string& json_retval) {
  if (json_retval == "stopRestoringSession") {
    const user_manager::User* user =
        user_manager::UserManager::Get()->GetPrimaryUser();
    CHECK(user);
    content::BrowserContext* browser_context =
        ash::BrowserContextHelper::Get()->GetBrowserContextByUser(user);
    CHECK(browser_context);
    FloatingWorkspaceService* service =
        FloatingWorkspaceServiceFactory::GetForProfile(browser_context);
    if (service) {
      service->StopRestoringSession();
    }
    floating_workspace_metrics_util::
        RecordFloatingWorkspaceStartupUiClosureReason(
            floating_workspace_metrics_util::StartupUiClosureReason::kManual);
  } else if (json_retval.empty()) {
    floating_workspace_metrics_util::
        RecordFloatingWorkspaceStartupUiClosureReason(
            floating_workspace_metrics_util::StartupUiClosureReason::
                kAutomatic);
  } else {
    NOTREACHED();
  }

  SystemWebDialogDelegate::OnDialogClosed(json_retval);
}

bool FloatingWorkspaceDialog::ShouldShowCloseButton() const {
  return false;
}

bool FloatingWorkspaceDialog::ShouldShowDialogTitle() const {
  return false;
}

bool FloatingWorkspaceDialog::ShouldCloseDialogOnEscape() const {
  return false;
}

gfx::NativeWindow FloatingWorkspaceDialog::GetNativeWindow() {
  if (g_dialog) {
    return g_dialog->dialog_window();
  }
  return nullptr;
}

void FloatingWorkspaceDialog::ShowDefaultScreen() {
  FloatingWorkspaceDialog::Show();
  FloatingWorkspaceDialogHandler* handler = GetHandler();
  if (handler) {
    handler->ShowDefaultScreen();
  }
}

void FloatingWorkspaceDialog::ShowNetworkScreen() {
  FloatingWorkspaceDialog::Show();
  FloatingWorkspaceDialogHandler* handler = GetHandler();
  if (handler) {
    handler->ShowNetworkScreen();
  }
}

void FloatingWorkspaceDialog::ShowErrorScreen() {
  FloatingWorkspaceDialog::Show();
  FloatingWorkspaceDialogHandler* handler = GetHandler();
  if (handler) {
    handler->ShowErrorScreen();
  }
}

void FloatingWorkspaceDialog::Close() {
  if (g_dialog) {
    g_dialog->SystemWebDialogDelegate::Close();
  }
}

// static
void FloatingWorkspaceDialog::Show() {
  if (g_dialog) {
    g_dialog->dialog_window()->Focus();
    return;
  }

  // Will be deleted by `SystemWebDialogDelegate::OnDialogClosed`.
  g_dialog = new FloatingWorkspaceDialog();
  g_dialog->ShowSystemDialog();
}

std::optional<FloatingWorkspaceDialog::State>
FloatingWorkspaceDialog::IsShown() {
  if (!g_dialog) {
    return std::nullopt;
  }
  FloatingWorkspaceDialogHandler* handler = GetHandler();
  if (!handler) {
    return std::nullopt;
  }
  return handler->state();
}

}  // namespace ash
