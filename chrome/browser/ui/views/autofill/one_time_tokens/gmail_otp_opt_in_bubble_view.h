// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_ONE_TIME_TOKENS_GMAIL_OTP_OPT_IN_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_ONE_TIME_TOKENS_GMAIL_OTP_OPT_IN_BUBBLE_VIEW_H_

#include <string>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/autofill/autofill_location_bar_bubble.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace content {
class WebContents;
}  // namespace content

namespace views {
class StyledLabel;
}  // namespace views

namespace autofill {

// Bubble view asking the user to opt in to fetching one-time verification codes
// from Gmail.
class GmailOtpOptInBubbleView : public AutofillLocationBarBubble {
  METADATA_HEADER(GmailOtpOptInBubbleView, AutofillLocationBarBubble)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kTurnOnButtonId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kNoThanksButtonId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCloseButtonId);

  GmailOtpOptInBubbleView(
      views::BubbleAnchor anchor,
      content::WebContents* web_contents,
      const std::u16string& account_email,
      base::RepeatingClosure learn_more_link_callback = base::DoNothing());
  GmailOtpOptInBubbleView(const GmailOtpOptInBubbleView&) = delete;
  GmailOtpOptInBubbleView& operator=(const GmailOtpOptInBubbleView&) = delete;
  ~GmailOtpOptInBubbleView() override;

  // AutofillBubbleBase:
  void Hide() override;

  // views::WidgetDelegate:
  void OnWidgetInitialized() override;

  views::StyledLabel* GetDescriptionLabelForTesting() const {
    return description_label_;
  }

 private:
  raw_ptr<views::StyledLabel> description_label_ = nullptr;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_ONE_TIME_TOKENS_GMAIL_OTP_OPT_IN_BUBBLE_VIEW_H_
