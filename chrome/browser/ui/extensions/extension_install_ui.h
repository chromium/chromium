// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALL_UI_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALL_UI_H_

#include <memory>

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

class Profile;
class SkBitmap;

// Manages the extension install UI bubble.
class ExtensionInstallUI {
 public:
  // Creates an ExtensionInstallUI subclass for the current OS platform.
  static std::unique_ptr<ExtensionInstallUI> Create(
      content::BrowserContext* context);

  ExtensionInstallUI(const ExtensionInstallUI&) = delete;
  ExtensionInstallUI& operator=(const ExtensionInstallUI&) = delete;

  virtual ~ExtensionInstallUI();

  // Called when an extension was installed.
  virtual void OnInstallSuccess(
      scoped_refptr<const extensions::Extension> extension,
      const SkBitmap* icon) = 0;

  // Called when an extension failed to install.
  virtual void OnInstallFailure(const extensions::CrxInstallError& error) = 0;

  // TODO(asargent) Normally we navigate to the new tab page when an app is
  // installed, but we're experimenting with instead showing a bubble when
  // an app is installed which points to the new tab button. This may become
  // the default behavior in the future.
  void SetUseAppInstalledBubble(bool use_bubble);

  // Sets whether to show the default UI after completing the installation.
  void SetSkipPostInstallUI(bool skip_ui);

  // For testing:
  static base::AutoReset<bool> disable_ui_for_tests(bool disable);

 protected:
  explicit ExtensionInstallUI(content::BrowserContext* context);

  Profile* profile() { return profile_; }
  bool skip_post_install_ui() const { return skip_post_install_ui_; }
  bool use_app_installed_bubble() const { return use_app_installed_bubble_; }

  // Whether the UI is disabled for testing. The method cannot have ForTest() in
  // the name, otherwise the presubmits get confused when it is called from
  // production code.
  static bool IsUiDisabled();

 private:
  raw_ptr<Profile, DanglingUntriaged> profile_;

  // Whether or not to show the default UI after completing the installation.
  bool skip_post_install_ui_ = false;

  // Whether to show an installed bubble on app install, or use the default
  // action of opening a new tab page.
  bool use_app_installed_bubble_ = false;
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALL_UI_H_
