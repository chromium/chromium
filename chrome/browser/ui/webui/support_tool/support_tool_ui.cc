// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/support_tool/support_tool_ui.h"

#include <string>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/support_tool_resources.h"
#include "chrome/grit/support_tool_resources_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

const char kSupportCaseIDQuery[] = "case_id";

namespace {
// Returns the support case ID that's extracted from `url` with query
// `kSupportCaseIDQuery`. Returns empty string if `url` doesn't contain support
// case ID.
std::string GetSupportCaseIDFromURL(const GURL& url) {
  std::string support_case_id;
  if (url.has_query()) {
    net::GetValueForKeyInQuery(url, kSupportCaseIDQuery, &support_case_id);
  }
  return support_case_id;
}

content::WebUIDataSource* CreateSupportToolHTMLSource(const GURL& url) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUISupportToolHost);

  source->AddString("caseId", GetSupportCaseIDFromURL(url));

  webui::SetupWebUIDataSource(
      source, base::make_span(kSupportToolResources, kSupportToolResourcesSize),
      IDR_SUPPORT_TOOL_SUPPORT_TOOL_CONTAINER_HTML);

  return source;
}

}  // namespace

SupportToolUI::SupportToolUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  // Set up the chrome://support-tool/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(
      profile, CreateSupportToolHTMLSource(web_ui->GetWebContents()->GetURL()));
}

SupportToolUI::~SupportToolUI() = default;
