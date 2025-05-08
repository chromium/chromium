// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_PAYMENTS_VIEW_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_PAYMENTS_VIEW_UTIL_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "build/branding_buildflags.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/ui/payments/payments_ui_closed_reasons.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/gfx/range/range.h"
#include "ui/views/layout/box_layout_view.h"

class GURL;

namespace views {
class View;
class Widget;
}  // namespace views

namespace autofill {

// Used for providing additional information needed to add links in text labels.
struct TextLinkInfo {
  TextLinkInfo();

  TextLinkInfo(const TextLinkInfo& other);
  TextLinkInfo& operator=(const TextLinkInfo& other);
  TextLinkInfo(TextLinkInfo&& other);
  TextLinkInfo& operator=(TextLinkInfo&& other);

  ~TextLinkInfo();

  gfx::Range offset;
  base::RepeatingCallback<void()> callback;
};

// Gets the user avatar icon if available, or else a placeholder.
ui::ImageModel GetProfileAvatar(const AccountInfo& account_info);

// Defines a title view with a label and an icon, to be used by dialogs
// that need to present the Google or Google Pay logo and custom
// horizontal padding.
class TitleWithIconAfterLabelView : public views::BoxLayoutView {
  METADATA_HEADER(TitleWithIconAfterLabelView, views::BoxLayoutView)

 public:
  enum class Icon {
    // Google Pay icon. The "Pay" portion is recolored for light/dark mode.
    GOOGLE_PAY,
    // Google super G.
    GOOGLE_G,
    // Google Pay logo next to an Affirm logo separated by a vertical line.
    GOOGLE_PAY_AND_AFFIRM,
    // Google Pay logo next to an Afterpay logo separated by a vertical line.
    GOOGLE_PAY_AND_AFTERPAY,
    // Google Pay logo next to an Zip logo separated by a vertical line.
    GOOGLE_PAY_AND_ZIP,
  };

  TitleWithIconAfterLabelView(const std::u16string& window_title,
                              Icon icon_to_show);
  ~TitleWithIconAfterLabelView() override;

 private:
  // views::View:
  gfx::Size GetMinimumSize() const override;
};

PaymentsUiClosedReason GetPaymentsUiClosedReasonFromWidget(
    const views::Widget* widget);

// Creates a view with a legal message.  Along with the legal message lines and
// link callbacks, the user email and the user avatar will be displayed at the
// bottom line of this view if both `user_email` and `user_avatar` are present.
std::unique_ptr<views::View> CreateLegalMessageView(
    const LegalMessageLines& legal_message_lines,
    const std::u16string& user_email,
    const ui::ImageModel& user_avatar,
    base::RepeatingCallback<void(const GURL&)> callback);

// TODO(crbug.com/40197696): Replace all payments' progress bars with this.
// Creates a progress bar with an explanatory text below.
std::unique_ptr<views::View> CreateProgressBarWithTextView(
    const std::u16string& progress_bar_text);

// Creates a view with a left-aligned icon and right-aligned text. This is
// useful for creating rows of text with an accompanying icon in a list. The
// optional `text_link_info` can be used to add a link in the text.
std::unique_ptr<views::View> CreateTextWithIconView(
    const std::u16string& text,
    std::optional<TextLinkInfo> text_link_info,
    const gfx::VectorIcon& icon);

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
// kGooglePayLogoIcon is square overall, despite the drawn portion being a
// rectangular area at the top. CreateTiledImage() will correctly clip it
// whereas setting the icon size would rescale it incorrectly and keep the
// bottom empty portion.
gfx::ImageSkia CreateTiledGooglePayLogo(int width,
                                        int height,
                                        const ui::ColorProvider* provider);
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

// Creates a view containing a label and a textfield. The view is arranged
// vertically with the label positioned above the textfield.
std::unique_ptr<views::View> CreateLabelAndTextfieldView(
    const std::u16string& text);

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_PAYMENTS_VIEW_UTIL_H_
