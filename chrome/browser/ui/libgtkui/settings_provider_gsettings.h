// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LIBGTKUI_SETTINGS_PROVIDER_GSETTINGS_H_
#define CHROME_BROWSER_UI_LIBGTKUI_SETTINGS_PROVIDER_GSETTINGS_H_

#include <gio/gio.h>

#include <string>

#include "base/macros.h"
#include "chrome/browser/ui/libgtkui/settings_provider.h"
#include "ui/base/glib/glib_signal.h"

namespace libgtkui {
class GtkUi;

// On GNOME desktops, subscribes to the gsettings key which controlls button
// order and the middle click action. Everywhere else, SetTiltebarButtons()
// just calls back into BrowserTitlebar with the default ordering.
class SettingsProviderGSettings : public SettingsProvider {
 public:
  // Sends data to the GtkUi when available.
  explicit SettingsProviderGSettings(GtkUi* delegate);
  ~SettingsProviderGSettings() override;

 private:
  CHROMEG_CALLBACK_1(SettingsProviderGSettings,
                     void,
                     OnDecorationButtonLayoutChanged,
                     GSettings*,
                     const gchar*);

  CHROMEG_CALLBACK_1(SettingsProviderGSettings,
                     void,
                     OnMiddleClickActionChanged,
                     GSettings*,
                     const gchar*);

  void ParseAndStoreButtonValue(const std::string&);

  void ParseAndStoreMiddleClickValue(const std::string&);

  GtkUi* delegate_;

  GSettings* button_settings_ = nullptr;
  GSettings* click_settings_ = nullptr;
  gulong signal_button_id_;
  gulong signal_middle_click_id_;

  DISALLOW_COPY_AND_ASSIGN(SettingsProviderGSettings);
};

}  // namespace libgtkui

#endif  // CHROME_BROWSER_UI_LIBGTKUI_SETTINGS_PROVIDER_GSETTINGS_H_
