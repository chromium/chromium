// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/integrators/one_time_tokens/otp_target_field_identifier.h"

#include <memory>
#include <optional>
#include <utility>
#include <vector>

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
#include "components/autofill/core/browser/test_utils/autofill_form_test_util.h"
#include "components/autofill/core/common/autofill_test_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Test;

class OtpTargetFieldIdentifierTest
    : public Test,
      public WithTestAutofillClientDriverManager<TestAutofillClient,
                                                 TestAutofillDriver> {
 protected:
  void SetUp() override {
    InitAutofillClient();
    CreateAutofillDriver();
    CreateAutofillDriver();
    autofill_driver(0).SetLocalFrameToken(test::MakeLocalFrameToken());
    autofill_driver(1).SetLocalFrameToken(test::MakeLocalFrameToken());
  }

  FormData AddForm(
      TestBrowserAutofillManager& manager,
      std::vector<FieldType> types,
      bool user_typed = false,
      std::optional<LocalFrameToken> field_host_frame = std::nullopt) {
    test::FormDescription form_description;
    form_description.host_frame = manager.driver().GetFrameToken();
    const LocalFrameToken host_frame =
        field_host_frame.value_or(manager.driver().GetFrameToken());
    for (FieldType type : types) {
      form_description.fields.push_back(
          {.server_type = type, .host_frame = host_frame});
    }
    FormData form = test::GetFormData(form_description);
    auto form_structure = std::make_unique<FormStructure>(form);
    test_api(*form_structure)
        .SetFieldTypes(test::GetServerTypes(form_description));
    test_api(*form_structure).AssignSections();
    if (user_typed && form_structure->field_count() > 0) {
      form_structure->field(0)->AddFieldModifier(FieldModifier::kUser);
    }
    test_api(manager).AddSeenFormStructure(std::move(form_structure));
    test_api(manager).OnFormsParsed({form});
    return form;
  }

  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
};

// Verifies that `GetTarget()`, `GetEligibleTargets()`, and
// `HasNonOtpFocusAnywhere()` track cross-frame focus across flattened forms,
// suppress `GetTarget()` (while keeping `GetEligibleTargets()` populated) when
// a non-OTP field has focus, ignore broadcast `FocusOnNonFormField()`
// notifications from unrelated frames, and clear focus when the child frame
// hosting a flattened field reports focus leaving the form.
TEST_F(OtpTargetFieldIdentifierTest, CrossFrameFocus) {
  OtpTargetFieldIdentifier identifier;
  identifier.RegisterManager(autofill_manager(0));
  identifier.RegisterManager(autofill_manager(1));

  // Flattened form owned by frame 0 with an OTP field and a username field
  // hosted in child frame 1.
  FormData flattened_form =
      AddForm(autofill_manager(0), {ONE_TIME_CODE, USERNAME},
              /*user_typed=*/false, autofill_driver(1).GetFrameToken());
  const FieldGlobalId otp_field_id = flattened_form.fields()[0].global_id();
  const FieldGlobalId login_field_id = flattened_form.fields()[1].global_id();

  // Unfocused: picks first eligible OTP field and records both hosting frame
  // and form-owning frame.
  std::optional<OtpTarget> target = identifier.GetTarget();
  ASSERT_TRUE(target.has_value());
  EXPECT_EQ(target->field, otp_field_id);
  EXPECT_EQ(target->frame, autofill_driver(1).GetFrameToken());
  EXPECT_EQ(target->form_owner, autofill_driver(0).GetFrameToken());
  EXPECT_THAT(identifier.GetEligibleTargets(),
              ElementsAre(Field(&OtpTarget::field, otp_field_id)));
  EXPECT_FALSE(identifier.HasNonOtpFocusAnywhere());
  EXPECT_FALSE(identifier.IsFieldFocused(otp_field_id));

  // Focus OTP field in child frame 1 reported via parent frame 0's manager.
  identifier.OnFocusChanged(autofill_driver(0).GetFrameToken(),
                            flattened_form.global_id(), otp_field_id);
  EXPECT_TRUE(identifier.IsFieldFocused(otp_field_id));
  target = identifier.GetTarget();
  ASSERT_TRUE(target.has_value());
  EXPECT_EQ(target->field, otp_field_id);
  EXPECT_EQ(target->form_owner, autofill_driver(0).GetFrameToken());
  EXPECT_FALSE(identifier.HasNonOtpFocusAnywhere());

  // Focus non-OTP field hosted in child frame 1 (reported via parent frame 0's
  // manager): suppresses `GetTarget()` and reports non-OTP focus, while
  // `GetEligibleTargets()` still returns the OTP field regardless of focus.
  identifier.OnFocusChanged(autofill_driver(0).GetFrameToken(),
                            flattened_form.global_id(), login_field_id);
  EXPECT_FALSE(identifier.GetTarget().has_value());
  EXPECT_THAT(identifier.GetEligibleTargets(),
              ElementsAre(Field(&OtpTarget::field, otp_field_id)));
  EXPECT_TRUE(identifier.HasNonOtpFocusAnywhere());
  EXPECT_TRUE(identifier.IsFieldFocused(login_field_id));

  // Broadcast `FocusOnNonFormField()` from an unrelated frame must NOT wipe
  // focus.
  identifier.OnFocusChanged(test::MakeLocalFrameToken(), std::nullopt,
                            std::nullopt);
  EXPECT_TRUE(identifier.HasNonOtpFocusAnywhere());

  // `FocusOnNonFormField()` originating from child frame 1 (which hosts
  // `login_field_id`) clears focus even though focus was originally reported
  // via parent frame 0.
  identifier.OnFocusChanged(autofill_driver(1).GetFrameToken(), std::nullopt,
                            std::nullopt);
  EXPECT_FALSE(identifier.HasNonOtpFocusAnywhere());
  EXPECT_FALSE(identifier.IsFieldFocused(login_field_id));
  target = identifier.GetTarget();
  ASSERT_TRUE(target.has_value());
  EXPECT_EQ(target->field, otp_field_id);
}

