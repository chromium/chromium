// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALL_UI_ANDROID_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALL_UI_ANDROID_H_

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/ui/extensions/extension_install_ui.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
}

class SkBitmap;

// Manages the extension install UI bubble on Android.
// TODO(crbug.com/397754565): Make this work. For now it's just a stub so that
// we can get CrxInstaller working.
class ExtensionInstallUIAndroid : public ExtensionInstallUI {
 public:
  explicit ExtensionInstallUIAndroid(content::BrowserContext* context);
  ExtensionInstallUIAndroid(const ExtensionInstallUIAndroid&) = delete;
  ExtensionInstallUIAndroid& operator=(const ExtensionInstallUIAndroid&) =
      delete;
  ~ExtensionInstallUIAndroid() override;

  // ExtensionInstallUI:
  void OnInstallSuccess(scoped_refptr<const extensions::Extension> extension,
                        const SkBitmap* icon) override;
  void OnInstallFailure(const extensions::CrxInstallError& error) override;

  // Shows the install bubble UI.
  static void ShowBubble(scoped_refptr<const extensions::Extension> extension,
                         const SkBitmap& icon);
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALL_UI_ANDROID_H_
