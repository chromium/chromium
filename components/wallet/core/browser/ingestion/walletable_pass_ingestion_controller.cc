// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/wallet/core/browser/ingestion/walletable_pass_ingestion_controller.h"

#include "base/barrier_closure.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "components/optimization_guide/core/hints/optimization_guide_decider.h"
#include "components/optimization_guide/core/model_execution/remote_model_executor.h"
#include "components/optimization_guide/proto/features/walletable_pass_extraction.pb.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/strike_database/strike_database_base.h"
#include "components/wallet/core/browser/data_models/data_model_utils.h"
#include "components/wallet/core/browser/data_models/wallet_pass.h"
#include "components/wallet/core/browser/ingestion/walletable_pass_client.h"
#include "components/wallet/core/browser/ingestion/walletable_pass_ingestion_utils.h"
#include "components/wallet/core/browser/metrics/wallet_metrics.h"
#include "components/wallet/core/browser/network/wallet_http_client.h"
#include "components/wallet/core/browser/walletable_permission_utils.h"
#include "components/wallet/core/common/wallet_features.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"
#include "url/gurl.h"

namespace wallet {
namespace {

using enum WalletablePassClient::WalletablePassBubbleResult;
using WalletablePassOptInFunnelEvents =
    metrics::WalletablePassOptInFunnelEvents;
using WalletablePassServerExtractionFunnelEvents =
    metrics::WalletablePassServerExtractionFunnelEvents;

optimization_guide::proto::PassCategory ToProtoPassCategory(
    PassCategory pass_category) {
  switch (pass_category) {
    case PassCategory::kLoyaltyCard:
      return optimization_guide::proto::PASS_CATEGORY_LOYALTY_CARD;
    case PassCategory::kEventPass:
      return optimization_guide::proto::PASS_CATEGORY_EVENT_PASS;
    case PassCategory::kTransitTicket:
      return optimization_guide::proto::PASS_CATEGORY_TRANSIT_TICKET;
    case PassCategory::kBoardingPass:
    case PassCategory::kPassport:
    case PassCategory::kDriverLicense:
    case PassCategory::kNationalIdentityCard:
    case PassCategory::kKTN:
    case PassCategory::kRedressNumber:
    case PassCategory::kUnspecified:
      return optimization_guide::proto::PASS_CATEGORY_UNSPECIFIED;
  }
}

std::optional<WalletPass> CreateBoardingPass(const WalletBarcode& barcode) {
  if (std::optional<BoardingPass> boarding_pass =
          BoardingPass::FromBarcode(barcode)) {
    WalletPass pass;
    pass.pass_data = std::move(*boarding_pass);
    return pass;
  }
  return std::nullopt;
}

}  // namespace

WalletablePassIngestionController::ProcessingState::ProcessingState() = default;
WalletablePassIngestionController::ProcessingState::~ProcessingState() =
    default;
WalletablePassIngestionController::ProcessingState::ProcessingState(
    const ProcessingState&) = default;
WalletablePassIngestionController::ProcessingState&
WalletablePassIngestionController::ProcessingState::operator=(
    const ProcessingState&) = default;

WalletablePassIngestionController::WalletablePassIngestionController(
    WalletablePassClient* client)
    : client_(CHECK_DEREF(client)),
      save_strike_db_(std::make_unique<WalletablePassSaveStrikeDatabaseByHost>(
          client->GetStrikeDatabase())),
      consent_strike_db_(std::make_unique<WalletablePassConsentStrikeDatabase>(
          client->GetStrikeDatabase())) {
  RegisterOptimizationTypes();
}

WalletablePassIngestionController::~WalletablePassIngestionController() =
    default;

void WalletablePassIngestionController::RegisterOptimizationTypes() {
  client_->GetOptimizationGuideDecider()->RegisterOptimizationTypes(
      {optimization_guide::proto::WALLETABLE_PASS_DETECTION_LOYALTY_ALLOWLIST,
       optimization_guide::proto::
           WALLETABLE_PASS_DETECTION_BOARDING_PASS_ALLOWLIST,
       optimization_guide::proto::
           WALLETABLE_PASS_DETECTION_EVENT_PASS_ALLOWLIST,
       optimization_guide::proto::
           WALLETABLE_PASS_DETECTION_TRANSIT_TICKET_ALLOWLIST});
}

void WalletablePassIngestionController::StartWalletablePassDetectionFlow(
    const GURL& url) {
  if (!IsEligibleForWalletablePassDetection(client_->GetIdentityManager(),
                                            client_->GetGeoIpCountryCode())) {
    return;
  }

  std::optional<PassCategory> pass_category = GetPassCategoryForURL(url);
  if (!pass_category) {
    return;
  }

  if (GetWalletablePassDetectionOptInStatus(client_->GetPrefService(),
                                            client_->GetIdentityManager())) {
    MaybeStartExtraction(url, *pass_category);
    metrics::LogOptInEvent(
        *pass_category, WalletablePassOptInFunnelEvents::kUserAlreadyOptedIn);
    return;
  }

  ShowConsentBubble(url, *pass_category);
}

void WalletablePassIngestionController::OnBarcodesDetected(
    base::RepeatingClosure barrier,
    std::vector<WalletBarcode> barcodes) {
  processing_state_.detected_barcodes = barcodes;
  barrier.Run();
}

void WalletablePassIngestionController::OnBoardingPassBarcodesDetected(
    const GURL& url,
    std::vector<WalletBarcode> barcodes) {
  for (const auto& barcode : barcodes) {
    // TODO(crbug.com/465616560): Handle multiple barcodes properly.
    std::optional<WalletPass> pass = CreateBoardingPass(barcode);
    if (pass) {
      ShowSaveBubble(url, std::move(*pass));
      return;
    }
  }
  // TODO(crbug.com/465909190): Report UMA for no barcode cases.
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
    return PassCategory::kLoyaltyCard;
  }

