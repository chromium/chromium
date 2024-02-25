// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_HEADER_PANEL_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_HEADER_PANEL_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/extensions/chrome_app_icon_delegate.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_panel.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/metadata/view_factory.h"

class Profile;
namespace extensions {
class Extension;
}

namespace views {
class ImageView;
}

namespace test {
class AppInfoDialogTestApi;
}

// A small summary panel with the app's name, icon, version, and various links
// that is displayed at the top of the app info dialog.
class AppInfoHeaderPanel : public AppInfoPanel,
                           public extensions::ChromeAppIconDelegate {
  METADATA_HEADER(AppInfoHeaderPanel, AppInfoPanel)

 public:
  AppInfoHeaderPanel(Profile* profile, const extensions::Extension* app);
  AppInfoHeaderPanel(const AppInfoHeaderPanel&) = delete;
  AppInfoHeaderPanel& operator=(const AppInfoHeaderPanel&) = delete;
  ~AppInfoHeaderPanel() override;

 private:
  friend class test::AppInfoDialogTestApi;

  // extensions::ChromeAppIconDelegate:
  void OnIconUpdated(extensions::ChromeAppIcon* icon) override;

  void CreateControls();

  // Opens the app in the web store. Must only be called if
  // CanShowAppInWebStore() returns true.
  void ShowAppInWebStore();
  bool CanShowAppInWebStore() const;

  // UI elements on the dialog. Elements are nullptr if they are not displayed.
  raw_ptr<views::ImageView> app_icon_view_ = nullptr;

  std::unique_ptr<extensions::ChromeAppIcon> app_icon_;
};

BEGIN_VIEW_BUILDER(/* no export */, AppInfoHeaderPanel, AppInfoPanel)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, AppInfoHeaderPanel)

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_HEADER_PANEL_H_
