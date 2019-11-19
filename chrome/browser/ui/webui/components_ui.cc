// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/components_ui.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/macros.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/localized_string.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "components/update_client/crx_update_item.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_CHROMEOS)
#include "components/user_manager/user_manager.h"
#endif

using content::WebUIMessageHandler;

namespace {

content::WebUIDataSource* CreateComponentsUIHTMLSource(Profile* profile) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIComponentsHost);

  source->OverrideContentSecurityPolicyScriptSrc(
      "script-src chrome://resources 'self' 'unsafe-eval';");

  static constexpr LocalizedString kStrings[] = {
      {"componentsTitle", IDS_COMPONENTS_TITLE},
      {"componentsNoneInstalled", IDS_COMPONENTS_NONE_INSTALLED},
      {"componentVersion", IDS_COMPONENTS_VERSION},
      {"checkUpdate", IDS_COMPONENTS_CHECK_FOR_UPDATE},
      {"noComponents", IDS_COMPONENTS_NO_COMPONENTS},
      {"statusLabel", IDS_COMPONENTS_STATUS_LABEL},
      {"checkingLabel", IDS_COMPONENTS_CHECKING_LABEL},
  };
  AddLocalizedStringsBulk(source, kStrings, base::size(kStrings));

  source->AddBoolean(
      "isGuest",
#if defined(OS_CHROMEOS)
      user_manager::UserManager::Get()->IsLoggedInAsGuest() ||
          user_manager::UserManager::Get()->IsLoggedInAsPublicAccount()
#else
      profile->IsOffTheRecord()
#endif
  );
  source->UseStringsJs();
  source->AddResourcePath("components.js", IDR_COMPONENTS_JS);
  source->SetDefaultResource(IDR_COMPONENTS_HTML);
  return source;
}

////////////////////////////////////////////////////////////////////////////////
//
// ComponentsDOMHandler
//
////////////////////////////////////////////////////////////////////////////////

// The handler for Javascript messages for the chrome://components/ page.
class ComponentsDOMHandler : public WebUIMessageHandler {
 public:
  ComponentsDOMHandler();
  ~ComponentsDOMHandler() override {}

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

  // Callback for the "requestComponentsData" message.
  void HandleRequestComponentsData(const base::ListValue* args);

  // Callback for the "checkUpdate" message.
  void HandleCheckUpdate(const base::ListValue* args);

 private:
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ComponentsDOMHandler);
};

ComponentsDOMHandler::ComponentsDOMHandler() {
}

void ComponentsDOMHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestComponentsData",
      base::BindRepeating(&ComponentsDOMHandler::HandleRequestComponentsData,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "checkUpdate",
      base::BindRepeating(&ComponentsDOMHandler::HandleCheckUpdate,
                          base::Unretained(this)));
}

void ComponentsDOMHandler::HandleRequestComponentsData(
    const base::ListValue* args) {
  base::DictionaryValue result;
  result.Set("components", ComponentsUI::LoadComponents());
  web_ui()->CallJavascriptFunctionUnsafe("returnComponentsData", result);
}

// This function is called when user presses button from html UI.
// TODO(shrikant): We need to make this button available based on current
// state e.g. If component state is currently updating then we need to disable
// button. (https://code.google.com/p/chromium/issues/detail?id=272540)
void ComponentsDOMHandler::HandleCheckUpdate(const base::ListValue* args) {
  if (args->GetSize() != 1) {
    NOTREACHED();
    return;
  }

  std::string component_id;
  if (!args->GetString(0, &component_id)) {
    NOTREACHED();
    return;
  }

  ComponentsUI::OnDemandUpdate(component_id);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
//
// ComponentsUI
//
///////////////////////////////////////////////////////////////////////////////

ComponentsUI::ComponentsUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<ComponentsDOMHandler>());

  // Set up the chrome://components/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateComponentsUIHTMLSource(profile));
  component_updater::ComponentUpdateService* cus =
      g_browser_process->component_updater();
  cus->AddObserver(this);
}

ComponentsUI::~ComponentsUI() {
  component_updater::ComponentUpdateService* cus =
      g_browser_process->component_updater();
  if (cus)
    cus->RemoveObserver(this);
}

// static
void ComponentsUI::OnDemandUpdate(const std::string& component_id) {
  component_updater::ComponentUpdateService* cus =
      g_browser_process->component_updater();
  cus->GetOnDemandUpdater().OnDemandUpdate(
      component_id, component_updater::OnDemandUpdater::Priority::FOREGROUND,
      component_updater::Callback());
}

