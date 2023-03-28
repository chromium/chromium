// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/save_update_address_profile_icon_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/autofill/save_update_address_profile_icon_controller.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/autofill/save_address_profile_view.h"
#include "chrome/browser/ui/views/autofill/update_address_profile_view.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace autofill {

SaveUpdateAddressProfileIconView::SaveUpdateAddressProfileIconView(
    CommandUpdater* command_updater,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate)
    : PageActionIconView(command_updater,
                         IDC_SAVE_AUTOFILL_ADDRESS,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate,
                         "SaveAutofillAddress") {
  SetAccessibilityProperties(/*role*/ absl::nullopt,
                             GetTextForTooltipAndAccessibleName());
}

SaveUpdateAddressProfileIconView::~SaveUpdateAddressProfileIconView() = default;

views::BubbleDialogDelegate* SaveUpdateAddressProfileIconView::GetBubble()
    const {
  SaveUpdateAddressProfileIconController* controller = GetController();
  if (!controller)
    return nullptr;

  if (controller->IsSaveBubble()) {
    return static_cast<autofill::SaveAddressProfileView*>(
        controller->GetBubbleView());
  }
  return static_cast<autofill::UpdateAddressProfileView*>(
      controller->GetBubbleView());
}

void SaveUpdateAddressProfileIconView::UpdateImpl() {
  SaveUpdateAddressProfileIconController* controller = GetController();
  bool command_enabled =
      SetCommandEnabled(controller && controller->IsBubbleActive());
  SetVisible(command_enabled);
  SetAccessibleName(GetTextForTooltipAndAccessibleName());
}

std::u16string
SaveUpdateAddressProfileIconView::GetTextForTooltipAndAccessibleName() const {
  SaveUpdateAddressProfileIconController* controller = GetController();
  if (!controller) {
    // If the controller is nullptr, the tab has been closed already, and the
    // icon will disappear soon. Return a save address prompt title to make
    // sure the accessible name isn't empty to avoid test flakiness.
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE);
  }
  return controller->GetPageActionIconTootip();
}

void SaveUpdateAddressProfileIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {}

const gfx::VectorIcon& SaveUpdateAddressProfileIconView::GetVectorIcon() const {
  // TODO(crbug.com/1167060): Update the icon upon having final mocks.
  return vector_icons::kLocationOnIcon;
}

SaveUpdateAddressProfileIconController*
SaveUpdateAddressProfileIconView::GetController() const {
  return SaveUpdateAddressProfileIconController::Get(GetWebContents());
}

BEGIN_METADATA(SaveUpdateAddressProfileIconView, PageActionIconView)
END_METADATA

}  // namespace autofill
