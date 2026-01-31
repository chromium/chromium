// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/save_update_address_profile_flow_manager.h"

#include "base/check_deref.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/android/android_theme_resources.h"
#include "chrome/browser/android/resource_mapper.h"
#include "chrome/browser/autofill/android/save_update_address_profile_prompt_mode.h"
#include "chrome/browser/autofill/android/save_update_address_profile_prompt_view.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/android/autofill/save_update_address_profile_flow_manager_test_api.h"
#include "chrome/browser/ui/autofill/autofill_message_model_test_api.h"
#include "chrome/browser/ui/autofill/mock_autofill_message_controller.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/content/browser/test_autofill_client_injector.h"
#include "components/autofill/content/browser/test_content_autofill_client.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile_test_api.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/messages/android/message_wrapper.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

using ::testing::_;
using ::testing::Property;
using ::testing::Return;
using ProfileRef = base::optional_ref<const AutofillProfile>;

class MockSaveUpdateAddressProfilePromptView
    : public SaveUpdateAddressProfilePromptView {
 public:
  MOCK_METHOD(bool,
              Show,
              (SaveUpdateAddressProfilePromptController * controller,
               const AutofillProfile& autofill_profile,
               SaveUpdateAddressProfilePromptMode prompt_mode),
              (override));
};

class TestSaveUpdateAddressProfileFlowManager
    : public SaveUpdateAddressProfileFlowManager {
 public:
  using SaveUpdateAddressProfileFlowManager::
      SaveUpdateAddressProfileFlowManager;

  std::unique_ptr<SaveUpdateAddressProfilePromptView> CreatePromptView()
      override {
    auto prompt_view =
        std::make_unique<MockSaveUpdateAddressProfilePromptView>();
    ON_CALL(*prompt_view, Show).WillByDefault(Return(true));
    return prompt_view;
  }
};

class SaveUpdateAutofillClient : public TestContentAutofillClient {
 public:
  using TestContentAutofillClient::TestContentAutofillClient;

  ~SaveUpdateAutofillClient() override = default;

  SaveUpdateAddressProfileFlowManager*
  GetSaveUpdateAddressProfileFlowManager() {
    return &flow_manager_;
  }

  MockAutofillMessageController& message_controller() {
    return message_controller_;
  }

 private:
  MockAutofillMessageController message_controller_;
  TestSaveUpdateAddressProfileFlowManager flow_manager_{this,
                                                        &message_controller_};
};

class SaveUpdateAddressProfileFlowManagerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  SaveUpdateAddressProfileFlowManagerTest() = default;

 protected:
  void SetUp() override;
  void TearDown() override;
  TestingProfile::TestingFactories GetTestingFactories() const override;

  void SigninUser(const std::string& email, signin::ConsentLevel consent_level);
  void OfferSave(const AutofillProfile& profile,
                 const AutofillProfile* original_profile,
                 SaveUpdateAddressProfilePromptMode prompt_mode);

  void TriggerPrimaryAction();
  void TriggerMessageDismissed(messages::DismissReason dismiss_reason);

  SaveUpdateAutofillClient& autofill_client() {
    return CHECK_DEREF(autofill_client_injector_[web_contents()]);
  }

  MockAutofillMessageController& message_controller() {
    return autofill_client().message_controller();
  }
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback>&
  save_callback() {
    return save_callback_;
  }

  AutofillProfile& profile() { return *profile_; }
  AutofillProfile* original_profile() { return original_profile_.get(); }
  SaveUpdateAddressProfileFlowManager* flow_manager() {
    return autofill_client().GetSaveUpdateAddressProfileFlowManager();
  }
  AutofillMessageModel* message_model() { return message_model_.get(); }
  messages::MessageWrapper* GetMessageWrapper() {
    return &test_api(*message_model_).GetMessage();
  }

 private:
  TestAutofillClientInjector<SaveUpdateAutofillClient>
      autofill_client_injector_;
  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback>
      save_callback_;

  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  std::unique_ptr<AutofillProfile> profile_;
  std::unique_ptr<AutofillProfile> original_profile_;
  std::unique_ptr<AutofillMessageModel> message_model_;
};

void SaveUpdateAddressProfileFlowManagerTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();
  identity_test_env_adaptor_ =
      std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
          ChromeRenderViewHostTestHarness::profile());
  profile_ = std::make_unique<AutofillProfile>(test::GetFullProfile());
  original_profile_ =
      std::make_unique<AutofillProfile>(test::GetFullProfile2());
}

void SaveUpdateAddressProfileFlowManagerTest::TearDown() {
  identity_test_env_adaptor_.reset();
  ChromeRenderViewHostTestHarness::TearDown();
}

TestingProfile::TestingFactories
SaveUpdateAddressProfileFlowManagerTest::GetTestingFactories() const {
  return IdentityTestEnvironmentProfileAdaptor::
      GetIdentityTestEnvironmentFactories();
}

void SaveUpdateAddressProfileFlowManagerTest::SigninUser(
    const std::string& email,
    signin::ConsentLevel consent_level) {
  identity_test_env_adaptor_->identity_test_env()->MakePrimaryAccountAvailable(
      email, consent_level);
}

void SaveUpdateAddressProfileFlowManagerTest::OfferSave(
    const AutofillProfile& profile,
    const AutofillProfile* original_profile,
    SaveUpdateAddressProfilePromptMode prompt_mode) {
  EXPECT_CALL(message_controller(), Show(_))
      .WillOnce([&](std::unique_ptr<AutofillMessageModel> model) {
        message_model_ = std::move(model);
      });
  flow_manager()->OfferSave(profile, original_profile, prompt_mode,
                            save_callback_.Get());
}

void SaveUpdateAddressProfileFlowManagerTest::TriggerPrimaryAction() {
  message_model_->OnActionClicked();
}

void SaveUpdateAddressProfileFlowManagerTest::TriggerMessageDismissed(
    messages::DismissReason dismiss_reason) {
  message_model_->OnDismissed(dismiss_reason);
}

// Tests that the save message properties (title, description with profile
// details, primary button text, icon) are set correctly during local or sync
// address profile saving process.
TEST_F(SaveUpdateAddressProfileFlowManagerTest,
       SaveMessageContent_LocalOrSyncAddressProfile) {
  OfferSave(profile(), /*original_profile=*/nullptr,
            SaveUpdateAddressProfilePromptMode::kSaveNewProfile);

  ASSERT_TRUE(message_model());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE),
            GetMessageWrapper()->GetTitle());
  EXPECT_EQ(u"John H. Doe, 666 Erebus St.",
            GetMessageWrapper()->GetDescription());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL),
            GetMessageWrapper()->GetPrimaryButtonText());
  EXPECT_EQ(1, GetMessageWrapper()->GetPrimaryButtonTextMaxLines());
  EXPECT_EQ(SaveUpdateAddressProfileFlowManager::kMessageDescriptionMaxLines,
            GetMessageWrapper()->GetDescriptionMaxLines());
  EXPECT_EQ(ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_AUTOFILL_ADDRESS),
            GetMessageWrapper()->GetIconResourceId());

  TriggerMessageDismissed(messages::DismissReason::UNKNOWN);
}

// Tests that the save message properties are set correctly during address
// profile migration flow.
TEST_F(SaveUpdateAddressProfileFlowManagerTest,
       SaveMessageContent_AddressProfileMigrationFlow) {
  test_api(profile()).set_record_type(AutofillProfile::RecordType::kAccount);
  SigninUser(TestingProfile::kDefaultProfileUserName,
             signin::ConsentLevel::kSignin);
  OfferSave(profile(), /*original_profile=*/nullptr,
            SaveUpdateAddressProfilePromptMode::kMigrateProfile);

  ASSERT_TRUE(message_model());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_ACCOUNT_MIGRATE_ADDRESS_PROMPT_TITLE),
            GetMessageWrapper()->GetTitle());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_SAVE_IN_ACCOUNT_MESSAGE_ADDRESS_MIGRATION_RECORD_TYPE_NOTICE),
      GetMessageWrapper()->GetDescription());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL),
            GetMessageWrapper()->GetPrimaryButtonText());
  EXPECT_EQ(1, GetMessageWrapper()->GetPrimaryButtonTextMaxLines());
  EXPECT_EQ(SaveUpdateAddressProfileFlowManager::kMessageDescriptionMaxLines,
            GetMessageWrapper()->GetDescriptionMaxLines());
  EXPECT_EQ(
      ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_AUTOFILL_UPLOAD_ADDRESS),
      GetMessageWrapper()->GetIconResourceId());

  TriggerMessageDismissed(messages::DismissReason::UNKNOWN);
}

