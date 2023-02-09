// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_feedback_data.h"

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/common/autofill_clock.h"

namespace autofill::data_logs {
namespace {
// Time limit within which the last autofill event is considered related to the
// feedback report.
constexpr base::TimeDelta kAutofillEventTimeLimit = base::Minutes(3);

std::string FillDataTypeToStr(FillDataType type) {
  switch (type) {
    case FillDataType::kUndefined:
      return "Undefined";
    case FillDataType::kAutofillProfile:
      return "AutofillProfile";
    case FillDataType::kCreditCard:
      return "CreditCard";
    case FillDataType::kSingleFieldFormFillerAutocomplete:
      return "SingleFieldFormFillerAutocomplete";
    case FillDataType::kSingleFieldFormFillerIban:
      return "SingleFieldFormFillerIban";
    case FillDataType::kSingleFieldFormFillerPromoCode:
      return "SingleFieldFormFillerPromoCode";
  }
}

base::Value::Dict BuildFieldDataLogs(AutofillField* field) {
  base::Value::Dict field_data;
  field_data.Set("field_signature",
                 base::NumberToString(field->GetFieldSignature().value()));
  field_data.Set("host_form_signature",
                 base::NumberToString(field->host_form_signature.value()));
  field_data.Set("id_attribute", field->id_attribute);
  field_data.Set("parseable_name_attribute", field->name_attribute);
  field_data.Set("autocomplete_attribute", field->autocomplete_attribute);
  field_data.Set("label_attribute", field->label);
  field_data.Set("placeholder_attribute", field->placeholder);
  field_data.Set("field_type", field->Type().ToString());
  field_data.Set("heuristic_type",
                 AutofillType(field->heuristic_type()).ToString());
  field_data.Set("server_type", AutofillType(field->server_type()).ToString());
  field_data.Set("server_type_is_override",
                 field->server_type_prediction_is_override());
  field_data.Set("html_type", FieldTypeToStringPiece(field->html_type()));
  field_data.Set("section", field->section.ToString());

  field_data.Set("is_empty", field->IsEmpty());
  field_data.Set("is_focusable", field->IsFocusable());
  field_data.Set("is_visible", field->is_visible);
  return field_data;
}

base::Value::Dict BuildLastAutofillEventLogs(AutofillManager* manager) {
  base::Value::Dict dict;

  FillDataType type = FillDataType::kUndefined;
  std::string associated_country;
  base::Time last_autofill_event_timestamp = base::Time();
  bool had_trigger_event = false;
  for (const auto& [form_id, form] : manager->form_structures()) {
    for (const auto& field : form->fields()) {
      for (const auto& field_log_event : field->field_log_events()) {
        if (const autofill::TriggerFillFieldLogEvent* trigger_event =
                absl::get_if<TriggerFillFieldLogEvent>(&field_log_event)) {
          had_trigger_event = true;
          if (trigger_event->timestamp > last_autofill_event_timestamp) {
            last_autofill_event_timestamp = trigger_event->timestamp;
            type = trigger_event->data_type;
            associated_country = trigger_event->associated_country_code;
          }
        }
      }
    }
  }
  // Only include last autofill event metadata if the event occurred less than
  // `kAutofillEventTimeLimit` minutes ago.
  if (had_trigger_event &&
      (AutofillClock::Now() - last_autofill_event_timestamp) <=
          kAutofillEventTimeLimit) {
    dict.Set("type", FillDataTypeToStr(type));
    dict.Set("associated_country", associated_country);
  }
  return dict;
}
}  // namespace

base::Value::Dict FetchAutofillFeedbackData(AutofillManager* manager) {
  base::Value::Dict dict;
  base::Value::List form_structures;
  form_structures.reserve(manager->form_structures().size());

  for (const auto& [form_id, form] : manager->form_structures()) {
    base::Value::Dict form_data;
    form_data.Set("form_signature",
                  base::NumberToString(form->form_signature().value()));
    form_data.Set("renderer_id",
                  base::NumberToString(form->global_id().renderer_id.value()));
    form_data.Set("host_frame", form->global_id().frame_token.ToString());
    form_data.Set("source_url",
                  url::Origin::Create(form->source_url()).Serialize());
    form_data.Set("main_frame_url", form->main_frame_origin().Serialize());
    form_data.Set("id_attribute", form->id_attribute());
    form_data.Set("name_attribute", form->name_attribute());

    base::Value::List fields;
    fields.reserve(form->fields().size());
    for (const auto& field : form->fields()) {
      fields.Append(BuildFieldDataLogs(field.get()));
    }

    form_data.Set("fields", std::move(fields));
    form_structures.Append(std::move(form_data));
  }

  dict.Set("form_structures", std::move(form_structures));

  base::Value::Dict last_autofill_event_data =
      BuildLastAutofillEventLogs(manager);
  if (!last_autofill_event_data.empty()) {
    dict.Set("last_autofill_event", std::move(last_autofill_event_data));
  }
  return dict;
}

}  // namespace autofill::data_logs
