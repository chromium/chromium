// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extensions_request_access_hover_card_coordinator.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/toolbar/toolbar_action_view_controller.h"
#include "chrome/browser/ui/views/chrome_widget_sublevel.h"
#include "chrome/browser/ui/views/extensions/extensions_dialogs_utils.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/models/dialog_model.h"
#include "ui/base/models/dialog_model_field.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"

void ExtensionsRequestAccessHoverCardCoordinator::ShowBubble(
    content::WebContents* web_contents,
    views::View* anchor_view,
    ExtensionsContainer* extensions_container,
    std::vector<extensions::ExtensionId>& extension_ids) {
  DCHECK(web_contents);
  DCHECK(extensions_container);
  DCHECK(!extension_ids.empty());

  ui::DialogModel::Builder dialog_builder =
      ui::DialogModel::Builder(std::make_unique<ui::DialogModelDelegate>());
  dialog_builder
      .OverrideShowCloseButton(false)
      // Make sure the widget is closed if the dialog gets destroyed while the
      // mouse is still on hover.
      .SetDialogDestroyingCallback(base::BindOnce(
          &ExtensionsRequestAccessHoverCardCoordinator::HideBubble,
          base::Unretained(this)));

  // TODO(crbug.com/40839674): Use extensions::IconImage instead of getting the
  // action's image. This requires the coordinator class to implement
  // extensions::IconImage::Observer.

  const std::u16string url = GetCurrentHost(web_contents);
  if (extension_ids.size() == 1) {
    ToolbarActionViewController* action =
        extensions_container->GetActionForId(extension_ids[0]);
    dialog_builder.SetIcon(GetIcon(action, web_contents))
        .AddParagraph(ui::DialogModelLabel::CreateWithReplacements(
            IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON_TOOLTIP_SINGLE_EXTENSION,
            {ui::DialogModelLabel::CreatePlainText(action->GetActionName()),
             ui::DialogModelLabel::CreateEmphasizedText(url)}));
  } else {
    dialog_builder.AddParagraph(ui::DialogModelLabel::CreateWithReplacement(
        IDS_EXTENSIONS_REQUEST_ACCESS_BUTTON_TOOLTIP_MULTIPLE_EXTENSIONS,
        ui::DialogModelLabel::CreateEmphasizedText(url)));
    for (auto extension_id : extension_ids) {
      ToolbarActionViewController* action =
          extensions_container->GetActionForId(extension_id);
      dialog_builder.AddMenuItem(
          GetIcon(action, web_contents), action->GetActionName(),
          base::DoNothing(),
          ui::DialogModelMenuItem::Params().SetIsEnabled(false));
    }
  }

  auto bubble = std::make_unique<views::BubbleDialogModelHost>(
      dialog_builder.Build(), anchor_view, views::BubbleBorder::TOP_RIGHT);
  // Hover card should not become active window when hovering over request
  // button in an inactive window. Setting this to false creates the need to
  // explicitly hide the hovercard.
  bubble->SetCanActivate(false);
  bubble_tracker_.SetView(bubble->GetContentsView());

  auto* widget = views::BubbleDialogDelegate::CreateBubble(std::move(bubble));
  // Ensure the hover card Widget assumes a higher z-order to avoid occlusion
  // by other secondary UI Widgets
  widget->SetZOrderSublevel(ChromeWidgetSublevel::kSublevelHoverable);

  widget->Show();
}

void ExtensionsRequestAccessHoverCardCoordinator::HideBubble() {
  if (IsShowing()) {
    bubble_tracker_.view()->GetWidget()->Close();
    bubble_tracker_.SetView(nullptr);
  }
}

bool ExtensionsRequestAccessHoverCardCoordinator::IsShowing() const {
  return bubble_tracker_.view() != nullptr;
}
