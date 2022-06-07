// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_STRUCTURE_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_STRUCTURE_TEST_API_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/strings/string_piece.h"
#include "components/autofill/core/browser/form_structure.h"

namespace autofill {

// Exposes some testing operations for FormStructure.
class FormStructureTestApi {
 public:
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
      : form_structure_(form_structure) {
    DCHECK(form_structure_);
  }

  const std::vector<std::unique_ptr<AutofillField>>& fields() {
    return form_structure_->fields_;
  }

  void IdentifySections(bool has_author_specified_sections) {
    form_structure_->IdentifySections(has_author_specified_sections);
  }

  bool phone_rationalized(const std::string& section) const {
    auto it = form_structure_->phone_rationalized_.find(section);
    return it != form_structure_->phone_rationalized_.end() && it->second;
  }

  void ParseFieldTypesWithPatterns(PatternSource pattern_source) {
    return form_structure_->ParseFieldTypesWithPatterns(pattern_source,
                                                        nullptr);
  }

 private:
  raw_ptr<FormStructure> form_structure_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_STRUCTURE_TEST_API_H_
