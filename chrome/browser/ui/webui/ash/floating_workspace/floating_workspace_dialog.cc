// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/floating_workspace/floating_workspace_dialog.h"

#include <string>

#include "chrome/browser/ash/floating_workspace/floating_workspace_service.h"
#include "chrome/browser/ash/floating_workspace/floating_workspace_service_factory.h"
#include "chrome/browser/ui/ash/login/oobe_dialog_size_utils.h"
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
    CHECK(service);
    service->StopRestoringSession();
  } else if (!json_retval.empty()) {
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

}  // namespace ash
