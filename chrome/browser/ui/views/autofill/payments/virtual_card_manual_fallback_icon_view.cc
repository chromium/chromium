// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/virtual_card_manual_fallback_icon_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/autofill/payments/virtual_card_manual_fallback_bubble_controller.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/autofill/payments/virtual_card_manual_fallback_bubble_views.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace autofill {

VirtualCardManualFallbackIconView::VirtualCardManualFallbackIconView(
    CommandUpdater* command_updater,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* delegate)
    : PageActionIconView(command_updater,
                         IDC_VIRTUAL_CARD_MANUAL_FALLBACK,
                         icon_label_bubble_delegate,
                         delegate,
                         "VirtualCardManualFallback") {
  GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_VIRTUAL_CARD_MANUAL_FALLBACK_ICON_TOOLTIP));
}

VirtualCardManualFallbackIconView::~VirtualCardManualFallbackIconView() =
    default;

views::BubbleDialogDelegate* VirtualCardManualFallbackIconView::GetBubble()
    const {
  VirtualCardManualFallbackBubbleController* controller = GetController();
  if (!controller) {
    return nullptr;
  }

  return static_cast<VirtualCardManualFallbackBubbleViews*>(
      controller->GetBubble());
}

void VirtualCardManualFallbackIconView::UpdateImpl() {
  if (!GetWebContents()) {
    return;
  }

  // |controller| may be nullptr due to lazy initialization.
  VirtualCardManualFallbackBubbleController* controller = GetController();
  bool command_enabled = controller && controller->ShouldIconBeVisible();
  SetVisible(SetCommandEnabled(command_enabled));
}

void VirtualCardManualFallbackIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {}

const gfx::VectorIcon& VirtualCardManualFallbackIconView::GetVectorIcon()
    const {
  return kCreditCardChromeRefreshIcon;
}

VirtualCardManualFallbackBubbleController*
VirtualCardManualFallbackIconView::GetController() const {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    return nullptr;
  }

  return VirtualCardManualFallbackBubbleController::Get(web_contents);
}

BEGIN_METADATA(VirtualCardManualFallbackIconView)
END_METADATA

}  // namespace autofill