// Verifies that inactive, embedded, and unregistered frames (including inactive
// child frames hosting fields in flattened forms) are excluded from OTP target
// resolution and user-typed input checks, and that deactivating either the
// form-owning frame or the field-hosting child frame clears its focus state.
TEST_F(OtpTargetFieldIdentifierTest, FrameLifecycleAndRegistration) {
  OtpTargetFieldIdentifier identifier;
  identifier.RegisterManager(autofill_manager(0));
  identifier.RegisterManager(autofill_manager(1));

  FormData otp_form_0 = AddForm(autofill_manager(0), {ONE_TIME_CODE},
                                /*user_typed=*/true);
  FormData otp_form_1 = AddForm(autofill_manager(1), {ONE_TIME_CODE});

  EXPECT_TRUE(identifier.AnyOtpFieldContainsTypedInput());
  EXPECT_THAT(
      identifier.GetEligibleTargets(),
      ElementsAre(
          Field(&OtpTarget::field, otp_form_0.fields()[0].global_id()),
          Field(&OtpTarget::field, otp_form_1.fields()[0].global_id())));

  // Focusing frame 0 and then marking frame 0 inactive suppresses focused
  // target resolution immediately and falls back to active frame 1 once
  // `OnFrameBecameInactive()` clears focus.
  identifier.OnFocusChanged(autofill_driver(0).GetFrameToken(),
                            otp_form_0.global_id(),
                            otp_form_0.fields()[0].global_id());
  autofill_driver(0).SetIsActive(false);
  EXPECT_FALSE(identifier.GetTarget().has_value());
  identifier.OnFrameBecameInactive(autofill_driver(0).GetFrameToken());

  EXPECT_FALSE(identifier.IsFieldFocused(otp_form_0.fields()[0].global_id()));
  EXPECT_FALSE(identifier.AnyOtpFieldContainsTypedInput());
  std::optional<OtpTarget> target = identifier.GetTarget();
  ASSERT_TRUE(target.has_value());
  EXPECT_EQ(target->field, otp_form_1.fields()[0].global_id());
  EXPECT_THAT(identifier.GetEligibleTargets(),
              ElementsAre(Field(&OtpTarget::field,
                                otp_form_1.fields()[0].global_id())));

  // Embedded frames (e.g. fenced frames) are also ignored.
  autofill_driver(1).SetIsEmbedded(true);
  EXPECT_FALSE(identifier.GetTarget().has_value());
  EXPECT_THAT(identifier.GetEligibleTargets(), IsEmpty());
  autofill_driver(1).SetIsEmbedded(false);
  EXPECT_TRUE(identifier.GetTarget().has_value());

  // If an active parent frame (frame 0) owns a flattened form whose OTP field
  // is hosted in an inactive child frame (frame 1), that field is excluded from
  // target resolution and typed-input checks, and deactivating the child frame
  // clears focus.
  autofill_driver(0).SetIsActive(true);
  test_api(autofill_manager(0)).Reset();
  FormData flattened_in_frame_0 =
      AddForm(autofill_manager(0), {ONE_TIME_CODE}, /*user_typed=*/true,
              autofill_driver(1).GetFrameToken());
  const FieldGlobalId child_otp_field =
      flattened_in_frame_0.fields()[0].global_id();
  identifier.OnFocusChanged(autofill_driver(0).GetFrameToken(),
                            flattened_in_frame_0.global_id(), child_otp_field);
  EXPECT_TRUE(identifier.IsFieldFocused(child_otp_field));
  EXPECT_TRUE(identifier.AnyOtpFieldContainsTypedInput());

  autofill_driver(1).SetIsActive(false);
  EXPECT_FALSE(identifier.GetTarget().has_value());
  identifier.OnFrameBecameInactive(autofill_driver(1).GetFrameToken());
  EXPECT_FALSE(identifier.IsFieldFocused(child_otp_field));
  EXPECT_FALSE(identifier.AnyOtpFieldContainsTypedInput());
  EXPECT_FALSE(identifier.GetTarget().has_value());
  EXPECT_THAT(identifier.GetEligibleTargets(), IsEmpty());

  // Reactivating frame 1 makes the flattened field eligible again; destroying
  // and unregistering child frame 1 excludes fields hosted in frame 1 even if
  // parent frame 0 still caches the form.
  autofill_driver(1).SetIsActive(true);
  EXPECT_TRUE(identifier.AnyOtpFieldContainsTypedInput());
  target = identifier.GetTarget();
  ASSERT_TRUE(target.has_value());
  EXPECT_EQ(target->field, child_otp_field);
  identifier.UnregisterManager(autofill_manager(1));
  DeleteAutofillDriver(autofill_driver(1));
  EXPECT_FALSE(identifier.AnyOtpFieldContainsTypedInput());
  EXPECT_FALSE(identifier.GetTarget().has_value());
  EXPECT_THAT(identifier.GetEligibleTargets(), IsEmpty());

  // Unregistering an active owning frame (frame 0) also excludes its forms.
  AddForm(autofill_manager(0), {ONE_TIME_CODE}, /*user_typed=*/true);
  EXPECT_TRUE(identifier.AnyOtpFieldContainsTypedInput());
  EXPECT_TRUE(identifier.GetTarget().has_value());
  identifier.UnregisterManager(autofill_manager(0));
  EXPECT_FALSE(identifier.AnyOtpFieldContainsTypedInput());
  EXPECT_FALSE(identifier.GetTarget().has_value());
  EXPECT_THAT(identifier.GetEligibleTargets(), IsEmpty());
}

// Verifies that `TypedInAnyFieldAfterOtpDetection()` only records user typing
// that occurs after an OTP field has been detected in the tab and resets when
// OTP fields are cleared.
TEST_F(OtpTargetFieldIdentifierTest, TypingAfterOtpDetection) {
  OtpTargetFieldIdentifier identifier;
  EXPECT_FALSE(identifier.TypedInAnyFieldAfterOtpDetection());

  // Typing before an OTP field is detected does not set the flag.
  identifier.OnTextFieldValueChanged();
  EXPECT_FALSE(identifier.TypedInAnyFieldAfterOtpDetection());

  // Typing after an OTP field is detected sets the flag until cleared.
  identifier.OnOtpFieldsDetected();
  identifier.OnTextFieldValueChanged();
  EXPECT_TRUE(identifier.TypedInAnyFieldAfterOtpDetection());

  identifier.OnOtpFieldsCleared();
  EXPECT_FALSE(identifier.TypedInAnyFieldAfterOtpDetection());
}

}  // namespace
}  // namespace autofill
