// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/filled_card_information_icon_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/autofill/payments/filled_card_information_bubble_controller.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/autofill/payments/filled_card_information_bubble_views.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"

namespace autofill {

FilledCardInformationIconView::FilledCardInformationIconView(
    CommandUpdater* command_updater,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* delegate)
    : PageActionIconView(command_updater,
                         IDC_FILLED_CARD_INFORMATION,
                         icon_label_bubble_delegate,
                         delegate,
                         "FilledCardInformation") {
  GetViewAccessibility().SetName(l10n_util::GetStringUTF16(
      IDS_AUTOFILL_FILLED_CARD_INFORMATION_ICON_TOOLTIP_VIRTUAL_CARD));
}

FilledCardInformationIconView::~FilledCardInformationIconView() = default;

views::BubbleDialogDelegate* FilledCardInformationIconView::GetBubble() const {
  FilledCardInformationBubbleController* controller = GetController();
  if (!controller) {
    return nullptr;
  }

  return static_cast<FilledCardInformationBubbleViews*>(
      controller->GetBubble());
}

void FilledCardInformationIconView::UpdateImpl() {
  if (!GetWebContents()) {
    return;
  }

  // |controller| may be nullptr due to lazy initialization.
  FilledCardInformationBubbleController* controller = GetController();
  bool command_enabled = controller && controller->ShouldIconBeVisible();
  SetVisible(SetCommandEnabled(command_enabled));
}

void FilledCardInformationIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {}

const gfx::VectorIcon& FilledCardInformationIconView::GetVectorIcon() const {
  return kCreditCardChromeRefreshIcon;
}

FilledCardInformationBubbleController*
FilledCardInformationIconView::GetController() const {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    return nullptr;
  }

  return FilledCardInformationBubbleController::Get(web_contents);
}

BEGIN_METADATA(FilledCardInformationIconView)
END_METADATA

}  // namespace autofill
