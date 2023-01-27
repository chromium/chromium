// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_dialogs_browsertest.h"

#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_builder.h"
#include "ui/views/layout/animating_layout_manager_test_util.h"

scoped_refptr<const extensions::Extension>
ExtensionsDialogBrowserTest::InstallExtension(const std::string& name) {
  scoped_refptr<const extensions::Extension> extension(
      extensions::ExtensionBuilder(name).Build());
  extensions::ExtensionSystem::Get(browser()->profile())
      ->extension_service()
      ->AddExtension(extension.get());
  views::test::WaitForAnimatingLayoutManager(extensions_container());
  return extension;
}

ExtensionsToolbarContainer*
ExtensionsDialogBrowserTest::extensions_container() {
  return BrowserView::GetBrowserViewForBrowser(browser())
      ->toolbar()
      ->extensions_container();
}
