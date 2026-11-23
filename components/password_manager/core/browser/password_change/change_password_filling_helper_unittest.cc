// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_change/change_password_filling_helper.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/unguessable_token.h"
#include "components/actor/public/mojom/actor_types.mojom-shared.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/mock_password_form_cache.h"
#include "components/password_manager/core/browser/mock_password_manager.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"
#include "components/password_manager/core/browser/password_string.h"
#include "components/password_manager/core/browser/stub_form_saver.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace password_manager {
namespace {

using ::base::test::TestFuture;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

class MockPasswordManagerDriver : public StubPasswordManagerDriver {
 public:
  MOCK_METHOD(PasswordManagerInterface*, GetPasswordManager, (), (override));
  MOCK_METHOD(
      void,
      FillChangePasswordForm,
      (autofill::FieldRendererId password_element_id,
       autofill::FieldRendererId new_password_element_id,
       autofill::FieldRendererId confirm_password_element_id,
       const PasswordString& old_password,
       const PasswordString& new_password,
       base::OnceCallback<void(const std::optional<autofill::FormData>&)>
           form_data_callback),
      (override));
};

class ChangePasswordFillingHelperTest : public testing::Test {
 public:
  void SetUp() override {
    driver_ = std::make_unique<NiceMock<MockPasswordManagerDriver>>();
    other_driver_ = std::make_unique<NiceMock<MockPasswordManagerDriver>>();

    ON_CALL(*driver_, GetPasswordManager())
        .WillByDefault(Return(&password_manager_));
    ON_CALL(*other_driver_, GetPasswordManager())
        .WillByDefault(Return(&password_manager_));
    ON_CALL(password_manager_, GetPasswordFormCache())
        .WillByDefault(Return(&form_cache_));
    ON_CALL(form_cache_, GetFormManagers())
        .WillByDefault(
            [this]() -> base::span<const std::unique_ptr<PasswordFormManager>> {
              return form_managers_;
            });
  }

  void TearDown() override {
    form_managers_.clear();
    form_fetchers_.clear();
    other_driver_.reset();
    driver_.reset();
  }

  autofill::FormFieldData MakeFormField(
      uint32_t renderer_id,
      autofill::FormControlType control_type =
          autofill::FormControlType::kInputPassword,
      bool is_autofilled = false) {
    autofill::FormFieldData field;
    field.set_renderer_id(autofill::FieldRendererId(renderer_id));
    field.set_form_control_type(control_type);
    field.set_is_autofilled_according_to_renderer(is_autofilled);
    return field;
  }

  void AddFormManager(
      std::vector<autofill::FormFieldData> fields,
      PasswordManagerDriver* driver = nullptr,
      autofill::FormRendererId form_id = autofill::FormRendererId(1)) {
    if (!driver) {
      driver = driver_.get();
    }
    autofill::FormData form_data;
    form_data.set_url(GURL("https://example.com"));
    form_data.set_action(GURL("https://example.com/submit"));
    form_data.set_renderer_id(form_id);
    form_data.set_fields(std::move(fields));

    auto fetcher = std::make_unique<FakeFormFetcher>();
    auto save_manager = std::make_unique<PasswordSaveManagerImpl>(
        std::make_unique<StubFormSaver>(), /*account_form_saver=*/nullptr);
    auto manager = std::make_unique<PasswordFormManager>(
        &password_client_, driver->AsWeakPtr(), form_data, fetcher.get(),
        std::move(save_manager), /*metrics_recorder=*/nullptr);
    fetcher->NotifyFetchCompleted();

    form_fetchers_.push_back(std::move(fetcher));
    form_managers_.push_back(std::move(manager));
  }

  autofill::FieldGlobalId MakeGlobalId(const autofill::LocalFrameToken& frame,
                                       uint32_t renderer_id) {
    return {frame, autofill::FieldRendererId(renderer_id)};
  }

