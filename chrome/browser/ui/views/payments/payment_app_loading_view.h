// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_APP_LOADING_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_APP_LOADING_VIEW_H_

#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
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

  // Requests the view to hide itself. `on_hidden_callback` is always executed
  // asynchronously. If the loading message has been shown for less than the
  // minimum display duration (500ms), execution of `on_hidden_callback` is
  // delayed until the minimum duration has elapsed.
  void Hide(base::OnceClosure on_hidden_callback);

  views::Label* loading_message_label_for_testing() const {
    return loading_message_label_;
  }
  base::OneShotTimer& delay_message_timer_for_testing() {
    return delay_message_timer_;
  }

 private:
  void ShowLoadingMessage();

  raw_ptr<views::View> header_view_ = nullptr;
  raw_ptr<PaymentHandlerOriginLabel> origin_label_ = nullptr;
  raw_ptr<PaymentHandlerProgressBar> progress_bar_ = nullptr;
  raw_ptr<PaymentHandlerCloseButton> close_button_ = nullptr;
  raw_ptr<views::Label> loading_message_label_ = nullptr;

  base::OneShotTimer delay_message_timer_;
  base::OneShotTimer hide_timer_;
  std::optional<base::TimeTicks> message_shown_time_;
};

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_APP_LOADING_VIEW_H_