  if (client_->GetOptimizationGuideDecider()->CanApplyOptimization(
          url,
          optimization_guide::proto::
              WALLETABLE_PASS_DETECTION_BOARDING_PASS_ALLOWLIST,
          /*optimization_metadata=*/nullptr) ==
      optimization_guide::OptimizationGuideDecision::kTrue) {
    return PassCategory::kBoardingPass;
  }

  if (client_->GetOptimizationGuideDecider()->CanApplyOptimization(
          url,
          optimization_guide::proto::
              WALLETABLE_PASS_DETECTION_EVENT_PASS_ALLOWLIST,
          /*optimization_metadata=*/nullptr) ==
      optimization_guide::OptimizationGuideDecision::kTrue) {
    return PassCategory::kEventPass;
  }

  if (client_->GetOptimizationGuideDecider()->CanApplyOptimization(
          url,
          optimization_guide::proto::
              WALLETABLE_PASS_DETECTION_TRANSIT_TICKET_ALLOWLIST,
          /*optimization_metadata=*/nullptr) ==
      optimization_guide::OptimizationGuideDecision::kTrue) {
    return PassCategory::kTransitTicket;
  }

  return std::nullopt;
}

void WalletablePassIngestionController::ShowConsentBubble(
    const GURL& url,
    PassCategory pass_category) {
  if (consent_strike_db_->ShouldBlockFeature()) {
    metrics::LogOptInEvent(
        pass_category,
        WalletablePassOptInFunnelEvents::kConsentBubbleWasBlockedByStrike);
    return;
  }
  client_->ShowWalletablePassConsentBubble(
      pass_category,
      base::BindOnce(
          &WalletablePassIngestionController::OnGetConsentBubbleResult,
          weak_ptr_factory_.GetWeakPtr(), url, pass_category));
  metrics::LogOptInEvent(
      pass_category, WalletablePassOptInFunnelEvents::kConsentBubbleWasShown);
}

