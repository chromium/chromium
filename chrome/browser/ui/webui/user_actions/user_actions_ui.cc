// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/user_actions/user_actions_ui.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/user_actions/user_actions_ui_handler.h"
#include "chrome/common/url_constants.h"
#include "components/grit/user_actions_ui_resources.h"
#include "components/grit/user_actions_ui_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"

UserActionsUI::UserActionsUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up the chrome://user-actions/ source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(Profile::FromWebUI(web_ui),
                                             chrome::kChromeUIUserActionsHost);
  html_source->AddResourcePaths(kUserActionsUiResources);
  html_source->AddResourcePath("", IDR_USER_ACTIONS_UI_USER_ACTIONS_HTML);

  web_ui->AddMessageHandler(std::make_unique<UserActionsUIHandler>());
}

UserActionsUI::~UserActionsUI() = default;
