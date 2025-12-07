// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/facilitated_payments_driver.h"

#include <utility>

#include "base/check_deref.h"
#include "base/strings/utf_string_conversions.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/payment_link_manager.h"
#include "components/facilitated_payments/core/browser/pix_manager.h"
#include "components/facilitated_payments/core/validation/pix_code_validator.h"

namespace payments::facilitated {

FacilitatedPaymentsDriver::FacilitatedPaymentsDriver(
    FacilitatedPaymentsClient* client,
    FacilitatedPaymentsApiClientCreator api_client_creator)
    : facilitated_payments_client_(CHECK_DEREF(client)),
      api_client_creator_(std::move(api_client_creator)) {}

FacilitatedPaymentsDriver::~FacilitatedPaymentsDriver() = default;

void FacilitatedPaymentsDriver::DidNavigateToOrAwayFromPage() const {
  if (pix_manager_) {
    pix_manager_->Reset();
  }
  if (payment_link_manager_) {
    payment_link_manager_->Reset();
  }
}

void FacilitatedPaymentsDriver::OnTextCopiedToClipboard(
    const GURL& render_frame_host_url,
    const url::Origin& render_frame_host_origin,
    const std::u16string& copied_text,
    ukm::SourceId ukm_source_id) {
  if (!PixCodeValidator::ContainsPixIdentifier(
          base::UTF16ToUTF8(copied_text))) {
    return;
  }
  if (!pix_manager_) {
    pix_manager_ = std::make_unique<PixManager>(
        &facilitated_payments_client_.get(), api_client_creator_,
        facilitated_payments_client_->GetOptimizationGuideDecider());
  }
  pix_manager_->OnPixCodeCopiedToClipboard(
      render_frame_host_url, render_frame_host_origin,
      base::UTF16ToUTF8(copied_text), ukm_source_id);
}

void FacilitatedPaymentsDriver::TriggerPaymentLinkPushPayment(
    const GURL& payment_link_url,
    const GURL& page_url,
    ukm::SourceId ukm_source_id) {
  if (!payment_link_manager_) {
    payment_link_manager_ = std::make_unique<PaymentLinkManager>(
        &facilitated_payments_client_.get(), api_client_creator_,
        facilitated_payments_client_->GetOptimizationGuideDecider());
  }
  payment_link_manager_->TriggerPaymentLinkPushPayment(payment_link_url,
                                                       page_url, ukm_source_id);
}

void FacilitatedPaymentsDriver::SetPixManagerForTesting(
    std::unique_ptr<PixManager> pix_manager) {
  pix_manager_ = std::move(pix_manager);
}

void FacilitatedPaymentsDriver::SetPaymentLinkManagerForTesting(
    std::unique_ptr<PaymentLinkManager> payment_link_manager) {
  payment_link_manager_ = std::move(payment_link_manager);
}

}  // namespace payments::facilitated
