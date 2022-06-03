// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_WEB_APP_EXTENSION_SHORTCUT_MAC_H_
#define CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_WEB_APP_EXTENSION_SHORTCUT_MAC_H_

namespace base {
class CommandLine;
}

namespace web_app {

// Rebuild the shortcut and relaunch it.
bool MaybeRebuildShortcut(const base::CommandLine& command_line);

}  // namespace web_app

#endif  // CHROME_BROWSER_WEB_APPLICATIONS_EXTENSIONS_WEB_APP_EXTENSION_SHORTCUT_MAC_H_
