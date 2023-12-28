// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_SUMMARY_PANEL_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_SUMMARY_PANEL_H_

#include <stdint.h>

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/apps/app_info_dialog/app_info_panel.h"
#include "extensions/common/constants.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/metadata/view_factory.h"

class LaunchOptionsComboboxModel;
class Profile;

namespace extensions {
class Extension;
}

namespace views {
class Combobox;
class Label;
class View;
}  // namespace views

// The summary panel of the app info dialog, which provides basic information
// and controls related to the app.
class AppInfoSummaryPanel : public AppInfoPanel {
  METADATA_HEADER(AppInfoSummaryPanel, AppInfoPanel)

 public:
  AppInfoSummaryPanel(Profile* profile, const extensions::Extension* app);
  AppInfoSummaryPanel(const AppInfoSummaryPanel&) = delete;
  AppInfoSummaryPanel& operator=(const AppInfoSummaryPanel&) = delete;
  ~AppInfoSummaryPanel() override;

 private:
  // Internal initialisation methods.
  void AddDescriptionAndLinksControl(views::View* vertical_stack);
  void AddDetailsControl(views::View* vertical_stack);
  void AddLaunchOptionControl(views::View* vertical_stack);
  void AddSubviews();

  // Called when the combobox selection changes.
  void LaunchOptionsChanged();

  // Called asynchronously to calculate and update the size of the app displayed
  // in the dialog.
  void StartCalculatingAppSize();
  void OnAppSizeCalculated(const std::u16string& size);

  // Returns the launch type of the app (e.g. pinned tab, fullscreen, etc).
  extensions::LaunchType GetLaunchType() const;

  // Sets the launch type of the app to the given type. Must only be called if
  // CanSetLaunchType() returns true.
  void SetLaunchType(extensions::LaunchType) const;
  bool CanSetLaunchType() const;

  // Opens the app's homepage URL as specified in the manifest. Must only be
  // called if CanShowAppHomePage() returns true.
  void ShowAppHomePage();
  bool CanShowAppHomePage() const;

  // Displays the licenses for the app. Must only be called if
  // CanDisplayLicenses() returns true.
  void DisplayLicenses();
  bool CanDisplayLicenses() const;
  std::vector<GURL> GetLicenseUrls() const;

  // UI elements on the dialog.
  raw_ptr<views::Label, DanglingUntriaged> size_value_ = nullptr;

  std::unique_ptr<LaunchOptionsComboboxModel> launch_options_combobox_model_;
  raw_ptr<views::Combobox> launch_options_combobox_ = nullptr;

  base::WeakPtrFactory<AppInfoSummaryPanel> weak_ptr_factory_{this};
};

BEGIN_VIEW_BUILDER(/* no export */, AppInfoSummaryPanel, AppInfoPanel)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, AppInfoSummaryPanel)

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_SUMMARY_PANEL_H_