void WalletablePassIngestionController::OnGetConsentBubbleResult(
    const GURL& url,
    PassCategory pass_category,
    WalletablePassClient::WalletablePassBubbleResult result) {
  switch (result) {
    case kAccepted:
      SetWalletablePassDetectionOptInStatus(client_->GetPrefService(),
                                            client_->GetIdentityManager(),
                                            client_->GetGeoIpCountryCode(),
                                            /*opt_in_status=*/true);
      consent_strike_db_->ClearStrikes();
      MaybeStartExtraction(url, pass_category);
      metrics::LogOptInEvent(
          pass_category,
          WalletablePassOptInFunnelEvents::kConsentBubbleWasAccepted);
      break;
    case kDeclined:
      consent_strike_db_->AddStrikes(
          WalletablePassConsentStrikeDatabaseTraits::kMaxStrikeLimit);
      metrics::LogOptInEvent(
          pass_category,
          WalletablePassOptInFunnelEvents::kConsentBubbleWasRejected);
      break;
    case kClosed:
      consent_strike_db_->AddStrikes(
          WalletablePassConsentStrikeDatabaseTraits::kMaxStrikeLimit);
      metrics::LogOptInEvent(
          pass_category,
          WalletablePassOptInFunnelEvents::kConsentBubbleWasClosed);
      break;
    case kLostFocus:
      consent_strike_db_->AddStrike();
      metrics::LogOptInEvent(
          pass_category,
          WalletablePassOptInFunnelEvents::kConsentBubbleLostFocus);
      break;
    case kUnknown:
      consent_strike_db_->AddStrike();
      metrics::LogOptInEvent(
          pass_category,
          WalletablePassOptInFunnelEvents::kConsentBubbleClosedUnknownReason);
      break;
    case kDiscarded:
      consent_strike_db_->AddStrike();
      metrics::LogOptInEvent(
          pass_category,
          WalletablePassOptInFunnelEvents::kConsentBubbleWasDiscarded);
      break;
  }
}

void WalletablePassIngestionController::MaybeStartExtraction(
    const GURL& url,
    PassCategory pass_category) {
  if (save_strike_db_->ShouldBlockFeature(
          WalletablePassSaveStrikeDatabaseByHost::GetId(
              PassCategoryToString(pass_category), url.GetHost()))) {
    metrics::LogServerExtractionEvent(
        pass_category, WalletablePassServerExtractionFunnelEvents::
                           kExtractionBlockedBySaveStrike);
    return;
  }

  // Reset state for new extraction. This invalidates any pending callbacks from
  // previous extraction attempts.
  processing_weak_ptr_factory_.InvalidateWeakPtrs();
  processing_state_ = ProcessingState();

  // For boarding passes, all necessary data can be parsed directly from the
  // barcode, making an LLM call unnecessary.
  if (pass_category == PassCategory::kBoardingPass) {
    DetectBarcodes(base::BindOnce(
        &WalletablePassIngestionController::OnBoardingPassBarcodesDetected,
        processing_weak_ptr_factory_.GetWeakPtr(), url));
    return;
  }

  // Run barcode detection and page annotation extraction in parallel. The
  // barrier waits for both tasks to complete before invoking FinishExtraction
  // to merge results.
  base::RepeatingClosure barrier = base::BarrierClosure(
      2, base::BindOnce(&WalletablePassIngestionController::FinishExtraction,
                        processing_weak_ptr_factory_.GetWeakPtr(), url));

  DetectBarcodes(
      base::BindOnce(&WalletablePassIngestionController::OnBarcodesDetected,
                     processing_weak_ptr_factory_.GetWeakPtr(), barrier));

  GetAnnotatedPageContent(base::BindOnce(
      &WalletablePassIngestionController::OnGetAnnotatedPageContent,
      processing_weak_ptr_factory_.GetWeakPtr(), url, pass_category, barrier));
}

