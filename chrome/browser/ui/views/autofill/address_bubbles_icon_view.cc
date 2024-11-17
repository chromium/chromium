// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/address_bubbles_icon_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/promos/promos_types.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/autofill/address_bubbles_icon_controller.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/autofill/address_bubble_base_view.h"
#include "chrome/browser/ui/views/promos/ios_promo_bubble.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace autofill {

AddressBubblesIconView::AddressBubblesIconView(
    CommandUpdater* command_updater,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate)
    : PageActionIconView(command_updater,
                         IDC_SAVE_AUTOFILL_ADDRESS,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate,
                         "SaveAutofillAddress",
                         kActionShowAddressesBubbleOrPage) {
  GetViewAccessibility().SetName(GetTextForTooltipAndAccessibleName());
}

AddressBubblesIconView::~AddressBubblesIconView() = default;

views::BubbleDialogDelegate* AddressBubblesIconView::GetBubble()
    const {
  AddressBubblesIconController* controller = GetController();
  if (!controller) {
    return nullptr;
  }

  return static_cast<autofill::AddressBubbleBaseView*>(
      controller->GetBubbleView());
}

void AddressBubblesIconView::UpdateImpl() {
  AddressBubblesIconController* controller = GetController();
  const bool command_enabled =
      SetCommandEnabled(controller && controller->IsBubbleActive());
  bool should_show =
      command_enabled && !delegate()->ShouldHidePageActionIcon(this);

  // TODO(crbug.com/372209715): Extract out of GOOGLE_CHROME_BRANDING buildflag.
#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
  // Show the icon if the Desktop to iOS address promo is currently being shown.
  should_show =
      should_show || IOSPromoBubble::IsPromoTypeVisible(IOSPromoType::kAddress);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

  SetVisible(should_show);
  GetViewAccessibility().SetName(GetTextForTooltipAndAccessibleName());
}

std::u16string
AddressBubblesIconView::GetTextForTooltipAndAccessibleName() const {
  AddressBubblesIconController* controller = GetController();
  if (!controller) {
    // If the controller is nullptr, the tab has been closed already, and the
    // icon will disappear soon. Return a save address prompt title to make
    // sure the accessible name isn't empty to avoid test flakiness.
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE);
  }
  return controller->GetPageActionIconTootip();
}

void AddressBubblesIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {}

const gfx::VectorIcon& AddressBubblesIconView::GetVectorIcon() const {
  // TODO(crbug.com/40164487): Update the icon upon having final mocks.
  return vector_icons::kLocationOnChromeRefreshIcon;
}

AddressBubblesIconController*
AddressBubblesIconView::GetController() const {
  return AddressBubblesIconController::Get(GetWebContents());
}

BEGIN_METADATA(AddressBubblesIconView)
END_METADATA

}  // namespace autofill
