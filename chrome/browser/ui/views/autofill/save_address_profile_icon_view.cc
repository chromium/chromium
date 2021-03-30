// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/save_address_profile_icon_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/autofill/save_address_profile_icon_controller.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/autofill/save_address_profile_view.h"
#include "components/vector_icons/vector_icons.h"

namespace autofill {

SaveAddressProfileIconView::SaveAddressProfileIconView(
    CommandUpdater* command_updater,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate)
    : PageActionIconView(command_updater,
                         IDC_SAVE_AUTOFILL_ADDRESS,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate) {}

SaveAddressProfileIconView::~SaveAddressProfileIconView() = default;

views::BubbleDialogDelegate* SaveAddressProfileIconView::GetBubble() const {
  SaveAddressProfileIconController* controller = GetController();
  if (!controller)
    return nullptr;
  return static_cast<autofill::SaveAddressProfileView*>(
      controller->GetSaveBubbleView());
}

void SaveAddressProfileIconView::UpdateImpl() {
  if (!GetWebContents())
    return;

  SaveAddressProfileIconController* controller = GetController();
  bool command_enabled =
      SetCommandEnabled(controller && controller->IsBubbleActive());
  SetVisible(command_enabled);
}

std::u16string SaveAddressProfileIconView::GetTextForTooltipAndAccessibleName()
    const {
  // TODO(crbug.com/1167060): Update upon having final mocks.
  return std::u16string();
}

void SaveAddressProfileIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {}

const gfx::VectorIcon& SaveAddressProfileIconView::GetVectorIcon() const {
  // TODO(crbug.com/1167060): Update the icon upon having final mocks.
  return vector_icons::kLocationOnIcon;
}

SaveAddressProfileIconController* SaveAddressProfileIconView::GetController()
    const {
  return SaveAddressProfileIconController::Get(GetWebContents());
}

}  // namespace autofill
