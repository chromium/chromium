// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_APP_LOADING_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_APP_LOADING_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/view.h"
#include "url/gurl.h"

class SkBitmap;

namespace payments {

// A structured loading view overlay shown during service worker payment app
// invocation.
class PaymentAppLoadingView : public views::View {
  METADATA_HEADER(PaymentAppLoadingView, views::View)

 public:
  PaymentAppLoadingView(const SkBitmap* icon,
                        const GURL& app_origin,
                        const GURL& top_origin,
                        views::Button::PressedCallback close_callback);

  PaymentAppLoadingView(const PaymentAppLoadingView&) = delete;
  PaymentAppLoadingView& operator=(const PaymentAppLoadingView&) = delete;

  ~PaymentAppLoadingView() override;
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_APP_LOADING_VIEW_H_
