// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/at_memory_metrics_recorder.h"

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_search_result.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_util.h"
#include "components/autofill/core/common/aliases.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/core/model_quality/model_quality_logs_uploader_service.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace autofill {

namespace {

std::optional<AtMemoryQueryCompletedStatus> GetQueryCompletedStatus(
    const MemorySearchResults& result) {
  switch (result.status) {
    case MemorySearchStatus::kUnsupportedQuery:
      return AtMemoryQueryCompletedStatus::kQueryUnsupported;
    case MemorySearchStatus::kNoConnectionFailure:
      return AtMemoryQueryCompletedStatus::kNetworkError;
    case MemorySearchStatus::kInferenceFailure:
      return AtMemoryQueryCompletedStatus::kInferenceFailure;
    case MemorySearchStatus::kInternalFailure:
      return AtMemoryQueryCompletedStatus::kInternalError;
    case MemorySearchStatus::kFinalResponseSuccess:
      return result.entries.empty()
                 ? AtMemoryQueryCompletedStatus::kNoData
                 : AtMemoryQueryCompletedStatus::kQueryReturnedData;
    case MemorySearchStatus::kPartialResponseSuccess:
      return std::nullopt;
  }

  NOTREACHED();
}

std::string_view FetchPiiSourceToString(
    AtMemoryMetricsRecorder::FetchPiiSource source) {
  switch (source) {
    case AtMemoryMetricsRecorder::FetchPiiSource::kAutofillAi:
      return "AutofillAi";
    case AtMemoryMetricsRecorder::FetchPiiSource::kCreditCard:
      return "CreditCard";
    case AtMemoryMetricsRecorder::FetchPiiSource::kIban:
      return "Iban";
    case AtMemoryMetricsRecorder::FetchPiiSource::kPersonalContext:
      return "PersonalContext";
  }
  NOTREACHED();
}

std::string_view MemoryDataTypeToCategoryString(MemoryDataType type) {
  switch (type) {
    case MemoryDataType::kUnknown:
      return "Unknown";

    case MemoryDataType::kNameFull:
    case MemoryDataType::kAddressFull:
    case MemoryDataType::kAddressStreetAddress:
    case MemoryDataType::kAddressCity:
    case MemoryDataType::kAddressState:
    case MemoryDataType::kAddressZip:
    case MemoryDataType::kAddressCountry:
    case MemoryDataType::kPhone:
    case MemoryDataType::kEmail:
    case MemoryDataType::kCompanyName:
      return "Address";

    case MemoryDataType::kCreditCardNumber:
    case MemoryDataType::kCreditCardExpirationDate:
    case MemoryDataType::kCreditCardSecurityCode:
    case MemoryDataType::kCreditCardNameOnCard:
    case MemoryDataType::kCreditCardNickname:
      return "CreditCard";

    case MemoryDataType::kDriversLicenseName:
    case MemoryDataType::kDriversLicenseState:
    case MemoryDataType::kDriversLicenseNumber:
    case MemoryDataType::kDriversLicenseIssueDate:
    case MemoryDataType::kDriversLicenseExpirationDate:
      return "DriversLicense";

    case MemoryDataType::kFlightReservationFlightNumber:
    case MemoryDataType::kFlightReservationTicketNumber:
    case MemoryDataType::kFlightReservationConfirmationCode:
    case MemoryDataType::kFlightReservationPassengerName:
    case MemoryDataType::kFlightReservationDepartureAirport:
    case MemoryDataType::kFlightReservationArrivalAirport:
    case MemoryDataType::kFlightReservationDepartureDate:
    case MemoryDataType::kFlightReservationArrivalDate:
      return "FlightReservation";

    case MemoryDataType::kIban:
    case MemoryDataType::kIbanNickname:
      return "Iban";

    case MemoryDataType::kKnownTravelerNumberName:
    case MemoryDataType::kKnownTravelerNumberNumber:
    case MemoryDataType::kKnownTravelerNumberExpirationDate:
      return "KnownTravelerNumber";

    case MemoryDataType::kNationalIdCardName:
    case MemoryDataType::kNationalIdCardCountry:
    case MemoryDataType::kNationalIdCardNumber:
    case MemoryDataType::kNationalIdCardIssueDate:
    case MemoryDataType::kNationalIdCardExpirationDate:
      return "NationalIdCard";

    case MemoryDataType::kOrderId:
    case MemoryDataType::kOrderAccount:
    case MemoryDataType::kOrderDate:
    case MemoryDataType::kOrderMerchantName:
    case MemoryDataType::kOrderMerchantDomain:
    case MemoryDataType::kOrderProductNames:
    case MemoryDataType::kOrderGrandTotal:
      return "Order";

    case MemoryDataType::kPassportName:
    case MemoryDataType::kPassportCountry:
    case MemoryDataType::kPassportNumber:
    case MemoryDataType::kPassportIssueDate:
    case MemoryDataType::kPassportExpirationDate:
      return "Passport";

    case MemoryDataType::kRedressNumberName:
    case MemoryDataType::kRedressNumberNumber:
      return "RedressNumber";

    case MemoryDataType::kShipmentTrackingNumber:
    case MemoryDataType::kShipmentAssociatedOrderId:
    case MemoryDataType::kShipmentDeliveryAddress:
    case MemoryDataType::kShipmentDeliveryZipCode:
    case MemoryDataType::kShipmentCarrierName:
    case MemoryDataType::kShipmentCarrierDomain:
    case MemoryDataType::kShipmentEstimatedDeliveryDate:
    case MemoryDataType::kShipmentShippedDate:
      return "Shipment";

    case MemoryDataType::kVehicleMake:
    case MemoryDataType::kVehicleModel:
    case MemoryDataType::kVehicleYear:
    case MemoryDataType::kVehicleOwner:
    case MemoryDataType::kVehiclePlateNumber:
    case MemoryDataType::kVehiclePlateState:
    case MemoryDataType::kVehicleVin:
      return "Vehicle";
  }
  NOTREACHED();
}

// Returns the category string used for histogram metric logging based on the
// types of non-Autofill data present in `result`. Returns "Empty" if there are
// no non-Autofill entries, "MultipleTypes" if entries span multiple categories,
// or the category name if all non-Autofill entries share the same category.
std::string_view GetQueryDatatypeCategory(const MemorySearchResults& result) {
  std::optional<std::string_view> category;
  for (const MemorySearchResult& entry : result.entries) {
    if (std::ranges::contains(entry.sources, MemoryEntrySourceType::kAutofill,
                              &MemoryEntrySource::type)) {
      continue;
    }
    const std::string_view entry_category =
        MemoryDataTypeToCategoryString(entry.type);
    if (!category.has_value()) {
      category = entry_category;
    } else if (*category != entry_category) {
      return "MultipleTypes";
    }
  }
  return category.value_or("Empty");
}

}  // namespace

