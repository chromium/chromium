// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing_hub/sharing_hub_icon_view.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"
#include "chrome/browser/ui/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "chrome/browser/ui/sharing_hub/sharing_hub_bubble_controller.h"
#include "chrome/browser/ui/views/sharing_hub/sharing_hub_bubble_view_impl.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "components/omnibox/browser/vector_icons.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace sharing_hub {

namespace {

bool IsQRCodeDialogOpen(content::WebContents* web_contents) {
  qrcode_generator::QRCodeGeneratorBubbleController* controller =
      qrcode_generator::QRCodeGeneratorBubbleController::Get(web_contents);
  return controller && controller->IsBubbleShown();
}

bool IsSendTabToSelfDialogOpen(content::WebContents* web_contents) {
  send_tab_to_self::SendTabToSelfBubbleController* controller =
      send_tab_to_self::SendTabToSelfBubbleController::
          CreateOrGetFromWebContents(web_contents);
  return controller && controller->IsBubbleShown();
}

}  // namespace

SharingHubIconView::SharingHubIconView(
    CommandUpdater* command_updater,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate)
    : PageActionIconView(command_updater,
                         IDC_SHARING_HUB,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate) {
  SetVisible(false);
}

SharingHubIconView::~SharingHubIconView() = default;

views::BubbleDialogDelegate* SharingHubIconView::GetBubble() const {
  SharingHubBubbleController* controller = GetController();
  if (!controller) {
    return nullptr;
  }

  return static_cast<SharingHubBubbleViewImpl*>(
      controller->sharing_hub_bubble_view());
}

void SharingHubIconView::UpdateImpl() {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    return;
  }

  // |controller| may be nullptr due to lazy initialization.
  SharingHubBubbleController* controller = GetController();
  bool enabled = controller && controller->ShouldOfferOmniboxIcon();

  SetCommandEnabled(enabled);
  SetVisible(enabled);

  if (IsQRCodeDialogOpen(web_contents) ||
      IsSendTabToSelfDialogOpen(web_contents)) {
    SetHighlighted(true);
  }
}

void SharingHubIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {}

const gfx::VectorIcon& SharingHubIconView::GetVectorIcon() const {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  return omnibox::kShareIcon;
#elif defined(OS_MAC)
  return omnibox::kShareMacIcon;
#elif defined(OS_WIN)
  return omnibox::kShareWinIcon;
#else
  return omnibox::kSendIcon;
#endif
}

bool SharingHubIconView::ShouldShowLabel() const {
  return false;
}

std::u16string SharingHubIconView::GetTextForTooltipAndAccessibleName() const {
  return l10n_util::GetStringUTF16(IDS_SHARING_HUB_TOOLTIP);
}

SharingHubBubbleController* SharingHubIconView::GetController() const {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents) {
    return nullptr;
  }
  return SharingHubBubbleController::CreateOrGetFromWebContents(web_contents);
}

BEGIN_METADATA(SharingHubIconView, PageActionIconView)
END_METADATA

}  // namespace sharing_hub
