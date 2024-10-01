// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_TEST_UTILS_H_

#include <string_view>
#include <vector>

#include "base/location.h"
#include "base/test/scoped_feature_list.h"
#include "base/types/strong_alias.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class FormFieldData;

namespace test {

// The AutofillTestEnvironment encapsulates global state for test data that
// should be reset automatically after each test.
class AutofillTestEnvironment {
 public:
  struct Options {
    bool disable_server_communication = true;
  };

  static AutofillTestEnvironment& GetCurrent(const base::Location& = FROM_HERE);

  AutofillTestEnvironment(const AutofillTestEnvironment&) = delete;
  AutofillTestEnvironment& operator=(const AutofillTestEnvironment&) = delete;
  ~AutofillTestEnvironment();

  LocalFrameToken NextLocalFrameToken();
  RemoteFrameToken NextRemoteFrameToken();
  FormRendererId NextFormRendererId();
  FieldRendererId NextFieldRendererId();

 protected:
  explicit AutofillTestEnvironment(const Options& options = {
                                       .disable_server_communication = false});

 private:
  static AutofillTestEnvironment* current_instance_;
  base::test::ScopedFeatureList scoped_feature_list_;

  // Use some distinct 64 bit numbers to start the counters.
  uint64_t local_frame_token_counter_high_ = 0xAAAAAAAAAAAAAAAA;
  uint64_t local_frame_token_counter_low_ = 0xBBBBBBBBBBBBBBBB;
  uint64_t remote_frame_token_counter_high_ = 0xBBBBBBBBBBBBBBBB;
  uint64_t remote_frame_token_counter_low_ = 0xAAAAAAAAAAAAAAAA;
  FormRendererId::underlying_type form_renderer_id_counter_ = 10;
  FieldRendererId::underlying_type field_renderer_id_counter_ = 10;
};

// This encapsulates global unittest state. By default this environment
// enables the `kAutofillServerCommunication` feature.
class AutofillUnitTestEnvironment : public AutofillTestEnvironment {
 public:
  explicit AutofillUnitTestEnvironment(
      const Options& options = {.disable_server_communication = false});
};

// This encapsulates global browsertest state. By default this environment
// disables the `kAutofillServerCommunication` feature.
class AutofillBrowserTestEnvironment : public AutofillTestEnvironment {
 public:
  explicit AutofillBrowserTestEnvironment(
      const Options& options = {.disable_server_communication = true});
};

using RandomizeFrame = base::StrongAlias<struct RandomizeFrameTag, bool>;

// Creates non-empty LocalFrameToken.
//
// If `randomize` is true, the LocalFrameToken changes for successive calls.
// Within each unit test, the generated values are deterministically predictable
// (because the test's AutofillTestEnvironment restarts the generation).
//
// If `randomize` is false, the LocalFrameToken is stable across multiple calls.
LocalFrameToken MakeLocalFrameToken(
    RandomizeFrame randomize = RandomizeFrame(true));

// Creates non-empty RemoteFrameToken.
//
// If `randomize` is true, the RemoteFrameToken changes for successive calls.
// Within each unit test, the generated values are deterministically predictable
// (because the test's AutofillTestEnvironment restarts the generation).
//
// If `randomize` is false, the RemoteFrameToken is stable across multiple
// calls.
RemoteFrameToken MakeRemoteFrameToken(
    RandomizeFrame randomize = RandomizeFrame(true));

// Creates new, pairwise distinct FormRendererIds.
inline FormRendererId MakeFormRendererId() {
  return AutofillTestEnvironment::GetCurrent().NextFormRendererId();
}

// Creates new, pairwise distinct FieldRendererIds.
inline FieldRendererId MakeFieldRendererId() {
  return AutofillTestEnvironment::GetCurrent().NextFieldRendererId();
}

// Creates new, pairwise distinct FormGlobalIds. If `randomize` is true, the
// LocalFrameToken is generated randomly, otherwise it is stable across multiple
// calls.
inline FormGlobalId MakeFormGlobalId(
    RandomizeFrame randomize = RandomizeFrame(true)) {
  return {MakeLocalFrameToken(randomize), MakeFormRendererId()};
}

// Creates new, pairwise distinct FieldGlobalIds. If `randomize` is true, the
// LocalFrameToken is generated randomly, otherwise it is stable.
inline FieldGlobalId MakeFieldGlobalId(
    RandomizeFrame randomize = RandomizeFrame(true)) {
  return {MakeLocalFrameToken(randomize), MakeFieldRendererId()};
}

// Returns a copy of `form` in which the host frame of its and its fields is
// set to `frame_token`.
FormData CreateFormDataForFrame(FormData form, LocalFrameToken frame_token);

// Returns a copy of `form` with cleared values.
FormData WithoutValues(FormData form);

// Returns a copy of `form` with `is_autofilled` set as specified.
FormData AsAutofilled(FormData form, bool is_autofilled = true);

// Strips those members from `form` and `field` that are not serialized via
// mojo, i.e., resets them to `{}`.
FormData WithoutUnserializedData(FormData form);
FormFieldData WithoutUnserializedData(FormFieldData field);

// A valid France IBAN number.
inline constexpr char kIbanValue[] = "FR76 3000 6000 0112 3456 7890 189";
inline constexpr char16_t kIbanValue16[] = u"FR76 3000 6000 0112 3456 7890 189";
// Two valid Switzerland IBAN numbers.
inline constexpr char kIbanValue_1[] = "CH56 0483 5012 3456 7800 9";
inline constexpr char kIbanValue_2[] = "CH93 0076 2011 6238 5295 7";

// Provides a quick way to populate a `FormFieldData`.
[[nodiscard]] FormFieldData CreateTestFormField(std::string_view label,
                                                std::string_view name,
                                                std::string_view value,
                                                FormControlType type);

[[nodiscard]] FormFieldData CreateTestFormField(std::string_view label,
                                                std::string_view name,
                                                std::string_view value,
                                                FormControlType type,
                                                std::string_view autocomplete);
[[nodiscard]] FormFieldData CreateTestFormField(std::string_view label,
                                                std::string_view name,
                                                std::string_view value,
                                                FormControlType type,
                                                std::string_view autocomplete,
                                                uint64_t max_length);

// Provides a quick way to populate a select field.
[[nodiscard]] FormFieldData CreateTestSelectField(
    std::string_view label,
    std::string_view name,
    std::string_view value,
    const std::vector<const char*>& values,
    const std::vector<const char*>& contents);

[[nodiscard]] FormFieldData CreateTestSelectField(
    std::string_view label,
    std::string_view name,
    std::string_view value,
    std::string_view autocomplete,
    const std::vector<const char*>& values,
    const std::vector<const char*>& contents);

[[nodiscard]] FormFieldData CreateTestSelectField(
    const std::vector<const char*>& values);

[[nodiscard]] FormFieldData CreateTestSelectField(
    std::string_view label,
    std::string_view name,
    std::string_view value,
    std::string_view autocomplete,
    const std::vector<const char*>& values,
    const std::vector<const char*>& contents,
    FormControlType type);

// Provides a quick way to populate a datalist field.
[[nodiscard]] FormFieldData CreateTestDatalistField(
    std::string_view label,
    std::string_view name,
    std::string_view value,
    const std::vector<const char*>& values,
    const std::vector<const char*>& labels);

// Populates `form` with data corresponding to a simple personal information
// form, including name and email, but no address-related fields.
[[nodiscard]] FormData CreateTestPersonalInformationFormData();

// Populates `form` with data corresponding to a simple credit card form.
[[nodiscard]] FormData CreateTestCreditCardFormData(bool is_https,
                                                    bool use_month_type,
                                                    bool split_names = false);

// Populates `form_data` with data corresponding to an IBAN form (a form with a
// single IBAN field).
[[nodiscard]] FormData CreateTestIbanFormData(
    std::string_view value = kIbanValue,
    bool is_https = true);

// Creates a 'FormData` with a username and a password fields.
[[nodiscard]] FormData CreateTestPasswordFormData();

// Creates a `FormData` with a single unclassified field.
[[nodiscard]] FormData CreateTestUnclassifiedFormData();

MATCHER_P(DeepEqualsFormData,
          form_data,
          negation ? "does not equal" : "equals") {
  return FormData::DeepEqual(arg, form_data);
}

}  // namespace test

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_TEST_UTILS_H_