  // Configures `helper_` with validated fields in `frame_token_` backed by
  // matching `PasswordFormManager` fields on `driver_`.
  [[nodiscard]] bool PrepareHelperWithFields(bool has_current,
                                             bool has_new,
                                             bool has_confirm) {
    std::vector<autofill::FormFieldData> fields;
    if (has_current) {
      fields.push_back(MakeFormField(10));
    }
    if (has_new) {
      fields.push_back(MakeFormField(20));
    }
    if (has_confirm) {
      fields.push_back(MakeFormField(30));
    }
    AddFormManager(std::move(fields));

    auto frame = helper_.ValidateAndSetFieldIds(
        has_current ? MakeGlobalId(frame_token_, 10)
                    : autofill::FieldGlobalId{},
        has_new ? MakeGlobalId(frame_token_, 20) : autofill::FieldGlobalId{},
        has_confirm ? MakeGlobalId(frame_token_, 30)
                    : autofill::FieldGlobalId{});
    if (!frame.has_value()) {
      ADD_FAILURE() << "ValidateAndSetFieldIds failed: "
                    << frame.error().message;
      return false;
    }
    ChangePasswordActionResult result =
        helper_.ValidateDriverAndTargetFields(*driver_);
    EXPECT_TRUE(result.IsOk()) << result.message;
    return result.IsOk();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  autofill::LocalFrameToken frame_token_{base::UnguessableToken::Create()};
  StubPasswordManagerClient password_client_;
  NiceMock<MockPasswordManager> password_manager_;
  NiceMock<MockPasswordFormCache> form_cache_;
  std::unique_ptr<NiceMock<MockPasswordManagerDriver>> driver_;
  std::unique_ptr<NiceMock<MockPasswordManagerDriver>> other_driver_;
  std::vector<std::unique_ptr<FakeFormFetcher>> form_fetchers_;
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers_;
  ChangePasswordFillingHelper helper_;
};

TEST_F(ChangePasswordFillingHelperTest, ValidateProvidedFields_NoneProvided) {
  ChangePasswordActionResult result =
      ChangePasswordFillingHelper::ValidateProvidedFields(
          /*has_current_password=*/false,
          /*has_new_password=*/false,
          /*has_confirm_new_password=*/false);
  EXPECT_EQ(result.code, actor::mojom::ActionResultCode::kArgumentsInvalid);
  EXPECT_FALSE(result.requires_page_stabilization);
  EXPECT_EQ(result.message, "At least one password field must be provided.");
}

TEST_F(ChangePasswordFillingHelperTest, ValidateProvidedFields_Succeeds) {
  EXPECT_TRUE(
      ChangePasswordFillingHelper::ValidateProvidedFields(true, false, false)
          .IsOk());
  EXPECT_TRUE(
      ChangePasswordFillingHelper::ValidateProvidedFields(false, true, false)
          .IsOk());
  EXPECT_TRUE(
      ChangePasswordFillingHelper::ValidateProvidedFields(false, false, true)
          .IsOk());
  EXPECT_TRUE(
      ChangePasswordFillingHelper::ValidateProvidedFields(true, true, true)
          .IsOk());
}

TEST_F(ChangePasswordFillingHelperTest, ValidateAndSetFieldIds_NoFields) {
  auto result = helper_.ValidateAndSetFieldIds(autofill::FieldGlobalId{},
                                               autofill::FieldGlobalId{},
                                               autofill::FieldGlobalId{});
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code,
            actor::mojom::ActionResultCode::kArgumentsInvalid);
  EXPECT_EQ(result.error().message,
            "At least one password field must be provided.");
}

TEST_F(ChangePasswordFillingHelperTest,
       ValidateAndSetFieldIds_PartialFieldId_Fails) {
  {
    ChangePasswordFillingHelper helper;
    auto missing_frame_result = helper.ValidateAndSetFieldIds(
        MakeGlobalId(autofill::LocalFrameToken{}, 10),
        autofill::FieldGlobalId{}, autofill::FieldGlobalId{});
    ASSERT_FALSE(missing_frame_result.has_value());
    EXPECT_EQ(missing_frame_result.error().code,
              actor::mojom::ActionResultCode::kArgumentsInvalid);
    EXPECT_EQ(missing_frame_result.error().message,
              "Invalid password field identifier.");
  }

  {
    ChangePasswordFillingHelper helper;
    auto missing_renderer_id_result = helper.ValidateAndSetFieldIds(
        autofill::FieldGlobalId{},
        autofill::FieldGlobalId{frame_token_, autofill::FieldRendererId{}},
        autofill::FieldGlobalId{});
    ASSERT_FALSE(missing_renderer_id_result.has_value());
    EXPECT_EQ(missing_renderer_id_result.error().code,
              actor::mojom::ActionResultCode::kArgumentsInvalid);
    EXPECT_EQ(missing_renderer_id_result.error().message,
              "Invalid password field identifier.");
  }
}

TEST_F(ChangePasswordFillingHelperTest,
       ValidateAndSetFieldIds_DifferentFrames_Fails) {
  autofill::LocalFrameToken other_frame{base::UnguessableToken::Create()};
  auto result = helper_.ValidateAndSetFieldIds(MakeGlobalId(frame_token_, 10),
                                               MakeGlobalId(other_frame, 20),
                                               autofill::FieldGlobalId{});
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error().code,
            actor::mojom::ActionResultCode::kArgumentsInvalid);
  EXPECT_EQ(result.error().message,
            "All password fields must belong to the same frame.");
}

