// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/quota_internals/quota_internals_ui.h"

#include <memory>
#include <string>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/quota_internals/quota_internals_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/quota_internals_resources.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

using content::WebContents;

namespace {

content::WebUIDataSource* CreateQuotaInternalsHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIQuotaInternalsHost);

  source->UseStringsJs();
  source->AddResourcePath(
      "event_handler.js", IDR_QUOTA_INTERNALS_EVENT_HANDLER_JS);
  source->AddResourcePath(
      "message_dispatcher.js", IDR_QUOTA_INTERNALS_MESSAGE_DISPATCHER_JS);
  source->SetDefaultResource(IDR_QUOTA_INTERNALS_MAIN_HTML);
  return source;
}

}  // namespace

QuotaInternalsUI::QuotaInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  web_ui->AddMessageHandler(
      std::make_unique<quota_internals::QuotaInternalsHandler>());
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateQuotaInternalsHTMLSource());
}
