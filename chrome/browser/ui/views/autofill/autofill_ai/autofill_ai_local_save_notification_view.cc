// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_ai/autofill_ai_local_save_notification_view.h"

#include "chrome/browser/ui/autofill/autofill_ai/autofill_ai_import_data_controller.h"
#include "chrome/browser/ui/views/chrome_layout_provider.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/label.h"

namespace autofill {

AutofillAiLocalSaveNotificationView::AutofillAiLocalSaveNotificationView(
    views::BubbleAnchor anchor_view,
    content::WebContents* web_contents,
    AutofillAiImportDataController* controller)
    : AutofillLocationBarBubble(anchor_view, web_contents),
      controller_(controller->GetWeakPtr()) {
  SetTitle(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_LOCAL_SAVE_NOTIFICATION_TITLE));
  SetShowCloseButton(true);
  SetButtons(static_cast<int>(ui::mojom::DialogButton::kOk));
  SetButtonLabel(
      ui::mojom::DialogButton::kOk,
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_AI_LOCAL_SAVE_NOTIFICATION_DISMISS_BUTTON_LABEL));

  set_fixed_width(ChromeLayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH));
  set_margins(ChromeLayoutProvider::Get()->GetDialogInsetsForContentType(
      views::DialogContentType::kText, views::DialogContentType::kText));
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  AddChildView(views::Builder<views::Label>()
                   .SetText(l10n_util::GetStringUTF16(
                       IDS_AUTOFILL_AI_LOCAL_SAVE_NOTIFICATION_DESCRIPTION))
                   .SetTextContext(views::style::CONTEXT_DIALOG_BODY_TEXT)
                   .SetTextStyle(views::style::STYLE_SECONDARY)
                   .SetMultiLine(true)
                   .SetHorizontalAlignment(gfx::ALIGN_LEFT)
                   .Build());
}

AutofillAiLocalSaveNotificationView::~AutofillAiLocalSaveNotificationView() =
    default;

void AutofillAiLocalSaveNotificationView::Hide() {
  CloseBubble();
}

void AutofillAiLocalSaveNotificationView::WindowClosing() {
  if (controller_) {
    controller_->OnBubbleClosed(
        AutofillClient::AutofillAiBubbleResult::kUnknown);
  }
  controller_ = nullptr;
}

BEGIN_METADATA(AutofillAiLocalSaveNotificationView)
END_METADATA

}  // namespace autofill
