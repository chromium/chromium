// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/qrcode_generator/qrcode_generator_icon_view.h"

#include "chrome/app/chrome_command_ids.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/browser/sharing_hub/sharing_hub_features.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/qrcode_generator/qrcode_generator_bubble_controller.h"
#include "chrome/browser/ui/views/qrcode_generator/qrcode_generator_bubble.h"
#include "chrome/grit/generated_resources.h"
#include "components/omnibox/browser/omnibox_edit_model.h"
#include "components/omnibox/browser/omnibox_view.h"
#include "content/public/browser/browser_context.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace qrcode_generator {

QRCodeGeneratorIconView::QRCodeGeneratorIconView(
    CommandUpdater* command_updater,
    IconLabelBubbleView::Delegate* icon_label_bubble_delegate,
    PageActionIconView::Delegate* page_action_icon_delegate)
    : PageActionIconView(command_updater,
                         IDC_QRCODE_GENERATOR,
                         icon_label_bubble_delegate,
                         page_action_icon_delegate,
                         "QRCodeGenerator"),
      bubble_requested_(false) {
  SetVisible(false);
  SetLabel(l10n_util::GetStringUTF16(IDS_OMNIBOX_QRCODE_GENERATOR_ICON_LABEL));
}

QRCodeGeneratorIconView::~QRCodeGeneratorIconView() = default;

views::BubbleDialogDelegate* QRCodeGeneratorIconView::GetBubble() const {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return nullptr;

  QRCodeGeneratorBubbleController* bubble_controller =
      QRCodeGeneratorBubbleController::Get(web_contents);
  if (!bubble_controller)
    return nullptr;

  return static_cast<QRCodeGeneratorBubble*>(
      bubble_controller->qrcode_generator_bubble_view());
}

void QRCodeGeneratorIconView::UpdateImpl() {
  content::WebContents* web_contents = GetWebContents();
  if (!web_contents)
    return;

  const OmniboxView* omnibox_view = delegate()->GetOmniboxView();
  if (!omnibox_view)
    return;

  // The bubble is anchored to the sharing hub icon when the sharing hub is
  // enabled, so this icon is no longer required.
  if (sharing_hub::SharingHubOmniboxEnabled(
          web_contents->GetBrowserContext())) {
    SetVisible(false);
    return;
  }

  bool feature_available =
      QRCodeGeneratorBubbleController::IsGeneratorAvailable(
          web_contents->GetLastCommittedURL());

  bool visible = GetBubble() != nullptr ||
                 (feature_available && omnibox_view->model()->has_focus() &&
                  !omnibox_view->model()->user_input_in_progress());

  // Once the bubble has initialized, or focus returned to the omnibox,
  // clear the initializing flag.
  if (visible && bubble_requested_)
    bubble_requested_ = false;

  // If the bubble is in the process of showing, prevent losing the
  // inkdrop or going through a hide/show cycle.
  visible |= bubble_requested_;

  // The icon is cleared on navigations and similar in
  // LocationVarView::Update().
  if (visible)
    SetVisible(true);
}

void QRCodeGeneratorIconView::OnExecuting(
    PageActionIconView::ExecuteSource execute_source) {
  bubble_requested_ = true;
}

const gfx::VectorIcon& QRCodeGeneratorIconView::GetVectorIcon() const {
  return kQrcodeGeneratorIcon;
}

std::u16string QRCodeGeneratorIconView::GetTextForTooltipAndAccessibleName()
    const {
  return l10n_util::GetStringUTF16(IDS_OMNIBOX_QRCODE_GENERATOR_ICON_TOOLTIP);
}

bool QRCodeGeneratorIconView::ShouldShowLabel() const {
  return false;
}

BEGIN_METADATA(QRCodeGeneratorIconView, PageActionIconView)
END_METADATA

}  // namespace qrcode_generator
