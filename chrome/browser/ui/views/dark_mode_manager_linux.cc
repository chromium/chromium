// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/dark_mode_manager_linux.h"

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/ui_features.h"
#include "components/dbus/thread_linux/dbus_thread_linux.h"
#include "dbus/bus.h"
#include "dbus/message.h"
#include "dbus/object_proxy.h"
#include "ui/linux/linux_ui.h"
#include "ui/linux/linux_ui_factory.h"
#include "ui/native_theme/native_theme.h"

namespace ui {

namespace {

scoped_refptr<dbus::Bus> CreateBus() {
  dbus::Bus::Options options;
  options.bus_type = dbus::Bus::SESSION;
  options.connection_type = dbus::Bus::PRIVATE;
  options.dbus_task_runner = dbus_thread_linux::GetTaskRunner();
  return base::MakeRefCounted<dbus::Bus>(options);
}

}  // namespace

DarkModeManagerLinux::DarkModeManagerLinux()
    : DarkModeManagerLinux(
          CreateBus(),
          ui::GetDefaultLinuxUiTheme(),
          &ui::GetLinuxUiThemes(),
          std::vector<raw_ptr<ui::NativeTheme, VectorExperimental>>{
              ui::NativeTheme::GetInstanceForNativeUi(),
              ui::NativeTheme::GetInstanceForWeb()}) {}

DarkModeManagerLinux::DarkModeManagerLinux(
    scoped_refptr<dbus::Bus> bus,
    LinuxUiTheme* default_linux_ui_theme,
    const std::vector<raw_ptr<LinuxUiTheme, VectorExperimental>>*
        linux_ui_themes,
    std::vector<raw_ptr<NativeTheme, VectorExperimental>> native_themes)
    : linux_ui_themes_(linux_ui_themes),
      native_themes_(native_themes),
      bus_(bus),
      settings_proxy_(bus_->GetObjectProxy(
          kFreedesktopSettingsService,
          dbus::ObjectPath(kFreedesktopSettingsObjectPath))) {
  // Subscribe to changes in the color scheme preference.
  settings_proxy_->ConnectToSignal(
      kFreedesktopSettingsInterface, kSettingChangedSignal,
      base::BindRepeating(&DarkModeManagerLinux::OnPortalSettingChanged,
                          weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&DarkModeManagerLinux::OnSignalConnected,
                     weak_ptr_factory_.GetWeakPtr()));

  // Read initial color scheme preference.
  {
    dbus::MethodCall method_call(kFreedesktopSettingsInterface, kReadMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(kSettingsNamespace);
    writer.AppendString(kColorSchemeKey);
    settings_proxy_->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DarkModeManagerLinux::OnReadColorSchemeResponse,
                       weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&DarkModeManagerLinux::OnReadError,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // Read initial accent color preference.
  if (base::FeatureList::IsEnabled(features::kUsePortalAccentColor)) {
    dbus::MethodCall method_call(kFreedesktopSettingsInterface, kReadMethod);
    dbus::MessageWriter writer(&method_call);
    writer.AppendString(kSettingsNamespace);
    writer.AppendString(kAccentColorKey);
    settings_proxy_->CallMethodWithErrorCallback(
        &method_call, dbus::ObjectProxy::TIMEOUT_USE_DEFAULT,
        base::BindOnce(&DarkModeManagerLinux::OnReadAccentColorResponse,
                       weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&DarkModeManagerLinux::OnReadError,
                       weak_ptr_factory_.GetWeakPtr()));
  }

  // Read the toolkit preference while asynchronously fetching the
  // portal preference.
  if (default_linux_ui_theme) {
    auto* native_theme = default_linux_ui_theme->GetNativeTheme();
    native_theme_observer_.Observe(native_theme);
    SetColorScheme(native_theme->ShouldUseDarkColors(), true);
  }
}

DarkModeManagerLinux::~DarkModeManagerLinux() {
  settings_proxy_ = nullptr;
  dbus::Bus* const bus_ptr = bus_.get();
  // `task_runner` may be nullptr in testing.
  if (auto* task_runner = bus_ptr->GetDBusTaskRunner()) {
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&dbus::Bus::ShutdownAndBlock, std::move(bus_)));
  }
}

void DarkModeManagerLinux::OnNativeThemeUpdated(
    ui::NativeTheme* observed_theme) {
  SetColorScheme(observed_theme->ShouldUseDarkColors(), true);
}

void DarkModeManagerLinux::OnSignalConnected(const std::string& interface_name,
                                             const std::string& signal_name,
                                             bool connected) {
  // Nothing to do.  Continue using the toolkit setting if !connected.
}

void DarkModeManagerLinux::OnPortalSettingChanged(dbus::Signal* signal) {
  dbus::MessageReader reader(signal);

  std::string namespace_changed;
  std::string key_changed;
  dbus::MessageReader variant_reader(nullptr);
  if (!reader.PopString(&namespace_changed) ||
      !reader.PopString(&key_changed) || !reader.PopVariant(&variant_reader)) {
    LOG(ERROR) << "Received malformed Setting Changed signal from "
                  "org.freedesktop.portal.Settings";
    return;
  }

  if (namespace_changed != kSettingsNamespace) {
    return;
  }

  if (key_changed == kColorSchemeKey) {
    uint32_t new_color_scheme;
    if (!variant_reader.PopUint32(&new_color_scheme)) {
      LOG(ERROR)
          << "Failed to read color-scheme value from SettingChanged signal";
      return;
    }

    SetColorScheme(new_color_scheme == kFreedesktopColorSchemeDark, false);
  } else if (key_changed == kAccentColorKey &&
             base::FeatureList::IsEnabled(features::kUsePortalAccentColor)) {
    SetAccentColor(&variant_reader);
  }
}

void DarkModeManagerLinux::OnReadColorSchemeResponse(dbus::Response* response) {
  if (!response) {
    // Continue using the toolkit setting.
    return;
  }

  dbus::MessageReader reader(response);
  dbus::MessageReader variant_reader(nullptr);
  if (!reader.PopVariant(&variant_reader)) {
    LOG(ERROR) << "Failed to read variant from Read method response";
    return;
  }

  uint32_t new_color_scheme;
  if (!variant_reader.PopVariantOfUint32(&new_color_scheme)) {
    LOG(ERROR) << "Failed to read color-scheme value from Read "
                  "method response";
    return;
  }

  // Once we read the org.freedesktop.appearance color-scheme setting
  // successfully, it should always take precedence over the toolkit color
  // scheme.
  ignore_toolkit_theme_changes_ = true;

  SetColorScheme(new_color_scheme == kFreedesktopColorSchemeDark, false);
}

void DarkModeManagerLinux::OnReadAccentColorResponse(dbus::Response* response) {
  if (!response) {
    // Continue using the toolkit setting.
    return;
  }

  dbus::MessageReader reader(response);
  dbus::MessageReader outer_variant_reader(nullptr);
  if (!reader.PopVariant(&outer_variant_reader)) {
    LOG(ERROR) << "Failed to read variant from Read method response";
    return;
  }

  dbus::MessageReader inner_variant_reader(nullptr);
  if (!outer_variant_reader.PopVariant(&inner_variant_reader)) {
    LOG(ERROR) << "Failed to read variant from Read method response";
    return;
  }

  SetAccentColor(&inner_variant_reader);
}

void DarkModeManagerLinux::OnReadError(dbus::ErrorResponse* error) {
  // Ignore errors.  It's expected that the settings portal may not exist.
}

void DarkModeManagerLinux::SetColorScheme(bool prefer_dark_theme,
                                          bool from_toolkit_theme) {
  if (from_toolkit_theme && ignore_toolkit_theme_changes_) {
    return;
  }
  if (!from_toolkit_theme) {
    for (ui::LinuxUiTheme* linux_ui_theme : *linux_ui_themes_) {
      linux_ui_theme->SetDarkTheme(prefer_dark_theme);
    }
  }
  if (prefer_dark_theme_ == prefer_dark_theme) {
    return;
  }
  prefer_dark_theme_ = prefer_dark_theme;

  for (NativeTheme* theme : native_themes_) {
    theme->set_use_dark_colors(prefer_dark_theme_);
    theme->set_preferred_color_scheme(
        prefer_dark_theme_ ? NativeTheme::PreferredColorScheme::kDark
                           : NativeTheme::PreferredColorScheme::kLight);
    theme->NotifyOnNativeThemeUpdated();
  }
}

void DarkModeManagerLinux::SetAccentColor(dbus::MessageReader* reader) {
  dbus::MessageReader struct_reader(nullptr);
  if (!reader->PopStruct(&struct_reader)) {
    LOG(ERROR) << "Failed to read struct";
    return;
  }

  bool valid = true;
  int color[3] = {0, 0, 0};
  for (int& channel : color) {
    double d;
    if (!struct_reader.PopDouble(&d)) {
      LOG(ERROR) << "Failed to read double";
      return;
    }
    if (d >= 0.0 && d <= 1.0) {
      channel = d * 255;
    } else {
      // The org.freedesktop.impl.portal.Settings spec requires out-of-range RGB
      // values to be treated as unset.
      valid = false;
    }
  }

  std::optional<SkColor> accent_color;
  if (valid) {
    accent_color = SkColorSetRGB(color[0], color[1], color[2]);
  }

  for (ui::LinuxUiTheme* linux_ui_theme : *linux_ui_themes_) {
    linux_ui_theme->SetAccentColor(accent_color);
  }
}

}  // namespace ui