// Tests that the save message properties are set correctly when a new address
// profile is saved in account.
TEST_F(SaveUpdateAddressProfileFlowManagerTest,
       SaveMessageContent_AccountAddressProfile) {
  test_api(profile()).set_record_type(AutofillProfile::RecordType::kAccount);
  SigninUser(TestingProfile::kDefaultProfileUserName,
             signin::ConsentLevel::kSignin);
  OfferSave(profile(), /*original_profile=*/nullptr,
            SaveUpdateAddressProfilePromptMode::kSaveNewProfile);

  ASSERT_TRUE(message_model());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE),
            GetMessageWrapper()->GetTitle());
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_AUTOFILL_SAVE_IN_ACCOUNT_MESSAGE_ADDRESS_RECORD_TYPE_NOTICE,
                base::ASCIIToUTF16(TestingProfile::kDefaultProfileUserName)),
            GetMessageWrapper()->GetDescription());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL),
            GetMessageWrapper()->GetPrimaryButtonText());
  EXPECT_EQ(1, GetMessageWrapper()->GetPrimaryButtonTextMaxLines());
  EXPECT_EQ(SaveUpdateAddressProfileFlowManager::kMessageDescriptionMaxLines,
            GetMessageWrapper()->GetDescriptionMaxLines());
  EXPECT_EQ(ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_AUTOFILL_ADDRESS),
            GetMessageWrapper()->GetIconResourceId());

  TriggerMessageDismissed(messages::DismissReason::UNKNOWN);
}

// Tests that the update message properties are set correctly.
TEST_F(SaveUpdateAddressProfileFlowManagerTest, UpdateMessageContent) {
  OfferSave(profile(), original_profile(),
            SaveUpdateAddressProfilePromptMode::kUpdateProfile);

  ASSERT_TRUE(message_model());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_TITLE),
            GetMessageWrapper()->GetTitle());
  EXPECT_EQ(u"Jane A. Smith, 123 Main Street",
            GetMessageWrapper()->GetDescription());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_UPDATE_ADDRESS_PROMPT_OK_BUTTON_LABEL),
            GetMessageWrapper()->GetPrimaryButtonText());
  EXPECT_EQ(1, GetMessageWrapper()->GetPrimaryButtonTextMaxLines());
  EXPECT_EQ(SaveUpdateAddressProfileFlowManager::kMessageDescriptionMaxLines,
            GetMessageWrapper()->GetDescriptionMaxLines());
  EXPECT_EQ(ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_AUTOFILL_ADDRESS),
            GetMessageWrapper()->GetIconResourceId());

  TriggerMessageDismissed(messages::DismissReason::UNKNOWN);
}

// Tests that the update message properties (title, description with original
// profile details, primary button text, icon) are set correctly for Home
// profile.
TEST_F(SaveUpdateAddressProfileFlowManagerTest,
       HomeAddressUpdateMessageContent) {
  test_api(*original_profile())
      .set_record_type(AutofillProfile::RecordType::kAccountHome);
  OfferSave(profile(), original_profile(),
            SaveUpdateAddressProfilePromptMode::kUpdateProfile);

  ASSERT_TRUE(message_model());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_TITLE),
            GetMessageWrapper()->GetTitle());
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_SAVE_ADDRESS_PROMPT_OK_BUTTON_LABEL),
            GetMessageWrapper()->GetPrimaryButtonText());
  EXPECT_EQ(1, GetMessageWrapper()->GetPrimaryButtonTextMaxLines());
  EXPECT_EQ(u"Jane A. Smith, 123 Main Street",
            GetMessageWrapper()->GetDescription());
  EXPECT_EQ(SaveUpdateAddressProfileFlowManager::kMessageDescriptionMaxLines,
            GetMessageWrapper()->GetDescriptionMaxLines());
  EXPECT_EQ(ResourceMapper::MapToJavaDrawableId(IDR_ANDROID_AUTOFILL_ADDRESS),
            GetMessageWrapper()->GetIconResourceId());

  TriggerMessageDismissed(messages::DismissReason::UNKNOWN);
}

