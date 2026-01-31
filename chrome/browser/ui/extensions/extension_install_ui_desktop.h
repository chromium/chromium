// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALL_UI_DESKTOP_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALL_UI_DESKTOP_H_

#include "base/memory/scoped_refptr.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/extensions/extension_install_ui.h"
#include "content/public/browser/web_contents.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class Extension;
}

class SkBitmap;

// Manages the extension install UI bubble on Windows/Mac/Linux/ChromeOS.
class ExtensionInstallUIDesktop : public ExtensionInstallUI {
 public:
  explicit ExtensionInstallUIDesktop(content::BrowserContext* context);
  ExtensionInstallUIDesktop(const ExtensionInstallUIDesktop&) = delete;
  ExtensionInstallUIDesktop& operator=(const ExtensionInstallUIDesktop&) =
      delete;
  ~ExtensionInstallUIDesktop() override;

  // ExtensionInstallUI:
  void OnInstallSuccess(scoped_refptr<const extensions::Extension> extension,
                        const SkBitmap* icon) override;
  void OnInstallFailure(const extensions::CrxInstallError& error) override;
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALL_UI_DESKTOP_H_