TEST_F(ChangePasswordFillingHelperTest,
       ValidateAndSetFieldIds_DuplicateIds_Fails) {
  const autofill::FieldGlobalId field_id = MakeGlobalId(frame_token_, 20);
  const autofill::FieldGlobalId empty_id;
  struct DuplicateCase {
    const char* name;
    autofill::FieldGlobalId current;
    autofill::FieldGlobalId new_password;
    autofill::FieldGlobalId confirm;
  };
  const DuplicateCase cases[] = {
      {"current_eq_new", field_id, field_id, empty_id},
      {"current_eq_confirm", field_id, empty_id, field_id},
      {"new_eq_confirm", empty_id, field_id, field_id},
  };

  for (const auto& test_case : cases) {
    SCOPED_TRACE(test_case.name);
    ChangePasswordFillingHelper helper;
    auto result = helper.ValidateAndSetFieldIds(
        test_case.current, test_case.new_password, test_case.confirm);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code,
              actor::mojom::ActionResultCode::kArgumentsInvalid);
    EXPECT_EQ(result.error().message,
              "Password field targets must be distinct.");
  }
}

TEST_F(ChangePasswordFillingHelperTest, ValidateAndSetFieldIds_Succeeds) {
  auto result = helper_.ValidateAndSetFieldIds(MakeGlobalId(frame_token_, 10),
                                               MakeGlobalId(frame_token_, 20),
                                               MakeGlobalId(frame_token_, 30));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, frame_token_);
}

TEST_F(ChangePasswordFillingHelperTest,
       ValidateDriverAndTargetFields_NullPasswordManager_Fails) {
  EXPECT_CALL(*driver_, GetPasswordManager()).WillOnce(Return(nullptr));
  ASSERT_TRUE(helper_
                  .ValidateAndSetFieldIds(autofill::FieldGlobalId{},
                                          MakeGlobalId(frame_token_, 20),
                                          autofill::FieldGlobalId{})
                  .has_value());

  ChangePasswordActionResult result =
      helper_.ValidateDriverAndTargetFields(*driver_);
  EXPECT_EQ(result.code,
            actor::mojom::ActionResultCode::kFormFillingUnknownAutofillError);
  EXPECT_EQ(result.message,
            "Target field is not a valid password field: "
            "new_password_field (id 20).");
}

TEST_F(ChangePasswordFillingHelperTest,
       ValidateDriverAndTargetFields_NullFormCache_Fails) {
  EXPECT_CALL(password_manager_, GetPasswordFormCache())
      .WillOnce(Return(nullptr));
  ASSERT_TRUE(helper_
                  .ValidateAndSetFieldIds(autofill::FieldGlobalId{},
                                          MakeGlobalId(frame_token_, 20),
                                          autofill::FieldGlobalId{})
                  .has_value());

  ChangePasswordActionResult result =
      helper_.ValidateDriverAndTargetFields(*driver_);
  EXPECT_EQ(result.code,
            actor::mojom::ActionResultCode::kFormFillingUnknownAutofillError);
  EXPECT_EQ(result.message,
            "Target field is not a valid password field: "
            "new_password_field (id 20).");
}

