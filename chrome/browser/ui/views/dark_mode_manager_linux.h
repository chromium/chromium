// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_DARK_MODE_MANAGER_LINUX_H_
#define CHROME_BROWSER_UI_VIEWS_DARK_MODE_MANAGER_LINUX_H_

#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/native_theme_observer.h"

namespace dbus {
class Bus;
class ErrorResponse;
class MessageReader;
class ObjectProxy;
class Response;
class Signal;
}  // namespace dbus

namespace ui {

class LinuxUiTheme;
class DarkModeManagerLinuxTest;

// Observes the system color scheme preference using
// org.freedesktop.portal.Settings. Falls back to the toolkit preference if
// org.freedesktop.portal.Settings is unavailable.  Propagates the dark mode
// preference to the web theme.
class DarkModeManagerLinux : public NativeThemeObserver {
 public:
  DarkModeManagerLinux();
  DarkModeManagerLinux(
      scoped_refptr<dbus::Bus> bus,
      LinuxUiTheme* default_linux_ui_theme,
      const std::vector<raw_ptr<LinuxUiTheme, VectorExperimental>>*
          linux_ui_themes,
      std::vector<raw_ptr<NativeTheme, VectorExperimental>> native_themes);
  DarkModeManagerLinux(const DarkModeManagerLinux&) = delete;
  DarkModeManagerLinux& operator=(const DarkModeManagerLinux&) = delete;
  ~DarkModeManagerLinux() override;

  bool prefer_dark_theme() const { return prefer_dark_theme_; }

 private:
  friend class DarkModeManagerLinuxTest;
  FRIEND_TEST_ALL_PREFIXES(DarkModeManagerLinuxTest, UseNativeThemeSetting);
  FRIEND_TEST_ALL_PREFIXES(DarkModeManagerLinuxTest, UsePortalSetting);
  FRIEND_TEST_ALL_PREFIXES(DarkModeManagerLinuxTest, UsePortalAccentColor);

  constexpr static char kFreedesktopSettingsService[] =
      "org.freedesktop.portal.Desktop";
  constexpr static char kFreedesktopSettingsObjectPath[] =
      "/org/freedesktop/portal/desktop";
  constexpr static char kFreedesktopSettingsInterface[] =
      "org.freedesktop.portal.Settings";
  constexpr static char kSettingChangedSignal[] = "SettingChanged";
  constexpr static char kReadMethod[] = "Read";
  constexpr static char kSettingsNamespace[] = "org.freedesktop.appearance";
  constexpr static char kColorSchemeKey[] = "color-scheme";
  constexpr static char kAccentColorKey[] = "accent-color";
  constexpr static int kFreedesktopColorSchemeDark = 1;

  // ui::NativeThemeObserver:
  void OnNativeThemeUpdated(ui::NativeTheme* observed_theme) override;

  // D-Bus async handlers
  void OnSignalConnected(const std::string& interface_name,
                         const std::string& signal_name,
                         bool connected);
  void OnPortalSettingChanged(dbus::Signal* signal);
  void OnReadColorSchemeResponse(dbus::Response* response);
  void OnReadAccentColorResponse(dbus::Response* response);
  void OnReadError(dbus::ErrorResponse* error);

  // Sets `prefer_dark_theme_` and propagates to the web theme.
  void SetColorScheme(bool prefer_dark_theme, bool from_toolkit_theme);

  void SetAccentColor(dbus::MessageReader* reader);

  raw_ptr<const std::vector<raw_ptr<LinuxUiTheme, VectorExperimental>>>
      linux_ui_themes_;
  std::vector<raw_ptr<NativeTheme, VectorExperimental>> native_themes_;

  scoped_refptr<dbus::Bus> bus_;
  raw_ptr<dbus::ObjectProxy> settings_proxy_;

  bool prefer_dark_theme_ = false;
  bool ignore_toolkit_theme_changes_ = false;

  base::ScopedObservation<NativeTheme, NativeThemeObserver>
      native_theme_observer_{this};

  base::WeakPtrFactory<DarkModeManagerLinux> weak_ptr_factory_{this};
};

}  // namespace ui

#endif  // CHROME_BROWSER_UI_VIEWS_DARK_MODE_MANAGER_LINUX_H_
