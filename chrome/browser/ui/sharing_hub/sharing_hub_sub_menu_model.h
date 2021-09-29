// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SHARING_HUB_SHARING_HUB_SUB_MENU_MODEL_H_
#define CHROME_BROWSER_UI_SHARING_HUB_SHARING_HUB_SUB_MENU_MODEL_H_

#include <map>
#include "base/memory/raw_ptr.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/models/simple_menu_model.h"

class Browser;

namespace sharing_hub {

class SharingHubModel;

class SharingHubSubMenuModel : public ui::SimpleMenuModel,
                               public ui::SimpleMenuModel::Delegate {
 public:
  explicit SharingHubSubMenuModel(Browser* browser);

  SharingHubSubMenuModel(const SharingHubSubMenuModel&) = delete;
  SharingHubSubMenuModel& operator=(const SharingHubSubMenuModel&) = delete;

  ~SharingHubSubMenuModel() override;

  // Overridden from ui::SimpleMenuModel::Delegate:
  bool IsCommandIdEnabled(int command_id) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

 private:
  SharingHubModel* GetSharingHubModel() const;
  void Build(content::WebContents* web_contents);
  bool IsThirdPartyAction(int id);

  raw_ptr<Browser> browser_;
  raw_ptr<content::WebContents> web_contents_;
  std::vector<int> third_party_action_ids_;

  // A list of user action names mapped to action id.
  std::map<int, std::string> user_actions_by_id_;
};
}  // namespace sharing_hub

#endif  // CHROME_BROWSER_UI_SHARING_HUB_SHARING_HUB_SUB_MENU_MODEL_H_
