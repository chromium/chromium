// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/autofill_and_password_manager_internals/password_manager_internals_ui.h"

#include "base/functional/bind.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/autofill_and_password_manager_internals/internals_ui_handler.h"
#include "chrome/common/url_constants.h"
#include "components/password_manager/content/browser/password_manager_log_router_factory.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

PasswordManagerInternalsUI::PasswordManagerInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  autofill::CreateAndAddInternalsHTMLSource(
      Profile::FromWebUI(web_ui),
      chrome::kChromeUIPasswordManagerInternalsHost);
  web_ui->AddMessageHandler(std::make_unique<autofill::InternalsUIHandler>(
      "setup-password-manager-internals",
      base::BindRepeating(&password_manager::PasswordManagerLogRouterFactory::
                              GetForBrowserContext)));
}

PasswordManagerInternalsUI::~PasswordManagerInternalsUI() = default;

PasswordManagerInternalsUIConfig::PasswordManagerInternalsUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUIPasswordManagerInternalsHost) {}

PasswordManagerInternalsUIConfig::~PasswordManagerInternalsUIConfig() = default;