AtMemoryMetricsRecorder::AtMemoryMetricsRecorder(
    optimization_guide::ModelQualityLogsUploaderService* uploader_service,
    ukm::UkmRecorder* ukm_recorder,
    ukm::SourceId ukm_source_id,
    GURL url,
    std::u16string_view title,
    const FieldGlobalId& field_id,
    FormSignature form_signature,
    FieldSignature field_signature)
    : session_id_token_(base::Token::CreateRandom()),
      url_(std::move(url)),
      title_(title),
      field_id_(field_id),
      form_signature_(form_signature),
      field_signature_(field_signature),
      ukm_recorder_(ukm_recorder),
      ukm_source_id_(ukm_source_id),
      uploader_service_(uploader_service) {}

AtMemoryMetricsRecorder::~AtMemoryMetricsRecorder() {
  // Only log summary metrics if the popup was successfully shown.
  // This avoids polluting the "No Query Submitted" data with cases where the
  // popup was hidden immediately after initialization (e.g., due to focus
  // loss) before the user could see or interact with it.
  if (!source_.has_value()) {
    return;
  }

  AtMemoryUiSessionOutcome session_outcome;
  if (suggestion_filled_in_session_) {
    session_outcome = AtMemoryUiSessionOutcome::kSuggestionFilled;
  } else if (suggestion_accepted_in_session_) {
    session_outcome = AtMemoryUiSessionOutcome::kSuggestionAcceptedNotFilled;
  } else if (query_count_ == 0) {
    session_outcome = AtMemoryUiSessionOutcome::kDismissedBeforeQuery;
  } else if (query_response_count_ == 0) {
    // TODO(crbug.com/535486238): Reconsider calculating this once statefulness
    // is implemented.
    session_outcome = AtMemoryUiSessionOutcome::kDismissedBeforeResults;
  } else if (suggestion_acceptance_.suggestions_received) {
    session_outcome =
        AtMemoryUiSessionOutcome::kDismissedResultsBeforeAcceptance;
  } else {
    session_outcome = AtMemoryUiSessionOutcome::kDismissedEmptyResults;
  }
  base::UmaHistogramEnumeration("Autofill.AtMemory.UiSessionOutcome",
                                session_outcome);

  base::UmaHistogramBoolean("Autofill.AtMemory.QuerySubmitted",
                            query_count_ > 0);
  MaybeLogSuggestionAccepted();
  base::UmaHistogramBoolean("Autofill.AtMemory.SuggestionAcceptedInSession",
                            suggestion_accepted_in_session_);
  if (suggestion_acceptance_.accepted_data_type.has_value()) {
    base::UmaHistogramBoolean("Autofill.AtMemory.SuggestionFilled",
                              suggestion_filled_in_session_);
    if (fetch_pii_.duration && fetch_pii_.source) {
      base::UmaHistogramTimes(
          base::StrCat({"Autofill.AtMemory.Latency.FetchPii.",
                        FetchPiiSourceToString(*fetch_pii_.source)}),
          *fetch_pii_.duration);
    }
  }

  if (CanLogUkm()) {
    MaybeFlushSearchQueryUkm();

    ukm::builders::AtMemory_UiSession(ukm_source_id_)
        .SetFieldSessionIdentifier(
            autofill_metrics::FieldGlobalIdToHash64Bit(field_id_))
        .SetFieldSignature(HashFieldSignature(field_signature_))
        .SetFormSignature(HashFormSignature(form_signature_))
        .SetQuerySubmitted(query_count_ > 0)
        .SetSearchBarDisplayed(std::to_underlying(*source_))
        .SetSuggestionAccepted(suggestion_accepted_in_session_)
        .SetSuggestionFilled(suggestion_filled_in_session_)
        .SetUiSessionId(session_id_token_.low())
        .Record(ukm_recorder_);
  }
}

