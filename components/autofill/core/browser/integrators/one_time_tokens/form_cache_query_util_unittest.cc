// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/form_cache_query_util.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/test/task_environment.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_structure_test_api.h"
#include "components/autofill/core/browser/foundations/autofill_manager_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/otp_types.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_util.h"
#include "components/autofill/core/common/autofill_test_util.h"
#include "components/autofill/core/common/form_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace autofill {
namespace {

using ::autofill::test::FormDescription;
using ::testing::FieldsAre;
using ::testing::Test;

class FormCacheQueryUtilTest
    : public Test,
      public WithTestAutofillClientDriverManager<TestAutofillClient,
                                                 TestAutofillDriver> {
 public:
  void SetUp() override {
    InitAutofillClient();
    autofill_client().set_last_committed_primary_main_frame_url(
        GURL("https://example.test"));
    CreateAutofillDriver();
  }

  FormStructure* AddForm(const FormDescription& form_description) {
    FormData form = test::GetFormData(form_description);
    auto form_structure = std::make_unique<FormStructure>(form);
    test_api(*form_structure)
        .SetFieldTypes(test::GetServerTypes(form_description));
    test_api(*form_structure).AssignSections();
    FormStructure* added_form = form_structure.get();
    test_api(autofill_manager())
        .AddSeenFormStructure(std::move(form_structure));
    test_api(autofill_manager()).OnFormsParsed({form});
    return added_form;
  }

  FormStructure* AddNonOtpForm() {
    return AddForm({
        .fields = {{.server_type = NAME_FIRST}},
        .host_frame = autofill_driver().GetFrameToken(),
        .main_frame_origin = main_origin_,
    });
  }

  FormStructure* AddOtpForm(url::Origin otp_origin = url::Origin::Create(
                                GURL("https://sub.example.test"))) {
    return AddForm({
        .fields =
            {
                {.server_type = NAME_FIRST},
                {.server_type = ONE_TIME_CODE,
                 .host_frame = autofill_driver().GetFrameToken(),
                 .origin = std::move(otp_origin)},
            },
        .host_frame = autofill_driver().GetFrameToken(),
        .main_frame_origin = main_origin_,
    });
  }

 protected:
  const url::Origin main_origin_ =
      url::Origin::Create(GURL("https://example.test"));

 private:
  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
};

// Verifies that `HasOtpForm()` ignores non-OTP and cross-site OTP forms and
// returns true once an eligible OTP form is cached.
TEST_F(FormCacheQueryUtilTest, HasOtpForm) {
  EXPECT_FALSE(HasOtpForm(autofill_manager()));
  AddNonOtpForm();
  AddOtpForm(url::Origin::Create(GURL("https://other.test")));
  EXPECT_FALSE(HasOtpForm(autofill_manager()));

  AddOtpForm();
  EXPECT_TRUE(HasOtpForm(autofill_manager()));
}

// Verifies that `FirstEligibleOtpField()` skips non-OTP and cross-site OTP
// forms and returns the first eligible OTP field.
TEST_F(FormCacheQueryUtilTest, FirstEligibleOtpField) {
  AddNonOtpForm();
  AddOtpForm(url::Origin::Create(GURL("https://other.test")));
  EXPECT_THAT(FirstEligibleOtpField(autofill_manager()),
              FieldsAre(nullptr, nullptr));

  FormStructure* otp_form = AddOtpForm();
  EXPECT_THAT(FirstEligibleOtpField(autofill_manager()),
              FieldsAre(otp_form, otp_form->field(1)));
}

// Verifies that `FindOtpField()` rejects non-OTP fields, unknown field IDs, and
// cross-site OTP fields, and locates an eligible OTP field when `form_id` is
// exact, `std::nullopt`, or stale.
TEST_F(FormCacheQueryUtilTest, FindOtpField) {
  FormStructure* form = AddOtpForm();
  FormStructure* cross_site_form =
      AddOtpForm(url::Origin::Create(GURL("https://other.test")));

  EXPECT_THAT(FindOtpField(autofill_manager(), form->global_id(),
                           form->field(0)->global_id()),
              FieldsAre(nullptr, nullptr));
  EXPECT_THAT(FindOtpField(autofill_manager(), form->global_id(),
                           test::MakeFieldGlobalId()),
              FieldsAre(nullptr, nullptr));
  EXPECT_THAT(FindOtpField(autofill_manager(), cross_site_form->global_id(),
                           cross_site_form->field(1)->global_id()),
              FieldsAre(nullptr, nullptr));

  EXPECT_THAT(FindOtpField(autofill_manager(), form->global_id(),
                           form->field(1)->global_id()),
              FieldsAre(form, form->field(1)));
  EXPECT_THAT(FindOtpField(autofill_manager(), std::nullopt,
                           form->field(1)->global_id()),
              FieldsAre(form, form->field(1)));
  EXPECT_THAT(FindOtpField(autofill_manager(), test::MakeFormGlobalId(),
                           form->field(1)->global_id()),
              FieldsAre(form, form->field(1)));
}

// Verifies that `AnyOtpFieldContainsTypedInput()` only reports user input typed
// into an OTP field and ignores user input in other fields of the form.
TEST_F(FormCacheQueryUtilTest, AnyOtpFieldContainsTypedInput) {
  FormStructure* form = AddOtpForm();
  form->field(0)->AddFieldModifier(FieldModifier::kUser);
  EXPECT_FALSE(AnyOtpFieldContainsTypedInput(autofill_manager()));

  form->field(1)->AddFieldModifier(FieldModifier::kUser);
  EXPECT_TRUE(AnyOtpFieldContainsTypedInput(autofill_manager()));
}

// Verifies that `BuildOtpTarget()` populates all target identifiers and
// preserves the field's own origin even when it is opaque.
TEST_F(FormCacheQueryUtilTest, BuildOtpTarget) {
  FormStructure* explicit_form = AddOtpForm();
  OtpTarget explicit_target = BuildOtpTarget(autofill_manager(), *explicit_form,
                                             *explicit_form->field(1));
  EXPECT_EQ(explicit_target,
            OtpTarget(url::Origin::Create(GURL("https://sub.example.test")),
                      explicit_form->field(1)->host_frame(),
                      explicit_form->global_id(),
                      explicit_form->field(1)->global_id(),
                      autofill_driver().GetFrameToken()));

  FormStructure* opaque_form = AddOtpForm(url::Origin());
  OtpTarget opaque_target =
      BuildOtpTarget(autofill_manager(), *opaque_form, *opaque_form->field(1));
  EXPECT_TRUE(opaque_target.origin.opaque());
  EXPECT_EQ(opaque_target.field, opaque_form->field(1)->global_id());
}

}  // namespace
}  // namespace autofill
