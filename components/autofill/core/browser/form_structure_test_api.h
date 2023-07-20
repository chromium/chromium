// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_STRUCTURE_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_STRUCTURE_TEST_API_H_

#include <string>

#include "base/containers/contains.h"
#include "base/memory/raw_ref.h"
#include "base/strings/string_piece.h"
#include "components/autofill/core/browser/form_structure.h"

namespace autofill {

// Exposes some testing operations for FormStructure.
class FormStructureTestApi {
 public:
  using ShouldBeParsedParams = FormStructure::ShouldBeParsedParams;

  static void ParseApiQueryResponse(
      base::StringPiece payload,
      const std::vector<FormStructure*>& forms,
      const std::vector<FormSignature>& queried_form_signatures,
      AutofillMetrics::FormInteractionsUkmLogger* ukm_logger,
      LogManager* log_manager = nullptr) {
    FormStructure::ParseApiQueryResponse(
        payload, forms, queried_form_signatures, ukm_logger, log_manager);
  }

  static void ProcessQueryResponse(
      const AutofillQueryResponse& response,
      const std::vector<FormStructure*>& forms,
      const std::vector<FormSignature>& queried_form_signatures,
      AutofillMetrics::FormInteractionsUkmLogger* form_interactions_ukm_logger,
      LogManager* log_manager = nullptr) {
    FormStructure::ProcessQueryResponse(
        response, forms, queried_form_signatures, form_interactions_ukm_logger,
        log_manager);
  }

  explicit FormStructureTestApi(FormStructure* form_structure)
      : form_structure_(*form_structure) {}

  [[nodiscard]] bool ShouldBeParsed(ShouldBeParsedParams params = {},
                                    LogManager* log_manager = nullptr) {
    return form_structure_->ShouldBeParsed(params, log_manager);
  }

  // Set the heuristic and server types for each field. The `heuristic_types`
  // and `server_types` vectors must be aligned with the indices of the fields
  // in the form. For each field in `heuristic_types` there must be exactly one
  // `GetActivePatternSource()` prediction and any number of alternative
  // predictions.
  void SetFieldTypes(
      const std::vector<std::vector<std::pair<PatternSource, ServerFieldType>>>&
          heuristic_types,
      const std::vector<AutofillQueryResponse::FormSuggestion::FieldSuggestion::
                            FieldPrediction>& server_types);
  void SetFieldTypes(
      const std::vector<std::vector<std::pair<PatternSource, ServerFieldType>>>&
          heuristic_types,
      const std::vector<ServerFieldType>& server_types);

  // Set the heuristic and server types for each field. The `heuristic_types`
  // and `server_types` vectors must be aligned with the indices of the fields
  // in the form.
  void SetFieldTypes(const std::vector<ServerFieldType>& heuristic_types,
                     const std::vector<ServerFieldType>& server_types);

  void SetFieldTypes(const std::vector<ServerFieldType>& overall_types) {
    SetFieldTypes(/*heuristic_types=*/overall_types,
                  /*server_types=*/overall_types);
  }

  mojom::SubmissionIndicatorEvent get_submission_event() const {
    return form_structure_->submission_event_;
  }

  // Returns a vote type if a field contains a vote relating USERNAME correction
  // (CREDENTIALS_REUSED, USERNAME_OVERWRITTEN, USERNAME_EDITED). If none,
  // returns NO_INFORMATION.
  AutofillUploadContents::Field::VoteType get_username_vote_type();

  void IdentifySections(bool ignore_autocomplete) {
    form_structure_->IdentifySections(ignore_autocomplete);
  }

  bool phone_rationalized(const Section& section) const {
    return base::Contains(form_structure_->phone_rationalized_, section);
  }

  void ParseFieldTypesWithPatterns(PatternSource pattern_source) {
    return form_structure_->ParseFieldTypesWithPatterns(pattern_source,
                                                        nullptr);
  }

 private:
  const raw_ref<FormStructure> form_structure_;
};

inline FormStructureTestApi test_api(FormStructure* form_structure) {
  return FormStructureTestApi(form_structure);
}

inline FormStructureTestApi test_api(FormStructure& form_structure) {
  return FormStructureTestApi(&form_structure);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_STRUCTURE_TEST_API_H_
