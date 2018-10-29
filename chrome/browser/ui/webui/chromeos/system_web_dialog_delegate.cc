// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/system_web_dialog_delegate.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/browser_context.h"

namespace chromeos {

SystemWebDialogDelegate::SystemWebDialogDelegate(const GURL& gurl,
                                                 const base::string16& title)
    : gurl_(gurl),
      title_(title),
      modal_type_(session_manager::SessionManager::Get()->session_state() ==
                          session_manager::SessionState::ACTIVE
                      ? ui::MODAL_TYPE_NONE
                      : ui::MODAL_TYPE_SYSTEM) {}

SystemWebDialogDelegate::~SystemWebDialogDelegate() {}

ui::ModalType SystemWebDialogDelegate::GetDialogModalType() const {
  return modal_type_;
}

base::string16 SystemWebDialogDelegate::GetDialogTitle() const {
  return title_;
}

GURL SystemWebDialogDelegate::GetDialogContentURL() const {
  return gurl_;
}

void SystemWebDialogDelegate::GetWebUIMessageHandlers(
    std::vector<content::WebUIMessageHandler*>* handlers) const {}

void SystemWebDialogDelegate::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kDialogWidth, kDialogHeight);
}

std::string SystemWebDialogDelegate::GetDialogArgs() const {
  return std::string();
}

void SystemWebDialogDelegate::OnDialogShown(
    content::WebUI* webui,
    content::RenderViewHost* render_view_host) {
  webui_ = webui;
}

void SystemWebDialogDelegate::OnDialogClosed(const std::string& json_retval) {
  delete this;
}

void SystemWebDialogDelegate::OnCloseContents(content::WebContents* source,
                                              bool* out_close_dialog) {
  *out_close_dialog = true;
}

bool SystemWebDialogDelegate::ShouldShowDialogTitle() const {
  return !title_.empty();
}

void SystemWebDialogDelegate::ShowSystemDialog(bool is_minimal_style) {
  content::BrowserContext* browser_context =
      ProfileManager::GetActiveUserProfile();
  int container_id = GetDialogModalType() == ui::MODAL_TYPE_NONE
                         ? ash::kShellWindowId_AlwaysOnTopContainer
                         : ash::kShellWindowId_LockSystemModalContainer;
  dialog_window_ = chrome::ShowWebDialogInContainer(
      container_id, browser_context, this, is_minimal_style);
}

}  // namespace chromeos
