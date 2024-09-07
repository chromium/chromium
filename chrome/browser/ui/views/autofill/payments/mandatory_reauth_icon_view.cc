// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/mandatory_reauth_icon_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/autofill/payments/mandatory_reauth_bubble_controller.h"
#include "chrome/browser/ui/autofill/payments/mandatory_reauth_bubble_controller_impl.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/autofill/payments/mandatory_reauth_confirmation_bubble_view.h"
#include "chrome/browser/ui/views/autofill/payments/mandatory_reauth_opt_in_bubble_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace autofill {

MandatoryReauthIconView::MandatoryReauthIconView(
    CommandUpdater* command_updater,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* delegate)
    : PageActionIconView(command_updater,
                         IDC_AUTOFILL_MANDATORY_REAUTH,
                         icon_label_bubble_delegate,
                         delegate,
                         "MandatoryReauth") {
  GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_MANDATORY_REAUTH_ICON_TOOLTIP));
}

MandatoryReauthIconView::~MandatoryReauthIconView() = default;

views::BubbleDialogDelegate* MandatoryReauthIconView::GetBubble() const {
  MandatoryReauthBubbleController* controller = GetController();
  if (!controller) {
    return nullptr;
  }

  if (controller->GetBubbleType() == MandatoryReauthBubbleType::kConfirmation) {
    return static_cast<autofill::MandatoryReauthConfirmationBubbleView*>(
        controller->GetBubbleView());
  } else {
    return static_cast<autofill::MandatoryReauthOptInBubbleView*>(
        controller->GetBubbleView());
  }
}

void MandatoryReauthIconView::UpdateImpl() {
  if (!GetWebContents()) {
    return;
  }

  // `controller` may be nullptr due to lazy initialization.
  MandatoryReauthBubbleController* controller = GetController();
  bool command_enabled = controller && controller->IsIconVisible();
  SetVisible(SetCommandEnabled(command_enabled));
}

void MandatoryReauthIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {}

const gfx::VectorIcon& MandatoryReauthIconView::GetVectorIcon() const {
  return kCreditCardChromeRefreshIcon;
}

MandatoryReauthBubbleController* MandatoryReauthIconView::GetController()
    const {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    return nullptr;
  }

  return MandatoryReauthBubbleControllerImpl::FromWebContents(web_contents);
}

BEGIN_METADATA(MandatoryReauthIconView)
END_METADATA

}  // namespace autofill
