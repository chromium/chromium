// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/appearance_handler.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "content/public/browser/web_ui.h"

namespace settings {

AppearanceHandler::AppearanceHandler(content::WebUI* webui)
    : profile_(Profile::FromWebUI(webui)) {}

AppearanceHandler::~AppearanceHandler() {}

void AppearanceHandler::OnJavascriptAllowed() {}
void AppearanceHandler::OnJavascriptDisallowed() {}

void AppearanceHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "useDefaultTheme",
      base::BindRepeating(&AppearanceHandler::HandleUseDefaultTheme,
                          base::Unretained(this)));
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  web_ui()->RegisterMessageCallback(
      "useSystemTheme",
      base::BindRepeating(&AppearanceHandler::HandleUseSystemTheme,
                          base::Unretained(this)));
#endif
}

void AppearanceHandler::HandleUseDefaultTheme(const base::ListValue* args) {
  ThemeServiceFactory::GetForProfile(profile_)->UseDefaultTheme();
}

#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
void AppearanceHandler::HandleUseSystemTheme(const base::ListValue* args) {
  if (profile_->IsSupervised())
    NOTREACHED();
  else
    ThemeServiceFactory::GetForProfile(profile_)->UseSystemTheme();
}
#endif

}  // namespace settings
