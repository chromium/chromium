// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/autofill_and_password_manager_internals/password_manager_internals_ui.h"

#include <optional>

#include "base/functional/bind.h"
#include "build/branding_buildflags.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/autofill_and_password_manager_internals/internals_ui_handler.h"
#include "chrome/common/url_constants.h"
#include "components/password_manager/content/browser/password_manager_log_router_factory.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"

#if !BUILDFLAG(IS_ANDROID)
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
#include "chrome/grit/chrome_unscaled_resources.h"
#else
#include "chrome/grit/password_manager_resources.h"
#endif
#endif

PasswordManagerInternalsUI::PasswordManagerInternalsUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  content::WebUIDataSource& source = autofill::CreateAndAddInternalsHTMLSource(
      Profile::FromWebUI(web_ui),
      chrome::kChromeUIPasswordManagerInternalsHost);
  static constexpr std::optional<int> kLogoResourceId =
#if BUILDFLAG(IS_ANDROID)
      std::nullopt;
#elif BUILDFLAG(GOOGLE_CHROME_BRANDING)
      IDR_CHROME_PASSWORD_MANAGER_LOGO;
#else
      IDR_PASSWORD_MANAGER_IMAGES_PASSWORD_MANAGER_LOGO_SVG;
#endif
  if constexpr (kLogoResourceId.has_value()) {
    source.AddResourcePath("password_manager_logo.svg", *kLogoResourceId);
  }
  web_ui->AddMessageHandler(std::make_unique<autofill::InternalsUIHandler>(
      "setup-password-manager-internals",
      /*call_on_load_argument=*/base::Value(false),
      base::BindRepeating(&password_manager::PasswordManagerLogRouterFactory::
                              GetForBrowserContext)));
}

PasswordManagerInternalsUI::~PasswordManagerInternalsUI() = default;

PasswordManagerInternalsUIConfig::PasswordManagerInternalsUIConfig()
    : DefaultWebUIConfig(content::kChromeUIScheme,
                         chrome::kChromeUIPasswordManagerInternalsHost) {}

PasswordManagerInternalsUIConfig::~PasswordManagerInternalsUIConfig() = default;
