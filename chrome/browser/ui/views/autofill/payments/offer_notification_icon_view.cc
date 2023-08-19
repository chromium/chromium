// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/payments/offer_notification_icon_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/ui/autofill/payments/offer_notification_bubble_controller.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/autofill/payments/offer_notification_bubble_views.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/omnibox/browser/omnibox_field_trial.h"
#include "components/strings/grit/components_strings.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/paint_vector_icon.h"

namespace autofill {

OfferNotificationIconView::OfferNotificationIconView(
    CommandUpdater* command_updater,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate)
    : PageActionIconView(command_updater,
                         IDC_OFFERS_AND_REWARDS_FOR_PAGE,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate,
                         "PaymentsOfferNotification") {
  SetAccessibilityProperties(
      /*role*/ absl::nullopt,
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_OFFERS_REMINDER_ICON_TOOLTIP_TEXT));
}

OfferNotificationIconView::~OfferNotificationIconView() = default;

views::BubbleDialogDelegate* OfferNotificationIconView::GetBubble() const {
  OfferNotificationBubbleController* controller = GetController();
  if (!controller)
    return nullptr;

  return static_cast<autofill::OfferNotificationBubbleViews*>(
      controller->GetOfferNotificationBubbleView());
}

void OfferNotificationIconView::UpdateImpl() {
  if (!GetWebContents())
    return;

  // |controller| may be nullptr due to lazy initialization.
  OfferNotificationBubbleController* controller = GetController();

  bool command_enabled =
      SetCommandEnabled(controller && controller->IsIconVisible());
  SetVisible(command_enabled);
}

void OfferNotificationIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {}

const gfx::VectorIcon& OfferNotificationIconView::GetVectorIcon() const {
  return OmniboxFieldTrial::IsChromeRefreshIconsEnabled()
             ? kLocalOfferFlippedRefreshIcon
             : kLocalOfferFlippedIcon;
}

OfferNotificationBubbleController* OfferNotificationIconView::GetController()
    const {
  return OfferNotificationBubbleController::Get(GetWebContents());
}

BEGIN_METADATA(OfferNotificationIconView, PageActionIconView)
END_METADATA

}  // namespace autofill
