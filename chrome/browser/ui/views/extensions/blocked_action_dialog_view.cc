// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/blocked_action_dialog_view.h"

#include "chrome/browser/ui/views/extensions/extensions_toolbar_button.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/widget/widget.h"

namespace extensions {

void ShowBlockedActionDialog(Browser* browser,
                             const ExtensionId& extension_id,
                             base::OnceClosure callback) {
  ShowBlockedActionDialogView(browser, extension_id, std::move(callback));
}

}  // namespace extensions

// static
void ShowBlockedActionDialogView(Browser* browser,
                                 const extensions::ExtensionId& extension_id,
                                 base::OnceClosure callback) {
  std::unique_ptr<ui::DialogModel> dialog_model =
      ui::DialogModel::Builder()
          .SetTitle(l10n_util::GetStringUTF16(
              IDS_EXTENSION_BLOCKED_ACTION_BUBBLE_HEADING))
          .AddOkButton(std::move(callback),
                       l10n_util::GetStringUTF16(
                           IDS_EXTENSION_BLOCKED_ACTION_BUBBLE_OK_BUTTON))
          .OverrideShowCloseButton(true)
          .Build();

  // TODO(crbug.com/1322796): Multiple classes use this. We should pull getting
  // an anchor view and showing a BubbleDialogDelegate into a common location.
  BrowserView* const browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  ExtensionsToolbarContainer* const container =
      browser_view ? browser_view->toolbar_button_provider()
                         ->GetExtensionsToolbarContainer()
                   : nullptr;
  DCHECK(container);
  views::View* anchor_view = container->GetViewForId(extension_id);

  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      std::move(dialog_model),
      anchor_view ? anchor_view : container->GetExtensionsButton(),
      views::BubbleBorder::TOP_RIGHT);

  container->ShowWidgetForExtension(
      views::BubbleDialogDelegate::CreateBubble(std::move(bubble)),
      extension_id);
}
