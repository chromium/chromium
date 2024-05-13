// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/android_autofill/browser/android_form_event_logger.h"

#include "base/containers/enum_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"

using base::UmaHistogramBoolean;

namespace autofill {

AndroidFormEventLogger::AndroidFormEventLogger(
    const std::string& form_type_name)
    : form_type_name_(form_type_name) {}

AndroidFormEventLogger::~AndroidFormEventLogger() {
  RecordFunnelAndKeyMetrics();
}

void AndroidFormEventLogger::OnDidParseForm() {
  has_parsed_form_ = true;
}

void AndroidFormEventLogger::OnDidInteractWithAutofillableForm() {
  has_logged_interacted_ = true;
}

void AndroidFormEventLogger::OnDidFillSuggestion() {
  has_logged_suggestion_filled_ = true;
}

void AndroidFormEventLogger::OnWillSubmitForm() {
  if (!has_logged_interacted_) {
    return;
  }
  has_logged_will_submit_ = true;
}

void AndroidFormEventLogger::OnTypedIntoNonFilledField() {
  has_logged_typed_into_non_filled_field_ = true;
}

void AndroidFormEventLogger::OnEditedAutofilledField() {
  has_logged_edited_autofilled_field_ = true;
}

void AndroidFormEventLogger::RecordFunnelAndKeyMetrics() {
  UmaHistogramBoolean("Autofill.WebView.Funnel.ParsedAsType." + form_type_name_,
                      has_parsed_form_);
  // Log chronological funnel.
  if (!has_parsed_form_) {
    return;
  }

  UmaHistogramBoolean(
      "Autofill.WebView.Funnel.InteractionAfterParsedAsType." + form_type_name_,
      has_logged_interacted_);
  if (has_logged_interacted_) {
    UmaHistogramBoolean(
        "Autofill.WebView.Funnel.FillAfterInteraction." + form_type_name_,
        has_logged_suggestion_filled_);
  }
  if (has_logged_interacted_ && has_logged_suggestion_filled_) {
    UmaHistogramBoolean(
        "Autofill.WebView.Funnel.SubmissionAfterFill." + form_type_name_,
        has_logged_will_submit_);
  }
  // Log key success metrics, always preconditioned on a form submission (except
  // for the Autofill.KeyMetrics.FormSubmission metrics which measure whether
  // a submission happens).
  if (has_logged_will_submit_) {
    if (has_logged_suggestion_filled_) {
      // Whether a filled form and submitted form required no fixes to filled
      // fields.
      UmaHistogramBoolean(
          "Autofill.WebView.KeyMetrics.FillingCorrectness." + form_type_name_,
          !has_logged_edited_autofilled_field_);
    }
    // Whether a submitted form was filled.
    UmaHistogramBoolean(
        "Autofill.WebView.KeyMetrics.FillingAssistance." + form_type_name_,
        has_logged_suggestion_filled_);
  }
  if (has_logged_typed_into_non_filled_field_ ||
      has_logged_suggestion_filled_) {
    // Whether a (non-)autofilled form was submitted.
    UmaHistogramBoolean(
        base::StrCat(
            {"Autofill.WebView.KeyMetrics.FormSubmission.",
             (has_logged_suggestion_filled_ ? "Autofilled." : "NotAutofilled."),
             form_type_name_}),
        has_logged_will_submit_);
  }
}

}  // namespace autofill
