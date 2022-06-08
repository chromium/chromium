// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_dialogs_utils.h"

#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_item_view.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/flex_layout_view.h"

namespace {

ExtensionsToolbarContainer* GetExtensionsToolbarContainer(
    BrowserView* browser_view) {
  return browser_view ? browser_view->toolbar_button_provider()
                            ->GetExtensionsToolbarContainer()
                      : nullptr;
}

}  // namespace

ExtensionsToolbarContainer* GetExtensionsToolbarContainer(Browser* browser) {
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  return GetExtensionsToolbarContainer(browser_view);
}

ExtensionsToolbarContainer* GetExtensionsToolbarContainer(
    gfx::NativeWindow parent) {
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForNativeWindow(parent);
  return GetExtensionsToolbarContainer(browser_view);
}

// TODO(crbug.com/1325171): Use extensions::IconImage instead of getting the
// action's image. The icon displayed should be the "product" icon and not the
// "action" action based on the web contents.
ui::ImageModel GetIcon(ToolbarActionViewController* action,
                       content::WebContents* web_contents) {
  return ui::ImageModel::FromImageSkia(
      action->GetIcon(web_contents, InstalledExtensionMenuItemView::kIconSize)
          .AsImageSkia());
}

std::u16string GetCurrentHost(content::WebContents* web_contents) {
  DCHECK(web_contents);
  auto url = web_contents->GetLastCommittedURL();
  // Hide the scheme when necessary (e.g hide "https://" but don't
  // "chrome://").
  return url_formatter::FormatUrl(
      url,
      url_formatter::kFormatUrlOmitDefaults |
          url_formatter::kFormatUrlOmitHTTPS |
          url_formatter::kFormatUrlOmitTrivialSubdomains |
          url_formatter::kFormatUrlTrimAfterHost,
      base::UnescapeRule::NORMAL, nullptr, nullptr, nullptr);
}

views::View* GetDialogAnchorView(ExtensionsToolbarContainer* container,
                                 const extensions::ExtensionId& extension_id) {
  DCHECK(container);
  views::View* const action_view = container->GetViewForId(extension_id);
  return action_view ? action_view : container->GetExtensionsButton();
}

void ShowDialog(gfx::NativeWindow parent,
                const extensions::ExtensionId& extension_id,
                std::unique_ptr<ui::DialogModel> dialog_model) {
  ExtensionsToolbarContainer* container = GetExtensionsToolbarContainer(parent);
  if (container && container->GetVisible()) {
    ShowDialog(container, extension_id, std::move(dialog_model));
  } else {
    constrained_window::ShowBrowserModal(std::move(dialog_model), parent);
  }
}

void ShowDialog(ExtensionsToolbarContainer* container,
                const extensions::ExtensionId& extension_id,
                std::unique_ptr<ui::DialogModel> dialog_model) {
  DCHECK(container);
  views::View* const anchor_view = GetDialogAnchorView(container, extension_id);
  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      std::move(dialog_model), anchor_view, views::BubbleBorder::TOP_RIGHT);

  container->ShowWidgetForExtension(
      views::BubbleDialogDelegate::CreateBubble(std::move(bubble)),
      extension_id);
}
