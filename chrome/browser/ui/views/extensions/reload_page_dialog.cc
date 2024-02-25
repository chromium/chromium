// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extensions_dialogs_utils.h"
#include "chrome/browser/ui/views/extensions/extensions_toolbar_container.h"
#include "chrome/grit/generated_resources.h"
#include "extensions/common/extension_features.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/dialog_model.h"

namespace {

std::u16string GetTitle(
    const std::vector<ToolbarActionViewController*> actions) {
  if (actions.size() == 0) {
    return l10n_util::GetStringUTF16(
        IDS_EXTENSION_SITE_RELOAD_PAGE_BUBBLE_HEADING);
  }
  if (actions.size() == 1) {
    return l10n_util::GetStringFUTF16(
        IDS_EXTENSION_RELOAD_PAGE_BUBBLE_ALLOW_SINGLE_EXTENSION_TITLE,
        actions[0]->GetActionName());
  }
  return l10n_util::GetStringUTF16(
      IDS_EXTENSION_RELOAD_PAGE_BUBBLE_ALLOW_MULTIPLE_EXTENSIONS_TITLE);
}

}  // namespace

namespace extensions {

void ShowReloadPageDialog(
    Browser* browser,
    const std::vector<extensions::ExtensionId>& extension_ids,
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

    dialog_builder.SetTitle(GetTitle(actions))
        .AddOkButton(base::BindOnce(std::move(callback)),
                     ui::DialogModel::Button::Params().SetLabel(
                         l10n_util::GetStringUTF16(
                             IDS_EXTENSION_RELOAD_PAGE_BUBBLE_OK_BUTTON)));

    content::WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    if (extension_ids.size() == 1) {
      dialog_builder.SetIcon(GetIcon(actions[0], web_contents));
    } else if (extension_ids.size() > 1) {
      for (auto* action : actions) {
        dialog_builder.AddMenuItem(
            GetIcon(action, web_contents), action->GetActionName(),
            base::DoNothing(),
            ui::DialogModelMenuItem::Params().SetIsEnabled(false));
      }
    }
  } else {
    dialog_builder
        .SetTitle(l10n_util::GetStringUTF16(
            IDS_EXTENSION_SITE_RELOAD_PAGE_BUBBLE_HEADING))
        .AddOkButton(base::BindOnce(std::move(callback)),
                     ui::DialogModel::Button::Params().SetLabel(
                         l10n_util::GetStringUTF16(
                             IDS_EXTENSION_RELOAD_PAGE_BUBBLE_OK_BUTTON)));
  }

  ShowDialog(container, extension_ids, dialog_builder.Build());
}

}  // namespace extensions
