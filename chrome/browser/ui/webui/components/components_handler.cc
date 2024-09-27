// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/components/components_handler.h"

#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/notreached.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/grit/generated_resources.h"
#include "components/update_client/crx_update_item.h"
#include "ui/base/l10n/l10n_util.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/common/webui_url_constants.h"
#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/crosapi/browser_manager.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/lacros/lacros_url_handling.h"
#endif
#endif  // BUILDFLAG(IS_CHROMEOS)

ComponentsHandler::ComponentsHandler(
    component_updater::ComponentUpdateService* component_updater)
    : component_updater_(component_updater) {
  DCHECK(component_updater_);
}
ComponentsHandler::~ComponentsHandler() = default;

void ComponentsHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "requestComponentsData",
      base::BindRepeating(&ComponentsHandler::HandleRequestComponentsData,
                          base::Unretained(this)));

  web_ui()->RegisterMessageCallback(
      "checkUpdate", base::BindRepeating(&ComponentsHandler::HandleCheckUpdate,
                                         base::Unretained(this)));

#if BUILDFLAG(IS_CHROMEOS)
  web_ui()->RegisterMessageCallback(
      "crosUrlComponentsRedirect",
      base::BindRepeating(&ComponentsHandler::HandleCrosUrlComponentsRedirect,
                          base::Unretained(this)));
#endif
}

void ComponentsHandler::OnJavascriptAllowed() {
  observation_.Observe(component_updater_.get());
}

void ComponentsHandler::OnJavascriptDisallowed() {
  observation_.Reset();
}

void ComponentsHandler::HandleRequestComponentsData(
    const base::Value::List& args) {
  AllowJavascript();
  const base::Value& callback_id = args[0];

  base::Value::Dict result;
  result.Set("components", LoadComponents());

#if BUILDFLAG(IS_CHROMEOS)
#if BUILDFLAG(IS_CHROMEOS_ASH)
  const bool showSystemFlagsLink = crosapi::browser_util::IsLacrosEnabled();
#else
  const bool showSystemFlagsLink = true;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  result.Set("showOsLink", showSystemFlagsLink);
#endif  // BUILDFLAG(IS_CHROMEOS)

  ResolveJavascriptCallback(callback_id, result);
}

// This function is called when user presses button from html UI.
// TODO(shrikant): We need to make this button available based on current
// state e.g. If component state is currently updating then we need to disable
// button. (https://code.google.com/p/chromium/issues/detail?id=272540)
void ComponentsHandler::HandleCheckUpdate(const base::Value::List& args) {
  if (args.size() != 1) {
    NOTREACHED_IN_MIGRATION();
    return;
  }

  if (!args[0].is_string()) {
    NOTREACHED_IN_MIGRATION();
    return;
  }
  const std::string& component_id = args[0].GetString();

  OnDemandUpdate(component_id);
}

void ComponentsHandler::OnEvent(const update_client::CrxUpdateItem& item) {
  base::Value::Dict parameters;
  parameters.Set("event", ServiceStatusToString(item.state));
  parameters.Set("id", item.id);
  if (item.component) {
    parameters.Set("version", item.component->version.GetString());
  }
  FireWebUIListener("component-event", parameters);
}

std::u16string ComponentsHandler::ServiceStatusToString(
    update_client::ComponentState state) {
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
    case update_client::ComponentState::kUpdated:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_SVC_STATUS_UPDATED);
    case update_client::ComponentState::kUpToDate:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_SVC_STATUS_UPTODATE);
    case update_client::ComponentState::kUpdateError:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_SVC_STATUS_UPDATE_ERROR);
    case update_client::ComponentState::kRun:  // Fall through.
    case update_client::ComponentState::kLastStatus:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_UNKNOWN);
  }
  return l10n_util::GetStringUTF16(IDS_COMPONENTS_UNKNOWN);
}

#if BUILDFLAG(IS_CHROMEOS)
void ComponentsHandler::HandleCrosUrlComponentsRedirect(
    const base::Value::List& args) {
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  lacros_url_handling::NavigateInAsh(GURL(chrome::kChromeUIComponentsUrl));
#else
  // Note: This will only be called by the UI when Lacros is available.
  DCHECK(crosapi::BrowserManager::Get());
  crosapi::BrowserManager::Get()->SwitchToTab(
      GURL(chrome::kChromeUIComponentsUrl),
      /*path_behavior=*/NavigateParams::RESPECT);
#endif
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void ComponentsHandler::OnDemandUpdate(const std::string& component_id) {
  component_updater_->GetOnDemandUpdater().OnDemandUpdate(
      component_id, component_updater::OnDemandUpdater::Priority::FOREGROUND,
      component_updater::Callback());
}

base::Value::List ComponentsHandler::LoadComponents() {
  const std::vector<std::string> component_ids =
      component_updater_->GetComponentIDs();

  // Construct `base::Value::Dict` to return to UI.
  base::Value::List component_list;
  for (const auto& component_id : component_ids) {
    update_client::CrxUpdateItem item;
    if (component_updater_->GetComponentDetails(component_id, &item)) {
      base::Value::Dict component_entry;
      component_entry.Set("id", component_id);
      component_entry.Set("status", ServiceStatusToString(item.state));
      if (item.component) {
        component_entry.Set("name", item.component->name);
        component_entry.Set("version", item.component->version.GetString());
      }
      component_list.Append(std::move(component_entry));
    }
  }

  return component_list;
}
