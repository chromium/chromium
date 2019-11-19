// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_LIBGTKUI_SETTINGS_PROVIDER_GTK_H_
#define CHROME_BROWSER_UI_LIBGTKUI_SETTINGS_PROVIDER_GTK_H_

#include <memory>
#include <string>
#include <vector>

#include "base/macros.h"
#include "chrome/browser/ui/libgtkui/settings_provider.h"
#include "ui/base/glib/glib_signal.h"
#include "ui/views/linux_ui/linux_ui.h"

typedef struct _GParamSpec GParamSpec;
typedef struct _GtkSettings GtkSettings;

namespace libgtkui {

class GtkUi;

class SettingsProviderGtk : public SettingsProvider {
 public:
  explicit SettingsProviderGtk(GtkUi* delegate);
  ~SettingsProviderGtk() override;

 private:
  class FrameActionSettingWatcher {
   public:
    FrameActionSettingWatcher(
        SettingsProviderGtk* settings_provider,
        const std::string& setting_name,
        views::LinuxUI::WindowFrameActionSource action_type,
        views::LinuxUI::WindowFrameAction default_action);
    ~FrameActionSettingWatcher();

    CHROMEG_CALLBACK_1(FrameActionSettingWatcher,
                       void,
                       OnSettingChanged,
                       GtkSettings*,
                       GParamSpec*);

   private:
    SettingsProviderGtk* settings_provider_;
    std::string setting_name_;
    views::LinuxUI::WindowFrameActionSource action_type_;
    views::LinuxUI::WindowFrameAction default_action_;
    unsigned long signal_id_;

    DISALLOW_COPY_AND_ASSIGN(FrameActionSettingWatcher);
  };

  void SetWindowButtonOrderingFromGtkLayout(const std::string& gtk_layout);

  CHROMEG_CALLBACK_1(SettingsProviderGtk,
                     void,
                     OnDecorationButtonLayoutChanged,
                     GtkSettings*,
                     GParamSpec*);

  CHROMEG_CALLBACK_1(SettingsProviderGtk,
                     void,
                     OnThemeChanged,
                     GtkSettings*,
                     GParamSpec*);

  GtkUi* delegate_;

  unsigned long signal_id_decoration_layout_;

  std::vector<std::unique_ptr<FrameActionSettingWatcher>>
      frame_action_setting_watchers_;

  DISALLOW_COPY_AND_ASSIGN(SettingsProviderGtk);
};

}  // namespace libgtkui

#endif  // CHROME_BROWSER_UI_LIBGTKUI_SETTINGS_PROVIDER_GTK_H_