void AtMemoryMetricsRecorder::OnPopupShown(
    AutofillSuggestionTriggerSource trigger_source,
    base::optional_ref<const AutofillSuggestionDelegate::SuggestionMetadata>
        parent_suggestion_metadata) {
  if (parent_suggestion_metadata.has_value()) {
    if (pending_log_entry_ &&
        !parent_suggestion_metadata->multi_index.empty()) {
      optimization_guide::proto::AtMemoryQuality* quality =
          pending_log_entry_->log_ai_data_request()
              ->mutable_at_memory()
              ->mutable_quality();
      size_t root_index = parent_suggestion_metadata->multi_index[0];
      if (root_index < static_cast<size_t>(quality->suggestions_size())) {
        auto* root_suggestion = quality->mutable_suggestions(root_index);
        root_suggestion->set_action(
            optimization_guide::proto::
                AT_MEMORY_SUGGESTION_ACTION_FLYOUT_MENU_OPENED);
      }
    }
    return;
  }
  if (source_.has_value()) {
    return;
  }

  switch (trigger_source) {
    case AutofillSuggestionTriggerSource::kAtMemoryContextMenu:
      source_ = AutofillMetrics::AtMemoryTriggerSource::kContextMenu;
      break;
    case AutofillSuggestionTriggerSource::kAtMemoryDoubleCtrl:
      source_ = AutofillMetrics::AtMemoryTriggerSource::kDoubleCtrl;
      break;
    case AutofillSuggestionTriggerSource::kAtMemoryKeyboardShortcut:
      source_ = AutofillMetrics::AtMemoryTriggerSource::kKeyboardShortcut;
      break;
    case AutofillSuggestionTriggerSource::kAtMemoryTriggerString:
      source_ = AutofillMetrics::AtMemoryTriggerSource::kTypedTrigger;
      break;
    case AutofillSuggestionTriggerSource::kUnspecified:
    case AutofillSuggestionTriggerSource::kFormControlElementClicked:
    case AutofillSuggestionTriggerSource::kTextareaFocusedWithoutClick:
    case AutofillSuggestionTriggerSource::kContentEditableClicked:
    case AutofillSuggestionTriggerSource::kTextFieldValueChanged:
    case AutofillSuggestionTriggerSource::kTextFieldDidReceiveKeyDown:
    case AutofillSuggestionTriggerSource::kOpenTextDataListChooser:
    case AutofillSuggestionTriggerSource::kPasswordManager:
    case AutofillSuggestionTriggerSource::kiOS:
    case AutofillSuggestionTriggerSource::kManualFallbackPasswords:
    case AutofillSuggestionTriggerSource::kComposeDialogLostFocus:
    case AutofillSuggestionTriggerSource::kComposeDelayedProactiveNudge:
    case AutofillSuggestionTriggerSource::kPasswordManagerProcessedFocusedField:
    case AutofillSuggestionTriggerSource::kProactivePasswordRecovery:
    case AutofillSuggestionTriggerSource::kGlic:
    case AutofillSuggestionTriggerSource::kAtMemoryInactivityNudge:
      // This class should only be used for AtMemory searches.
      NOTREACHED();
  }

  // `source_` is set only when the popup is successfully displayed. This
  // serves as a signal that the user has actually seen the suggestions.
  base::UmaHistogramEnumeration("Autofill.AtMemory.SearchBarDisplayed",
                                *source_);
}

