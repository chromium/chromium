// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALL_UI_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALL_UI_H_

#include "base/auto_reset.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class CrxInstallError;
class Extension;
}  // namespace extensions

class Browser;
class Profile;
class SkBitmap;

class ExtensionInstallUI {
 public:
  explicit ExtensionInstallUI(content::BrowserContext* context);

  ExtensionInstallUI(const ExtensionInstallUI&) = delete;
  ExtensionInstallUI& operator=(const ExtensionInstallUI&) = delete;

  ~ExtensionInstallUI();

  // Called when an extension was installed.
  void OnInstallSuccess(scoped_refptr<const extensions::Extension> extension,
                        const SkBitmap* icon);

  // Called when an extension failed to install.
  void OnInstallFailure(const extensions::CrxInstallError& error);

  // TODO(asargent) Normally we navigate to the new tab page when an app is
  // installed, but we're experimenting with instead showing a bubble when
  // an app is installed which points to the new tab button. This may become
  // the default behavior in the future.
  void SetUseAppInstalledBubble(bool use_bubble);

  // Sets whether to show the default UI after completing the installation.
  void SetSkipPostInstallUI(bool skip_ui);

  // Show the install bubble UI.
  static void ShowBubble(scoped_refptr<const extensions::Extension> extension,
                         Browser* browser,
                         const SkBitmap& icon);

  // For testing:
  static base::AutoReset<bool> disable_ui_for_tests(bool disable);

 private:
  raw_ptr<Profile, DanglingUntriaged> profile_;

  // Whether or not to show the default UI after completing the installation.
  bool skip_post_install_ui_;

  // Whether to show an installed bubble on app install, or use the default
  // action of opening a new tab page.
  bool use_app_installed_bubble_;
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALL_UI_H_
