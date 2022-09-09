// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALL_UI_FACTORY_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALL_UI_FACTORY_H_

#include <memory>

namespace content {
class BrowserContext;
}

namespace extensions {
class ExtensionInstallUI;

std::unique_ptr<extensions::ExtensionInstallUI> CreateExtensionInstallUI(
    content::BrowserContext* context);

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALL_UI_FACTORY_H_
