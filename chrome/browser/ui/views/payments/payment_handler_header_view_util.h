// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_HANDLER_HEADER_VIEW_UTIL_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_HANDLER_HEADER_VIEW_UTIL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/weak_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/progress_bar.h"
#include "ui/views/view.h"

class SkBitmap;

namespace payments {

// The progress bar used in the Payment Handler UI.
class PaymentHandlerProgressBar : public views::ProgressBar {
  METADATA_HEADER(PaymentHandlerProgressBar, views::ProgressBar)

 public:
  PaymentHandlerProgressBar();
  ~PaymentHandlerProgressBar() override;

  void SetColorBasedOnBackground(SkColor background_color);

  base::WeakPtr<PaymentHandlerProgressBar> GetWeakPtr();

 private:
  base::WeakPtrFactory<PaymentHandlerProgressBar> weak_ptr_factory_{this};
};

// The origin label used in the header of the Payment Handler UI.
class PaymentHandlerOriginLabel : public views::Label {
  METADATA_HEADER(PaymentHandlerOriginLabel, views::Label)

 public:
  PaymentHandlerOriginLabel();
  ~PaymentHandlerOriginLabel() override;

  void SetColorBasedOnBackground(SkColor background_color);

  base::WeakPtr<PaymentHandlerOriginLabel> GetWeakPtr();

 private:
  base::WeakPtrFactory<PaymentHandlerOriginLabel> weak_ptr_factory_{this};
};

// The close ('X') button used in the header of the Payment Handler UI.
class PaymentHandlerCloseButton : public views::ImageButton {
  METADATA_HEADER(PaymentHandlerCloseButton, views::ImageButton)

 public:
  explicit PaymentHandlerCloseButton(
      views::Button::PressedCallback pressed_callback);
  ~PaymentHandlerCloseButton() override;

  void SetColorBasedOnBackground(SkColor background_color);

  base::WeakPtr<PaymentHandlerCloseButton> GetWeakPtr();

 private:
  base::WeakPtrFactory<PaymentHandlerCloseButton> weak_ptr_factory_{this};
};

struct PaymentHandlerHeaderViews {
  base::WeakPtr<PaymentHandlerOriginLabel> origin_label;
  base::WeakPtr<PaymentHandlerCloseButton> close_button;
};

// Populates a header view containing the icon (if available), origin text, and
// the close button.
PaymentHandlerHeaderViews PopulatePaymentHandlerHeaderView(
    views::View* container,
    const SkBitmap* app_icon_bitmap,
    const std::u16string& origin_text,
    views::Button::PressedCallback close_callback);

// Computes and applies header backgrounds and item colors based on theme color.
void SetHeaderColors(views::View* header_view,
                     PaymentHandlerOriginLabel* origin_label,
                     PaymentHandlerProgressBar* progress_bar,
                     PaymentHandlerCloseButton* close_button,
                     std::optional<SkColor> theme_color);

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_HANDLER_HEADER_VIEW_UTIL_H_
