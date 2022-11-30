// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_SEARCH_FIELD_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_SEARCH_FIELD_H_

#include <memory>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "components/autofill/core/browser/form_parsing/form_field.h"
#include "components/autofill/core/common/language_code.h"

namespace autofill {

class AutofillField;
class AutofillScanner;
class LogManager;

// Search fields are not filled by autofill, but identifying them will help
// to reduce the number of false positives.
class SearchField : public FormField {
 public:
  static std::unique_ptr<FormField> Parse(AutofillScanner* scanner,
                                          const LanguageCode& page_language,
                                          PatternSource pattern_source,
                                          LogManager* log_manager);
  SearchField(const AutofillField* field);

  SearchField(const SearchField&) = delete;
  SearchField& operator=(const SearchField&) = delete;

 protected:
  void AddClassifications(FieldCandidatesMap& field_candidates) const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(SearchFieldTest, ParseSearchTerm);
  FRIEND_TEST_ALL_PREFIXES(SearchFieldTest, ParseNonSearchTerm);

  raw_ptr<const AutofillField> field_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_SEARCH_FIELD_H_
