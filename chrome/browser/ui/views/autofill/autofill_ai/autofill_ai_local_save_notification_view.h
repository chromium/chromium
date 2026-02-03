// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_AI_AUTOFILL_AI_LOCAL_SAVE_NOTIFICATION_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_AI_AUTOFILL_AI_LOCAL_SAVE_NOTIFICATION_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/views/autofill/autofill_location_bar_bubble.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class View;
}  // namespace views

namespace autofill {

class AutofillAiImportDataController;

// A bubble that informs the user that their data was saved locally because
// an upload request to the server was unsuccessful.
class AutofillAiLocalSaveNotificationView : public AutofillLocationBarBubble {
  METADATA_HEADER(AutofillAiLocalSaveNotificationView, views::View)

 public:
  AutofillAiLocalSaveNotificationView(
      views::BubbleAnchor anchor_view,
      content::WebContents* web_contents,
      AutofillAiImportDataController* controller);

  AutofillAiLocalSaveNotificationView(
      const AutofillAiLocalSaveNotificationView&) = delete;
  AutofillAiLocalSaveNotificationView& operator=(
      const AutofillAiLocalSaveNotificationView&) = delete;
  ~AutofillAiLocalSaveNotificationView() override;

  // AutofillBubbleBase:
  void Hide() override;

  // LocationBarBubbleDelegateView:
  void WindowClosing() override;

 private:
  base::WeakPtr<AutofillAiImportDataController> controller_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_AI_AUTOFILL_AI_LOCAL_SAVE_NOTIFICATION_VIEW_H_
