// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_ai/autofill_ai_local_save_notification_view.h"

#include "chrome/browser/ui/autofill/autofill_ai/autofill_ai_import_data_controller.h"
#include "ui/base/metadata/metadata_impl_macros.h"

namespace autofill {

AutofillAiLocalSaveNotificationView::AutofillAiLocalSaveNotificationView(
    views::BubbleAnchor anchor_view,
    content::WebContents* web_contents,
    AutofillAiImportDataController* controller)
    : AutofillLocationBarBubble(anchor_view, web_contents),
      controller_(controller->GetWeakPtr()) {
  // TODO(crbug.com/477845712): Add dialog content.
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
