// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/autofill_ai/metrics/autofill_ai_ukm_logger.h"

#include <algorithm>
#include <cstdint>
#include <type_traits>

#include "base/check_deref.h"
#include "base/notreached.h"
#include "base/types/cxx23_to_underlying.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/signatures.h"
#include "components/optimization_guide/core/model_quality/model_quality_log_entry.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/optimization_guide/proto/features/forms_classifications.pb.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "services/metrics/public/cpp/ukm_builders.h"

namespace autofill {

namespace {

optimization_guide::proto::AutofillAiEntityType GetEntityType(
    EntityType entity_type) {
  using ProtoType = optimization_guide::proto::AutofillAiEntityType;
  switch (entity_type.name()) {
    case EntityTypeName::kDriversLicense:
      return ProtoType::AUTOFILL_AI_ENTITY_TYPE_DRIVERS_LICENSE;
    case EntityTypeName::kFlightReservation:
      return ProtoType::AUTOFILL_AI_ENTITY_TYPE_FLIGHT_RESERVATION;
    case EntityTypeName::kKnownTravelerNumber:
      return ProtoType::AUTOFILL_AI_ENTITY_TYPE_KNOWN_TRAVELER_NUMBER;
    case EntityTypeName::kNationalIdCard:
      return ProtoType::AUTOFILL_AI_ENTITY_TYPE_NATIONAL_ID_CARD;
    case EntityTypeName::kPassport:
      return ProtoType::AUTOFILL_AI_ENTITY_TYPE_PASSPORT;
    case EntityTypeName::kRedressNumber:
      return ProtoType::AUTOFILL_AI_ENTITY_TYPE_REDRESS_NUMBER;
    case EntityTypeName::kVehicle:
      return ProtoType::AUTOFILL_AI_ENTITY_TYPE_VEHICLE;
  }
  return ProtoType::AUTOFILL_AI_ENTITY_TYPE_UNKNOWN;
}

optimization_guide::proto::FormatStringSource GetFormatStringSource(
    AutofillFormatStringSource format_string_source) {
  switch (format_string_source) {
    case AutofillFormatStringSource::kUnset:
      return optimization_guide::proto::FORMAT_STRING_SOURCE_UNSET;
    case AutofillFormatStringSource::kHeuristics:
      return optimization_guide::proto::FORMAT_STRING_SOURCE_HEURISTICS;
    case AutofillFormatStringSource::kModelResult:
      return optimization_guide::proto::FORMAT_STRING_SOURCE_ML_MODEL;
    case AutofillFormatStringSource::kServer:
      return optimization_guide::proto::FORMAT_STRING_SOURCE_SERVER;
  }
  NOTREACHED();
}

optimization_guide::proto::FormControlType GetFormControlType(
    FormControlType form_control_type) {
  switch (form_control_type) {
    case mojom::FormControlType::kContentEditable:
      return optimization_guide::proto::FORM_CONTROL_TYPE_CONTENT_EDITABLE;
    case mojom::FormControlType::kInputCheckbox:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_CHECKBOX;
    case mojom::FormControlType::kInputEmail:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_EMAIL;
    case mojom::FormControlType::kInputMonth:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_MONTH;
    case mojom::FormControlType::kInputNumber:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_NUMBER;
    case mojom::FormControlType::kInputPassword:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_PASSWORD;
    case mojom::FormControlType::kInputRadio:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_RADIO;
    case mojom::FormControlType::kInputSearch:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_SEARCH;
    case mojom::FormControlType::kInputTelephone:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_TELEPHONE;
    case mojom::FormControlType::kInputText:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_TEXT;
    case mojom::FormControlType::kInputUrl:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_URL;
    case mojom::FormControlType::kSelectOne:
      return optimization_guide::proto::FORM_CONTROL_TYPE_SELECT_ONE;
    case mojom::FormControlType::kTextArea:
      return optimization_guide::proto::FORM_CONTROL_TYPE_TEXT_AREA;
    case mojom::FormControlType::kInputDate:
      return optimization_guide::proto::FORM_CONTROL_TYPE_INPUT_DATE;
  }
  NOTREACHED();
}

optimization_guide::proto::AutofillAiFieldEventType GetFieldEventType(
    AutofillAiUkmLogger::EventType field_event_type) {
  switch (field_event_type) {
    case AutofillAiUkmLogger::EventType::kSuggestionShown:
      return optimization_guide::proto::
          AUTOFILL_AI_FIELD_EVENT_TYPE_SUGGESTION_SHOWN;
    case AutofillAiUkmLogger::EventType::kSuggestionFilled:
      return optimization_guide::proto::
          AUTOFILL_AI_FIELD_EVENT_TYPE_SUGGESTION_FILLED;
    case AutofillAiUkmLogger::EventType::kEditedAutofilledValue:
      return optimization_guide::proto::
          AUTOFILL_AI_FIELD_EVENT_TYPE_EDITED_AUTOFILLED_FIELD;
    case AutofillAiUkmLogger::EventType::kFieldFilled:
      return optimization_guide::proto::
          AUTOFILL_AI_FIELD_EVENT_TYPE_FIELD_FILLED;
  }
  NOTREACHED();
}

optimization_guide::proto::AutofillAiPromptUserDecision GetUserDecision(
    AutofillClient::AutofillAiBubbleClosedReason close_reason) {
  switch (close_reason) {
    case AutofillClient::AutofillAiBubbleClosedReason::kAccepted:
      return optimization_guide::proto::
          AUTOFILL_AI_PROMPT_USER_DECISION_ACCEPTED;
    case AutofillClient::AutofillAiBubbleClosedReason::kCancelled:
    case AutofillClient::AutofillAiBubbleClosedReason::kClosed:
      return optimization_guide::proto::
          AUTOFILL_AI_PROMPT_USER_DECISION_DECLINED;
    case AutofillClient::AutofillAiBubbleClosedReason::kNotInteracted:
    case AutofillClient::AutofillAiBubbleClosedReason::kLostFocus:
    case AutofillClient::AutofillAiBubbleClosedReason::kUnknown:
      return optimization_guide::proto::
          AUTOFILL_AI_PROMPT_USER_DECISION_IGNORED;
  }
  NOTREACHED();
}

optimization_guide::proto::AutofillAiPromptType GetPromptType(
    AutofillClient::AutofillAiImportPromptType prompt_type) {
  switch (prompt_type) {
    case AutofillClient::AutofillAiImportPromptType::kSave:
      return optimization_guide::proto::AUTOFILL_AI_PROMPT_TYPE_SAVE_ENTITY;
    case AutofillClient::AutofillAiImportPromptType::kUpdate:
      return optimization_guide::proto::AUTOFILL_AI_PROMPT_TYPE_UPDATE_ENTITY;
    case AutofillClient::AutofillAiImportPromptType::kMigrate:
      return optimization_guide::proto::AUTOFILL_AI_PROMPT_TYPE_MIGRATE_ON_SAVE;
  }
}

optimization_guide::proto::AutofillAiEntityStorageType GetStorageType(
    EntityInstance::RecordType record_type) {
  switch (record_type) {
    case EntityInstance::RecordType::kLocal:
      return optimization_guide::proto::AUTOFILL_AI_ENTITY_STORAGE_TYPE_LOCAL;
    case EntityInstance::RecordType::kServerWallet:
      return optimization_guide::proto::AUTOFILL_AI_ENTITY_STORAGE_TYPE_WALLET;
  }
}

}  // namespace

AutofillAiUkmLogger::AutofillAiUkmLogger(AutofillClient* client)
    : client_(CHECK_DEREF(client)) {}

AutofillAiUkmLogger::~AutofillAiUkmLogger() = default;

void AutofillAiUkmLogger::LogKeyMetrics(ukm::SourceId ukm_source_id,
                                        const FormStructure& form,
                                        EntityType entity_type,
                                        bool data_to_fill_available,
                                        bool suggestions_shown,
                                        bool suggestion_filled,
                                        bool edited_autofilled_field,
                                        bool opt_in_status) {
  const int autofill_filled_field_count = std::ranges::count_if(
      form, [&](const std::unique_ptr<AutofillField>& field) {
        switch (field->filling_product()) {
          case FillingProduct::kAddress:
          case FillingProduct::kCreditCard:
          case FillingProduct::kMerchantPromoCode:
          case FillingProduct::kIban:
          case FillingProduct::kPassword:
          case FillingProduct::kPlusAddresses:
          case FillingProduct::kAutofillAi:
          case FillingProduct::kLoyaltyCard:
          case FillingProduct::kIdentityCredential:
          case FillingProduct::kOneTimePassword:
            return true;
          case FillingProduct::kAutocomplete:
          case FillingProduct::kCompose:
          case FillingProduct::kDataList:
          case FillingProduct::kPasskey:
          case FillingProduct::kNone:
            return false;
        }
      });
  const int autofill_ai_filled_field_count = std::ranges::count(
      form, FillingProduct::kAutofillAi, &AutofillField::filling_product);

  const bool perfect_filling = IsFormPerfectlyFilled(form.ToFormData());

  if (optimization_guide::ModelQualityLogsUploaderService* uploader_ =
          client_->GetMqlsUploadService();
      uploader_ &&
      MayPerformAutofillAiAction(*client_, AutofillAiAction::kLogToMqls)) {
    // Note that the actual logging of the metric happens when `log_entry` goes
    // out of scope and is destroyed.
    optimization_guide::ModelQualityLogEntry log_entry(uploader_->GetWeakPtr());

    optimization_guide::proto::AutofillAiKeyMetrics* mqls_key_metrics =
        log_entry.log_ai_data_request()
            ->mutable_forms_classifications()
            ->mutable_quality()
            ->mutable_key_metrics();

    mqls_key_metrics->set_domain(
        net::registry_controlled_domains::GetDomainAndRegistry(
            form.main_frame_origin(),
            net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES));
    mqls_key_metrics->set_form_signature(form.form_signature().value());
    mqls_key_metrics->set_form_session_identifier(
        autofill_metrics::FormGlobalIdToHash64Bit(form.global_id()));
    mqls_key_metrics->set_autofill_filled_field_count(
        autofill_filled_field_count);
    mqls_key_metrics->set_autofill_ai_filled_field_count(
        autofill_ai_filled_field_count);
    mqls_key_metrics->set_entity_type(GetEntityType(entity_type));
    mqls_key_metrics->set_filling_readiness(data_to_fill_available);
    mqls_key_metrics->set_filling_assistance(suggestion_filled);
    if (suggestions_shown) {
      mqls_key_metrics->set_filling_acceptance(suggestion_filled);
    }
    if (suggestion_filled) {
      mqls_key_metrics->set_filling_correctness(!edited_autofilled_field);
      mqls_key_metrics->set_perfect_filling(perfect_filling);
    }
  }

  if (!CanLogUkm(ukm_source_id)) {
    return;
  }

  ukm::builders::AutofillAi_KeyMetrics builder(ukm_source_id);
  builder.SetFormSignature(HashFormSignature(form.form_signature()))
      .SetFormSessionIdentifier(
          autofill_metrics::FormGlobalIdToHash64Bit(form.global_id()))
      .SetFillingReadiness(data_to_fill_available)
      .SetFillingAssistance(suggestion_filled)
      .SetOptInStatus(opt_in_status)
      .SetAutofillFilledFieldCount(autofill_filled_field_count)
      .SetAutofillAiFilledFieldCount(autofill_ai_filled_field_count)
      .SetEntityType(base::to_underlying(entity_type.name()));
  if (suggestions_shown) {
    builder.SetFillingAcceptance(suggestion_filled);
  }
  if (suggestion_filled) {
    builder.SetFillingCorrectness(!edited_autofilled_field)
        .SetPerfectFilling(perfect_filling);
  }
  builder.Record(client_->GetUkmRecorder());
}

void AutofillAiUkmLogger::LogImportPromptResult(
    const FormData& form,
    AutofillClient::AutofillAiImportPromptType prompt_type,
    EntityType entity_type,
    EntityInstance::RecordType record_type,
    AutofillClient::AutofillAiBubbleClosedReason close_reason,
    ukm::SourceId ukm_source_id) {
  uint64_t form_session_id =
      autofill_metrics::FormGlobalIdToHash64Bit(form.global_id());
  if (optimization_guide::ModelQualityLogsUploaderService* uploader_ =
          client_->GetMqlsUploadService();
      uploader_ &&
      MayPerformAutofillAiAction(*client_, AutofillAiAction::kLogToMqls)) {
    const std::string domain =
        net::registry_controlled_domains::GetDomainAndRegistry(
            form.main_frame_origin(),
            net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES);
    // Note that the actual logging of the metric happens when `log_entry` goes
    // out of scope and is destroyed. Also note that in this case it is not
    // necessary to check if the user is opted in because it is assumed that all
    // field event types can only occur if the user is opted in for Autofill AI.
    optimization_guide::ModelQualityLogEntry log_entry(
        client_->GetMqlsUploadService()->GetWeakPtr());

    optimization_guide::proto::AutofillAiUserPromptMetrics*
        mqls_user_prompt_event = log_entry.log_ai_data_request()
                                     ->mutable_forms_classifications()
                                     ->mutable_quality()
                                     ->mutable_user_prompt_metrics();

    mqls_user_prompt_event->set_domain(domain);
    mqls_user_prompt_event->set_form_signature(
        CalculateFormSignature(form).value());
    mqls_user_prompt_event->set_form_session_identifier(form_session_id);
    mqls_user_prompt_event->set_storage_type(GetStorageType(record_type));
    mqls_user_prompt_event->set_prompt_type(GetPromptType(prompt_type));
    mqls_user_prompt_event->set_entity_type(GetEntityType(entity_type));
    mqls_user_prompt_event->set_result(GetUserDecision(close_reason));
  }

  if (!CanLogUkm(ukm_source_id)) {
    return;
  }
  ukm::builders::AutofillAi_UserPromptMetrics(ukm_source_id)
      .SetFormSessionIdentifier(form_session_id)
      .SetFormSignature(CalculateFormSignature(form).value())
      .SetEntityType(base::to_underlying(entity_type.name()))
      .SetStorageType(GetStorageType(record_type))
      .SetPromptType(GetPromptType(prompt_type))
      .SetResult(GetUserDecision(close_reason))
      .Record(client_->GetUkmRecorder());
}

void AutofillAiUkmLogger::LogFieldEvent(ukm::SourceId ukm_source_id,
                                        const FormStructure& form,
                                        const AutofillField& field,
                                        EntityType entity_type,
                                        EntityInstance::RecordType record_type,
                                        EventType event_type) {
  const FormSignature form_signature = form.form_signature();
  const uint64_t form_session_identifier =
      autofill_metrics::FormGlobalIdToHash64Bit(form.global_id());
  const int form_event_order = field_event_count_per_form_[form.global_id()]++;
  const uint64_t field_session_identifier =
      autofill_metrics::FieldGlobalIdToHash64Bit(field.global_id());
  const FieldTypeSet field_types = field.Type().GetTypes();
  const FieldTypeSet ai_field_types = field.Type().GetAutofillAiTypes();

  if (optimization_guide::ModelQualityLogsUploaderService* uploader_ =
          client_->GetMqlsUploadService();
      uploader_ &&
      MayPerformAutofillAiAction(*client_, AutofillAiAction::kLogToMqls)) {
    // Note that the actual logging of the metric happens when `log_entry` goes
    // out of scope and is destroyed. Also note that in this case it is not
    // necessary to check if the user is opted in because it is assumed that all
    // field event types can only occur if the user is opted in for Autofill AI.
    optimization_guide::ModelQualityLogEntry log_entry(
        client_->GetMqlsUploadService()->GetWeakPtr());

    optimization_guide::proto::AutofillAiFieldEvent* mqls_field_event =
        log_entry.log_ai_data_request()
            ->mutable_forms_classifications()
            ->mutable_quality()
            ->mutable_field_event();

    mqls_field_event->set_domain(
        net::registry_controlled_domains::GetDomainAndRegistry(
            form.main_frame_origin(),
            net::registry_controlled_domains::EXCLUDE_PRIVATE_REGISTRIES));
    mqls_field_event->set_form_signature(form_signature.value());
    mqls_field_event->set_form_session_identifier(form_session_identifier);
    mqls_field_event->set_form_session_event_order(form_event_order);
    mqls_field_event->set_field_signature(field.GetFieldSignature().value());
    mqls_field_event->set_field_session_identifier(field_session_identifier);
    mqls_field_event->set_field_rank(field.rank());
    mqls_field_event->set_field_rank_in_signature_group(
        field.rank_in_signature_group());
    mqls_field_event->set_field_type(base::to_underlying(
        !field_types.empty() ? *field_types.begin() : UNKNOWN_TYPE));
    mqls_field_event->set_ai_field_type(base::to_underlying(
        !ai_field_types.empty() ? *ai_field_types.begin() : UNKNOWN_TYPE));
    for (FieldType field_type : field_types) {
      mqls_field_event->add_field_types(field_type);
    }
    for (FieldType ai_field_type : ai_field_types) {
      mqls_field_event->add_ai_field_types(ai_field_type);
    }
    mqls_field_event->set_format_string_source(
        GetFormatStringSource(field.format_string_source()));
    mqls_field_event->set_form_control_type(
        GetFormControlType(field.form_control_type()));
    mqls_field_event->set_event_type(GetFieldEventType(event_type));
    mqls_field_event->set_entity_type(GetEntityType(entity_type));
    mqls_field_event->set_storage_type(GetStorageType(record_type));
  }

  if (!CanLogUkm(ukm_source_id)) {
    return;
  }

  auto next_field_type = [&field_types, it = field_types.begin()]() mutable {
    return it != field_types.end() ? std::optional<int64_t>(*it++)
                                   : std::nullopt;
  };
  auto next_ai_field_type = [&ai_field_types,
                             it = ai_field_types.begin()]() mutable {
    return it != ai_field_types.end() ? std::optional<int64_t>(*it++)
                                      : std::nullopt;
  };
  using UkmEvent = ukm::builders::AutofillAi_FieldEvent;
  auto maybe_set = [](UkmEvent& event, auto setter,
                      std::optional<int64_t> value) {
    if (value.has_value()) {
      std::invoke(setter, event, *value);
    }
  };
  UkmEvent e(ukm_source_id);
  e.SetFormSignature(HashFormSignature(form.form_signature()));
  e.SetFormSessionIdentifier(form_session_identifier);
  e.SetFormSessionEventOrder(form_event_order);
  e.SetFieldSignature(HashFieldSignature(field.GetFieldSignature()));
  e.SetFieldSessionIdentifier(field_session_identifier);
  e.SetFormatStringSource(base::to_underlying(field.format_string_source()));
  maybe_set(e, &UkmEvent::SetFieldType, next_field_type());
  maybe_set(e, &UkmEvent::SetFieldType2, next_field_type());
  maybe_set(e, &UkmEvent::SetFieldType3, next_field_type());
  maybe_set(e, &UkmEvent::SetFieldType4, next_field_type());
  maybe_set(e, &UkmEvent::SetAiFieldType, next_ai_field_type());
  maybe_set(e, &UkmEvent::SetAiFieldType2, next_ai_field_type());
  maybe_set(e, &UkmEvent::SetAiFieldType3, next_ai_field_type());
  maybe_set(e, &UkmEvent::SetAiFieldType4, next_ai_field_type());
  e.SetEventType(base::to_underlying(event_type));
  e.SetEntityType(base::to_underlying(entity_type.name()));
  e.Record(client_->GetUkmRecorder());
}

bool AutofillAiUkmLogger::CanLogUkm(ukm::SourceId ukm_source_id) const {
  return client_->GetUkmRecorder() != nullptr &&
         ukm_source_id != ukm::kInvalidSourceId;
}

}  // namespace autofill
