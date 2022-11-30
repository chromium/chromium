// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/extensions/extension_message_bubble_bridge.h"

#include <utility>

#include "chrome/browser/extensions/extension_message_bubble_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/grit/generated_resources.h"
#include "components/vector_icons/vector_icons.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_id.h"
#include "ui/base/l10n/l10n_util.h"

ExtensionMessageBubbleBridge::ExtensionMessageBubbleBridge(
    std::unique_ptr<extensions::ExtensionMessageBubbleController> controller)
    : controller_(std::move(controller)) {}

ExtensionMessageBubbleBridge::~ExtensionMessageBubbleBridge() {}

bool ExtensionMessageBubbleBridge::ShouldShow() {
  return controller_->ShouldShow();
}

bool ExtensionMessageBubbleBridge::ShouldCloseOnDeactivate() {
  return controller_->CloseOnDeactivate();
}

bool ExtensionMessageBubbleBridge::IsPolicyIndicationNeeded(
    const extensions::Extension* extension) {
  return controller_->delegate()->SupportsPolicyIndicator() &&
         extensions::Manifest::IsPolicyLocation(extension->location());
}

std::u16string ExtensionMessageBubbleBridge::GetHeadingText() {
  return controller_->delegate()->GetTitle();
}

std::u16string ExtensionMessageBubbleBridge::GetBodyText(
    bool anchored_to_action) {
  return controller_->delegate()->GetMessageBody(
      anchored_to_action, controller_->GetExtensionIdList().size());
}

std::u16string ExtensionMessageBubbleBridge::GetItemListText() {
  return controller_->GetExtensionListForDisplay();
}

std::u16string ExtensionMessageBubbleBridge::GetActionButtonText() {
  const extensions::ExtensionIdList& list = controller_->GetExtensionIdList();
  DCHECK(!list.empty());
  // Normally, the extension is enabled, but this might not be the case (such as
  // for the SuspiciousExtensionBubbleDelegate, which warns the user about
  // disabled extensions).
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(controller_->profile())
          ->GetExtensionById(list[0],
                             extensions::ExtensionRegistry::EVERYTHING);

  DCHECK(extension);
  // An empty string is returned so that we don't display the button prompting
  // to remove policy-installed extensions.
  if (IsPolicyIndicationNeeded(extension))
    return std::u16string();
  return controller_->delegate()->GetActionButtonLabel();
}

std::u16string ExtensionMessageBubbleBridge::GetDismissButtonText() {
  return controller_->delegate()->GetDismissButtonLabel();
}

ui::DialogButton ExtensionMessageBubbleBridge::GetDefaultDialogButton() {
  // TODO(estade): we should set a default where appropriate. See
  // http://crbug.com/751279
  return ui::DIALOG_BUTTON_NONE;
}

std::string ExtensionMessageBubbleBridge::GetAnchorActionId() {
  return controller_->GetExtensionIdList().size() == 1u
             ? controller_->GetExtensionIdList()[0]
             : std::string();
}

void ExtensionMessageBubbleBridge::OnBubbleShown(
    base::OnceClosure close_bubble_callback) {
  controller_->OnShown(std::move(close_bubble_callback));
}

void ExtensionMessageBubbleBridge::OnBubbleClosed(CloseAction action) {
  switch (action) {
    case CLOSE_DISMISS_USER_ACTION:
    case CLOSE_DISMISS_DEACTIVATION: {
      bool close_by_deactivate = action == CLOSE_DISMISS_DEACTIVATION;
      controller_->OnBubbleDismiss(close_by_deactivate);
      break;
    }
    case CLOSE_EXECUTE:
      controller_->OnBubbleAction();
      break;
    case CLOSE_LEARN_MORE:
      controller_->OnLinkClicked();
      break;
  }
}

std::unique_ptr<ToolbarActionsBarBubbleDelegate::ExtraViewInfo>
ExtensionMessageBubbleBridge::GetExtraViewInfo() {
  const extensions::ExtensionIdList& list = controller_->GetExtensionIdList();
  int include_mask = controller_->delegate()->ShouldLimitToEnabledExtensions() ?
      extensions::ExtensionRegistry::ENABLED :
      extensions::ExtensionRegistry::EVERYTHING;
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(controller_->profile())
          ->GetExtensionById(list[0], include_mask);

  DCHECK(extension);

  std::unique_ptr<ExtraViewInfo> extra_view_info =
      std::make_unique<ExtraViewInfo>();

  if (IsPolicyIndicationNeeded(extension)) {
    DCHECK_EQ(1u, list.size());
    extra_view_info->resource = &vector_icons::kBusinessIcon;
    extra_view_info->text =
        l10n_util::GetStringUTF16(IDS_EXTENSIONS_INSTALLED_BY_ADMIN);
    extra_view_info->is_learn_more = false;
  } else {
    extra_view_info->text = controller_->delegate()->GetLearnMoreLabel();
    extra_view_info->is_learn_more = true;
  }

  return extra_view_info;
}