void WalletablePassIngestionController::OnGetAnnotatedPageContent(
    const GURL& url,
    PassCategory pass_category,
    base::RepeatingClosure barrier,
    std::optional<optimization_guide::proto::AnnotatedPageContent>
        annotated_page_content) {
  if (!annotated_page_content) {
    metrics::LogServerExtractionEvent(
        pass_category, WalletablePassServerExtractionFunnelEvents::
                           kGetAnnotatedPageContentFailed);
    barrier.Run();
    return;
  }

  ExtractWalletablePass(url, pass_category, std::move(*annotated_page_content),
                        barrier);
}

void WalletablePassIngestionController::ExtractWalletablePass(
    const GURL& url,
    PassCategory pass_category,
    optimization_guide::proto::AnnotatedPageContent annotated_page_content,
    base::RepeatingClosure barrier) {
  // Construct request
  optimization_guide::proto::WalletablePassExtractionRequest request;
  request.set_pass_category(ToProtoPassCategory(pass_category));
  request.mutable_page_context()->set_url(url.spec());
  request.mutable_page_context()->set_title(GetPageTitle());
  *request.mutable_page_context()->mutable_annotated_page_content() =
      std::move(annotated_page_content);

  client_->GetRemoteModelExecutor()->ExecuteModel(
      optimization_guide::ModelBasedCapabilityKey::kWalletablePassExtraction,
      std::move(request),
      /*options=*/{},
      base::BindOnce(
          &WalletablePassIngestionController::OnExtractWalletablePass,
          processing_weak_ptr_factory_.GetWeakPtr(), pass_category, barrier));
}

void WalletablePassIngestionController::OnExtractWalletablePass(
    PassCategory pass_category,
    base::RepeatingClosure barrier,
    optimization_guide::OptimizationGuideModelExecutionResult result,
    std::unique_ptr<optimization_guide::ModelQualityLogEntry> log_entry) {
  // Handle model execution failure first.
  if (!result.response.has_value()) {
    metrics::LogServerExtractionEvent(
        pass_category,
        WalletablePassServerExtractionFunnelEvents::kModelExecutionFailed);
    barrier.Run();
    return;
  }

  // The execution succeeded, now attempt to parse the response.
  auto parsed_response = optimization_guide::ParsedAnyMetadata<
      optimization_guide::proto::WalletablePassExtractionResponse>(
      *result.response);
  if (!parsed_response) {
    metrics::LogServerExtractionEvent(
        pass_category,
        WalletablePassServerExtractionFunnelEvents::kResponseCannotBeParsed);
    barrier.Run();
    return;
  }

  if (parsed_response->walletable_pass_size() == 0) {
    metrics::LogServerExtractionEvent(
        pass_category,
        WalletablePassServerExtractionFunnelEvents::kNoPassExtracted);
    barrier.Run();
    return;
  }

  if (parsed_response->walletable_pass(0).pass_case() ==
      optimization_guide::proto::WalletablePass::PASS_NOT_SET) {
    metrics::LogServerExtractionEvent(
        pass_category,
        WalletablePassServerExtractionFunnelEvents::kInvalidPassType);
    barrier.Run();
    return;
  }

  processing_state_.extracted_pass =
      ExtractWalletPassFromProto(parsed_response->walletable_pass(0));
  if (!processing_state_.extracted_pass) {
    metrics::LogServerExtractionEvent(
        pass_category, WalletablePassServerExtractionFunnelEvents::
                           kWalletablePassConversionFailed);
    barrier.Run();
    return;
  }
  metrics::LogServerExtractionEvent(
      pass_category,
      WalletablePassServerExtractionFunnelEvents::kExtractionSucceeded);
  barrier.Run();
}

