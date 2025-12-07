// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/cryptohome/cryptohome_ui.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/cryptohome/cryptohome_web_ui_handler.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

namespace ash {

namespace {

// Returns HTML data source for chrome://cryptohome.
void CreateAndAddCryptohomeUIHTMLSource(Profile* profile) {
  content::WebUIDataSource* source = content::WebUIDataSource::CreateAndAdd(
      profile, chrome::kChromeUICryptohomeHost);
  source->AddResourcePath("cryptohome.js", IDR_CRYPTOHOME_JS);
  source->SetDefaultResource(IDR_CRYPTOHOME_HTML);
}

}  // namespace

CryptohomeUI::CryptohomeUI(content::WebUI* web_ui) : WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<CryptohomeWebUIHandler>());

  CreateAndAddCryptohomeUIHTMLSource(Profile::FromWebUI(web_ui));
}

}  // namespace ash
