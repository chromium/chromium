// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_dialogs_browsertest.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "extensions_toolbar_container.h"

scoped_refptr<const extensions::Extension>
ExtensionsDialogBrowserTest::InstallExtension(const std::string& name) {
  scoped_refptr<const extensions::Extension> extension(
      extensions::ExtensionBuilder(name).Build());
  extensions::ExtensionSystem::Get(browser()->profile())
      ->extension_service()
      ->AddExtension(extension.get());
  return extension;
}

ExtensionsToolbarContainer*
ExtensionsDialogBrowserTest::extensions_container() {
  return BrowserView::GetBrowserViewForBrowser(browser())
      ->toolbar()
      ->extensions_container();
}
