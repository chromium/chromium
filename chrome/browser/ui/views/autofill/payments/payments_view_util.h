// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_PAYMENTS_VIEW_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_PAYMENTS_VIEW_UTIL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/payments/legal_message_line.h"
#include "components/autofill/core/browser/ui/payments/payments_bubble_closed_reasons.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/layout/table_layout_view.h"
#include "ui/views/view.h"

class GURL;

namespace views {
class Label;
class Throbber;
class Widget;
}  // namespace views

namespace autofill {

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
  };

  TitleWithIconAfterLabelView(const std::u16string& window_title,
                              Icon icon_to_show);
  ~TitleWithIconAfterLabelView() override;

 private:
  // views::View:
  gfx::Size GetMinimumSize() const override;
};

// Defines a view with legal message. This class handles the legal message
// parsing and the links clicking events.
class LegalMessageView : public views::BoxLayoutView {
  METADATA_HEADER(LegalMessageView, views::BoxLayoutView)

 public:
  using LinkClickedCallback = base::RepeatingCallback<void(const GURL&)>;

  // Along with the legal message lines and link callbacks, user email and
  // avatar will be displayed at the bottom line of this view if both
  // `user_email` and `user_avatar` are present.
  LegalMessageView(const LegalMessageLines& legal_message_lines,
                   const std::u16string& user_email,
                   const ui::ImageModel& user_avatar,
                   LinkClickedCallback callback);
  ~LegalMessageView() override;
};

PaymentsBubbleClosedReason GetPaymentsBubbleClosedReasonFromWidget(
    const views::Widget* widget);

// TODO(crbug.com/40197696): Replace all payments' progress bar usages with
// this. Creates a progress bar with an explanatory text below.
class ProgressBarWithTextView : public views::BoxLayoutView {
  METADATA_HEADER(ProgressBarWithTextView, views::BoxLayoutView)

 public:
  explicit ProgressBarWithTextView(const std::u16string& progress_bar_text);
  ~ProgressBarWithTextView() override;

 private:
  // views::View:
  void OnThemeChanged() override;
  void AddedToWidget() override;

  raw_ptr<views::Label> progress_label_ = nullptr;
  raw_ptr<views::Throbber> progress_throbber_ = nullptr;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PAYMENTS_PAYMENTS_VIEW_UTIL_H_
