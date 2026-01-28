// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/core/browser/facilitated_payments_driver.h"

#include <optional>
#include <string>
#include <utility>

#include "base/check_deref.h"
#include "base/feature_list.h"
#include "base/strings/string_view_rust.h"
#include "base/strings/utf_string_conversions.h"
#include "components/facilitated_payments/core/browser/facilitated_payments_client.h"
#include "components/facilitated_payments/core/browser/payment_link_manager.h"
#include "components/facilitated_payments/core/browser/pix_manager.h"
#include "components/facilitated_payments/core/features/features.h"
#include "components/facilitated_payments/core/metrics/facilitated_payments_metrics.h"
#include "components/facilitated_payments/core/validation/pix_code_validator.h"
#include "components/facilitated_payments/core/validation/pix_validator_cxx.rs.h"

namespace payments::facilitated {

namespace {

// Not all possible result variants are mapped to results for metrics: things
// that might result in a lot of false positives from random clipboard text are
// not logged. The most important is to try to capture error cases that might
// indicate unsupported Pix codes.
std::optional<PixCodeRustValidationResult>
ConvertRustPixQrCodeResultToResultForMetrics(PixQrCodeResult type) {
  switch (type) {
    case PixQrCodeResult::Dynamic:
      return PixCodeRustValidationResult::kDynamic;
    case PixQrCodeResult::Static:
      return PixCodeRustValidationResult::kStatic;
    case PixQrCodeResult::NotPaymentCode:
      return std::nullopt;
    case PixQrCodeResult::MissingPayloadFormatIndicator:
      return std::nullopt;
    case PixQrCodeResult::InvalidMerchantPresentedCode:
      return std::nullopt;
    case PixQrCodeResult::MissingGloballyUniqueIdentifier:
      return std::nullopt;
    case PixQrCodeResult::NonPixMerchantPresentedCode:
      return PixCodeRustValidationResult::kNonPixMerchantPresentedCode;
    case PixQrCodeResult::EmptyAdditionalDataFieldTemplate:
      return PixCodeRustValidationResult::kEmptyAdditionalDataFieldTemplate;
    case PixQrCodeResult::NonFinalCrc:
      return PixCodeRustValidationResult::kNonFinalCrc;
    case PixQrCodeResult::UnknownPixCodeType:
      return PixCodeRustValidationResult::kUnknownPixCodeType;
  }
}

}  // namespace

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
    const GURL& main_frame_url,
    const std::optional<GURL>& iframe_url,
    const url::Origin& main_frame_origin,
    const std::u16string& copied_text,
    ukm::SourceId ukm_source_id) {
  std::string copied_text_utf8 = base::UTF16ToUTF8(copied_text);
  // Even if the feature is not enabled, always run the Rust validator to log
  // metrics about whether or not the results agree.
  std::optional<PixCodeRustValidationResult> rust_validation_result =
      ConvertRustPixQrCodeResultToResultForMetrics(
          get_pix_qr_code_type(base::StringViewToRustSlice(copied_text_utf8)));
  if (rust_validation_result) {
    LogPaymentCodeRustValidationResult(*rust_validation_result);
  }
  // When using the C++ validator, perform a fast but incomplete test first,
  // since full C++ validation requires a utility process.
  if (base::FeatureList::IsEnabled(kUseRustPixCodeValidator)) {
    // If the Rust validator is enabled, bail out now if there is nothing
    // interesting to log for metrics.
    if (!rust_validation_result) {
      return;
    }
  } else if (!PixCodeValidator::ContainsPixIdentifier(copied_text_utf8)) {
    return;
  }
  if (!pix_manager_) {
    pix_manager_ = std::make_unique<PixManager>(
        &facilitated_payments_client_.get(), api_client_creator_,
        facilitated_payments_client_->GetOptimizationGuideDecider());
  }
  pix_manager_->OnPixCodeCopiedToClipboard(
      main_frame_url, iframe_url, main_frame_origin, rust_validation_result,
      std::move(copied_text_utf8), ukm_source_id);
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