TEST_F(ChangePasswordFillingHelperTest,
       ValidateDriverAndTargetFields_EmptyFormCache_Fails) {
  ASSERT_TRUE(helper_
                  .ValidateAndSetFieldIds(autofill::FieldGlobalId{},
                                          MakeGlobalId(frame_token_, 20),
                                          autofill::FieldGlobalId{})
                  .has_value());

  ChangePasswordActionResult result =
      helper_.ValidateDriverAndTargetFields(*driver_);
  EXPECT_EQ(result.code,
            actor::mojom::ActionResultCode::kFormFillingUnknownAutofillError);
  EXPECT_EQ(result.message,
            "Target field is not a valid password field: "
            "new_password_field (id 20).");
}

TEST_F(ChangePasswordFillingHelperTest,
       ValidateDriverAndTargetFields_MultipleManagers_MatchesTargetForm) {
  AddFormManager({MakeFormField(99)}, driver_.get(),
                 autofill::FormRendererId(1));
  AddFormManager({MakeFormField(10), MakeFormField(20), MakeFormField(30)},
                 driver_.get(), autofill::FormRendererId(2));

  ASSERT_TRUE(helper_
                  .ValidateAndSetFieldIds(MakeGlobalId(frame_token_, 10),
                                          MakeGlobalId(frame_token_, 20),
                                          MakeGlobalId(frame_token_, 30))
                  .has_value());

  EXPECT_TRUE(helper_.ValidateDriverAndTargetFields(*driver_).IsOk());
}

TEST_F(ChangePasswordFillingHelperTest,
       ValidateDriverAndTargetFields_DifferentDriver_Fails) {
  AddFormManager({MakeFormField(20)}, other_driver_.get());
  ASSERT_TRUE(helper_
                  .ValidateAndSetFieldIds(autofill::FieldGlobalId{},
                                          MakeGlobalId(frame_token_, 20),
                                          autofill::FieldGlobalId{})
                  .has_value());

  ChangePasswordActionResult result =
      helper_.ValidateDriverAndTargetFields(*driver_);
  EXPECT_EQ(result.code,
            actor::mojom::ActionResultCode::kFormFillingUnknownAutofillError);
  EXPECT_EQ(result.message,
            "Target field is not a valid password field: "
            "new_password_field (id 20).");
}

TEST_F(ChangePasswordFillingHelperTest,
       ValidateDriverAndTargetFields_UnownedPasswordFields_Succeeds) {
  AddFormManager({MakeFormField(10), MakeFormField(20), MakeFormField(30)},
                 driver_.get(), autofill::FormRendererId());
  ASSERT_TRUE(helper_
                  .ValidateAndSetFieldIds(MakeGlobalId(frame_token_, 10),
                                          MakeGlobalId(frame_token_, 20),
                                          MakeGlobalId(frame_token_, 30))
                  .has_value());

  EXPECT_TRUE(helper_.ValidateDriverAndTargetFields(*driver_).IsOk());
}

TEST_F(ChangePasswordFillingHelperTest,
       ValidateDriverAndTargetFields_FieldsSplitAcrossManagers_Succeeds) {
  AddFormManager({MakeFormField(10)}, driver_.get(),
                 autofill::FormRendererId(1));
  AddFormManager({MakeFormField(20)}, driver_.get(),
                 autofill::FormRendererId(2));
  ASSERT_TRUE(helper_
                  .ValidateAndSetFieldIds(MakeGlobalId(frame_token_, 10),
                                          MakeGlobalId(frame_token_, 20),
                                          autofill::FieldGlobalId{})
                  .has_value());

  EXPECT_TRUE(helper_.ValidateDriverAndTargetFields(*driver_).IsOk());
}

