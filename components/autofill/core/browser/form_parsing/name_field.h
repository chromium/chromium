// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_NAME_FIELD_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_NAME_FIELD_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_parsing/form_field.h"

namespace autofill {

class AutofillScanner;
class LogManager;

// A form field that can parse either a FullNameField or a FirstLastNameField.
class NameField : public FormField {
 public:
  static std::unique_ptr<FormField> Parse(AutofillScanner* scanner,
                                          LogManager* log_manager);

 protected:
  NameField() {}

  void AddClassifications(FieldCandidatesMap* field_candidates) const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(NameFieldTest, FirstMiddleLast);
  FRIEND_TEST_ALL_PREFIXES(NameFieldTest, FirstMiddleLast2);
  FRIEND_TEST_ALL_PREFIXES(NameFieldTest, FirstLast);
  FRIEND_TEST_ALL_PREFIXES(NameFieldTest, FirstLast2);
  FRIEND_TEST_ALL_PREFIXES(NameFieldTest, FirstLastMiddleWithSpaces);
  FRIEND_TEST_ALL_PREFIXES(NameFieldTest, FirstLastEmpty);
  FRIEND_TEST_ALL_PREFIXES(NameFieldTest, FirstMiddleLastEmpty);
  FRIEND_TEST_ALL_PREFIXES(NameFieldTest, MiddleInitial);
  FRIEND_TEST_ALL_PREFIXES(NameFieldTest, MiddleInitialAtEnd);

  DISALLOW_COPY_AND_ASSIGN(NameField);
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FORM_PARSING_NAME_FIELD_H_
