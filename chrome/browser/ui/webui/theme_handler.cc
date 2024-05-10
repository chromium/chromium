// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/theme_handler.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/browser/ui/webui/webui_util_desktop.h"
#include "chrome/grit/theme_resources.h"
#include "content/public/browser/web_ui.h"

///////////////////////////////////////////////////////////////////////////////
// ThemeHandler

ThemeHandler::ThemeHandler() = default;

ThemeHandler::~ThemeHandler() {
  ThemeServiceFactory::GetForProfile(GetProfile())->RemoveObserver(this);
}

void ThemeHandler::RegisterMessages() {
  // These are not actual message registrations, but can't be done in the
  // constructor since they need the web_ui value to be set, which is done
  // post-construction, but before registering messages.
  InitializeCSSCaches();
  web_ui()->RegisterMessageCallback(
      "observeThemeChanges",
      base::BindRepeating(&ThemeHandler::HandleObserveThemeChanges,
                          base::Unretained(this)));
}

void ThemeHandler::OnJavascriptAllowed() {
  // Listen for theme installation.
  ThemeServiceFactory::GetForProfile(GetProfile())->AddObserver(this);

  // Or native theme change.
  if (web_ui()) {
    theme_observation_.Observe(
        webui::GetNativeThemeDeprecated(web_ui()->GetWebContents()));
  }
}

void ThemeHandler::OnJavascriptDisallowed() {
  ThemeServiceFactory::GetForProfile(GetProfile())->RemoveObserver(this);
  theme_observation_.Reset();
}

void ThemeHandler::OnThemeChanged() {
  SendThemeChanged();
}

void ThemeHandler::OnNativeThemeUpdated(ui::NativeTheme* observed_theme) {
  // There are two types of theme update. a) The observed theme change. e.g.
  // switch between light/dark mode. b) A different theme is enabled. e.g.
  // switch between GTK and classic theme on Linux. Reset observer in case b).
  ui::NativeTheme* current_theme =
      webui::GetNativeThemeDeprecated(web_ui()->GetWebContents());
  if (observed_theme != current_theme) {
    theme_observation_.Reset();
    theme_observation_.Observe(current_theme);
  }
  SendThemeChanged();
}

void ThemeHandler::HandleObserveThemeChanges(
    const base::Value::List& /*args*/) {
  AllowJavascript();
}

void ThemeHandler::SendThemeChanged() {
  InitializeCSSCaches();
  bool has_custom_bg = ThemeService::GetThemeProviderForProfile(GetProfile())
                           .HasCustomImage(IDR_THEME_NTP_BACKGROUND);
  // TODO(dbeam): why does this need to be a dictionary?
  base::Value::Dict dictionary;
  dictionary.Set("hasCustomBackground", has_custom_bg);
  FireWebUIListener("theme-changed", dictionary);
}

void ThemeHandler::InitializeCSSCaches() {
  Profile* profile = GetProfile();
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));
}

Profile* ThemeHandler::GetProfile() {
  return Profile::FromWebUI(web_ui());
}