void AtMemoryMetricsRecorder::OnQuerySubmitted(std::u16string_view query) {
  ++query_count_;

  query_to_suggestions_shown_timer_.emplace();

  if (CanLogUkm()) {
    MaybeFlushSearchQueryUkm();
    ukm_search_query_builder_.emplace(ukm_source_id_);
    ukm_search_query_builder_->SetUiSessionId(session_id_token_.low());
    ukm_search_query_builder_->SetUiSessionOrder(query_count_ - 1);
    ukm_search_query_builder_->SetSuggestionAccepted(false);
  }

  if (uploader_service_) {
    pending_log_entry_ =
        std::make_unique<optimization_guide::ModelQualityLogEntry>(
            uploader_service_->GetWeakPtr());
    optimization_guide::proto::AtMemoryQuality* quality =
        pending_log_entry_->log_ai_data_request()
            ->mutable_at_memory()
            ->mutable_quality();
    quality->set_session_id(session_id_token_.ToString());
    quality->set_query(base::UTF16ToUTF8(query));
    quality->set_url(url_.spec());
    quality->set_title(base::UTF16ToUTF8(title_));
    quality->set_form_signature(form_signature_.value());
    quality->set_field_signature(field_signature_.value());
    // Rely on log entry destructor to upload the log entry, so this will flush
    // when a new query comes in or the funnel metrics object gets destroyed.
  }
}

void AtMemoryMetricsRecorder::OnSuggestionAccepted(
    MemoryDataType memory_data_type,
    MemorySourcesBitmask sources_bitmask,
    base::optional_ref<const AutofillSuggestionDelegate::SuggestionMetadata>
        metadata) {
  suggestion_acceptance_.accepted_data_type = memory_data_type;
  suggestion_accepted_in_session_ = true;
  suggestion_acceptance_.accepted_sources_bitmask = sources_bitmask;

  if (ukm_search_query_builder_) {
    ukm_search_query_builder_->SetSuggestionAccepted(true);
    ukm_search_query_builder_->SetAcceptedSuggestionDataType(
        std::to_underlying(memory_data_type));
    ukm_search_query_builder_->SetAcceptedSuggestionDataSources(
        sources_bitmask);
  }

  if (metadata.has_value() && !metadata->multi_index.empty()) {
    base::UmaHistogramSparse("Autofill.AtMemory.AcceptedSuggestionIndex",
                             static_cast<int>(metadata->multi_index[0]));
    const int secondary_index = metadata->multi_index.size() > 1
                                    ? static_cast<int>(metadata->multi_index[1])
                                    : -1;
    base::UmaHistogramSparse(
        "Autofill.AtMemory.AcceptedSuggestionSecondaryIndex", secondary_index);

    if (ukm_search_query_builder_) {
      ukm_search_query_builder_->SetAcceptedSuggestionIndex(
          metadata->multi_index[0]);
      ukm_search_query_builder_->SetAcceptedSuggestionSecondaryIndex(
          secondary_index);
    }

    if (pending_log_entry_) {
      optimization_guide::proto::AtMemoryQuality* quality =
          pending_log_entry_->log_ai_data_request()
              ->mutable_at_memory()
              ->mutable_quality();
      if (metadata->multi_index[0] >=
          static_cast<size_t>(quality->suggestions_size())) {
        // This should never happen, but if it does, we should not crash.
        return;
      }
      auto* accepted_suggestion =
          quality->mutable_suggestions(metadata->multi_index[0]);
      accepted_suggestion->set_action(
          secondary_index == -1
              ? optimization_guide::proto::AT_MEMORY_SUGGESTION_ACTION_ACCEPTED
              : optimization_guide::proto::
                    AT_MEMORY_SUGGESTION_ACTION_FLYOUT_MENU_ATTRIBUTE_ACCEPTED);
    }
  }
}

