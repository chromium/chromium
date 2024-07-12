// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/webid_utils.h"

#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/view.h"

namespace webid {
int SelectSingleIdpTitleResourceId(blink::mojom::RpContext rp_context) {
  switch (rp_context) {
    case blink::mojom::RpContext::kSignIn:
      return IDS_ACCOUNT_SELECTION_SHEET_TITLE_EXPLICIT_SIGN_IN;
    case blink::mojom::RpContext::kSignUp:
      return IDS_ACCOUNT_SELECTION_SHEET_TITLE_EXPLICIT_SIGN_UP;
    case blink::mojom::RpContext::kUse:
      return IDS_ACCOUNT_SELECTION_SHEET_TITLE_EXPLICIT_USE;
    case blink::mojom::RpContext::kContinue:
      return IDS_ACCOUNT_SELECTION_SHEET_TITLE_EXPLICIT_CONTINUE;
  }
}

// Returns the title to be shown in the dialog. This does not include the
// subtitle. For screen reader purposes, GetAccessibleTitle() is used instead.
std::u16string GetTitle(const std::u16string& rp_for_display,
                        const std::optional<std::u16string>& idp_title,
                        blink::mojom::RpContext rp_context) {
  std::u16string frame_in_title = rp_for_display;
  return idp_title.has_value()
             ? l10n_util::GetStringFUTF16(
                   SelectSingleIdpTitleResourceId(rp_context), frame_in_title,
                   idp_title.value())
             : l10n_util::GetStringFUTF16(
                   IDS_MULTI_IDP_ACCOUNT_SELECTION_SHEET_TITLE_EXPLICIT,
                   frame_in_title);
}

void SendAccessibilityEvent(views::Widget* widget,
                            std::u16string announcement) {
  if (!widget) {
    return;
  }

  views::View* const root_view = widget->GetRootView();
  root_view->GetViewAccessibility().AnnounceText(announcement);
}
}  // namespace webid
