// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/libgtkui/settings_provider_gsettings.h"

#include <gio/gio.h>

#include <memory>

#include "base/environment.h"
#include "base/nix/xdg_util.h"
#include "chrome/browser/ui/libgtkui/gtk_ui.h"
#include "chrome/browser/ui/libgtkui/gtk_util.h"

namespace {

const char kGnomePreferencesSchema[] = "org.gnome.desktop.wm.preferences";
const char kCinnamonPreferencesSchema[] = "org.cinnamon.muffin";

const char kButtonLayoutKey[] = "button-layout";
const char kButtonLayoutChangedSignal[] = "changed::button-layout";

const char kMiddleClickActionKey[] = "action-middle-click-titlebar";
const char kMiddleClickActionChangedSignal[] =
    "changed::action-middle-click-titlebar";

const char kDefaultButtonString[] = ":minimize,maximize,close";

}  // namespace

namespace libgtkui {

// Public interface:

SettingsProviderGSettings::SettingsProviderGSettings(GtkUi* delegate)
    : delegate_(delegate) {
  DCHECK(delegate_);

  // Of all the supported distros, this code path should only be used by Ubuntu
  // 14.04 (all the others have a sufficient gtk version to use the GTK API).
  // The default in 14.04 is Unity, but Cinnamon has enough usage to justify
  // also checking its value.
  std::unique_ptr<base::Environment> env(base::Environment::Create());
  const gchar* settings_schema = base::nix::GetDesktopEnvironment(env.get()) ==
                                         base::nix::DESKTOP_ENVIRONMENT_CINNAMON
                                     ? kCinnamonPreferencesSchema
                                     : kGnomePreferencesSchema;

  GSettingsSchema* button_schema = g_settings_schema_source_lookup(
      g_settings_schema_source_get_default(), settings_schema, FALSE);
  if (!button_schema ||
      !g_settings_schema_has_key(button_schema, kButtonLayoutKey) ||
      !(button_settings_ = g_settings_new(settings_schema))) {
    ParseAndStoreButtonValue(kDefaultButtonString);
  } else {
    // Get the inital value of the keys we're interested in.
    OnDecorationButtonLayoutChanged(button_settings_, kButtonLayoutKey);
    signal_button_id_ = g_signal_connect(
        button_settings_, kButtonLayoutChangedSignal,
        G_CALLBACK(OnDecorationButtonLayoutChangedThunk), this);
  }

  GSettingsSchema* click_schema = g_settings_schema_source_lookup(
      g_settings_schema_source_get_default(), kGnomePreferencesSchema, FALSE);
  // If this fails, the default action has already been set in gtk_ui.cc.
  if (click_schema &&
      g_settings_schema_has_key(click_schema, kMiddleClickActionKey) &&
      (click_settings_ = g_settings_new(kGnomePreferencesSchema))) {
    OnMiddleClickActionChanged(click_settings_, kMiddleClickActionKey);
    signal_middle_click_id_ =
        g_signal_connect(click_settings_, kMiddleClickActionChangedSignal,
                         G_CALLBACK(OnMiddleClickActionChangedThunk), this);
  }
}

SettingsProviderGSettings::~SettingsProviderGSettings() {
  if (button_settings_) {
    if (signal_button_id_)
      g_signal_handler_disconnect(button_settings_, signal_button_id_);
    g_free(button_settings_);
  }
  if (click_settings_) {
    if (signal_middle_click_id_)
      g_signal_handler_disconnect(click_settings_, signal_middle_click_id_);
    g_free(click_settings_);
  }
}

// Private:

void SettingsProviderGSettings::OnDecorationButtonLayoutChanged(
    GSettings* settings,
    const gchar* key) {
  gchar* button_layout = g_settings_get_string(settings, kButtonLayoutKey);
  if (!button_layout)
    return;
  ParseAndStoreButtonValue(button_layout);
  g_free(button_layout);
}

void SettingsProviderGSettings::ParseAndStoreButtonValue(
    const std::string& button_string) {
  std::vector<views::FrameButton> leading_buttons;
  std::vector<views::FrameButton> trailing_buttons;
  ParseButtonLayout(button_string, &leading_buttons, &trailing_buttons);
  delegate_->SetWindowButtonOrdering(leading_buttons, trailing_buttons);
}

void SettingsProviderGSettings::OnMiddleClickActionChanged(GSettings* settings,
                                                           const gchar* key) {
  gchar* click_action = g_settings_get_string(settings, kMiddleClickActionKey);
  if (!click_action)
    return;
  ParseAndStoreMiddleClickValue(click_action);
  g_free(click_action);
}

void SettingsProviderGSettings::ParseAndStoreMiddleClickValue(
    const std::string& click_action) {
  GtkUi::WindowFrameAction action;

  if (click_action == "none") {
    action = views::LinuxUI::WindowFrameAction::kNone;
  } else if (click_action == "lower") {
    action = views::LinuxUI::WindowFrameAction::kLower;
  } else if (click_action == "minimize") {
    action = views::LinuxUI::WindowFrameAction::kMinimize;
  } else if (click_action == "toggle-maximize") {
    action = views::LinuxUI::WindowFrameAction::kToggleMaximize;
  } else {
    // While we want to have the default state be lower if there isn't a
    // value, we want to default to no action if the user has explicitly
    // chose an action that we don't implement.
    action = views::LinuxUI::WindowFrameAction::kNone;
  }

  delegate_->SetWindowFrameAction(
      views::LinuxUI::WindowFrameActionSource::kMiddleClick, action);
}

}  // namespace libgtkui