void WalletablePassIngestionController::FinishExtraction(const GURL& url) {
  if (processing_state_.extracted_pass) {
    auto set_barcode = [&](auto& pass) {
      if (!processing_state_.detected_barcodes.empty()) {
        // TODO(crbug.com/465616560): Handle multiple barcodes properly.
        pass.barcode = processing_state_.detected_barcodes[0];
      }
    };
    std::visit(
        absl::Overload([&](LoyaltyCard& pass) { set_barcode(pass); },
                       [&](EventPass& pass) { set_barcode(pass); },
                       [&](BoardingPass& pass) {
                         // TODO(crbug.com/465909190): Report UMA for
                         // unexpected boarding pass from LLM response.
                         // Ideally, boarding pass branch should never
                         // be triggered, because LLM never returns
                         // boarding pass proto.
                       },
                       [&](TransitTicket& pass) { set_barcode(pass); },
                       [&](Passport& pass) {}, [&](DriverLicense& pass) {},
                       [&](NationalIdentityCard& pass) {}, [&](KTN& pass) {},
                       [&](RedressNumber& pass) {}),
        processing_state_.extracted_pass->pass_data);
    ShowSaveBubble(url, std::move(*processing_state_.extracted_pass));
  }
}

void WalletablePassIngestionController::ShowSaveBubble(
    const GURL& url,
    WalletPass walletable_pass) {
  const PassCategory pass_category = walletable_pass.GetPassCategory();

  client_->ShowWalletablePassSaveBubble(
      walletable_pass,
      base::BindOnce(&WalletablePassIngestionController::OnGetSaveBubbleResult,
                     weak_ptr_factory_.GetWeakPtr(), url, walletable_pass));
  metrics::LogSaveEvent(
      pass_category,
      metrics::WalletablePassSaveFunnelEvents::kSaveBubbleWasShown);
}

void WalletablePassIngestionController::OnGetSaveBubbleResult(
    const GURL& url,
    WalletPass walletable_pass,
    WalletablePassClient::WalletablePassBubbleResult result) {
  const PassCategory pass_category = walletable_pass.GetPassCategory();
  const std::string category = PassCategoryToString(pass_category);
  switch (result) {
    case kAccepted:
      if (base::FeatureList::IsEnabled(features::kWalletablePassSave)) {
        // TODO(crbug.com/465616560): Call GetWalletHttpClient::UpsertPublicPass
      }
      save_strike_db_->ClearStrikes(
          WalletablePassSaveStrikeDatabaseByHost::GetId(category,
                                                        url.GetHost()));
      metrics::LogSaveEvent(
          pass_category,
          metrics::WalletablePassSaveFunnelEvents::kSaveBubbleWasAccepted);
      break;
    case kDeclined:
      // Add strikes for cases where user rejects explicitly.
      save_strike_db_->AddStrike(WalletablePassSaveStrikeDatabaseByHost::GetId(
          category, url.GetHost()));
      metrics::LogSaveEvent(
          pass_category,
          metrics::WalletablePassSaveFunnelEvents::kSaveBubbleWasRejected);
      break;
    case kClosed:
      // Add strikes for cases where user rejects explicitly.
      save_strike_db_->AddStrike(WalletablePassSaveStrikeDatabaseByHost::GetId(
          category, url.GetHost()));
      metrics::LogSaveEvent(
          pass_category,
          metrics::WalletablePassSaveFunnelEvents::kSaveBubbleWasClosed);
      break;
    case kLostFocus:
      metrics::LogSaveEvent(
          pass_category,
          metrics::WalletablePassSaveFunnelEvents::kSaveBubbleLostFocus);
      break;
    case kUnknown:
      metrics::LogSaveEvent(pass_category,
                            metrics::WalletablePassSaveFunnelEvents::
                                kSaveBubbleClosedUnknownReason);
      break;
    case kDiscarded:
      metrics::LogSaveEvent(
          pass_category,
          metrics::WalletablePassSaveFunnelEvents::kSaveBubbleWasDiscarded);
      break;
  }
}

void WalletablePassIngestionController::OnPassSaved(
    const GURL& url,
    const base::expected<WalletPass, WalletHttpClient::WalletRequestError>&
        result) {
  // TODO(crbug.com/470178423): Log save success / failure to UMA and cache url.
}

}  // namespace wallet
