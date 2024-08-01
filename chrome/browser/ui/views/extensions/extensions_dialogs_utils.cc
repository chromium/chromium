// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_dialogs_utils.h"

#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/url_formatter/elide_url.h"
#include "extensions/common/constants.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/widget/widget.h"

namespace {

ExtensionsToolbarContainer* GetExtensionsToolbarContainer(
    BrowserView* browser_view) {
  return browser_view ? browser_view->toolbar_button_provider()
                            ->GetExtensionsToolbarContainer()
                      : nullptr;
}

// Returns the action view corresponding to the extension if a single
// extension is specified in  extension_ids ; otherwise, returns the
// extensions button.
views::View* GetDialogAnchorView(
    ExtensionsToolbarContainer* container,
    const std::vector<extensions::ExtensionId>& extension_ids) {
  DCHECK(container);

  if (extension_ids.size() == 1) {
    views::View* const action_view = container->GetViewForId(extension_ids[0]);
    return action_view ? action_view : container->GetExtensionsButton();
  }

  return container->GetExtensionsButton();
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
ui::ImageModel GetIcon(ToolbarActionViewController* action,
                       content::WebContents* web_contents) {
  return action->GetIcon(web_contents,
                         gfx::Size(extension_misc::EXTENSION_ICON_SMALLISH,
                                   extension_misc::EXTENSION_ICON_SMALLISH));
}

std::u16string GetCurrentHost(content::WebContents* web_contents) {
  DCHECK(web_contents);
  auto url = web_contents->GetLastCommittedURL();
  // Hide the scheme when necessary (e.g hide "https://" but don't
  // "chrome://").
  return url_formatter::FormatUrlForDisplayOmitSchemePathAndTrivialSubdomains(
      url);
}

void ShowDialog(gfx::NativeWindow parent,
                const extensions::ExtensionId& extension_id,
                std::unique_ptr<ui::DialogModel> dialog_model) {
  ExtensionsToolbarContainer* const container =
      parent ? GetExtensionsToolbarContainer(parent) : nullptr;
  if (container && container->GetVisible()) {
    ShowDialog(container, {extension_id}, std::move(dialog_model));
  } else {
    constrained_window::ShowBrowserModal(std::move(dialog_model), parent);
  }
}

void ShowDialog(ExtensionsToolbarContainer* container,
                const std::vector<extensions::ExtensionId>& extension_ids,
                std::unique_ptr<ui::DialogModel> dialog_model) {
  DCHECK(container);

  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      std::move(dialog_model), GetDialogAnchorView(container, extension_ids),
      views::BubbleBorder::TOP_RIGHT);
  views::Widget* widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(bubble));

  if (extension_ids.size() == 1) {
    // Show the widget using the anchor view of the specific extension (which
    // the container may need to popup out).
    // TODO(emiliapaz): Consider moving showing the widget for extension to the
    // utils to declutter the container file.
    container->ShowWidgetForExtension(widget, extension_ids[0]);
  } else {
    // Show the widget using the default dialog anchor view.
    widget->Show();
  }
}

void ShowDialog(Browser* browser,
                std::unique_ptr<ui::DialogModel> dialog_model) {
  ToolbarButtonProvider* toolbar_button_provider =
      BrowserView::GetBrowserViewForBrowser(browser)->toolbar_button_provider();
  CHECK(toolbar_button_provider);

  views::View* const anchor_view =
      toolbar_button_provider->GetDefaultExtensionDialogAnchorView();
  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      std::move(dialog_model), std::move(anchor_view),
      views::BubbleBorder::TOP_RIGHT);
  views::Widget* widget =
      views::BubbleDialogDelegate::CreateBubble(std::move(bubble));

  widget->Show();
}