TEST_F(ChangePasswordFillingHelperTest,
       ValidateDriverAndTargetFields_PartiallyUnmanagedFields_Fails) {
  AddFormManager({MakeFormField(10)});
  ASSERT_TRUE(helper_
                  .ValidateAndSetFieldIds(MakeGlobalId(frame_token_, 10),
                                          MakeGlobalId(frame_token_, 20),
                                          MakeGlobalId(frame_token_, 30))
                  .has_value());

  ChangePasswordActionResult result =
      helper_.ValidateDriverAndTargetFields(*driver_);
  EXPECT_EQ(result.code,
            actor::mojom::ActionResultCode::kFormFillingUnknownAutofillError);
  EXPECT_EQ(result.message,
            "Target field is not a valid password field: "
            "new_password_field (id 20), "
            "confirm_new_password_field (id 30).");
}

TEST_F(ChangePasswordFillingHelperTest,
       ValidateDriverAndTargetFields_NonPasswordFieldInManagedForm_Fails) {
  AddFormManager(
      {MakeFormField(10, autofill::FormControlType::kInputText),
       MakeFormField(20, autofill::FormControlType::kInputPassword)});
  ASSERT_TRUE(helper_
                  .ValidateAndSetFieldIds(MakeGlobalId(frame_token_, 10),
                                          MakeGlobalId(frame_token_, 20),
                                          autofill::FieldGlobalId{})
                  .has_value());

  ChangePasswordActionResult result =
      helper_.ValidateDriverAndTargetFields(*driver_);
  EXPECT_EQ(result.code,
            actor::mojom::ActionResultCode::kFormFillingUnknownAutofillError);
  EXPECT_EQ(result.message,
            "Target field is not a valid password field: "
            "current_password_field (id 10).");
}

