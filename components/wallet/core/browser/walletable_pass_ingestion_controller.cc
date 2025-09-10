// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/walletable_pass_ingestion_controller.h"

#include "base/check_deref.h"
#include "components/optimization_guide/core/hints/optimization_guide_decider.h"
#include "components/optimization_guide/core/optimization_guide_model_executor.h"
#include "components/optimization_guide/proto/features/walletable_pass_extraction.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "url/gurl.h"

namespace wallet {

WalletablePassIngestionController::WalletablePassIngestionController(
    WalletablePassClient* client)
    : client_(CHECK_DEREF(client)) {
  RegisterOptimizationTypes();
}

WalletablePassIngestionController::~WalletablePassIngestionController() =
    default;

void WalletablePassIngestionController::RegisterOptimizationTypes() {
  client_->GetOptimizationGuideDecider()->RegisterOptimizationTypes(
      {optimization_guide::proto::WALLETABLE_PASS_DETECTION_ALLOWLIST});
}

void WalletablePassIngestionController::StartWalletablePassDetectionFlow(
    const GURL& url) {
  if (!IsEligibleForExtraction(url)) {
    return;
  }

  GetAnnotatedPageContent(base::BindOnce(
      &WalletablePassIngestionController::OnGetAnnotatedPageContent,
      weak_ptr_factory_.GetWeakPtr(), url));
}

bool WalletablePassIngestionController::IsEligibleForExtraction(
    const GURL& url) const {
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }

  // Check if URL is allowlisted via optimization guide
  return client_->GetOptimizationGuideDecider()->CanApplyOptimization(
             url,
             optimization_guide::proto::WALLETABLE_PASS_DETECTION_ALLOWLIST,
             /*optimization_metadata=*/nullptr) ==
         optimization_guide::OptimizationGuideDecision::kTrue;
}

void WalletablePassIngestionController::OnGetAnnotatedPageContent(
    const GURL& url,
    std::optional<optimization_guide::proto::AnnotatedPageContent>
        annotated_page_content) {
  if (!annotated_page_content) {
    // TODO(crbug.com/441892746): Report getting annotated page content failure
    // to UMA
    return;
  }

  ExtractWalletablePass(url.spec(), std::move(*annotated_page_content));
}

void WalletablePassIngestionController::ExtractWalletablePass(
    const std::string& url,
    optimization_guide::proto::AnnotatedPageContent annotated_page_content) {
  // Construct request
  optimization_guide::proto::WalletablePassExtractionRequest request;
  request.mutable_page_context()->set_url(url);
  request.mutable_page_context()->set_title(GetPageTitle());
  *request.mutable_page_context()->mutable_annotated_page_content() =
      std::move(annotated_page_content);

  client_->GetOptimizationGuideModelExecutor()->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kWalletablePassExtraction,
      std::move(request),
      /*execution_timeout=*/std::nullopt,
      base::BindOnce(
          &WalletablePassIngestionController::OnExtractWalletablePass,
          weak_ptr_factory_.GetWeakPtr()));
}

void WalletablePassIngestionController::OnExtractWalletablePass(
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  // Handle model execution failure first.
  if (!result.response.has_value()) {
    // TODO(crbug.com/441892746): Report model execution failure to UMA
    return;
  }

  // The execution succeeded, now attempt to parse the response.
  auto parsed_response = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::WalletablePassExtractionResponse>(
      *result.response);
  if (!parsed_response) {
    // TODO(crbug.com/441892746): Report invalid or unparsable response to UMA
    return;
  }

  if (parsed_response->walletable_pass_size() == 0) {
    // TODO(crbug.com/441892746): Report no walletable pass found to UMA
    return;
  }

  // TODO(crbug.com/441830204): Get the first walletable pass and show in UI.
}

}  // namespace wallet