// Tests that the action callback is triggered when the user clicks on the
// primary action button of the save message.
TEST_F(SaveUpdateAddressProfileFlowManagerTest, ProceedOnActionClickWhenSave) {
  OfferSave(profile(), /*original_profile=*/nullptr,
            SaveUpdateAddressProfilePromptMode::kSaveNewProfile);
  EXPECT_FALSE(SaveUpdateAddressProfileFlowManagerTestApi(*flow_manager())
                   .GetPromptController());

  EXPECT_CALL(save_callback(), Run(_, Property(&ProfileRef::has_value, false)))
      .Times(0);
  TriggerPrimaryAction();

  EXPECT_TRUE(SaveUpdateAddressProfileFlowManagerTestApi(*flow_manager())
                  .GetPromptController());

  EXPECT_CALL(save_callback(),
              Run(AutofillClient::AddressPromptUserDecision::kIgnored,
                  Property(&ProfileRef::has_value, false)));
  TriggerMessageDismissed(messages::DismissReason::PRIMARY_ACTION);
}

// Tests that the action callback is triggered when the user clicks on the
// primary action button of the update message.
TEST_F(SaveUpdateAddressProfileFlowManagerTest,
       ProceedOnActionClickWhenUpdate) {
  OfferSave(profile(), original_profile(),
            SaveUpdateAddressProfilePromptMode::kUpdateProfile);
  EXPECT_FALSE(SaveUpdateAddressProfileFlowManagerTestApi(*flow_manager())
                   .GetPromptController());

  EXPECT_CALL(save_callback(), Run(_, Property(&ProfileRef::has_value, false)))
      .Times(0);
  TriggerPrimaryAction();

  EXPECT_TRUE(SaveUpdateAddressProfileFlowManagerTestApi(*flow_manager())
                  .GetPromptController());

  EXPECT_CALL(save_callback(),
              Run(AutofillClient::AddressPromptUserDecision::kIgnored,
                  Property(&ProfileRef::has_value, false)));
  TriggerMessageDismissed(messages::DismissReason::PRIMARY_ACTION);
}

// Tests that the save callback is triggered with
// `AddressPromptUserDecision::kMessageDeclined` when the user
// dismisses the message via gesture.
TEST_F(SaveUpdateAddressProfileFlowManagerTest,
       DecisionIsMessageDeclinedOnGestureDismiss) {
  OfferSave(profile(), /*original_profile=*/nullptr,
            SaveUpdateAddressProfilePromptMode::kSaveNewProfile);

  EXPECT_CALL(save_callback(),
              Run(AutofillClient::AddressPromptUserDecision::kMessageDeclined,
                  Property(&ProfileRef::has_value, false)));
  TriggerMessageDismissed(messages::DismissReason::GESTURE);
}

// Tests that the save callback is triggered with
// `AddressPromptUserDecision::kMessageTimeout` when the message is
// auto-dismissed after a timeout.
TEST_F(SaveUpdateAddressProfileFlowManagerTest,
       DecisionIsMessageTimeoutOnTimerAutodismiss) {
  OfferSave(profile(), /*original_profile=*/nullptr,
            SaveUpdateAddressProfilePromptMode::kSaveNewProfile);

  EXPECT_CALL(save_callback(),
              Run(AutofillClient::AddressPromptUserDecision::kMessageTimeout,
                  Property(&ProfileRef::has_value, false)));
  TriggerMessageDismissed(messages::DismissReason::TIMER);
}

// Tests that the new message gets auto declined.
TEST_F(SaveUpdateAddressProfileFlowManagerTest, OnlyOnePromptAtATime) {
  OfferSave(profile(), /*original_profile=*/nullptr,
            SaveUpdateAddressProfilePromptMode::kSaveNewProfile);

  base::MockCallback<AutofillClient::AddressProfileSavePromptCallback>
      another_save_callback;

  EXPECT_CALL(another_save_callback,
              Run(AutofillClient::AddressPromptUserDecision::kAutoDeclined,
                  Property(&ProfileRef::has_value, false)));

  EXPECT_CALL(save_callback(), Run).Times(0);

  flow_manager()->OfferSave(*original_profile(),
                            /*original_profile=*/nullptr,
                            SaveUpdateAddressProfilePromptMode::kSaveNewProfile,
                            another_save_callback.Get());
}

}  // namespace

}  // namespace autofill
