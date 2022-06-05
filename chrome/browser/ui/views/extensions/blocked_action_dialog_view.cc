// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/blocked_action_dialog_view.h"

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

void ShowBlockedActionDialog(Browser* browser,
                             const ExtensionId& extension_id,
                             bool show_checkbox,
                             base::OnceCallback<void(bool)> callback) {
  ShowBlockedActionDialogView(browser, extension_id, show_checkbox,
                              std::move(callback));
}

}  // namespace extensions

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kCheckboxId);

}  // namespace

// static
void ShowBlockedActionDialogView(Browser* browser,
                                 const extensions::ExtensionId& extension_id,
                                 bool show_checkbox,
                                 base::OnceCallback<void(bool)> callback) {
  ExtensionsToolbarContainer* const container =
      GetExtensionsToolbarContainer(browser);
  DCHECK(container);

  auto on_ok_button_clicked = [](ui::DialogModel* dialog_model,
                                 bool did_show_checkbox,
                                 base::OnceCallback<void(bool)> callback) {
    bool checkbox_checked =
        dialog_model->GetCheckboxByUniqueId(kCheckboxId)->is_checked();
    std::move(callback).Run(did_show_checkbox && checkbox_checked);
  };

  ui::DialogModel::Builder dialog_builder;
  if (base::FeatureList::IsEnabled(
          extensions_features::kExtensionsMenuAccessControl)) {
    ToolbarActionViewController* extension =
        container->GetActionForId(extension_id);
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    dialog_builder
        .SetTitle(l10n_util::GetStringFUTF16(
            IDS_EXTENSION_BLOCKED_ACTION_BUBBLE_SINGLE_EXTENSION_TITLE,
            extension->GetActionName()))
        .SetIcon(GetIcon(extension, web_contents))
        .AddBodyText(ui::DialogModelLabel(l10n_util::GetStringUTF16(
            IDS_EXTENSION_BLOCKED_ACTION_BUBBLE_BODY_TEXT)))
        .AddOkButton(
            base::BindOnce(on_ok_button_clicked, dialog_builder.model(),
                           show_checkbox, std::move(callback)),
            l10n_util::GetStringUTF16(
                IDS_EXTENSION_BLOCKED_ACTION_BUBBLE_OK_BUTTON_LABEL));
    if (show_checkbox) {
      dialog_builder.AddCheckbox(
          kCheckboxId,
          ui::DialogModelLabel(l10n_util::GetStringUTF16(
              IDS_EXTENSION_BLOCKED_ACTION_BUBBLE_CHECKBOX_LABEL)));
    }
  } else {
    dialog_builder
        .SetTitle(l10n_util::GetStringUTF16(
            IDS_EXTENSION_BLOCKED_ACTION_BUBBLE_HEADING))
        .AddOkButton(base::BindOnce(std::move(callback), /*is_checked=*/false),
                     l10n_util::GetStringUTF16(
                         IDS_EXTENSION_BLOCKED_ACTION_BUBBLE_OK_BUTTON));
  }

  ShowDialog(container, extension_id, dialog_builder.Build());
}
