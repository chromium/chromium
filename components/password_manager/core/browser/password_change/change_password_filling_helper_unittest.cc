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
#include "base/types/expected.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "components/actor/public/mojom/actor_types.mojom-shared.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/core/browser/fake_form_fetcher.h"
#include "components/password_manager/core/browser/mock_password_form_cache.h"
#include "components/password_manager/core/browser/mock_password_manager.h"
#include "components/password_manager/core/browser/password_change/mock_password_change_credentials_provider.h"
#include "components/password_manager/core/browser/password_change/mock_password_change_service.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_save_manager_impl.h"
#include "components/password_manager/core/browser/password_string.h"
#include "components/password_manager/core/browser/stub_form_saver.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/webauthn/android/cred_man_support.h"
#include "components/webauthn/android/webauthn_cred_man_delegate.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace password_manager {
namespace {

using ::base::test::TestFuture;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

constexpr int32_t kActorSurfaceId = 1;
constexpr char16_t kDefaultOldPassword[] = u"provider_old_password";
constexpr char16_t kDefaultNewPassword[] = u"provider_new_password";

class MockPasswordManagerDriver : public StubPasswordManagerDriver {
 public:
  MOCK_METHOD(PasswordManagerInterface*, GetPasswordManager, (), (override));
  MOCK_METHOD(bool, IsInPrimaryMainFrame, (), (const, override));
  MOCK_METHOD(bool, IsDirectChildOfPrimaryMainFrame, (), (const, override));
  MOCK_METHOD(bool, IsNestedWithinFencedFrame, (), (const, override));
  MOCK_METHOD(bool, HasCrossOriginAncestor, (), (const, override));
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
#if BUILDFLAG(IS_ANDROID)
    webauthn::WebAuthnCredManDelegate::override_cred_man_support_for_testing(
        webauthn::CredManSupport::DISABLED);
#endif  // BUILDFLAG(IS_ANDROID)

    driver() = std::make_unique<NiceMock<MockPasswordManagerDriver>>();
    other_driver() = std::make_unique<NiceMock<MockPasswordManagerDriver>>();
    driver()->set_last_committed_url(GURL("https://example.com"));
    other_driver()->set_last_committed_url(GURL("https://example.com"));

    ON_CALL(*driver(), GetPasswordManager())
        .WillByDefault(Return(&password_manager_));
    ON_CALL(*driver(), IsInPrimaryMainFrame()).WillByDefault(Return(true));
    ON_CALL(*other_driver(), GetPasswordManager())
        .WillByDefault(Return(&password_manager_));
    ON_CALL(*other_driver(), IsInPrimaryMainFrame())
        .WillByDefault(Return(true));
    ON_CALL(password_manager_, GetPasswordFormCache())
        .WillByDefault(Return(&form_cache_));
    ON_CALL(form_cache_, GetFormManagers())
        .WillByDefault(
            [this]() -> base::span<const std::unique_ptr<PasswordFormManager>> {
              return form_managers_;
            });
    ON_CALL(credentials_provider_, GetOldPassword())
        .WillByDefault(Return(PasswordString(kDefaultOldPassword)));
    ON_CALL(credentials_provider_, GetNewPassword(_))
        .WillByDefault(Return(PasswordString(kDefaultNewPassword)));
    ON_CALL(password_change_service_,
            GetCredentialsProviderForActorSurface(kActorSurfaceId))
        .WillByDefault(Return(&credentials_provider_));
  }

  void TearDown() override {
    form_managers_.clear();
    form_fetchers_.clear();
    other_driver().reset();
    driver().reset();
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
      autofill::FormRendererId form_id = autofill::FormRendererId(1),
      bool parse_form = true) {
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
    if (parse_form) {
      static_cast<PasswordFormPredictionWaiter::Client*>(manager.get())
          ->OnWaitCompleted();
    }

    form_fetchers_.push_back(std::move(fetcher));
    form_managers_.push_back(std::move(manager));
  }

  autofill::FieldGlobalId MakeGlobalId(const autofill::LocalFrameToken& frame,
                                       uint32_t renderer_id) {
    return {frame, autofill::FieldRendererId(renderer_id)};
  }

