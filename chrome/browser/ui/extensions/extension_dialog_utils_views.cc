// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_dialog_utils.h"
#include "chrome/browser/ui/views/extensions/extension_view_utils.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "components/constrained_window/constrained_window_views.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/widget/widget.h"

namespace {

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

void ShowDialog(gfx::NativeWindow parent,
                const extensions::ExtensionId& extension_id,
                std::unique_ptr<ui::DialogModel> dialog_model) {
  ShowDialog(parent, std::vector({extension_id}), std::move(dialog_model));
}

void ShowModalDialog(gfx::NativeWindow parent,
                     std::unique_ptr<ui::DialogModel> dialog_model) {
  constrained_window::ShowBrowserModal(std::move(dialog_model), parent);
}

void ShowWebModalDialog(content::WebContents* web_contents,
                        std::unique_ptr<ui::DialogModel> dialog_model) {
  constrained_window::ShowWebModal(std::move(dialog_model), web_contents);
}

void ShowDialog(gfx::NativeWindow parent,
                const std::vector<extensions::ExtensionId>& extension_ids,
                std::unique_ptr<ui::DialogModel> dialog_model) {
  ExtensionsToolbarContainer* const container =
      parent ? GetExtensionsToolbarContainer(parent) : nullptr;
  if (container && container->GetVisible()) {
    ShowDialog(container, extension_ids, std::move(dialog_model));
  } else {
    // If the container is not available, show a modal dialog.
    ShowModalDialog(parent, std::move(dialog_model));
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
