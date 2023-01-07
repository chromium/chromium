// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/appearance_handler.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "content/public/browser/web_ui.h"

namespace settings {

AppearanceHandler::AppearanceHandler(content::WebUI* webui)
    : profile_(Profile::FromWebUI(webui)) {}

AppearanceHandler::~AppearanceHandler() = default;

void AppearanceHandler::OnJavascriptAllowed() {}
void AppearanceHandler::OnJavascriptDisallowed() {}

void AppearanceHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "useDefaultTheme",
      base::BindRepeating(&AppearanceHandler::HandleUseTheme,
                          base::Unretained(this), ui::SystemTheme::kDefault));
#if BUILDFLAG(IS_LINUX)
  web_ui()->RegisterMessageCallback(
      "useGtkTheme",
      base::BindRepeating(&AppearanceHandler::HandleUseTheme,
                          base::Unretained(this), ui::SystemTheme::kGtk));
  web_ui()->RegisterMessageCallback(
      "useQtTheme",
      base::BindRepeating(&AppearanceHandler::HandleUseTheme,
                          base::Unretained(this), ui::SystemTheme::kQt));
#endif
}

void AppearanceHandler::HandleUseTheme(ui::SystemTheme system_theme,
                                       const base::Value::List& args) {
  DCHECK(system_theme != ui::SystemTheme::kDefault || !profile_->IsChild());
  ThemeServiceFactory::GetForProfile(profile_)->UseTheme(system_theme);
}

}  // namespace settings
