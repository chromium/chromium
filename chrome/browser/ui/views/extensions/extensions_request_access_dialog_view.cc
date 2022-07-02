// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_request_access_dialog_view.h"

#include "base/callback_helpers.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_dialogs_utils.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"

namespace {

std::u16string GetTitle(
    const std::vector<ToolbarActionViewController*>& actions,
    std::u16string current_site) {
  if (actions.size() == 1) {
    return l10n_util::GetStringFUTF16(
        IDS_EXTENSIONS_REQUEST_ACCESS_BUBBLE_SINGLE_EXTENSION_TITLE,
        actions[0]->GetActionName(), current_site);
  }
  return l10n_util::GetStringFUTF16(
      IDS_EXTENSIONS_REQUEST_ACCESS_BUBBLE_MULTIPLE_EXTENSIONS_TITLE,
      current_site);
}

}  // namespace

void ShowExtensionsRequestAccessDialogView(
    content::WebContents* web_contents,
    views::View* anchor_view,
    std::vector<ToolbarActionViewController*> actions) {
  DCHECK(!actions.empty());
  DCHECK(web_contents);

  ui::DialogModel::Builder dialog_builder =
      ui::DialogModel::Builder(std::make_unique<ui::DialogModelDelegate>());
  dialog_builder
      .SetTitle(GetTitle(actions, GetCurrentHost(web_contents)))
      // TODO(crbug.com/1239772): Grant access to all the extensions when dialog
      // is accepted
      .AddOkButton(base::DoNothing(),
                   l10n_util::GetStringUTF16(
                       IDS_EXTENSIONS_REQUEST_ACCESS_BUBBLE_OK_BUTTON_LABEL))
      .AddCancelButton(
          base::DoNothing(),
          l10n_util::GetStringUTF16(
              IDS_EXTENSIONS_REQUEST_ACCESS_BUBBLE_CANCEL_BUTTON_LABEL));

  if (actions.size() == 1) {
    dialog_builder.SetIcon(GetIcon(actions[0], web_contents));
  } else {
    for (auto* action : actions) {
      dialog_builder.AddMenuItem(
          GetIcon(action, web_contents), action->GetActionName(),
          base::DoNothing(),
          ui::DialogModelMenuItem::Params().set_is_enabled(false));
    }
  }

  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      dialog_builder.Build(), anchor_view, views::BubbleBorder::TOP_RIGHT);
  views::BubbleDialogDelegate::CreateBubble(std::move(bubble))->Show();
}
