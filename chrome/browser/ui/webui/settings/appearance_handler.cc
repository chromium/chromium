// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/settings/appearance_handler.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/values.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/pref_names.h"
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

  web_ui()->RegisterMessageCallback(
      "openCustomizeChrome",
      base::BindRepeating(&AppearanceHandler::OpenCustomizeChrome,
                          base::Unretained(this)));
  if (features::IsToolbarPinningEnabled()) {
    web_ui()->RegisterMessageCallback(
        "openCustomizeChromeToolbarSection",
        base::BindRepeating(
            &AppearanceHandler::OpenCustomizeChromeToolbarSection,
            base::Unretained(this)));
    web_ui()->RegisterMessageCallback(
        "resetPinnedToolbarActions",
        base::BindRepeating(&AppearanceHandler::ResetPinnedToolbarActions,
                            base::Unretained(this)));
  }
  web_ui()->RegisterMessageCallback(
      "pinnedToolbarActionsAreDefault",
      base::BindRepeating(&AppearanceHandler::PinnedToolbarActionsAreDefault,
                          base::Unretained(this)));
}

void AppearanceHandler::HandleUseTheme(ui::SystemTheme system_theme,
                                       const base::Value::List& args) {
  DCHECK(system_theme != ui::SystemTheme::kDefault || !profile_->IsChild());
  ThemeServiceFactory::GetForProfile(profile_)->UseTheme(system_theme);
}

void AppearanceHandler::OpenCustomizeChrome(const base::Value::List& args) {
  auto* browser = chrome::FindLastActive();
  if (!browser) {
    return;
  }
  chrome::ExecuteCommand(browser, IDC_SHOW_CUSTOMIZE_CHROME_SIDE_PANEL);
}

void AppearanceHandler::OpenCustomizeChromeToolbarSection(
    const base::Value::List& args) {
  auto* browser = chrome::FindLastActive();
  CHECK(browser);
  chrome::ExecuteCommand(browser, IDC_SHOW_CUSTOMIZE_CHROME_TOOLBAR);
}

void AppearanceHandler::ResetPinnedToolbarActions(
    const base::Value::List& args) {
  PinnedToolbarActionsModel::Get(profile_)->ResetToDefault();
}

void AppearanceHandler::PinnedToolbarActionsAreDefault(
    const base::Value::List& args) {
  CHECK_EQ(1U, args.size());
  const base::Value& callback_id = args[0];
  const bool are_default =
      !features::IsToolbarPinningEnabled() ||
      PinnedToolbarActionsModel::Get(profile_)->IsDefault();

  AllowJavascript();
  ResolveJavascriptCallback(callback_id, base::Value(are_default));
}

}  // namespace settings
