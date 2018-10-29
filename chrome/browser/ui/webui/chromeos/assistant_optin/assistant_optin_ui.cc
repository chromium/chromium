// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/assistant_optin/assistant_optin_ui.h"

#include <memory>

#include "ash/public/cpp/shell_window_ids.h"
#include "base/bind.h"
#include "base/macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "components/arc/arc_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace chromeos {

namespace {

bool is_active = false;

constexpr int kAssistantOptInDialogWidth = 768;
constexpr int kAssistantOptInDialogHeight = 640;

}  // namespace

AssistantOptInUI::AssistantOptInUI(content::WebUI* web_ui)
    : ui::WebDialogUI(web_ui), weak_factory_(this) {
  // Set up the chrome://assistant-optin source.
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIAssistantOptInHost);

  auto assistant_handler = std::make_unique<AssistantOptInFlowScreenHandler>();
  auto* assistant_handler_ptr = assistant_handler.get();
  web_ui->AddMessageHandler(std::move(assistant_handler));
  assistant_handler_ptr->SetupAssistantConnection();

  base::DictionaryValue localized_strings;
  assistant_handler_ptr->GetLocalizedStrings(&localized_strings);
  source->AddLocalizedStrings(localized_strings);
  source->SetJsonPath("strings.js");
  source->AddResourcePath("assistant_optin.js", IDR_ASSISTANT_OPTIN_JS);
  source->AddResourcePath("assistant_logo.png", IDR_ASSISTANT_LOGO_PNG);
  source->SetDefaultResource(IDR_ASSISTANT_OPTIN_HTML);
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), source);

  // Do not zoom for Assistant opt-in web contents.
  content::HostZoomMap* zoom_map =
      content::HostZoomMap::GetForWebContents(web_ui->GetWebContents());
  DCHECK(zoom_map);
  zoom_map->SetZoomLevelForHost(web_ui->GetWebContents()->GetURL().host(), 0);
}

AssistantOptInUI::~AssistantOptInUI() = default;

// AssistantOptInDialog

// static
void AssistantOptInDialog::Show(
    ash::mojom::AssistantSetup::StartAssistantOptInFlowCallback callback) {
  DCHECK(!is_active);
  AssistantOptInDialog* dialog = new AssistantOptInDialog(std::move(callback));

  int container_id = dialog->GetDialogModalType() == ui::MODAL_TYPE_NONE
                         ? ash::kShellWindowId_DefaultContainer
                         : ash::kShellWindowId_LockSystemModalContainer;
  auto* window = chrome::ShowWebDialogInContainer(
      container_id, ProfileManager::GetActiveUserProfile(), dialog, true);

  MultiUserWindowManager::GetInstance()->SetWindowOwner(
      window,
      user_manager::UserManager::Get()->GetActiveUser()->GetAccountId());
}

// static
bool AssistantOptInDialog::IsActive() {
  return is_active;
}

AssistantOptInDialog::AssistantOptInDialog(
    ash::mojom::AssistantSetup::StartAssistantOptInFlowCallback callback)
    : SystemWebDialogDelegate(GURL(chrome::kChromeUIAssistantOptInURL),
                              base::string16()),
      callback_(std::move(callback)) {
  DCHECK(!is_active);
  is_active = true;
}

AssistantOptInDialog::~AssistantOptInDialog() {
  is_active = false;
}

void AssistantOptInDialog::GetDialogSize(gfx::Size* size) const {
  size->SetSize(kAssistantOptInDialogWidth, kAssistantOptInDialogHeight);
}

std::string AssistantOptInDialog::GetDialogArgs() const {
  return std::string();
}

bool AssistantOptInDialog::ShouldShowDialogTitle() const {
  return false;
}

void AssistantOptInDialog::OnDialogClosed(const std::string& json_retval) {
  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  const bool completed =
      prefs->GetBoolean(arc::prefs::kVoiceInteractionEnabled) &&
      prefs->GetBoolean(arc::prefs::kArcVoiceInteractionValuePropAccepted);
  std::move(callback_).Run(completed);
  SystemWebDialogDelegate::OnDialogClosed(json_retval);
}

}  // namespace chromeos
