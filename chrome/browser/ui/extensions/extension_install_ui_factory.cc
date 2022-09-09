// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_install_ui_factory.h"

#include "chrome/browser/ui/extensions/extension_install_ui_default.h"

namespace extensions {

std::unique_ptr<ExtensionInstallUI> CreateExtensionInstallUI(
    content::BrowserContext* context) {
  return std::unique_ptr<ExtensionInstallUI>(
      new ExtensionInstallUIDefault(context));
}

}  // namespace extensions