void AtMemoryMetricsRecorder::OnQueryResponseReceived(
    const MemorySearchResults& result) {
  ++query_response_count_;
  if (std::optional<AtMemoryQueryCompletedStatus> status =
          GetQueryCompletedStatus(result)) {
    base::UmaHistogramEnumeration("Autofill.AtMemory.QueryCompleted", *status);
    if (ukm_search_query_builder_) {
      ukm_search_query_builder_->SetQueryCompleted(std::to_underlying(*status));
    }
  }

  MaybeLogSuggestionAccepted();
  suggestion_acceptance_ = {.suggestions_received = !result.entries.empty()};

  if (!query_to_suggestions_shown_timer_) {
    return;
  }

  base::TimeDelta time_since_query_submitted =
      query_to_suggestions_shown_timer_->Elapsed();
  base::UmaHistogramTimes("Autofill.AtMemory.Latency.Query",
                          time_since_query_submitted);
  base::UmaHistogramTimes(base::StrCat({"Autofill.AtMemory.Latency.Query.",
                                        GetQueryDatatypeCategory(result)}),
                          time_since_query_submitted);
  if (ukm_search_query_builder_) {
    ukm_search_query_builder_->SetQueryLatencyMs(
        time_since_query_submitted.InMilliseconds());
  }
  query_to_suggestions_shown_timer_.reset();

  if (!pending_log_entry_) {
    return;
  }
  if (!result.server_request_id.empty()) {
    pending_log_entry_->set_model_execution_id(result.server_request_id);
  }
  auto* quality = pending_log_entry_->log_ai_data_request()
                      ->mutable_at_memory()
                      ->mutable_quality();
  quality->set_query_submitted_to_suggestions_shown_ms(
      time_since_query_submitted.InMilliseconds());

  for (const auto& suggestion : result.entries) {
    auto* quality_suggestion = quality->add_suggestions();
    bool has_autofill_source = std::ranges::contains(
        suggestion.sources, MemoryEntrySourceType::kAutofill,
        &MemoryEntrySource::type);
    quality_suggestion->set_source(
        has_autofill_source
            ? optimization_guide::proto::AT_MEMORY_SUGGESTION_SOURCE_AUTOFILL
            : optimization_guide::proto::
                  AT_MEMORY_SUGGESTION_SOURCE_CONTEXT_MEMORY_SERVICE);
    quality_suggestion->set_response_results_index(
        suggestion.remote_response_index.value_or(-1));
  }
}

void AtMemoryMetricsRecorder::OnFetchPiiStarted(FetchPiiSource source) {
  fetch_pii_.source = source;
  fetch_pii_.start_time = base::TimeTicks::Now();
  fetch_pii_.duration.reset();
}

void AtMemoryMetricsRecorder::OnFetchPiiCompleted() {
  CHECK(fetch_pii_.start_time);
  fetch_pii_.duration.emplace(base::TimeTicks::Now() - *fetch_pii_.start_time);
}

void AtMemoryMetricsRecorder::OnFetchPersonalContextPiiDataFailed(
    AtMemoryQueryService::SpiiRetrievalFailureReason reason) {
  base::UmaHistogramEnumeration(
      "Autofill.AtMemory.FetchPersonalContextPiiData.FailureReason", reason);
}

void AtMemoryMetricsRecorder::MarkFilled() {
  suggestion_filled_in_session_ = true;
  if (ukm_search_query_builder_) {
    ukm_search_query_builder_->SetSuggestionFilled(true);
  }
}

void AtMemoryMetricsRecorder::MaybeLogSuggestionAccepted() {
  if (suggestion_acceptance_.suggestions_received) {
    base::UmaHistogramBoolean(
        "Autofill.AtMemory.SuggestionAccepted",
        suggestion_acceptance_.accepted_data_type.has_value());
  }
  if (suggestion_acceptance_.accepted_data_type.has_value()) {
    base::UmaHistogramEnumeration(
        "Autofill.AtMemory.AcceptedSuggestionDataType",
        *suggestion_acceptance_.accepted_data_type);
    base::UmaHistogramCounts100("Autofill.AtMemory.QueryCountBeforeAcceptance",
                                query_count_);
  }
  if (suggestion_acceptance_.accepted_sources_bitmask.has_value()) {
    base::UmaHistogramSparse("Autofill.AtMemory.AcceptedSuggestionDataSources",
                             *suggestion_acceptance_.accepted_sources_bitmask);
  }
}

bool AtMemoryMetricsRecorder::CanLogUkm() const {
  return ukm_recorder_ && ukm_source_id_ != ukm::kInvalidSourceId;
}

void AtMemoryMetricsRecorder::MaybeFlushSearchQueryUkm() {
  if (ukm_search_query_builder_) {
    ukm_search_query_builder_->Record(ukm_recorder_);
    ukm_search_query_builder_.reset();
  }
}

}  // namespace autofill
