// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_HANDLER_WEB_FLOW_VIEW_TEST_API_H_
#define CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_HANDLER_WEB_FLOW_VIEW_TEST_API_H_

#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "chrome/browser/ui/views/payments/payment_handler_web_flow_view_controller.h"

class LocationIconView;
class PermissionDashboardView;

namespace payments {

class PaymentHandlerWebFlowViewTestApi {
 public:
  explicit PaymentHandlerWebFlowViewTestApi(
      PaymentHandlerWebFlowViewController* controller)
      : controller_(CHECK_DEREF(controller)) {}
  PaymentHandlerWebFlowViewTestApi(const PaymentHandlerWebFlowViewTestApi&) =
      delete;
  PaymentHandlerWebFlowViewTestApi& operator=(
      const PaymentHandlerWebFlowViewTestApi&) = delete;
  ~PaymentHandlerWebFlowViewTestApi() = default;

  LocationIconView* location_icon_view() {
    return controller_->location_icon_view();
  }

  PermissionDashboardView* permission_dashboard_view() {
    return controller_->permission_dashboard_view();
  }

  bool is_indicator_chip_collapse_timer_running() const {
    return controller_->indicator_chip_collapse_timer_.IsRunning();
  }

  void fire_indicator_chip_collapse_timer() {
    controller_->indicator_chip_collapse_timer_.FireNow();
  }

  bool is_indicator_dismiss_timer_running() const {
    return controller_->indicator_dismiss_timer_.IsRunning();
  }

  void fire_indicator_dismiss_timer() {
    controller_->indicator_dismiss_timer_.FireNow();
  }

  PaymentHandlerWebFlowViewController::IndicatorType indicator_type() const {
    return controller_->indicator_type_;
  }

  PaymentHandlerWebFlowViewController::IndicatorDisplayPhase indicator_phase()
      const {
    return controller_->indicator_phase_;
  }

 private:
  const raw_ref<PaymentHandlerWebFlowViewController> controller_;
};

inline PaymentHandlerWebFlowViewTestApi test_api(
    PaymentHandlerWebFlowViewController* controller) {
  return PaymentHandlerWebFlowViewTestApi(controller);
}

}  // namespace payments

#endif  // CHROME_BROWSER_UI_VIEWS_PAYMENTS_PAYMENT_HANDLER_WEB_FLOW_VIEW_TEST_API_H_
