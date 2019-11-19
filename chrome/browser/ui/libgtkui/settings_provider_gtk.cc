// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/libgtkui/settings_provider_gtk.h"

#include "base/strings/string_split.h"
#include "chrome/browser/ui/libgtkui/gtk_ui.h"
#include "chrome/browser/ui/libgtkui/gtk_util.h"

namespace libgtkui {

namespace {

std::string GetDecorationLayoutFromGtkWindow() {
#if GTK_CHECK_VERSION(3, 90, 0)
  NOTREACHED();
  static const char kDefaultGtkLayout[] = "menu:minimize,maximize,close";
  return kDefaultGtkLayout;
#else
  static ScopedStyleContext context;
  if (!context) {
    context = GetStyleContextFromCss("");
    gtk_style_context_add_class(context, "csd");
  }

  gchar* layout_c = nullptr;
  gtk_style_context_get_style(context, "decoration-button-layout", &layout_c,
                              nullptr);
  DCHECK(layout_c);
  std::string layout(layout_c);
  g_free(layout_c);
  return layout;
#endif
}

void ParseActionString(const std::string& value,
                       GtkUi::WindowFrameAction* action) {
  if (value == "none")
    *action = views::LinuxUI::WindowFrameAction::kNone;
  else if (value == "lower")
    *action = views::LinuxUI::WindowFrameAction::kLower;
  else if (value == "minimize")
    *action = views::LinuxUI::WindowFrameAction::kMinimize;
  else if (value == "toggle-maximize")
    *action = views::LinuxUI::WindowFrameAction::kToggleMaximize;
  else if (value == "menu")
    *action = views::LinuxUI::WindowFrameAction::kMenu;
}

}  // namespace

SettingsProviderGtk::FrameActionSettingWatcher::FrameActionSettingWatcher(
    SettingsProviderGtk* settings_provider,
    const std::string& setting_name,
    views::LinuxUI::WindowFrameActionSource action_type,
    views::LinuxUI::WindowFrameAction default_action)
    : settings_provider_(settings_provider),
      setting_name_(setting_name),
      action_type_(action_type),
      default_action_(default_action) {
  GtkSettings* settings = gtk_settings_get_default();
  std::string notify_setting = "notify::" + setting_name;
  signal_id_ = g_signal_connect(settings, notify_setting.c_str(),
                                G_CALLBACK(OnSettingChangedThunk), this);
  DCHECK(signal_id_);
  OnSettingChanged(settings, nullptr);
}

SettingsProviderGtk::FrameActionSettingWatcher::~FrameActionSettingWatcher() {
  if (signal_id_)
    g_signal_handler_disconnect(gtk_settings_get_default(), signal_id_);
}

void SettingsProviderGtk::FrameActionSettingWatcher::OnSettingChanged(
    GtkSettings* settings,
    GParamSpec* param) {
  std::string value =
      GetGtkSettingsStringProperty(settings, setting_name_.c_str());
  GtkUi::WindowFrameAction action = default_action_;
  ParseActionString(value, &action);
  settings_provider_->delegate_->SetWindowFrameAction(action_type_, action);
}

SettingsProviderGtk::SettingsProviderGtk(GtkUi* delegate)
    : delegate_(delegate), signal_id_decoration_layout_(0) {
  DCHECK(delegate_);
  GtkSettings* settings = gtk_settings_get_default();
  if (GtkVersionCheck(3, 14)) {
    signal_id_decoration_layout_ = g_signal_connect(
        settings, "notify::gtk-decoration-layout",
        G_CALLBACK(OnDecorationButtonLayoutChangedThunk), this);
    DCHECK(signal_id_decoration_layout_);
    OnDecorationButtonLayoutChanged(settings, nullptr);

    frame_action_setting_watchers_.push_back(
        std::make_unique<FrameActionSettingWatcher>(
            this, "gtk-titlebar-middle-click",
            views::LinuxUI::WindowFrameActionSource::kMiddleClick,
            views::LinuxUI::WindowFrameAction::kNone));
    frame_action_setting_watchers_.push_back(
        std::make_unique<FrameActionSettingWatcher>(
            this, "gtk-titlebar-double-click",
            views::LinuxUI::WindowFrameActionSource::kDoubleClick,
            views::LinuxUI::WindowFrameAction::kToggleMaximize));
    frame_action_setting_watchers_.push_back(
        std::make_unique<FrameActionSettingWatcher>(
            this, "gtk-titlebar-right-click",
            views::LinuxUI::WindowFrameActionSource::kRightClick,
            views::LinuxUI::WindowFrameAction::kMenu));
  } else {
    signal_id_decoration_layout_ =
        g_signal_connect_after(settings, "notify::gtk-theme-name",
                               G_CALLBACK(OnThemeChangedThunk), this);
    DCHECK(signal_id_decoration_layout_);
    OnThemeChanged(settings, nullptr);
  }
}

SettingsProviderGtk::~SettingsProviderGtk() {
  if (signal_id_decoration_layout_) {
    g_signal_handler_disconnect(gtk_settings_get_default(),
                                signal_id_decoration_layout_);
  }
}

void SettingsProviderGtk::SetWindowButtonOrderingFromGtkLayout(
    const std::string& gtk_layout) {
  std::vector<views::FrameButton> leading_buttons;
  std::vector<views::FrameButton> trailing_buttons;
  ParseButtonLayout(gtk_layout, &leading_buttons, &trailing_buttons);
  delegate_->SetWindowButtonOrdering(leading_buttons, trailing_buttons);
}

void SettingsProviderGtk::OnDecorationButtonLayoutChanged(GtkSettings* settings,
                                                          GParamSpec* param) {
  SetWindowButtonOrderingFromGtkLayout(
      GetGtkSettingsStringProperty(settings, "gtk-decoration-layout"));
}

void SettingsProviderGtk::OnThemeChanged(GtkSettings* settings,
                                         GParamSpec* param) {
  std::string layout = GetDecorationLayoutFromGtkWindow();
  SetWindowButtonOrderingFromGtkLayout(layout);
}

}  // namespace libgtkui
