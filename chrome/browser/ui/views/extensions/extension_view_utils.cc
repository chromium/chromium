// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_view_utils.h"

#include "chrome/browser/ui/toolbar/toolbar_action_view_model.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/browser/web_contents.h"

namespace {

ExtensionsToolbarContainer* GetExtensionsToolbarContainer(
    BrowserView* browser_view) {
  return browser_view ? browser_view->toolbar_button_provider()
                            ->GetExtensionsToolbarContainer()
                      : nullptr;
}

}  // namespace

ExtensionsToolbarContainer* GetExtensionsToolbarContainer(Browser* browser) {
  CHECK(browser);
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  return GetExtensionsToolbarContainer(browser_view);
}

ExtensionsToolbarContainer* GetExtensionsToolbarContainer(
    gfx::NativeWindow parent) {
  CHECK(parent);
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForNativeWindow(parent);
  return GetExtensionsToolbarContainer(browser_view);
}

// TODO(crbug.com/40839674): Use extensions::IconImage instead of getting the
// action's image. The icon displayed should be the "product" icon and not the
// "action" action based on the web contents.
ui::ImageModel GetIcon(ToolbarActionViewModel* action,
                       content::WebContents* web_contents) {
  return action->GetIcon(web_contents,
                         gfx::Size(extension_misc::EXTENSION_ICON_SMALLISH,
                                   extension_misc::EXTENSION_ICON_SMALLISH));
}
