// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/components/components_handler.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check.h"
#include "base/notreached.h"
#include "base/values.h"
#include "chrome/grit/generated_resources.h"
#include "components/update_client/crx_update_item.h"
#include "ui/base/l10n/l10n_util.h"

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
}

void ComponentsHandler::OnJavascriptAllowed() {
  observation_.Observe(component_updater_);
}

void ComponentsHandler::OnJavascriptDisallowed() {
  observation_.Reset();
}

void ComponentsHandler::HandleRequestComponentsData(
    const base::ListValue* args) {
  AllowJavascript();
  const base::Value& callback_id = args->GetList()[0];

  base::DictionaryValue result;
  result.Set("components", LoadComponents());
  ResolveJavascriptCallback(callback_id, result);
}

// This function is called when user presses button from html UI.
// TODO(shrikant): We need to make this button available based on current
// state e.g. If component state is currently updating then we need to disable
// button. (https://code.google.com/p/chromium/issues/detail?id=272540)
void ComponentsHandler::HandleCheckUpdate(const base::ListValue* args) {
  if (args->GetSize() != 1) {
    NOTREACHED();
    return;
  }

  std::string component_id;
  if (!args->GetString(0, &component_id)) {
    NOTREACHED();
    return;
  }

  OnDemandUpdate(component_id);
}

void ComponentsHandler::OnEvent(Events event, const std::string& id) {
  base::DictionaryValue parameters;
  parameters.SetString("event", ComponentEventToString(event));
  if (!id.empty()) {
    if (event == Events::COMPONENT_UPDATED) {
      update_client::CrxUpdateItem item;
      if (component_updater_->GetComponentDetails(id, &item) && item.component)
        parameters.SetString("version", item.component->version.GetString());
    }
    parameters.SetString("id", id);
  }
  FireWebUIListener("component-event", parameters);
}

std::u16string ComponentsHandler::ComponentEventToString(Events event) {
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
    case Events::COMPONENT_UPDATE_UPDATING:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_EVT_STATUS_UPDATING);
  }
  return l10n_util::GetStringUTF16(IDS_COMPONENTS_UNKNOWN);
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
    case update_client::ComponentState::kDownloaded:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_SVC_STATUS_DOWNLOADED);
    case update_client::ComponentState::kUpdated:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_SVC_STATUS_UPDATED);
    case update_client::ComponentState::kUpToDate:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_SVC_STATUS_UPTODATE);
    case update_client::ComponentState::kUpdateError:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_SVC_STATUS_UPDATE_ERROR);
    case update_client::ComponentState::kUninstalled:  // Fall through.
    case update_client::ComponentState::kRegistration:
    case update_client::ComponentState::kRun:
    case update_client::ComponentState::kLastStatus:
      return l10n_util::GetStringUTF16(IDS_COMPONENTS_UNKNOWN);
  }
  return l10n_util::GetStringUTF16(IDS_COMPONENTS_UNKNOWN);
}

void ComponentsHandler::OnDemandUpdate(const std::string& component_id) {
  component_updater_->GetOnDemandUpdater().OnDemandUpdate(
      component_id, component_updater::OnDemandUpdater::Priority::FOREGROUND,
      component_updater::Callback());
}

std::unique_ptr<base::ListValue> ComponentsHandler::LoadComponents() {
  std::vector<std::string> component_ids;
  component_ids = component_updater_->GetComponentIDs();

  // Construct DictionaryValues to return to UI.
  auto component_list = std::make_unique<base::ListValue>();
  for (size_t j = 0; j < component_ids.size(); ++j) {
    update_client::CrxUpdateItem item;
    if (component_updater_->GetComponentDetails(component_ids[j], &item)) {
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
