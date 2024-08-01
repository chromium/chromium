// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/profile_internals/profile_internals_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/profile_internals/profile_internals_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/dev_ui_browser_resources.h"
#include "chrome/grit/profile_internals_resources.h"
#include "chrome/grit/profile_internals_resources_map.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"

ProfileInternalsUI::ProfileInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  // Set up the chrome://profile-internals source.
  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          browser_context, chrome::kChromeUIProfileInternalsHost);

  web_ui->AddMessageHandler(std::make_unique<ProfileInternalsHandler>());

  // Add required resources.
  webui::SetupWebUIDataSource(html_source,
                              base::make_span(kProfileInternalsResources,
                                              kProfileInternalsResourcesSize),
                              IDR_PROFILE_INTERNALS_PROFILE_INTERNALS_HTML);
}

ProfileInternalsUI::~ProfileInternalsUI() = default;