// static
std::unique_ptr<base::ListValue> ComponentsUI::LoadComponents() {
  component_updater::ComponentUpdateService* cus =
      g_browser_process->component_updater();
  std::vector<std::string> component_ids;
  component_ids = cus->GetComponentIDs();

  // Construct DictionaryValues to return to UI.
  auto component_list = std::make_unique<base::ListValue>();
  for (size_t j = 0; j < component_ids.size(); ++j) {
    update_client::CrxUpdateItem item;
    if (cus->GetComponentDetails(component_ids[j], &item)) {
      auto component_entry = std::make_unique<base::DictionaryValue>();
      component_entry->SetString("id", component_ids[j]);
      component_entry->SetString("status", ServiceStatusToString(item.state));
      if (item.component) {
        component_entry->SetString("name", item.component->name);
        component_entry->SetString("version",
                                   item.component->version.GetString());
      }
      component_list->Append(std::move(component_entry));
    }
  }

  return component_list;
}

// static
base::RefCountedMemory* ComponentsUI::GetFaviconResourceBytes(
      ui::ScaleFactor scale_factor) {
  return ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytesForScale(
      IDR_PLUGINS_FAVICON, scale_factor);
}

base::string16 ComponentsUI::ComponentEventToString(Events event) {
  switch (event) {
    case Events::COMPONENT_CHECKING_FOR_UPDATES:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_EVT_STATUS_STARTED);
    case Events::COMPONENT_WAIT:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_EVT_STATUS_SLEEPING);
    case Events::COMPONENT_UPDATE_FOUND:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_EVT_STATUS_FOUND);
    case Events::COMPONENT_UPDATE_READY:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_EVT_STATUS_READY);
    case Events::COMPONENT_UPDATED:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_EVT_STATUS_UPDATED);
    case Events::COMPONENT_NOT_UPDATED:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_EVT_STATUS_NOTUPDATED);
    case Events::COMPONENT_UPDATE_ERROR:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_EVT_STATUS_UPDATE_ERROR);
    case Events::COMPONENT_UPDATE_DOWNLOADING:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_EVT_STATUS_DOWNLOADING);
  }
  return l10n_util::GetStringUTF16(IDS_COMPONENTS_UNKNOWN);
}

base::string16 ComponentsUI::ServiceStatusToString(
    update_client::ComponentState state) {
  // TODO(sorin): handle kDownloaded. For now, just handle it as kUpdating.
  switch (state) {
    case update_client::ComponentState::kNew:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_SVC_STATUS_NEW);
    case update_client::ComponentState::kChecking:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_SVC_STATUS_CHECKING);
    case update_client::ComponentState::kCanUpdate:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_SVC_STATUS_UPDATE);
    case update_client::ComponentState::kDownloadingDiff:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_SVC_STATUS_DNL_DIFF);
    case update_client::ComponentState::kDownloading:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_SVC_STATUS_DNL);
    case update_client::ComponentState::kUpdatingDiff:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_SVC_STATUS_UPDT_DIFF);
    case update_client::ComponentState::kUpdating:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_SVC_STATUS_UPDATING);
    case update_client::ComponentState::kDownloaded:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_SVC_STATUS_DOWNLOADED);
    case update_client::ComponentState::kUpdated:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_SVC_STATUS_UPDATED);
    case update_client::ComponentState::kUpToDate:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_SVC_STATUS_UPTODATE);
    case update_client::ComponentState::kUpdateError:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_SVC_STATUS_UPDATE_ERROR);
    case update_client::ComponentState::kUninstalled:  // Fall through.
    case update_client::ComponentState::kRun:
    case update_client::ComponentState::kLastStatus:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_UNKNOWN);
  }
  return l10n_util::GetStringUTF16(IDS_COMPONENTS_UNKNOWN);
}

void ComponentsUI::OnEvent(Events event, const std::string& id) {
  base::DictionaryValue parameters;
  parameters.SetString("event", ComponentEventToString(event));
  if (!id.empty()) {
    if (event == Events::COMPONENT_UPDATED) {
      auto* component_updater = g_browser_process->component_updater();
      update_client::CrxUpdateItem item;
      if (component_updater->GetComponentDetails(id, &item) && item.component)
        parameters.SetString("version", item.component->version.GetString());
    }
    parameters.SetString("id", id);
  }
  web_ui()->CallJavascriptFunctionUnsafe("onComponentEvent", parameters);
}
