// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/walletable_pass_ingestion_controller.h"

#include "base/check_deref.h"
#include "components/optimization_guide/core/hints/optimization_guide_decider.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/proto/features/walletable_pass_extraction.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/strike_database/strike_database_base.h"
#include "url/gurl.h"

namespace wallet {
namespace {

using optimization_guide::proto::PassCategory;
using optimization_guide::proto::WalletablePass;
using enum WalletablePassClient::WalletablePassBubbleResult;
using enum optimization_guide::proto::PassCategory;

std::string GetWalletablePassCategory(const WalletablePass& walletable_pass) {
  switch (walletable_pass.pass_case()) {
    case WalletablePass::kLoyaltyCard:
      return "LoyaltyCard";
    case WalletablePass::kEventPass:
      return "EventPass";
    case WalletablePass::PASS_NOT_SET:
      // Should be handled by the caller before this function is invoked.
      NOTREACHED();
  }
}

}  // namespace

WalletablePassIngestionController::WalletablePassIngestionController(
    WalletablePassClient* client)
    : client_(CHECK_DEREF(client)),
      save_strike_db_(std::make_unique<WalletablePassSaveStrikeDatabaseByHost>(
          client->GetStrikeDatabase())) {
  RegisterOptimizationTypes();
}

WalletablePassIngestionController::~WalletablePassIngestionController() =
    default;

void WalletablePassIngestionController::RegisterOptimizationTypes() {
  client_->GetOptimizationGuideDecider()->RegisterOptimizationTypes(
      {optimization_guide::proto::WALLETABLE_PASS_DETECTION_LOYALTY_ALLOWLIST});
}

void WalletablePassIngestionController::StartWalletablePassDetectionFlow(
    const GURL& url) {
  std::optional<PassCategory> pass_category = GetPassCategoryForURL(url);
  if (!pass_category) {
    return;
  }

  // TODO(crbug.com/444148314): Request user consent if not consented yet.
  ShowConsentBubble(url, *pass_category);
}

std::optional<PassCategory>
WalletablePassIngestionController::GetPassCategoryForURL(
    const GURL& url) const {
  if (!url.is_valid() || !url.SchemeIsHTTPOrHTTPS()) {
    return std::nullopt;
  }

  // Check if URL is allowlisted via optimization guide
  if (client_->GetOptimizationGuideDecider()->CanApplyOptimization(
          url,
          optimization_guide::proto::
              WALLETABLE_PASS_DETECTION_LOYALTY_ALLOWLIST,
          /*optimization_metadata=*/nullptr) ==
      optimization_guide::OptimizationGuideDecision::kTrue) {
    return PASS_CATEGORY_LOYALTY_CARD;
  }

  // TODO(crbug.com/455680372): Check more allowlists.
  return std::nullopt;
}

void WalletablePassIngestionController::ShowConsentBubble(
    const GURL& url,
    PassCategory pass_category) {
  // TODO(crbug.com/444147446): Check strike before showing the consent bubble.
  client_->ShowWalletablePassConsentBubble(base::BindOnce(
      &WalletablePassIngestionController::OnGetConsentBubbleResult,
      weak_ptr_factory_.GetWeakPtr(), url, pass_category));
}

void WalletablePassIngestionController::OnGetConsentBubbleResult(
    const GURL& url,
    PassCategory pass_category,
    WalletablePassClient::WalletablePassBubbleResult result) {
  switch (result) {
    case kAccepted:
      // TODO(crbug.com/444148314): Write consent result to local storage
      GetAnnotatedPageContent(base::BindOnce(
          &WalletablePassIngestionController::OnGetAnnotatedPageContent,
          weak_ptr_factory_.GetWeakPtr(), url, pass_category));
      break;
    case kDeclined:
    case kClosed:
      // Add strikes for cases where user rejects explicitly
      // TODO(crbug.com/452779539): Report user rejects explicitly to UMA.
      break;
    case kLostFocus:
    case kUnknown:
      // TODO(crbug.com/452779539): Report other outcomes to UMA.
      break;
  }
}

void WalletablePassIngestionController::OnGetAnnotatedPageContent(
    const GURL& url,
    PassCategory pass_category,
    std::optional<optimization_guide::proto::AnnotatedPageContent>
        annotated_page_content) {
  if (!annotated_page_content) {
    // TODO(crbug.com/441892746): Report getting annotated page content failure
    // to UMA
    return;
  }

  ExtractWalletablePass(url, pass_category, std::move(*annotated_page_content));
}

void WalletablePassIngestionController::ExtractWalletablePass(
    const GURL& url,
    PassCategory pass_category,
    optimization_guide::proto::AnnotatedPageContent annotated_page_content) {
  // Construct request
  optimization_guide::proto::WalletablePassExtractionRequest request;
  request.set_pass_category(pass_category);
  request.mutable_page_context()->set_url(url.spec());
  request.mutable_page_context()->set_title(GetPageTitle());
  *request.mutable_page_context()->mutable_annotated_page_content() =
      std::move(annotated_page_content);

  client_->GetRemoteModelExecutor()->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kWalletablePassExtraction,
      std::move(request),
      /*execution_timeout=*/std::nullopt,
      base::BindOnce(
          &WalletablePassIngestionController::OnExtractWalletablePass,
          weak_ptr_factory_.GetWeakPtr(), url));
}

void WalletablePassIngestionController::OnExtractWalletablePass(
    const GURL& url,
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

  if (parsed_response->walletable_pass(0).pass_case() ==
      WalletablePass::PASS_NOT_SET) {
    // TODO(crbug.com/441892746): Report invalid walletable pass found to UMA
    return;
  }

  auto walletable_pass =
      std::make_unique<WalletablePass>(parsed_response->walletable_pass(0));
  ShowSaveBubble(url, std::move(walletable_pass));
}

void WalletablePassIngestionController::ShowSaveBubble(
    const GURL& url,
    std::unique_ptr<WalletablePass> walletable_pass) {
  const std::string category = GetWalletablePassCategory(*walletable_pass);
  if (save_strike_db_->ShouldBlockFeature(
          WalletablePassSaveStrikeDatabaseByHost::GetId(category,
                                                        url.GetHost()))) {
    // TODO(crbug.com/452779539): Report save bubble blocked to UMA
    return;
  }

  WalletablePass* pass_ptr = walletable_pass.get();
  client_->ShowWalletablePassSaveBubble(
      *pass_ptr,
      base::BindOnce(&WalletablePassIngestionController::OnGetSaveBubbleResult,
                     weak_ptr_factory_.GetWeakPtr(), url,
                     std::move(walletable_pass)));
}

void WalletablePassIngestionController::OnGetSaveBubbleResult(
    const GURL& url,
    std::unique_ptr<WalletablePass> walletable_pass,
    WalletablePassClient::WalletablePassBubbleResult result) {
  const std::string category = GetWalletablePassCategory(*walletable_pass);
  switch (result) {
    case kAccepted:
      // TODO(crbug.com/452579752): Save pass to Wallet.
      save_strike_db_->ClearStrikes(
          WalletablePassSaveStrikeDatabaseByHost::GetId(category,
                                                        url.GetHost()));
      break;
    case kDeclined:
    case kClosed:
      // Add strikes for cases where user rejects explicitly
      save_strike_db_->AddStrike(WalletablePassSaveStrikeDatabaseByHost::GetId(
          category, url.GetHost()));
      // TODO(crbug.com/452779539): Report user rejects explicitly to UMA.
      break;
    case kLostFocus:
    case kUnknown:
      // TODO(crbug.com/452779539): Report other outcomes to UMA.
      break;
  }
}

}  // namespace wallet
