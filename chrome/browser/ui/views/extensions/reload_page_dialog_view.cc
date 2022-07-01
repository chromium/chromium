// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/reload_page_dialog_view.h"

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/feature_list.h"
#include "chrome/browser/extensions/site_permissions_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_dialogs_utils.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/extension_features.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"
#include "ui/views/widget/widget.h"

namespace extensions {

void ShowReloadPageDialog(
    Browser* browser,
    const std::vector<extensions::ExtensionId>& extension_ids,
    bool is_updating_permissions,
    base::OnceClosure callback) {
  ShowReloadPageDialogView(browser, extension_ids, is_updating_permissions,
                           std::move(callback));
}

}  // namespace extensions

namespace {

std::u16string GetTitle(const std::vector<ToolbarActionViewController*> actions,
                        bool is_updating_permissions) {
  if (is_updating_permissions) {
    return l10n_util::GetStringUTF16(
        IDS_EXTENSION_BLOCKED_ACTION_BUBBLE_UPDATE_PERMISSIONS_TITLE);
  }
  if (actions.size() == 1) {
    return l10n_util::GetStringFUTF16(
        IDS_EXTENSION_BLOCKED_ACTION_BUBBLE_SINGLE_EXTENSION_TITLE,
        actions[0]->GetActionName());
  }
  return l10n_util::GetStringUTF16(
      IDS_EXTENSION_BLOCKED_ACTION_BUBBLE_MULTIPLE_EXTENSIONS_TITLE);
}

}  // namespace

// static
void ShowReloadPageDialogView(
    Browser* browser,
    const std::vector<extensions::ExtensionId>& extension_ids,
    bool is_updating_permissions,
    base::OnceClosure callback) {
  ExtensionsToolbarContainer* const container =
      GetExtensionsToolbarContainer(browser);
  DCHECK(container);

  ui::DialogModel::Builder dialog_builder;
  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    std::vector<ToolbarActionViewController*> actions;
    for (const auto& extension_id : extension_ids) {
      actions.push_back(container->GetActionForId(extension_id));
    }

    dialog_builder.SetTitle(GetTitle(actions, is_updating_permissions))
        .AddOkButton(base::BindOnce(std::move(callback)),
                     l10n_util::GetStringUTF16(
                         IDS_EXTENSION_BLOCKED_ACTION_BUBBLE_OK_BUTTON));

    content::WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    if (extension_ids.size() == 1) {
      dialog_builder.SetIcon(GetIcon(actions[0], web_contents));
    } else {
      for (auto* action : actions) {
        dialog_builder.AddMenuItem(
            GetIcon(action, web_contents), action->GetActionName(),
            base::DoNothing(),
            ui::DialogModelMenuItem::Params().set_is_enabled(false));
      }
    }
  } else {
    dialog_builder
        .SetTitle(l10n_util::GetStringUTF16(
            IDS_EXTENSION_BLOCKED_ACTION_BUBBLE_HEADING))
        .AddOkButton(base::BindOnce(std::move(callback)),
                     l10n_util::GetStringUTF16(
                         IDS_EXTENSION_BLOCKED_ACTION_BUBBLE_OK_BUTTON));
  }

  ShowDialog(container, extension_ids, dialog_builder.Build());
}
