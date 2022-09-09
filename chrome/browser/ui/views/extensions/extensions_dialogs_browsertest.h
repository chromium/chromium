// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_DIALOGS_BROWSERTEST_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_DIALOGS_BROWSERTEST_H_

#include <string>
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"

namespace extensions {
class Extension;
}

class ExtensionsToolbarContainer;

class ExtensionsDialogBrowserTest : public DialogBrowserTest {
 public:
  ExtensionsDialogBrowserTest() = default;
  ExtensionsDialogBrowserTest(const ExtensionsDialogBrowserTest&) = delete;
  const ExtensionsDialogBrowserTest& operator=(
      const ExtensionsDialogBrowserTest&) = delete;
  ~ExtensionsDialogBrowserTest() override = default;

  scoped_refptr<const extensions::Extension> InstallExtension(
      const std::string& name);

  ExtensionsToolbarContainer* extensions_container();
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_DIALOGS_BROWSERTEST_H_
