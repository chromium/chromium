// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_VIEWS_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_VIEWS_UTIL_H_

#include <memory>
#include <string>

#include "base/strings/string16.h"
#include "third_party/blink/public/mojom/payments/payment_request.mojom.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/text_constants.h"

namespace autofill {
class AutofillProfile;
}

namespace views {
class Background;
class Border;
class ButtonListener;
class ImageView;
class Label;
class View;
}

namespace payments {

class PaymentOptionsProvider;
class PaymentsProfileComparator;
enum class PaymentShippingType;

constexpr int kPaymentRequestRowHorizontalInsets = 16;
constexpr int kPaymentRequestRowVerticalInsets = 8;

// Extra inset relative to the header when a right edge should line up with the
// close button's X rather than its invisible right edge.
constexpr int kPaymentRequestRowExtraRightInset = 8;
constexpr int kPaymentRequestButtonSpacing = 10;

// Dimensions of the dialog itself.
constexpr int kDialogMinWidth = 512;
constexpr int kDialogHeight = 450;

// Fixed width of the amount sections in the payment sheet and the order summary
// sheet, in pixels.
constexpr int kAmountSectionWidth = 96;

enum class PaymentRequestCommonTags {
  BACK_BUTTON_TAG = 0,
  CLOSE_BUTTON_TAG,
  PAY_BUTTON_TAG,
  // This is the max value of tags for controls common to multiple
  // PaymentRequest contexts. Individual screens that handle both common and
  // specific events with tags can start their specific tags at this value.
  PAYMENT_REQUEST_COMMON_TAG_MAX
};

int GetActualDialogWidth();

// Creates and returns a header for all the sheets in the PaymentRequest dialog.
// The header contains an optional back arrow button (if |show_back_arrow| is
// true), a |title| label. |delegate| becomes the delegate for the back and
// close buttons. |background| is applied to |container| and its color is used
// to decide which color to use to paint the arrow.
// +---------------------------+
// | <- | header_content_view  |
// +---------------------------+
void PopulateSheetHeaderView(bool show_back_arrow,
                             std::unique_ptr<views::View> header_content_view,
                             views::ButtonListener* delegate,
                             views::View* container,
                             std::unique_ptr<views::Background> background);

// Returns an instrument image view for the given |img| or |icon_resource_id|
// and wanted |opacity|. Includes a rounded rect border. Callers need to set the
// size of the resulting ImageView. Callers should set a |tooltip_text|.
std::unique_ptr<views::ImageView> CreateAppIconView(
    int icon_resource_id,
    gfx::ImageSkia img,
    const base::string16& tooltip_text,
    float opacity = 1.0f);

std::unique_ptr<views::View> CreateProductLogoFooterView();

// Represents formatting options for each of the different contexts in which an
// Address label may be displayed.
enum class AddressStyleType { SUMMARY, DETAILED };

// Extracts and formats descriptive text from the given |profile| to represent
// the address in the context specified by |type|. The missing information will
// be computed using |comp| and displayed as the last line in an informative
// manner. |enabled| indicates whether the various label lines look enabled or
// disabled. Populates |accessible_content|, which shouldn't be null, with the
// screen reader string representing this section.
std::unique_ptr<views::View> GetShippingAddressLabelWithMissingInfo(
    AddressStyleType type,
    const std::string& locale,
    const autofill::AutofillProfile& profile,
    const PaymentsProfileComparator& comp,
    base::string16* accessible_content,
    bool enabled = true);

// Extracts and formats descriptive text from the given |profile| to represent
// the contact info in the context specified by |type|. Includes/excludes name,
// email, and phone fields according to the respective boolean fields. Populates
// |accessible_content|, which shouldn't be null, with the screen reader string
// representing this section.
std::unique_ptr<views::View> GetContactInfoLabel(
    AddressStyleType type,
    const std::string& locale,
    const autofill::AutofillProfile& profile,
    const PaymentOptionsProvider& options,
    const PaymentsProfileComparator& comp,
    base::string16* accessible_content);

// Creates a views::Border object with |insets| that can paint the gray
// horizontal ruler used as a separator between items in the Payment Request
// dialog.
std::unique_ptr<views::Border> CreatePaymentRequestRowBorder(
    SkColor color,
    const gfx::Insets& insets);

// Creates a label with a bold font.
std::unique_ptr<views::Label> CreateBoldLabel(const base::string16& text);

// Creates a label with a medium-weight font, with appropriate fallbacks for
// platforms that have no medium font, or where a user has configured their
// default font with a heavier weight.
std::unique_ptr<views::Label> CreateMediumLabel(const base::string16& text);

// Creates a label with grey, "hint" text and the provided |alignment|.
std::unique_ptr<views::Label> CreateHintLabel(
    const base::string16& text,
    gfx::HorizontalAlignment alignment = gfx::ALIGN_CENTER);

// Creates a 2 line label containing |shipping_option|'s label and amount. If
// |emphasize_label| is true, the label part will be in medium weight. Populates
// |accessible_content|, which shouldn't be null, with the screen reader string
// representing this section.
std::unique_ptr<views::View> CreateShippingOptionLabel(
    payments::mojom::PaymentShippingOption* shipping_option,
    const base::string16& formatted_amount,
    bool emphasize_label,
    base::string16* accessible_content);

// Creates a warning message when address is not valid or an informational
// message when the user has not selected their shipping address yet. The
// warning icon is displayed only for warning messages.
// ---------------------------------------------
// | Warning icon | Warning message            |
// ---------------------------------------------
std::unique_ptr<views::View> CreateWarningView(const base::string16& message,
                                               bool show_icon);

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_REQUEST_VIEWS_UTIL_H_
