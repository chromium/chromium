// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_APP_LOADING_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_APP_LOADING_VIEW_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/ui/views/payments/payment_handler_header_view_util.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"
#include "url/gurl.h"

class SkBitmap;

namespace views {
class Label;
}  // namespace views

namespace payments {

// A structured loading view overlay shown during service worker payment app
// invocation.
class PaymentAppLoadingView : public views::View {
  METADATA_HEADER(PaymentAppLoadingView, views::View)

 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kTopViewId);
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCloseButtonId);
  PaymentAppLoadingView(const SkBitmap* icon,
                        const GURL& app_origin,
                        const GURL& top_origin,
                        views::Button::PressedCallback close_callback);

  PaymentAppLoadingView(const PaymentAppLoadingView&) = delete;
  PaymentAppLoadingView& operator=(const PaymentAppLoadingView&) = delete;

  ~PaymentAppLoadingView() override;

  // views::View:
  void OnThemeChanged() override;

  views::Label* loading_message_label_for_testing() const {
    return loading_message_label_;
  }

 private:
  raw_ptr<views::View> header_view_ = nullptr;
  raw_ptr<PaymentHandlerOriginLabel> origin_label_ = nullptr;
  raw_ptr<PaymentHandlerProgressBar> progress_bar_ = nullptr;
  raw_ptr<PaymentHandlerCloseButton> close_button_ = nullptr;
  raw_ptr<views::Label> loading_message_label_ = nullptr;
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_APP_LOADING_VIEW_H_
