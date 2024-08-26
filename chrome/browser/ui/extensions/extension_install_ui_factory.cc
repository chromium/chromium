// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_install_ui_factory.h"

#include <memory>

#include "chrome/browser/ui/extensions/extension_install_ui_default.h"

namespace extensions {

// TODO(crbug.com/361372991): We don't need to have a factory, callers can
// create ExtensionInstallUIDefault directly (and we can drop 'default' from the
// name).
std::unique_ptr<ExtensionInstallUIDefault> CreateExtensionInstallUI(
    content::BrowserContext* context) {
  return std::make_unique<ExtensionInstallUIDefault>(context);
}

}  // namespace extensions
