// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_DEVELOPER_PRIVATE_BRIDGE_H_
#define CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_DEVELOPER_PRIVATE_BRIDGE_H_

#include <string>

namespace extensions {

// The C++ counterpart to ExtensionDeveloperPrivateBridge.java.
class ExtensionDeveloperPrivateBridge {
 public:
  static void ShowSiteSettings(const std::string& extension_id);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSION_DEVELOPER_PRIVATE_BRIDGE_H_
