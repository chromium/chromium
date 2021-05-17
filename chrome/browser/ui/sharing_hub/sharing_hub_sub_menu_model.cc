// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/sharing_hub/sharing_hub_sub_menu_model.h"

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

bool SharingHubSubMenuModel::IsCommandIdEnabled(int command_id) const {
  // TODO crbug.com/1186848 ensure that commands are enabled based on sharing
  // criteria.
  return true;
}

void SharingHubSubMenuModel::ExecuteCommand(int command_id, int event_flags) {
  GlobalError* error =
      GlobalErrorServiceFactory::GetForProfile(browser_->profile())
          ->GetGlobalErrorByMenuItemCommandID(command_id);
  if (error) {
    error->ExecuteMenuItem(browser_);
    return;
  }

  // TODO crbug.com/1186848  Log metrics per command_id;
  chrome::ExecuteCommand(browser_, command_id);
}

void SharingHubSubMenuModel::Build(content::WebContents* web_contents) {
  if (!web_contents)
    return;

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  SharingHubService* const service =
      SharingHubServiceFactory::GetForProfile(profile);
  SharingHubModel* const model =
      service ? service->GetSharingHubModel() : nullptr;
  std::vector<SharingHubAction> actions;
  if (model) {
    model->GetActionList(web_contents, &actions);

    for (std::vector<SharingHubAction>::const_iterator it = actions.begin();
         it != actions.end(); ++it) {
      AddItemWithStringId(it->command_id, it->title);
    }
  }
  AddSeparator(ui::NORMAL_SEPARATOR);
}

}  // namespace sharing_hub