  // Configures `helper_` with validated fields in `frame_token_` backed by
  // matching `PasswordFormManager` fields on `driver()`.
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
        helper_.ValidateDriverAndTargetFields(*driver_, main_frame_origin_);
    EXPECT_TRUE(result.IsOk()) << result.message;
    return result.IsOk();
  }
  autofill::LocalFrameToken frame_token() const { return frame_token_; }
  const url::Origin& main_frame_origin() const { return main_frame_origin_; }
  NiceMock<MockPasswordManager>& password_manager() {
    return password_manager_;
  }
  std::unique_ptr<NiceMock<MockPasswordManagerDriver>>& driver() {
    return driver_;
  }
  std::unique_ptr<NiceMock<MockPasswordManagerDriver>>& other_driver() {
    return other_driver_;
  }
  std::vector<std::unique_ptr<PasswordFormManager>>& form_managers() {
    return form_managers_;
  }
  NiceMock<MockPasswordChangeCredentialsProvider>& credentials_provider() {
    return credentials_provider_;
  }
  NiceMock<MockPasswordChangeService>& password_change_service() {
    return password_change_service_;
  }
  ChangePasswordFillingHelper& helper() { return helper_; }

 private:
  base::test::TaskEnvironment task_environment_;
  autofill::LocalFrameToken frame_token_{base::UnguessableToken::Create()};
  url::Origin main_frame_origin_ =
      url::Origin::Create(GURL("https://example.com"));
  StubPasswordManagerClient password_client_;
  NiceMock<MockPasswordManager> password_manager_;
  NiceMock<MockPasswordFormCache> form_cache_;
  std::unique_ptr<NiceMock<MockPasswordManagerDriver>> driver_;
  std::unique_ptr<NiceMock<MockPasswordManagerDriver>> other_driver_;
  std::vector<std::unique_ptr<FakeFormFetcher>> form_fetchers_;
  std::vector<std::unique_ptr<PasswordFormManager>> form_managers_;
  NiceMock<MockPasswordChangeCredentialsProvider> credentials_provider_;
  NiceMock<MockPasswordChangeService> password_change_service_;
  ChangePasswordFillingHelper helper_{kActorSurfaceId,
                                      &password_change_service_};
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
  auto result = helper().ValidateAndSetFieldIds(autofill::FieldGlobalId{},
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
    ChangePasswordFillingHelper helper{kActorSurfaceId,
                                       &password_change_service()};
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
    ChangePasswordFillingHelper helper{kActorSurfaceId,
                                       &password_change_service()};
    auto missing_renderer_id_result = helper.ValidateAndSetFieldIds(
        autofill::FieldGlobalId{},
        autofill::FieldGlobalId{frame_token(), autofill::FieldRendererId{}},
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
  auto result = helper().ValidateAndSetFieldIds(MakeGlobalId(frame_token(), 10),
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
  const autofill::FieldGlobalId field_id = MakeGlobalId(frame_token(), 20);
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
    ChangePasswordFillingHelper helper{kActorSurfaceId,
                                       &password_change_service()};
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
  auto result = helper().ValidateAndSetFieldIds(
      MakeGlobalId(frame_token(), 10), MakeGlobalId(frame_token(), 20),
      MakeGlobalId(frame_token(), 30));
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, frame_token());
}

TEST_F(ChangePasswordFillingHelperTest,
       ValidateDriverAndTargetFields_NullPasswordManager_Fails) {
  EXPECT_CALL(*driver(), GetPasswordManager()).WillOnce(Return(nullptr));
  ASSERT_TRUE(helper()
                  .ValidateAndSetFieldIds(autofill::FieldGlobalId{},
                                          MakeGlobalId(frame_token(), 20),
                                          autofill::FieldGlobalId{})
                  .has_value());

  ChangePasswordActionResult result =
      helper().ValidateDriverAndTargetFields(*driver(), main_frame_origin());
  EXPECT_EQ(result.code,
            actor::mojom::ActionResultCode::kFormFillingUnknownAutofillError);
  EXPECT_EQ(result.message,
            "Target field is not a valid password field: "
            "new_password_field (id 20).");
}

TEST_F(ChangePasswordFillingHelperTest,
       ValidateDriverAndTargetFields_NullFormCache_Fails) {
  EXPECT_CALL(password_manager(), GetPasswordFormCache())
      .WillOnce(Return(nullptr));
  ASSERT_TRUE(helper()
                  .ValidateAndSetFieldIds(autofill::FieldGlobalId{},
                                          MakeGlobalId(frame_token(), 20),
                                          autofill::FieldGlobalId{})
                  .has_value());

  ChangePasswordActionResult result =
      helper().ValidateDriverAndTargetFields(*driver(), main_frame_origin());
  EXPECT_EQ(result.code,
            actor::mojom::ActionResultCode::kFormFillingUnknownAutofillError);
  EXPECT_EQ(result.message,
            "Target field is not a valid password field: "
            "new_password_field (id 20).");
}

TEST_F(ChangePasswordFillingHelperTest,
       ValidateDriverAndTargetFields_EmptyFormCache_Fails) {
  ASSERT_TRUE(helper()
                  .ValidateAndSetFieldIds(autofill::FieldGlobalId{},
                                          MakeGlobalId(frame_token(), 20),
                                          autofill::FieldGlobalId{})
                  .has_value());

  ChangePasswordActionResult result =
      helper().ValidateDriverAndTargetFields(*driver(), main_frame_origin());
  EXPECT_EQ(result.code,
            actor::mojom::ActionResultCode::kFormFillingUnknownAutofillError);
  EXPECT_EQ(result.message,
            "Target field is not a valid password field: "
            "new_password_field (id 20).");
}

TEST_F(ChangePasswordFillingHelperTest,
       ValidateDriverAndTargetFields_MultipleManagers_MatchesTargetForm) {
  AddFormManager({MakeFormField(99)}, driver().get(),
                 autofill::FormRendererId(1));
  AddFormManager({MakeFormField(10), MakeFormField(20), MakeFormField(30)},
                 driver().get(), autofill::FormRendererId(2));

  ASSERT_TRUE(helper()
                  .ValidateAndSetFieldIds(MakeGlobalId(frame_token(), 10),
                                          MakeGlobalId(frame_token(), 20),
                                          MakeGlobalId(frame_token(), 30))
                  .has_value());

  EXPECT_TRUE(helper()
                  .ValidateDriverAndTargetFields(*driver(), main_frame_origin())
                  .IsOk());
}

TEST_F(ChangePasswordFillingHelperTest,
       ValidateDriverAndTargetFields_DifferentDriver_Fails) {
  AddFormManager({MakeFormField(20)}, other_driver().get());
  ASSERT_TRUE(helper()
                  .ValidateAndSetFieldIds(autofill::FieldGlobalId{},
                                          MakeGlobalId(frame_token(), 20),
                                          autofill::FieldGlobalId{})
                  .has_value());

  ChangePasswordActionResult result =
      helper().ValidateDriverAndTargetFields(*driver(), main_frame_origin());
  EXPECT_EQ(result.code,
            actor::mojom::ActionResultCode::kFormFillingUnknownAutofillError);
  EXPECT_EQ(result.message,
            "Target field is not a valid password field: "
            "new_password_field (id 20).");
}

TEST_F(ChangePasswordFillingHelperTest,
       ValidateDriverAndTargetFields_UnownedPasswordFields_Succeeds) {
  AddFormManager({MakeFormField(10), MakeFormField(20), MakeFormField(30)},
                 driver().get(), autofill::FormRendererId());
  ASSERT_TRUE(helper()
                  .ValidateAndSetFieldIds(MakeGlobalId(frame_token(), 10),
                                          MakeGlobalId(frame_token(), 20),
                                          MakeGlobalId(frame_token(), 30))
                  .has_value());

  EXPECT_TRUE(helper()
                  .ValidateDriverAndTargetFields(*driver(), main_frame_origin())
                  .IsOk());
}

TEST_F(ChangePasswordFillingHelperTest,
       ValidateDriverAndTargetFields_FieldsSplitAcrossManagers_Succeeds) {
  AddFormManager({MakeFormField(10)}, driver().get(),
                 autofill::FormRendererId(1));
  AddFormManager({MakeFormField(20)}, driver().get(),
                 autofill::FormRendererId(2));
  ASSERT_TRUE(helper()
                  .ValidateAndSetFieldIds(MakeGlobalId(frame_token(), 10),
                                          MakeGlobalId(frame_token(), 20),
                                          autofill::FieldGlobalId{})
                  .has_value());

  EXPECT_TRUE(helper()
                  .ValidateDriverAndTargetFields(*driver(), main_frame_origin())
                  .IsOk());
}

TEST_F(ChangePasswordFillingHelperTest,
       ValidateDriverAndTargetFields_PartiallyUnmanagedFields_Fails) {
  AddFormManager({MakeFormField(10)});
  ASSERT_TRUE(helper()
                  .ValidateAndSetFieldIds(MakeGlobalId(frame_token(), 10),
                                          MakeGlobalId(frame_token(), 20),
                                          MakeGlobalId(frame_token(), 30))
                  .has_value());

  ChangePasswordActionResult result =
      helper().ValidateDriverAndTargetFields(*driver(), main_frame_origin());
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
  ASSERT_TRUE(helper()
                  .ValidateAndSetFieldIds(MakeGlobalId(frame_token(), 10),
                                          MakeGlobalId(frame_token(), 20),
                                          autofill::FieldGlobalId{})
                  .has_value());

  ChangePasswordActionResult result =
      helper().ValidateDriverAndTargetFields(*driver(), main_frame_origin());
  EXPECT_EQ(result.code,
            actor::mojom::ActionResultCode::kFormFillingUnknownAutofillError);
  EXPECT_EQ(result.message,
            "Target field is not a valid password field: "
            "current_password_field (id 10).");
}

TEST_F(ChangePasswordFillingHelperTest,
       ValidateDriverAndTargetFields_CrossOriginFrame_Fails) {
  driver()->set_last_committed_url(GURL("https://evil.com"));
  AddFormManager({MakeFormField(20)});
  ASSERT_TRUE(helper()
                  .ValidateAndSetFieldIds(autofill::FieldGlobalId{},
                                          MakeGlobalId(frame_token(), 20),
                                          autofill::FieldGlobalId{})
                  .has_value());

  ChangePasswordActionResult result =
      helper().ValidateDriverAndTargetFields(*driver(), main_frame_origin());
  EXPECT_EQ(result.code, actor::mojom::ActionResultCode::kUrlBlocked);
  EXPECT_EQ(result.message,
            "Target frame or origin is not eligible for password filling.");
}

TEST_F(ChangePasswordFillingHelperTest,
       ValidateDriverAndTargetFields_FencedFrame_Fails) {
  ON_CALL(*driver(), IsNestedWithinFencedFrame()).WillByDefault(Return(true));
  AddFormManager({MakeFormField(20)});
  ASSERT_TRUE(helper()
                  .ValidateAndSetFieldIds(autofill::FieldGlobalId{},
                                          MakeGlobalId(frame_token(), 20),
                                          autofill::FieldGlobalId{})
                  .has_value());

  ChangePasswordActionResult result =
      helper().ValidateDriverAndTargetFields(*driver(), main_frame_origin());
  EXPECT_EQ(result.code, actor::mojom::ActionResultCode::kUrlBlocked);
  EXPECT_EQ(result.message,
            "Target frame or origin is not eligible for password filling.");
}

TEST_F(ChangePasswordFillingHelperTest,
       ValidateDriverAndTargetFields_NestedCrossOriginAncestor_Fails) {
  driver()->set_last_committed_url(GURL("https://accounts.example.com"));
  ON_CALL(*driver(), IsInPrimaryMainFrame()).WillByDefault(Return(false));
  ON_CALL(*driver(), IsDirectChildOfPrimaryMainFrame())
      .WillByDefault(Return(false));
  ON_CALL(*driver(), HasCrossOriginAncestor()).WillByDefault(Return(true));
  AddFormManager({MakeFormField(20)});
  ASSERT_TRUE(helper()
                  .ValidateAndSetFieldIds(autofill::FieldGlobalId{},
                                          MakeGlobalId(frame_token(), 20),
                                          autofill::FieldGlobalId{})
                  .has_value());

  ChangePasswordActionResult result =
      helper().ValidateDriverAndTargetFields(*driver(), main_frame_origin());
  EXPECT_EQ(result.code, actor::mojom::ActionResultCode::kUrlBlocked);
  EXPECT_EQ(result.message,
            "Target frame or origin is not eligible for password filling.");
}

TEST_F(ChangePasswordFillingHelperTest,
       ValidateDriverAndTargetFields_SameSiteSubdomain_Succeeds) {
  driver()->set_last_committed_url(GURL("https://accounts.example.com"));
  ON_CALL(*driver(), IsInPrimaryMainFrame()).WillByDefault(Return(false));
  ON_CALL(*driver(), IsDirectChildOfPrimaryMainFrame())
      .WillByDefault(Return(true));
  ON_CALL(*driver(), HasCrossOriginAncestor()).WillByDefault(Return(true));
  AddFormManager({MakeFormField(20)});
  ASSERT_TRUE(helper()
                  .ValidateAndSetFieldIds(autofill::FieldGlobalId{},
                                          MakeGlobalId(frame_token(), 20),
                                          autofill::FieldGlobalId{})
                  .has_value());

  EXPECT_TRUE(helper()
                  .ValidateDriverAndTargetFields(*driver(), main_frame_origin())
                  .IsOk());
}

TEST_F(ChangePasswordFillingHelperTest,
       FillChangePasswordForm_DriverNavigatedCrossOrigin_Fails) {
  ASSERT_TRUE(PrepareHelperWithFields(/*has_current=*/true, /*has_new=*/true,
                                      /*has_confirm=*/true));
  driver()->set_last_committed_url(GURL("https://evil.com"));

  EXPECT_CALL(password_change_service(), GetCredentialsProviderForActorSurface)
      .Times(0);
  EXPECT_CALL(credentials_provider(), GetOldPassword()).Times(0);
  EXPECT_CALL(credentials_provider(), GetNewPassword(_)).Times(0);
  EXPECT_CALL(*driver(), FillChangePasswordForm).Times(0);

  TestFuture<ChangePasswordActionResult> future;
  helper().FillChangePasswordForm(future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  ChangePasswordActionResult result = future.Take();
  EXPECT_EQ(result.code, actor::mojom::ActionResultCode::kUrlBlocked);
  EXPECT_FALSE(result.requires_page_stabilization);
  EXPECT_EQ(result.message,
            "Target frame or origin is not eligible for password filling.");
}

TEST_F(ChangePasswordFillingHelperTest,
       FillChangePasswordForm_DriverDestroyed_Fails) {
  EXPECT_CALL(password_change_service(), GetCredentialsProviderForActorSurface)
      .Times(0);
  ASSERT_TRUE(PrepareHelperWithFields(/*has_current=*/true, /*has_new=*/true,
                                      /*has_confirm=*/true));
  form_managers().clear();
  driver().reset();

  EXPECT_CALL(credentials_provider(), GetOldPassword()).Times(0);
  EXPECT_CALL(credentials_provider(), GetNewPassword(_)).Times(0);

  TestFuture<ChangePasswordActionResult> future;
  helper().FillChangePasswordForm(future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  ChangePasswordActionResult result = future.Take();
  EXPECT_EQ(result.code, actor::mojom::ActionResultCode::kFrameWentAway);
  EXPECT_FALSE(result.requires_page_stabilization);
  EXPECT_EQ(result.message,
            "Target frame or password manager driver is unavailable.");
}

TEST_F(ChangePasswordFillingHelperTest,
       FillChangePasswordForm_NullPasswordChangeService_Fails) {
  AddFormManager({MakeFormField(10), MakeFormField(20), MakeFormField(30)});
  ChangePasswordFillingHelper helper(kActorSurfaceId,
                                     /*password_change_service=*/nullptr);
  ASSERT_TRUE(helper
                  .ValidateAndSetFieldIds(MakeGlobalId(frame_token(), 10),
                                          MakeGlobalId(frame_token(), 20),
                                          MakeGlobalId(frame_token(), 30))
                  .has_value());
  ASSERT_TRUE(
      helper.ValidateDriverAndTargetFields(*driver(), main_frame_origin())
          .IsOk());
  EXPECT_CALL(*driver(), FillChangePasswordForm).Times(0);

  TestFuture<ChangePasswordActionResult> future;
  helper.FillChangePasswordForm(future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  ChangePasswordActionResult result = future.Take();
  EXPECT_EQ(result.code,
            actor::mojom::ActionResultCode::kFormFillingUnknownAutofillError);
  EXPECT_FALSE(result.requires_page_stabilization);
  EXPECT_EQ(result.message,
            "No active password change flow to provide credentials.");
}

TEST_F(ChangePasswordFillingHelperTest,
       FillChangePasswordForm_NullCredentialsProvider_Fails) {
  EXPECT_CALL(password_change_service(),
              GetCredentialsProviderForActorSurface(kActorSurfaceId))
      .WillOnce(Return(nullptr));
  ASSERT_TRUE(PrepareHelperWithFields(/*has_current=*/true, /*has_new=*/true,
                                      /*has_confirm=*/true));
  EXPECT_CALL(*driver(), FillChangePasswordForm).Times(0);

  TestFuture<ChangePasswordActionResult> future;
  helper().FillChangePasswordForm(future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  ChangePasswordActionResult result = future.Take();
  EXPECT_EQ(result.code,
            actor::mojom::ActionResultCode::kFormFillingUnknownAutofillError);
  EXPECT_FALSE(result.requires_page_stabilization);
  EXPECT_EQ(result.message,
            "No active password change flow to provide credentials.");
}

TEST_F(ChangePasswordFillingHelperTest,
       FillChangePasswordForm_GetOldPasswordFails_ReturnsError) {
  ASSERT_TRUE(PrepareHelperWithFields(/*has_current=*/true, /*has_new=*/true,
                                      /*has_confirm=*/true));
  EXPECT_CALL(credentials_provider(), GetOldPassword())
      .WillOnce(Return(base::unexpected(
          PasswordChangeCredentialsProvider::Error::kNoPasswordChangeFlow)));
  EXPECT_CALL(credentials_provider(), GetNewPassword(_)).Times(0);
  EXPECT_CALL(*driver(), FillChangePasswordForm).Times(0);

  TestFuture<ChangePasswordActionResult> future;
  helper().FillChangePasswordForm(future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  ChangePasswordActionResult result = future.Take();
  EXPECT_EQ(result.code,
            actor::mojom::ActionResultCode::kFormFillingUnknownAutofillError);
  EXPECT_FALSE(result.requires_page_stabilization);
  EXPECT_EQ(result.message,
            "No active password change flow to provide credentials.");
}

TEST_F(ChangePasswordFillingHelperTest,
       FillChangePasswordForm_GetNewPasswordFails_ReturnsError) {
  ASSERT_TRUE(PrepareHelperWithFields(/*has_current=*/true, /*has_new=*/true,
                                      /*has_confirm=*/true));
  EXPECT_CALL(credentials_provider(), GetOldPassword())
      .WillOnce(Return(PasswordString(u"old_password")));
  EXPECT_CALL(credentials_provider(), GetNewPassword(form_managers()[0].get()))
      .WillOnce(Return(base::unexpected(
          PasswordChangeCredentialsProvider::Error::kGenerationFailed)));
  EXPECT_CALL(*driver(), FillChangePasswordForm).Times(0);

  TestFuture<ChangePasswordActionResult> future;
  helper().FillChangePasswordForm(future.GetCallback());
  EXPECT_FALSE(future.IsReady());
  ChangePasswordActionResult result = future.Take();
  EXPECT_EQ(result.code,
            actor::mojom::ActionResultCode::kFormFillingUnknownAutofillError);
  EXPECT_FALSE(result.requires_page_stabilization);
  EXPECT_EQ(result.message, "Failed to generate a new password.");
}

TEST_F(ChangePasswordFillingHelperTest,
       FillChangePasswordForm_OnlyCurrentPassword_DoesNotRequestNewPassword) {
  ASSERT_TRUE(PrepareHelperWithFields(/*has_current=*/true, /*has_new=*/false,
                                      /*has_confirm=*/false));
  const PasswordString kOldPassword(u"current_secret");
  EXPECT_CALL(credentials_provider(), GetOldPassword())
      .WillOnce(Return(kOldPassword));
  EXPECT_CALL(credentials_provider(), GetNewPassword(_)).Times(0);
  autofill::FormData form_data;
  form_data.set_fields(
      {MakeFormField(10, autofill::FormControlType::kInputPassword,
                     /*is_autofilled=*/true)});
  EXPECT_CALL(*driver(),
              FillChangePasswordForm(autofill::FieldRendererId(10),
                                     autofill::FieldRendererId(),
                                     autofill::FieldRendererId(), kOldPassword,
                                     PasswordString(), _))
      .WillOnce(base::test::RunOnceCallback<5>(form_data));

  TestFuture<ChangePasswordActionResult> future;
  helper().FillChangePasswordForm(future.GetCallback());
  ChangePasswordActionResult result = future.Take();
  EXPECT_TRUE(result.IsOk());
}

TEST_F(ChangePasswordFillingHelperTest,
       FillChangePasswordForm_OnlyConfirmPassword_UsesConfirmFieldFormManager) {
  ASSERT_TRUE(PrepareHelperWithFields(/*has_current=*/false, /*has_new=*/false,
                                      /*has_confirm=*/true));
  const PasswordString kNewPassword(u"generated_secret");
  EXPECT_CALL(credentials_provider(), GetOldPassword()).Times(0);
  EXPECT_CALL(credentials_provider(), GetNewPassword(form_managers()[0].get()))
      .WillOnce(Return(kNewPassword));
  autofill::FormData form_data;
  form_data.set_fields(
      {MakeFormField(30, autofill::FormControlType::kInputPassword,
                     /*is_autofilled=*/true)});
  EXPECT_CALL(*driver(),
              FillChangePasswordForm(autofill::FieldRendererId(),
                                     autofill::FieldRendererId(),
                                     autofill::FieldRendererId(30),
                                     PasswordString(), kNewPassword, _))
      .WillOnce(base::test::RunOnceCallback<5>(form_data));

  TestFuture<ChangePasswordActionResult> future;
  helper().FillChangePasswordForm(future.GetCallback());
  ChangePasswordActionResult result = future.Take();
  EXPECT_TRUE(result.IsOk());
}

TEST_F(ChangePasswordFillingHelperTest,
       FillChangePasswordForm_UnparsedFormManager_PassesNullToGetNewPassword) {
  AddFormManager({MakeFormField(20)}, /*driver=*/nullptr,
                 autofill::FormRendererId(1), /*parse_form=*/false);
  ASSERT_EQ(form_managers()[0]->GetParsedObservedForm(), nullptr);
  ASSERT_TRUE(helper()
                  .ValidateAndSetFieldIds(autofill::FieldGlobalId{},
                                          MakeGlobalId(frame_token(), 20),
                                          autofill::FieldGlobalId{})
                  .has_value());
  ASSERT_TRUE(helper()
                  .ValidateDriverAndTargetFields(*driver(), main_frame_origin())
                  .IsOk());

  const PasswordString kNewPassword(u"existing_generated_secret");
  EXPECT_CALL(credentials_provider(), GetOldPassword()).Times(0);
  EXPECT_CALL(credentials_provider(), GetNewPassword(nullptr))
      .WillOnce(Return(kNewPassword));
  autofill::FormData form_data;
  form_data.set_fields(
      {MakeFormField(20, autofill::FormControlType::kInputPassword,
                     /*is_autofilled=*/true)});
  EXPECT_CALL(*driver(),
              FillChangePasswordForm(autofill::FieldRendererId(),
                                     autofill::FieldRendererId(20),
                                     autofill::FieldRendererId(),
                                     PasswordString(), kNewPassword, _))
      .WillOnce(base::test::RunOnceCallback<5>(form_data));

  TestFuture<ChangePasswordActionResult> future;
  helper().FillChangePasswordForm(future.GetCallback());
  ChangePasswordActionResult result = future.Take();
  EXPECT_TRUE(result.IsOk());
}

TEST_F(ChangePasswordFillingHelperTest,
       FillChangePasswordForm_UnparsedNewField_FallsBackToConfirmManager) {
  AddFormManager({MakeFormField(20)}, /*driver=*/nullptr,
                 autofill::FormRendererId(1), /*parse_form=*/false);
  AddFormManager({MakeFormField(30)}, /*driver=*/nullptr,
                 autofill::FormRendererId(2), /*parse_form=*/true);
  ASSERT_EQ(form_managers()[0]->GetParsedObservedForm(), nullptr);
  ASSERT_NE(form_managers()[1]->GetParsedObservedForm(), nullptr);
  ASSERT_TRUE(helper()
                  .ValidateAndSetFieldIds(autofill::FieldGlobalId{},
                                          MakeGlobalId(frame_token(), 20),
                                          MakeGlobalId(frame_token(), 30))
                  .has_value());
  ASSERT_TRUE(helper()
                  .ValidateDriverAndTargetFields(*driver(), main_frame_origin())
                  .IsOk());

  const PasswordString kNewPassword(u"generated_secret");
  EXPECT_CALL(credentials_provider(), GetOldPassword()).Times(0);
  EXPECT_CALL(credentials_provider(), GetNewPassword(form_managers()[1].get()))
      .WillOnce(Return(kNewPassword));
  autofill::FormData form_data;
  form_data.set_fields(
      {MakeFormField(20, autofill::FormControlType::kInputPassword,
                     /*is_autofilled=*/true),
       MakeFormField(30, autofill::FormControlType::kInputPassword,
                     /*is_autofilled=*/true)});
  EXPECT_CALL(*driver(),
              FillChangePasswordForm(autofill::FieldRendererId(),
                                     autofill::FieldRendererId(20),
                                     autofill::FieldRendererId(30),
                                     PasswordString(), kNewPassword, _))
      .WillOnce(base::test::RunOnceCallback<5>(form_data));

  TestFuture<ChangePasswordActionResult> future;
  helper().FillChangePasswordForm(future.GetCallback());
  ChangePasswordActionResult result = future.Take();
  EXPECT_TRUE(result.IsOk());
}

TEST_F(ChangePasswordFillingHelperTest,
       FillChangePasswordForm_BothParsedManagers_PrefersNewFieldManager) {
  AddFormManager({MakeFormField(20)}, /*driver=*/nullptr,
                 autofill::FormRendererId(1), /*parse_form=*/true);
  AddFormManager({MakeFormField(30)}, /*driver=*/nullptr,
                 autofill::FormRendererId(2), /*parse_form=*/true);
  ASSERT_NE(form_managers()[0]->GetParsedObservedForm(), nullptr);
  ASSERT_NE(form_managers()[1]->GetParsedObservedForm(), nullptr);
  ASSERT_TRUE(helper()
                  .ValidateAndSetFieldIds(autofill::FieldGlobalId{},
                                          MakeGlobalId(frame_token(), 20),
                                          MakeGlobalId(frame_token(), 30))
                  .has_value());
  ASSERT_TRUE(helper()
                  .ValidateDriverAndTargetFields(*driver(), main_frame_origin())
                  .IsOk());

  const PasswordString kNewPassword(u"generated_secret");
  EXPECT_CALL(credentials_provider(), GetOldPassword()).Times(0);
  EXPECT_CALL(credentials_provider(), GetNewPassword(form_managers()[0].get()))
      .WillOnce(Return(kNewPassword));
  autofill::FormData form_data;
  form_data.set_fields(
      {MakeFormField(20, autofill::FormControlType::kInputPassword,
                     /*is_autofilled=*/true),
       MakeFormField(30, autofill::FormControlType::kInputPassword,
                     /*is_autofilled=*/true)});
  EXPECT_CALL(*driver(),
              FillChangePasswordForm(autofill::FieldRendererId(),
                                     autofill::FieldRendererId(20),
                                     autofill::FieldRendererId(30),
                                     PasswordString(), kNewPassword, _))
      .WillOnce(base::test::RunOnceCallback<5>(form_data));

  TestFuture<ChangePasswordActionResult> future;
  helper().FillChangePasswordForm(future.GetCallback());
  ChangePasswordActionResult result = future.Take();
  EXPECT_TRUE(result.IsOk());
}

TEST_F(ChangePasswordFillingHelperTest,
       FillChangePasswordForm_CallbackDropped_ReturnsNoEligibleForm) {
  ASSERT_TRUE(PrepareHelperWithFields(/*has_current=*/true, /*has_new=*/true,
                                      /*has_confirm=*/false));

  EXPECT_CALL(*driver(),
              FillChangePasswordForm(autofill::FieldRendererId(10),
                                     autofill::FieldRendererId(20),
                                     autofill::FieldRendererId(), _, _, _))
      .WillOnce(Return());

  TestFuture<ChangePasswordActionResult> future;
  helper().FillChangePasswordForm(future.GetCallback());
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
  EXPECT_CALL(*driver(), FillChangePasswordForm)
      .WillOnce(base::test::RunOnceCallback<5>(form_data));

  TestFuture<ChangePasswordActionResult> future;
  helper().FillChangePasswordForm(future.GetCallback());
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
  EXPECT_CALL(*driver(), FillChangePasswordForm)
      .WillOnce(base::test::RunOnceCallback<5>(form_data));

  TestFuture<ChangePasswordActionResult> future;
  helper().FillChangePasswordForm(future.GetCallback());
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
  EXPECT_CALL(*driver(),
              FillChangePasswordForm(autofill::FieldRendererId(10),
                                     autofill::FieldRendererId(20),
                                     autofill::FieldRendererId(30),
                                     PasswordString(kDefaultOldPassword),
                                     PasswordString(kDefaultNewPassword), _))
      .WillOnce(base::test::RunOnceCallback<5>(form_data));

  TestFuture<ChangePasswordActionResult> future;
  helper().FillChangePasswordForm(future.GetCallback());
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

  EXPECT_CALL(credentials_provider(), GetNewPassword(form_managers()[0].get()))
      .WillOnce(Return(PasswordString(kDefaultNewPassword)));

  autofill::FormData form_data;
  form_data.set_fields(
      {MakeFormField(20, autofill::FormControlType::kInputPassword,
                     /*is_autofilled=*/true)});
  EXPECT_CALL(*driver(),
              FillChangePasswordForm(
                  autofill::FieldRendererId(), autofill::FieldRendererId(20),
                  autofill::FieldRendererId(), PasswordString(),
                  PasswordString(kDefaultNewPassword), _))
      .WillOnce(base::test::RunOnceCallback<5>(form_data));

  TestFuture<ChangePasswordActionResult> future;
  helper().FillChangePasswordForm(future.GetCallback());
  ChangePasswordActionResult result = future.Take();
  EXPECT_TRUE(result.IsOk());
  EXPECT_TRUE(result.requires_page_stabilization);
  EXPECT_EQ(result.message,
            "Filled new generated password into field with id 20.");
}

}  // namespace
}  // namespace password_manager
