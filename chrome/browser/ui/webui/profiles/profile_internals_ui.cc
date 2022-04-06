// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/profiles/profile_internals_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/dev_ui_browser_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_controller.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"

ProfileInternalsUI::ProfileInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  // Set up the chrome://profile-internals source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::Create(chrome::kChromeUIProfileInternalsHost);
  html_source->SetDefaultResource(IDR_PROFILE_INTERNALS_HTML);
  content::WebUIDataSource::Add(Profile::FromWebUI(web_ui), html_source);
}

ProfileInternalsUI::~ProfileInternalsUI() = default;
