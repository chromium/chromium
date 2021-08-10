// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sharing_hub/sharing_hub_sub_menu_model.h"

#include "base/metrics/user_metrics.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharing_hub/sharing_hub_model.h"
#include "chrome/browser/sharing_hub/sharing_hub_service.h"
#include "chrome/browser/sharing_hub/sharing_hub_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/models/simple_menu_model.h"

namespace sharing_hub {

SharingHubSubMenuModel::SharingHubSubMenuModel(Browser* browser)
    : SimpleMenuModel(this), browser_(browser) {
  Build(browser_->tab_strip_model()->GetActiveWebContents());
}

SharingHubSubMenuModel::~SharingHubSubMenuModel() = default;

bool SharingHubSubMenuModel::IsCommandIdEnabled(int command_id) const {
  return true;
}

void SharingHubSubMenuModel::ExecuteCommand(int command_id, int event_flags) {
  if (IsThirdPartyAction(command_id)) {
    SharingHubModel* const model = GetSharingHubModel();
    if (!model)
      return;
    model->ExecuteThirdPartyAction(
        browser_->tab_strip_model()->GetActiveWebContents(), command_id);
  } else {
    GlobalError* error =
        GlobalErrorServiceFactory::GetForProfile(browser_->profile())
            ->GetGlobalErrorByMenuItemCommandID(command_id);
    if (error) {
      error->ExecuteMenuItem(browser_);
      return;
    }
    base::RecordComputedAction(user_actions_by_id_[command_id]);
    chrome::ExecuteCommand(browser_, command_id);
  }
}

SharingHubModel* SharingHubSubMenuModel::GetSharingHubModel() const {
  SharingHubService* const service =
      SharingHubServiceFactory::GetForProfile(browser_->profile());
  return service ? service->GetSharingHubModel() : nullptr;
}

void SharingHubSubMenuModel::Build(content::WebContents* web_contents) {
  if (!web_contents)
    return;
  web_contents_ = web_contents;
  SharingHubModel* const model = GetSharingHubModel();
  if (!model)
    return;

  std::vector<SharingHubAction> first_party_actions;
  std::vector<SharingHubAction> third_party_actions;
  model->GetFirstPartyActionList(web_contents, &first_party_actions);
  model->GetThirdPartyActionList(web_contents, &third_party_actions);

  for (auto action : first_party_actions) {
    AddItem(action.command_id, action.title);
    user_actions_by_id_[action.command_id] = action.feature_name_for_metrics;
  }
  AddSeparator(ui::NORMAL_SEPARATOR);
  for (auto action : third_party_actions) {
    if (action.third_party_icon.isNull()) {
      AddItemWithIcon(action.command_id, action.title,
                      ui::ImageModel::FromVectorIcon(*action.icon, /*color*/ -1,
                                                     /*icon_size*/ 16));
    } else {
      AddItemWithIcon(action.command_id, action.title,
                      ui::ImageModel::FromImageSkia(action.third_party_icon));
    }
    third_party_action_ids_.push_back(action.command_id);
  }
}

bool SharingHubSubMenuModel::IsThirdPartyAction(int id) {
  return std::find(third_party_action_ids_.begin(),
                   third_party_action_ids_.end(),
                   id) != third_party_action_ids_.end();
}

}  // namespace sharing_hub
