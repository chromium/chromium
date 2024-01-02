// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_DIALOG_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_DIALOG_VIEWS_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "extensions/browser/extension_registry_observer.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/metadata/view_factory.h"
#include "ui/views/view.h"

class Profile;

namespace extensions {
class Extension;
class ExtensionRegistry;
}  // namespace extensions

namespace views {
class ScrollView;
}

// View the information about a particular chrome application or extension.
// TODO(sashab): Rename App to Extension in the class name and |app| to
// |extension| in the member variables in this class and all AppInfoPanel
// classes.
class AppInfoDialog final : public views::View,
                            public extensions::ExtensionRegistryObserver {
 public:
  METADATA_HEADER(AppInfoDialog, views::View)

 public:
  static base::WeakPtr<AppInfoDialog>& GetLastDialogForTesting();

  AppInfoDialog(Profile* profile, const extensions::Extension* app);
  AppInfoDialog(const AppInfoDialog&) = delete;
  AppInfoDialog& operator=(const AppInfoDialog&) = delete;
  ~AppInfoDialog() override;

  views::View* arc_app_info_links_for_test() { return arc_app_info_links_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(AppInfoDialogViewsTest,
                           PinButtonsAreFocusedAfterPinUnpin);
  // Closes the dialog.
  void Close();

  void StartObservingExtensionRegistry();
  void StopObservingExtensionRegistry();

  // views::View:
  void OnThemeChanged() override;

  // Overridden from extensions::ExtensionRegistryObserver:
  void OnExtensionUnloaded(content::BrowserContext* browser_context,
                           const extensions::Extension* extension,
                           extensions::UnloadedExtensionReason reason) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;
  void OnShutdown(extensions::ExtensionRegistry* registry) override;

  // UI elements of the dialog.
  raw_ptr<views::View> dialog_header_ = nullptr;
  raw_ptr<views::ScrollView> dialog_body_ = nullptr;
  raw_ptr<views::View> dialog_footer_ = nullptr;
  raw_ptr<views::View> arc_app_info_links_ = nullptr;

  raw_ptr<Profile, AcrossTasksDanglingUntriaged> profile_;
  std::string app_id_;
  raw_ptr<extensions::ExtensionRegistry> extension_registry_ = nullptr;
  base::WeakPtrFactory<AppInfoDialog> weak_ptr_factory_{this};
};

BEGIN_VIEW_BUILDER(/* no export */, AppInfoDialog, views::View)
END_VIEW_BUILDER

DEFINE_VIEW_BUILDER(/* no export */, AppInfoDialog)

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_APP_INFO_DIALOG_APP_INFO_DIALOG_VIEWS_H_
