// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_STRUCTURE_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_STRUCTURE_TEST_API_H_

#include <string>
#include <string_view>

#include "base/containers/contains.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_sectioning_util.h"

namespace autofill {

// Exposes some testing operations for FormStructure.
class FormStructureTestApi {
 public:
  using ShouldBeParsedParams = FormStructure::ShouldBeParsedParams;

  explicit FormStructureTestApi(FormStructure& form_structure)
      : form_structure_(form_structure) {}

  AutofillField& PushField() {
    form_structure_->fields_.push_back(std::make_unique<AutofillField>());
    return *form_structure_->fields_.back();
  }

  [[nodiscard]] bool ShouldBeParsed(ShouldBeParsedParams params = {},
                                    LogManager* log_manager = nullptr) {
    return form_structure_->ShouldBeParsed(params, log_manager);
  }

  // Set the heuristic and server types for each field. The `heuristic_types`
  // and `server_types` vectors must be aligned with the indices of the fields
  // in the form. For each field in `heuristic_types` there must be exactly one
  // `GetActivePatternFile()` prediction and any number of alternative
  // predictions.
  void SetFieldTypes(
      const std::vector<std::vector<std::pair<HeuristicSource, FieldType>>>&
          heuristic_types,
      const std::vector<AutofillQueryResponse::FormSuggestion::FieldSuggestion::
                            FieldPrediction>& server_types);
  void SetFieldTypes(
      const std::vector<std::vector<std::pair<HeuristicSource, FieldType>>>&
          heuristic_types,
      const std::vector<FieldType>& server_types);

  // Set the heuristic and server types for each field. The `heuristic_types`
  // and `server_types` vectors must be aligned with the indices of the fields
  // in the form.
  void SetFieldTypes(const std::vector<FieldType>& heuristic_types,
                     const std::vector<FieldType>& server_types);

  void SetFieldTypes(const std::vector<FieldType>& overall_types) {
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

  void AssignSections() { autofill::AssignSections(form_structure_->fields_); }

  bool phone_rationalized(const Section& section) const {
    return base::Contains(form_structure_->phone_rationalized_, section);
  }

  FieldCandidatesMap ParseFieldTypesWithPatterns(
      ParsingContext& context) const {
    return form_structure_->ParseFieldTypesWithPatterns(context);
  }

  void AssignBestFieldTypes(const FieldCandidatesMap& field_type_map,
                            HeuristicSource heuristic_source) {
    form_structure_->AssignBestFieldTypes(field_type_map, heuristic_source);
  }

 private:
  const raw_ref<FormStructure> form_structure_;
};

inline FormStructureTestApi test_api(FormStructure& form_structure) {
  return FormStructureTestApi(form_structure);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_STRUCTURE_TEST_API_H_
