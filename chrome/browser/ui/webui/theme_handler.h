// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_THEME_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_THEME_HANDLER_H_

#include "base/macros.h"
#include "base/scoped_observation.h"
#include "chrome/browser/themes/theme_service_observer.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"

class Profile;

namespace ui {
class NativeTheme;
}

// A class to keep the ThemeSource up to date when theme changes.
class ThemeHandler : public content::WebUIMessageHandler,
                     public ThemeServiceObserver,
                     public ui::NativeThemeObserver {
 public:
  ThemeHandler();
  ~ThemeHandler() override;

 private:
  // content::WebUIMessageHandler:
  void RegisterMessages() override;
  void OnJavascriptAllowed() override;
  void OnJavascriptDisallowed() override;

  // Re/set the CSS caches.
  void InitializeCSSCaches();

  // ThemeServiceObserver implementation.
  void OnThemeChanged() override;

  // ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;

  // Handler for "observeThemeChanges" chrome.send() message. No arguments.
  void HandleObserveThemeChanges(const base::ListValue* args);

  // Notify the page (if allowed) that the theme has changed.
  void SendThemeChanged();

  Profile* GetProfile() const;

  base::ScopedObservation<ui::NativeTheme, ui::NativeThemeObserver>
      theme_observation_{this};

  DISALLOW_COPY_AND_ASSIGN(ThemeHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_THEME_HANDLER_H_
