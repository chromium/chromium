// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_PARSING_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_PARSING_TEST_UTILS_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_parsing/address_field.h"
#include "components/autofill/core/browser/form_parsing/autofill_scanner.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/language_code.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class FormFieldTest : public testing::Test {
 public:
  FormFieldTest(const FormFieldTest&) = delete;
  FormFieldTest();
  ~FormFieldTest() override;
  FormFieldTest& operator=(const FormFieldTest&) = delete;

 protected:
  // Add a field with |control_type|, the |name|, the |label| the expected
  // parsed type |expected_type|.
  void AddFormFieldData(std::string control_type,
                        std::string name,
                        std::string label,
                        ServerFieldType expected_type) {
    FormFieldData field_data;
    field_data.form_control_type = control_type;
    field_data.name = base::UTF8ToUTF16(name);
    field_data.label = base::UTF8ToUTF16(label);
    field_data.unique_renderer_id = MakeFieldRendererId();
    list_.push_back(std::make_unique<AutofillField>(field_data));
    expected_classifications_.insert(
        std::make_pair(field_data.unique_renderer_id, expected_type));
  }

  // Convenience wrapper for text control elements.
  void AddTextFormFieldData(std::string name,
                            std::string label,
                            ServerFieldType expected_classification) {
    AddFormFieldData("text", name, label, expected_classification);
  }

  // Apply parsing and verify the expected types.
  // |parsed| indicates if at least one field could be parsed successfully.
  // |page_language| the language to be used for parsing, default empty value
  // means the language is unknown and patterns of all languages are used.
  void ClassifyAndVerify(bool parsed = true,
                         const LanguageCode& page_language = LanguageCode("")) {
    AutofillScanner scanner(list_);
    field_ = Parse(&scanner, page_language);

    if (!parsed) {
      ASSERT_EQ(nullptr, field_.get());
      return;
    }
    ASSERT_NE(nullptr, field_.get());
    field_->AddClassificationsForTesting(&field_candidates_map_);

    for (const std::pair<FieldRendererId, ServerFieldType> it :
         expected_classifications_) {
      ASSERT_TRUE(field_candidates_map_.find(it.first) !=
                  field_candidates_map_.end());
      EXPECT_EQ(it.second, field_candidates_map_[it.first].BestHeuristicType());
    }
  }

  // Apply the parsing with a specific parser.
  virtual std::unique_ptr<FormField> Parse(
      AutofillScanner* scanner,
      const LanguageCode& page_language) = 0;

  FieldRendererId MakeFieldRendererId() {
    return FieldRendererId(++id_counter_);
  }

  std::vector<std::unique_ptr<AutofillField>> list_;
  std::unique_ptr<FormField> field_;
  FieldCandidatesMap field_candidates_map_;
  std::map<FieldRendererId, ServerFieldType> expected_classifications_;

 private:
  uint64_t id_counter_ = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_PARSING_TEST_UTILS_H_
