// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/theme_handler.h"

#include <memory>

#include "base/bind.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/grit/theme_resources.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_ui.h"

///////////////////////////////////////////////////////////////////////////////
// ThemeHandler

ThemeHandler::ThemeHandler() = default;
ThemeHandler::~ThemeHandler() = default;

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
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(
                     ThemeServiceFactory::GetForProfile(GetProfile())));
  // Or native theme change.
  theme_observer_.Add(ui::NativeTheme::GetInstanceForNativeUi());
}

void ThemeHandler::OnJavascriptDisallowed() {
  registrar_.RemoveAll();
  theme_observer_.RemoveAll();
}

void ThemeHandler::Observe(int type,
                           const content::NotificationSource& source,
                           const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_BROWSER_THEME_CHANGED, type);
  SendThemeChanged();
}

void ThemeHandler::OnNativeThemeUpdated(ui::NativeTheme* observed_theme) {
  DCHECK_EQ(observed_theme, ui::NativeTheme::GetInstanceForNativeUi());
  SendThemeChanged();
}

void ThemeHandler::HandleObserveThemeChanges(const base::ListValue* /*args*/) {
  AllowJavascript();
}

void ThemeHandler::SendThemeChanged() {
  InitializeCSSCaches();
  bool has_custom_bg = ThemeService::GetThemeProviderForProfile(GetProfile())
                           .HasCustomImage(IDR_THEME_NTP_BACKGROUND);
  // TODO(dbeam): why does this need to be a dictionary?
  base::DictionaryValue dictionary;
  dictionary.SetBoolean("hasCustomBackground", has_custom_bg);
  FireWebUIListener("theme-changed", dictionary);
}

void ThemeHandler::InitializeCSSCaches() {
  Profile* profile = GetProfile();
  content::URLDataSource::Add(profile, std::make_unique<ThemeSource>(profile));
}

Profile* ThemeHandler::GetProfile() const {
  return Profile::FromWebUI(web_ui());
}
