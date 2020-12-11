// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_DIALOG_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_DIALOG_VIEWS_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "extensions/browser/extension_registry_observer.h"
#include "ui/views/view.h"

class Profile;

namespace extensions {
class Extension;
class ExtensionRegistry;
}

namespace views {
class ScrollView;
}

// View the information about a particular chrome application or extension.
// TODO(sashab): Rename App to Extension in the class name and |app| to
// |extension| in the member variables in this class and all AppInfoPanel
// classes.
class AppInfoDialog : public views::View,
                      public extensions::ExtensionRegistryObserver {
 public:
  AppInfoDialog(Profile* profile, const extensions::Extension* app);
  ~AppInfoDialog() override;

  views::View* arc_app_info_links_for_test() { return arc_app_info_links_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(AppInfoDialogViewsTest,
                           PinButtonsAreFocusedAfterPinUnpin);
  // Closes the dialog.
  void Close();

  void StartObservingExtensionRegistry();
  void StopObservingExtensionRegistry();

  // Overridden from extensions::ExtensionRegistryObserver:
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;
  void OnShutdown(extensions::ExtensionRegistry* registry) override;

  // UI elements of the dialog.
  views::View* dialog_header_ = nullptr;
  views::ScrollView* dialog_body_ = nullptr;
  views::View* dialog_footer_ = nullptr;
  views::View* arc_app_info_links_ = nullptr;

  Profile* profile_;
  std::string app_id_;
  extensions::ExtensionRegistry* extension_registry_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(AppInfoDialog);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_DIALOG_VIEWS_H_