TEST_F(ChangePasswordFillingHelperTest,
       FillChangePasswordForm_DriverDestroyed_Fails) {
  ASSERT_TRUE(PrepareHelperWithFields(/*has_current=*/true, /*has_new=*/true,
                                      /*has_confirm=*/true));
  form_managers_.clear();
  driver_.reset();

  TestFuture<ChangePasswordActionResult> future;
  helper_.FillChangePasswordForm(future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  ChangePasswordActionResult result = future.Take();
  EXPECT_EQ(result.code, actor::mojom::ActionResultCode::kFrameWentAway);
  EXPECT_FALSE(result.requires_page_stabilization);
  EXPECT_EQ(result.message,
            "Target frame or password manager driver is unavailable.");
}

TEST_F(ChangePasswordFillingHelperTest,
       FillChangePasswordForm_CallbackDropped_ReturnsNoEligibleForm) {
  ASSERT_TRUE(PrepareHelperWithFields(/*has_current=*/true, /*has_new=*/true,
                                      /*has_confirm=*/false));

  EXPECT_CALL(*driver_,
              FillChangePasswordForm(autofill::FieldRendererId(10),
                                     autofill::FieldRendererId(20),
                                     autofill::FieldRendererId(), _, _, _))
      .WillOnce(Return());

  TestFuture<ChangePasswordActionResult> future;
  helper_.FillChangePasswordForm(future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  ChangePasswordActionResult result = future.Take();
  EXPECT_EQ(result.code,
            actor::mojom::ActionResultCode::kFormFillingUnknownAutofillError);
  EXPECT_FALSE(result.requires_page_stabilization);
  EXPECT_EQ(result.message, "Found no eligible form to fill.");
}

TEST_F(ChangePasswordFillingHelperTest,
       FillChangePasswordForm_AllFieldsUnfilled_Fails) {
  ASSERT_TRUE(PrepareHelperWithFields(/*has_current=*/true, /*has_new=*/true,
                                      /*has_confirm=*/false));

  autofill::FormData form_data;
  form_data.set_fields(
      {MakeFormField(10, autofill::FormControlType::kInputPassword,
                     /*is_autofilled=*/false),
       MakeFormField(20, autofill::FormControlType::kInputPassword,
                     /*is_autofilled=*/false)});
  EXPECT_CALL(*driver_, FillChangePasswordForm)
      .WillOnce(base::test::RunOnceCallback<5>(form_data));

  TestFuture<ChangePasswordActionResult> future;
  helper_.FillChangePasswordForm(future.GetCallback());
  ChangePasswordActionResult result = future.Take();
  EXPECT_EQ(result.code,
            actor::mojom::ActionResultCode::kFormFillingUnknownAutofillError);
  EXPECT_FALSE(result.requires_page_stabilization);
  EXPECT_EQ(result.message,
            "Failed to fill old password into field with id 10 and "
            "new generated password into field with id 20.");
}

TEST_F(ChangePasswordFillingHelperTest,
       FillChangePasswordForm_PartialFill_FailsWithDetails) {
  ASSERT_TRUE(PrepareHelperWithFields(/*has_current=*/true, /*has_new=*/true,
                                      /*has_confirm=*/true));

  autofill::FormData form_data;
  form_data.set_fields(
      {MakeFormField(10, autofill::FormControlType::kInputPassword,
                     /*is_autofilled=*/true),
       MakeFormField(20, autofill::FormControlType::kInputPassword,
                     /*is_autofilled=*/true)});
  EXPECT_CALL(*driver_, FillChangePasswordForm)
      .WillOnce(base::test::RunOnceCallback<5>(form_data));

  TestFuture<ChangePasswordActionResult> future;
  helper_.FillChangePasswordForm(future.GetCallback());
  ChangePasswordActionResult result = future.Take();
  EXPECT_EQ(result.code,
            actor::mojom::ActionResultCode::kFormFillingUnknownAutofillError);
  EXPECT_FALSE(result.requires_page_stabilization);
  EXPECT_EQ(result.message,
            "Filled old password into field with id 10 and "
            "new generated password into field with id 20. "
            "Failed to fill confirmation password into field with id 30.");
}

TEST_F(ChangePasswordFillingHelperTest,
       FillChangePasswordForm_AllFieldsFilled_Succeeds) {
  ASSERT_TRUE(PrepareHelperWithFields(/*has_current=*/true, /*has_new=*/true,
                                      /*has_confirm=*/true));

  autofill::FormData form_data;
  form_data.set_fields(
      {MakeFormField(10, autofill::FormControlType::kInputPassword,
                     /*is_autofilled=*/true),
       MakeFormField(20, autofill::FormControlType::kInputPassword,
                     /*is_autofilled=*/true),
       MakeFormField(30, autofill::FormControlType::kInputPassword,
                     /*is_autofilled=*/true)});
  EXPECT_CALL(*driver_,
              FillChangePasswordForm(autofill::FieldRendererId(10),
                                     autofill::FieldRendererId(20),
                                     autofill::FieldRendererId(30),
                                     PasswordString(u"dummy_current_password"),
                                     PasswordString(u"dummy_new_password"), _))
      .WillOnce(base::test::RunOnceCallback<5>(form_data));

  TestFuture<ChangePasswordActionResult> future;
  helper_.FillChangePasswordForm(future.GetCallback());
  ChangePasswordActionResult result = future.Take();
  EXPECT_TRUE(result.IsOk());
  EXPECT_TRUE(result.requires_page_stabilization);
  EXPECT_EQ(result.message,
            "Filled old password into field with id 10 and "
            "new generated password into field with id 20 and "
            "confirmation password into field with id 30.");
}

TEST_F(ChangePasswordFillingHelperTest,
       FillChangePasswordForm_SubsetOfFieldsFilled_Succeeds) {
  ASSERT_TRUE(PrepareHelperWithFields(/*has_current=*/false, /*has_new=*/true,
                                      /*has_confirm=*/false));

  autofill::FormData form_data;
  form_data.set_fields(
      {MakeFormField(20, autofill::FormControlType::kInputPassword,
                     /*is_autofilled=*/true)});
  EXPECT_CALL(*driver_,
              FillChangePasswordForm(autofill::FieldRendererId(),
                                     autofill::FieldRendererId(20),
                                     autofill::FieldRendererId(),
                                     PasswordString(u"dummy_current_password"),
                                     PasswordString(u"dummy_new_password"), _))
      .WillOnce(base::test::RunOnceCallback<5>(form_data));

  TestFuture<ChangePasswordActionResult> future;
  helper_.FillChangePasswordForm(future.GetCallback());
  ChangePasswordActionResult result = future.Take();
  EXPECT_TRUE(result.IsOk());
  EXPECT_TRUE(result.requires_page_stabilization);
  EXPECT_EQ(result.message,
            "Filled new generated password into field with id 20.");
}

}  // namespace
}  // namespace password_manager
