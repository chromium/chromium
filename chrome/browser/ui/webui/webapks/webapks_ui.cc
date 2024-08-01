// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ui/webui/webapks/webapks_ui.h"

#include <memory>
#include <string>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webapks/webapks_handler.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/webapks_resources.h"
#include "chrome/grit/webapks_resources_map.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

using content::WebUIDataSource;

namespace {

void CreateAndAddWebApksUIDataSource(Profile* profile) {
  WebUIDataSource* html_source =
      WebUIDataSource::CreateAndAdd(profile, chrome::kChromeUIWebApksHost);
  html_source->UseStringsJs();

  html_source->AddResourcePaths(
      base::make_span(kWebapksResources, kWebapksResourcesSize));
  html_source->SetDefaultResource(IDR_WEBAPKS_ABOUT_WEBAPKS_HTML);
}

}  // anonymous namespace

WebApksUI::WebApksUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<WebApksHandler>());
  CreateAndAddWebApksUIDataSource(Profile::FromWebUI(web_ui));
}

WebApksUI::~WebApksUI() {}
