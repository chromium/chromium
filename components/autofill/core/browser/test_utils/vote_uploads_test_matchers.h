// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_UTILS_VOTE_UPLOADS_TEST_MATCHERS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_UTILS_VOTE_UPLOADS_TEST_MATCHERS_H_

#include <initializer_list>
#include <string>

#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/common/signatures.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

// Returns a container matcher that applies `matcher` to the first element of
// the container.
inline auto FirstElementIs(auto matcher) {
  return ::testing::AllOf(
      ::testing::SizeIs(::testing::Gt(0u)),
      ResultOf([](const auto& container) { return container[0]; }, matcher));
}

// Matchers for `AutofillUploadContents`. These are in their own namespace to
// make their names briefer.
namespace upload_contents_matchers {

inline ::testing::Matcher<AutofillUploadContents> AutofillUsedIs(
    bool autofill_used) {
  return ::testing::Property(
      "autofill_used", &AutofillUploadContents::autofill_used, autofill_used);
}

// Creates a matcher for an `AutofillUploadContents`'s form_signature method
// against `form_signature`.
inline ::testing::Matcher<AutofillUploadContents> FormSignatureIs(
    FormSignature form_signature) {
  return ::testing::Property("form_signature",
                             &AutofillUploadContents::form_signature,
                             form_signature.value());
}

// Creates a matcher that matches `matchers` against the fields of an
// `AutofillUploadContents`. It requires that the match (and ordering) is exact.
template <typename... Matchers>
inline ::testing::Matcher<AutofillUploadContents> FieldsAre(
    Matchers... matchers) {
  return ::testing::Property("field_data", &AutofillUploadContents::field_data,
                             ::testing::ElementsAre(matchers...));
}

// Creates a matcher that matches `matchers` against the fields of an
// `AutofillUploadContents`. It requires the fields in the proto contain (i.e.
// are a super set of) the provided `matchers`.
template <typename... Matchers>
  requires(sizeof...(Matchers) > 0)
inline ::testing::Matcher<AutofillUploadContents> FieldsContain(
    Matchers... matchers) {
  return ::testing::Property(
      "field_data", &AutofillUploadContents::field_data,
      ::testing::IsSupersetOf(
          std::initializer_list<
              ::testing::Matcher<AutofillUploadContents::Field>>{matchers...}));
}

inline ::testing::Matcher<AutofillUploadContents> ObservedSubmissionIs(
    bool observed_submission) {
  return ::testing::Property("submission", &AutofillUploadContents::submission,
                             observed_submission);
}

inline ::testing::Matcher<AutofillUploadContents> SubmissionIndicatorEventIs(
    mojom::SubmissionIndicatorEvent event) {
  return ::testing::Property(
      "submission_event", &AutofillUploadContents::submission_event,
      static_cast<AutofillUploadContents::SubmissionIndicatorEvent>(event));
}

// Matchers for `AutofillUploadContents::Field`.
inline ::testing::Matcher<AutofillUploadContents::Field> FieldAutofillTypeIs(
    FieldTypeSet type_set) {
  auto extract_types = [](const AutofillUploadContents::Field& field) {
    FieldTypeSet s;
    for (auto type : field.autofill_type()) {
      s.insert(ToSafeFieldType(type, FieldType::NO_SERVER_DATA));
    }
    return s;
  };
  return ::testing::ResultOf(extract_types, ::testing::Eq(type_set));
}

inline ::testing::Matcher<AutofillUploadContents::Field> FieldSignatureIs(
    FieldSignature signature) {
  return ::testing::Property("signature",
                             &AutofillUploadContents::Field::signature,
                             signature.value());
}

inline ::testing::Matcher<AutofillUploadContents> PasswordLengthIsPositive() {
  return ::testing::Property("password_length",
                             &AutofillUploadContents::password_length,
                             ::testing::Gt(0u));
}

inline ::testing::Matcher<AutofillUploadContents> HasPasswordAttribute() {
  return ::testing::AnyOf(
      ::testing::Property("has_password_has_letter",
                          &AutofillUploadContents::has_password_has_letter,
                          true),
      ::testing::Property(
          "has_password_has_special_symbol",
          &AutofillUploadContents::has_password_has_special_symbol, true));
}

}  // namespace upload_contents_matchers

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_UTILS_VOTE_UPLOADS_TEST_MATCHERS_H_
