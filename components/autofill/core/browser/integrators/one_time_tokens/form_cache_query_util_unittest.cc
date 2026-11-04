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
#include "components/autofill/core/browser/foundations/autofill_driver_test_api.h"
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
    autofill_driver().SetLocalFrameToken(test::MakeLocalFrameToken());
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
// into an eligible OTP field of an OTP form and ignores user input in non-OTP
// fields or non-OTP forms.
TEST_F(FormCacheQueryUtilTest, AnyOtpFieldContainsTypedInput) {
  FormStructure* cross_site_form =
      AddOtpForm(url::Origin::Create(GURL("https://other.test")));
  cross_site_form->field(1)->AddFieldModifier(FieldModifier::kUser);
  EXPECT_FALSE(AnyOtpFieldContainsTypedInput(autofill_manager()));

  FormStructure* form = AddOtpForm();
  form->field(0)->AddFieldModifier(FieldModifier::kUser);
  EXPECT_FALSE(AnyOtpFieldContainsTypedInput(autofill_manager()));

  form->field(1)->AddFieldModifier(FieldModifier::kUser);
  EXPECT_TRUE(AnyOtpFieldContainsTypedInput(autofill_manager()));
}

// Verifies that `IsActiveNonEmbeddedDriver()`, `FindOtpField()`,
// `FirstEligibleOtpField()`, and `AnyOtpFieldContainsTypedInput()` ignore
// cached OTP fields when the owning `AutofillDriver` is inactive, embedded, or
// pending deletion.
TEST_F(FormCacheQueryUtilTest, OwningDriverInactiveOrEmbedded) {
  FormStructure* form = AddOtpForm();
  FieldGlobalId otp_field_id = form->field(1)->global_id();
  form->field(1)->AddFieldModifier(FieldModifier::kUser);

  ASSERT_TRUE(IsActiveNonEmbeddedDriver(autofill_driver()));
  ASSERT_THAT(FindOtpField(autofill_manager(), form->global_id(), otp_field_id),
              FieldsAre(form, form->field(1)));
  ASSERT_THAT(FirstEligibleOtpField(autofill_manager()),
              FieldsAre(form, form->field(1)));
  ASSERT_TRUE(AnyOtpFieldContainsTypedInput(autofill_manager()));

  autofill_driver().SetIsActive(false);
  EXPECT_FALSE(IsActiveNonEmbeddedDriver(autofill_driver()));
  EXPECT_THAT(FindOtpField(autofill_manager(), form->global_id(), otp_field_id),
              FieldsAre(nullptr, nullptr));
  EXPECT_THAT(FirstEligibleOtpField(autofill_manager()),
              FieldsAre(nullptr, nullptr));
  EXPECT_FALSE(AnyOtpFieldContainsTypedInput(autofill_manager()));

  autofill_driver().SetIsActive(true);
  autofill_driver().SetIsEmbedded(true);
  EXPECT_FALSE(IsActiveNonEmbeddedDriver(autofill_driver()));
  EXPECT_THAT(FindOtpField(autofill_manager(), form->global_id(), otp_field_id),
              FieldsAre(nullptr, nullptr));
  EXPECT_THAT(FirstEligibleOtpField(autofill_manager()),
              FieldsAre(nullptr, nullptr));
  EXPECT_FALSE(AnyOtpFieldContainsTypedInput(autofill_manager()));

  autofill_driver().SetIsEmbedded(false);
  const AutofillDriver::LifecycleState initial_state =
      autofill_driver().GetLifecycleState();
  test_api(autofill_driver())
      .SetLifecycleState(AutofillDriver::LifecycleState::kPendingDeletion);
  EXPECT_FALSE(IsActiveNonEmbeddedDriver(autofill_driver()));
  EXPECT_THAT(FindOtpField(autofill_manager(), form->global_id(), otp_field_id),
              FieldsAre(nullptr, nullptr));
  EXPECT_THAT(FirstEligibleOtpField(autofill_manager()),
              FieldsAre(nullptr, nullptr));
  EXPECT_FALSE(AnyOtpFieldContainsTypedInput(autofill_manager()));
  test_api(autofill_driver()).SetLifecycleState(initial_state);
}

// Verifies that flattened cross-frame OTP fields hosted by a child
// `AutofillDriver` are returned only while that child driver is active,
// non-embedded, and still alive in `AutofillDriverFactory`.
TEST_F(FormCacheQueryUtilTest, ChildFrameOtpField_HostDriverState) {
  CreateAutofillDriver();
  autofill_driver(1).SetLocalFrameToken(test::MakeLocalFrameToken());
  ActivateAutofillDriver(autofill_driver(0));
  ActivateAutofillDriver(autofill_driver(1));

  FormStructure* form = AddForm({
      .fields =
          {
              {.server_type = NAME_FIRST},
              {.server_type = ONE_TIME_CODE,
               .host_frame = autofill_driver(1).GetFrameToken(),
               .origin = url::Origin::Create(GURL("https://sub.example.test"))},
          },
      .host_frame = autofill_driver(0).GetFrameToken(),
      .main_frame_origin = main_origin_,
  });
  FieldGlobalId otp_field_id = form->field(1)->global_id();
  form->field(1)->AddFieldModifier(FieldModifier::kUser);

  EXPECT_THAT(FindOtpField(autofill_manager(), form->global_id(), otp_field_id),
              FieldsAre(form, form->field(1)));
  EXPECT_THAT(FirstEligibleOtpField(autofill_manager()),
              FieldsAre(form, form->field(1)));
  EXPECT_TRUE(AnyOtpFieldContainsTypedInput(autofill_manager()));

  // Deactivating the child frame suppresses lookup, eligibility, and
  // typed-input detection for fields hosted in that child frame.
  DeactivateAutofillDriver(autofill_driver(1));
  autofill_driver(1).SetIsActive(false);
  EXPECT_THAT(FindOtpField(autofill_manager(), form->global_id(), otp_field_id),
              FieldsAre(nullptr, nullptr));
  EXPECT_THAT(FirstEligibleOtpField(autofill_manager()),
              FieldsAre(nullptr, nullptr));
  EXPECT_FALSE(AnyOtpFieldContainsTypedInput(autofill_manager()));

  // Marking the reactivated child frame as embedded also suppresses it.
  ActivateAutofillDriver(autofill_driver(1));
  autofill_driver(1).SetIsActive(true);
  autofill_driver(1).SetIsEmbedded(true);
  EXPECT_THAT(FindOtpField(autofill_manager(), form->global_id(), otp_field_id),
              FieldsAre(nullptr, nullptr));
  EXPECT_THAT(FirstEligibleOtpField(autofill_manager()),
              FieldsAre(nullptr, nullptr));
  EXPECT_FALSE(AnyOtpFieldContainsTypedInput(autofill_manager()));

  // Restoring active, non-embedded state makes the child-frame field eligible
  // again until the child driver transitions to `kPendingDeletion` and is
  // removed from `AutofillDriverFactory`.
  autofill_driver(1).SetIsEmbedded(false);
  EXPECT_THAT(FindOtpField(autofill_manager(), form->global_id(), otp_field_id),
              FieldsAre(form, form->field(1)));
  DeleteAutofillDriver(autofill_driver(1));
  EXPECT_THAT(FindOtpField(autofill_manager(), form->global_id(), otp_field_id),
              FieldsAre(nullptr, nullptr));
  EXPECT_THAT(FirstEligibleOtpField(autofill_manager()),
              FieldsAre(nullptr, nullptr));
  EXPECT_FALSE(AnyOtpFieldContainsTypedInput(autofill_manager()));
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
