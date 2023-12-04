// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_external_delegate.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_compose_delegate.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/browser/autofill_granular_filling_utils.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/granular_filling_metrics.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/mock_autofill_compose_delegate.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_test_helpers.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/plus_addresses/plus_address_metrics.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "url/origin.h"

namespace autofill {

namespace {

using test::CreateTestAddressFormData;
using test::CreateTestCreditCardFormData;
using test::CreateTestPersonalInformationFormData;
using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::Matcher;
using ::testing::Mock;
using ::testing::NiceMock;
using ::testing::Property;
using ::testing::Return;
using ::testing::SizeIs;
using ::testing::StartsWith;

using SuggestionPosition = autofill::AutofillPopupDelegate::SuggestionPosition;

constexpr auto kDefaultTriggerSource =
    AutofillSuggestionTriggerSource::kFormControlElementClicked;

constexpr std::string_view kPlusAddressSuggestionMetric =
    "Autofill.PlusAddresses.Suggestion.Events";

// Creates a `PopupItemId::kFieldByFieldFilling` suggestion.
// `guid` is used to set `Suggestion::payload` as
// `Suggestion::Guid(guid)`. This method also sets the
// `Suggestion::field_by_field_filling_type_used` to `fbf_type_used`.
Suggestion CreateFieldByFieldFillingSuggestion(const std::string& guid,
                                               ServerFieldType fbf_type_used) {
  Suggestion suggestion =
      test::CreateAutofillSuggestion(PopupItemId::kFieldByFieldFilling,
                                     u"field by field", Suggestion::Guid(guid));
  suggestion.field_by_field_filling_type_used = std::optional(fbf_type_used);
  return suggestion;
}

Matcher<const AutofillTriggerDetails&> EqualsAutofillTriggerDetails(
    AutofillTriggerDetails details) {
  return AllOf(
      Field(&AutofillTriggerDetails::trigger_source, details.trigger_source),
      Field(&AutofillTriggerDetails::field_types_to_fill,
            details.field_types_to_fill));
}

template <typename SuggestionsMatcher>
auto PopupOpenArgsAre(
    SuggestionsMatcher suggestions_matcher,
    AutofillSuggestionTriggerSource trigger_source = kDefaultTriggerSource) {
  using PopupOpenArgs = AutofillClient::PopupOpenArgs;
  return AllOf(Field(&PopupOpenArgs::suggestions, suggestions_matcher),
               Field(&PopupOpenArgs::trigger_source, trigger_source));
}

class MockPersonalDataManager : public TestPersonalDataManager {
 public:
  MockPersonalDataManager() = default;
  ~MockPersonalDataManager() override = default;
  MOCK_METHOD(void, AddObserver, (PersonalDataManagerObserver*), (override));
  MOCK_METHOD(void, RemoveObserver, (PersonalDataManagerObserver*), (override));
  MOCK_METHOD(bool, IsAutofillProfileEnabled, (), (const override));
  MOCK_METHOD(void, UpdateProfile, (const AutofillProfile&), (override));
  MOCK_METHOD(void, RemoveByGUID, (const std::string&), (override));
};

class MockAutofillDriver : public TestAutofillDriver {
 public:
  MockAutofillDriver() = default;
  MockAutofillDriver(const MockAutofillDriver&) = delete;
  MockAutofillDriver& operator=(const MockAutofillDriver&) = delete;
  // Mock methods to enable testability.
  MOCK_METHOD(void,
              RendererShouldAcceptDataListSuggestion,
              (const FieldGlobalId&, const std::u16string&),
              (override));
  MOCK_METHOD(void, RendererShouldClearFilledSection, (), (override));
  MOCK_METHOD(void, RendererShouldClearPreviewedForm, (), (override));
  MOCK_METHOD(void,
              RendererShouldTriggerSuggestions,
              (const FieldGlobalId&, AutofillSuggestionTriggerSource),
              (override));
};

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient() = default;
  MockAutofillClient(const MockAutofillClient&) = delete;
  MockAutofillClient& operator=(const MockAutofillClient&) = delete;
  MOCK_METHOD(void,
              ScanCreditCard,
              (CreditCardScanCallback callbacK),
              (override));
  MOCK_METHOD(void,
              ShowAutofillPopup,
              (const autofill::AutofillClient::PopupOpenArgs& open_args,
               base::WeakPtr<AutofillPopupDelegate> delegate),
              (override));
  MOCK_METHOD(void,
              UpdateAutofillPopupDataListValues,
              (base::span<const SelectOption> options),
              (override));
  MOCK_METHOD(void, HideAutofillPopup, (PopupHidingReason), (override));
  MOCK_METHOD(void,
              OpenPromoCodeOfferDetailsURL,
              (const GURL& url),
              (override));
  MOCK_METHOD(plus_addresses::PlusAddressService*,
              GetPlusAddressService,
              (),
              (override));
  MOCK_METHOD(void,
              OfferPlusAddressCreation,
              (const url::Origin&, plus_addresses::PlusAddressCallback),
              (override));
  MOCK_METHOD(AutofillComposeDelegate*, GetComposeDelegate, (), (override));
  MOCK_METHOD(void,
              ShowEditAddressProfileDialog,
              (const AutofillProfile&, AddressProfileSavePromptCallback),
              (override));
  MOCK_METHOD(void,
              ShowDeleteAddressProfileDialog,
              (const AutofillProfile&, AddressProfileDeleteDialogCallback),
              (override));

#if BUILDFLAG(IS_IOS)
  // Mock the client query ID check.
  bool IsLastQueriedField(FieldGlobalId field_id) override {
    return !last_queried_field_id_ || last_queried_field_id_ == field_id;
  }

  void set_last_queried_field(FieldGlobalId field_id) {
    last_queried_field_id_ = field_id;
  }
 private:
  FieldGlobalId last_queried_field_id_;
#endif
};

class MockBrowserAutofillManager : public TestBrowserAutofillManager {
 public:
  MockBrowserAutofillManager(AutofillDriver* driver, MockAutofillClient* client)
      : TestBrowserAutofillManager(driver, client) {}
  MockBrowserAutofillManager(const MockBrowserAutofillManager&) = delete;
  MockBrowserAutofillManager& operator=(const MockBrowserAutofillManager&) =
      delete;

  MOCK_METHOD(bool,
              ShouldShowScanCreditCard,
              (const FormData& form, const FormFieldData& field),
              (override));
  MOCK_METHOD(void,
              OnUserHideSuggestions,
              (const FormData& form, const FormFieldData& field),
              (override));
  MOCK_METHOD(void,
              FillOrPreviewCreditCardForm,
              (mojom::ActionPersistence action_persistence,
               const FormData& form,
               const FormFieldData& field,
               const CreditCard& credit_card,
               const AutofillTriggerDetails& trigger_details),
              (override));

  bool ShouldShowCardsFromAccountOption(const FormData& form,
                                        const FormFieldData& field) override {
    return should_show_cards_from_account_option_;
  }

  void ShowCardsFromAccountOption() {
    should_show_cards_from_account_option_ = true;
  }
  MOCK_METHOD(void,
              UndoAutofill,
              (mojom::ActionPersistence action_persistence,
               FormData form,
               const FormFieldData& trigger_field),
              (override));
  MOCK_METHOD(void,
              FillOrPreviewProfileForm,
              (mojom::ActionPersistence,
               const FormData&,
               const FormFieldData&,
               const AutofillProfile&,
               const AutofillTriggerDetails&),
              (override));
  MOCK_METHOD(void,
              FillCreditCardForm,
              (const FormData& form,
               const FormFieldData& field,
               const CreditCard& credit_card,
               const std::u16string& cvc,
               const AutofillTriggerDetails& trigger_details),
              (override));
  MOCK_METHOD(void,
              FillOrPreviewField,
              (mojom::ActionPersistence,
               mojom::TextReplacement,
               const FormData&,
               const FormFieldData&,
               const std::u16string&,
               PopupItemId),
              (override));

 private:
  bool should_show_cards_from_account_option_ = false;
};

}  // namespace

class AutofillExternalDelegateUnitTest : public testing::Test {
 protected:
  void SetUp() override {
    client().set_personal_data_manager(
        std::make_unique<MockPersonalDataManager>());
    autofill_driver_ = std::make_unique<NiceMock<MockAutofillDriver>>();
    driver().set_autofill_manager(
        std::make_unique<NiceMock<MockBrowserAutofillManager>>(
            autofill_driver_.get(), &client()));
  }

  // Issue an OnQuery call.
  void IssueOnQuery() {
    FormGlobalId form_id = test::MakeFormGlobalId();
    queried_form_ = test::GetFormData({
        .fields = {{.role = NAME_FIRST,
                    .host_frame = queried_form_triggering_field_id_.frame_token,
                    .unique_renderer_id =
                        queried_form_triggering_field_id_.renderer_id,
                    .autocomplete_attribute = "given-name"}},
        .host_frame = form_id.frame_token,
        .unique_renderer_id = form_id.renderer_id,
    });
    external_delegate().OnQuery(queried_form_, queried_form_.fields[0],
                                gfx::RectF());
  }

  // Returns the triggering `AutofillField`. This is the only field in the form
  // created in `IssueOnQuery()`.
  AutofillField* get_triggering_autofill_field() {
    return manager().GetAutofillField(queried_form_, queried_form_.fields[0]);
  }

  void IssueOnSuggestionsReturned(FieldGlobalId field_id) {
    std::vector<Suggestion> suggestions;
    suggestions.emplace_back();
    suggestions[0].popup_item_id = PopupItemId::kAddressEntry;
    external_delegate().OnSuggestionsReturned(field_id, suggestions,
                                              kDefaultTriggerSource);
  }

  Matcher<const FormData&> HasQueriedFormId() {
    return Property(&FormData::global_id, queried_form_.global_id());
  }

  Matcher<const FormFieldData&> HasQueriedFieldId() {
    return Property(&FormFieldData::global_id,
                    queried_form_triggering_field_id_);
  }

  void DestroyAutofillDriver() { autofill_driver_.reset(); }

  MockAutofillClient& client() { return autofill_client_; }
  MockAutofillDriver& driver() { return *autofill_driver_; }
  AutofillExternalDelegate& external_delegate() {
    return *test_api(manager()).external_delegate();
  }
  MockBrowserAutofillManager& manager() {
    return static_cast<MockBrowserAutofillManager&>(
        driver().GetAutofillManager());
  }
  MockPersonalDataManager& pdm() {
    return *static_cast<MockPersonalDataManager*>(
        client().GetPersonalDataManager());
  }

  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;

  // Form containing the triggering field that initialized the external delegate
  // `OnQuery`.
  FormData queried_form_;
  // Triggering field id, it is the only field in `form_`.
  FieldGlobalId queried_form_triggering_field_id_ = test::MakeFieldGlobalId();

 private:
  NiceMock<MockAutofillClient> autofill_client_;
  std::unique_ptr<MockAutofillDriver> autofill_driver_;
};

// Variant for use in cases when we expect the BrowserAutofillManager would
// normally set the |should_show_cards_from_account_option_| bit.
class AutofillExternalDelegateCardsFromAccountTest
    : public AutofillExternalDelegateUnitTest {
 protected:
  void SetUp() override {
    AutofillExternalDelegateUnitTest::SetUp();
    manager().ShowCardsFromAccountOption();
  }
};

TEST_F(AutofillExternalDelegateUnitTest, GetPopupTypeForCreditCardForm) {
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  manager().OnFormsSeen({form}, {});

  for (const FormFieldData& field : form.fields) {
    external_delegate().OnQuery(form, field, gfx::RectF());
    EXPECT_EQ(PopupType::kCreditCards, external_delegate().GetPopupType());
  }
}

TEST_F(AutofillExternalDelegateUnitTest, GetPopupTypeForAddressForm) {
  FormData form = CreateTestAddressFormData();
  manager().OnFormsSeen({form}, {});

  for (const FormFieldData& field : form.fields) {
    external_delegate().OnQuery(form, field, gfx::RectF());
    EXPECT_EQ(PopupType::kAddresses, external_delegate().GetPopupType());
  }
}

TEST_F(AutofillExternalDelegateUnitTest,
       GetPopupTypeForPersonalInformationForm) {
  FormData form = CreateTestPersonalInformationFormData();
  manager().OnFormsSeen({form}, {});

  for (const FormFieldData& field : form.fields) {
    external_delegate().OnQuery(form, field, gfx::RectF());
    EXPECT_EQ(PopupType::kPersonalInformation,
              external_delegate().GetPopupType());
  }
}

// Test that the address editor is not shown if there's no Autofill profile with
// the provided GUID.
TEST_F(AutofillExternalDelegateUnitTest, ShowEditorForNonexistingProfile) {
  IssueOnQuery();

  const std::string guid = base::Uuid().AsLowercaseString();
  EXPECT_CALL(client(), ShowEditAddressProfileDialog).Times(0);

  auto suggestion = Suggestion(PopupItemId::kEditAddressProfile);
  suggestion.payload = Suggestion::Guid(guid);
  external_delegate().DidAcceptSuggestion(
      suggestion, SuggestionPosition{.row = 0},
      AutofillSuggestionTriggerSource::kFormControlElementClicked);
}

// Test that the address editor is shown for the GUID identifying existing
// Autofill profile.
TEST_F(AutofillExternalDelegateUnitTest, ShowEditorForExistingProfile) {
  IssueOnQuery();

  const AutofillProfile profile = test::GetFullProfile();
  pdm().AddProfile(profile);
  EXPECT_CALL(client(), ShowEditAddressProfileDialog(profile, _));

  auto suggestion = Suggestion(PopupItemId::kEditAddressProfile);
  suggestion.payload = Suggestion::Guid(profile.guid());
  external_delegate().DidAcceptSuggestion(
      suggestion, SuggestionPosition{.row = 0},
      AutofillSuggestionTriggerSource::kFormControlElementClicked);
}

// Test that the editor changes are not persisted if the user has canceled
// editing.
TEST_F(AutofillExternalDelegateUnitTest, UserCancelsEditing) {
  IssueOnQuery();

  base::HistogramTester histogram;
  const AutofillProfile profile = test::GetFullProfile();
  pdm().AddProfile(profile);
  EXPECT_CALL(client(), ShowEditAddressProfileDialog(profile, _))
      .WillOnce([](auto profile, auto save_prompt_callback) {
        std::move(save_prompt_callback)
            .Run(AutofillClient::SaveAddressProfileOfferUserDecision::
                     kEditDeclined,
                 profile);
      });
  // No changes should be saved when user cancels editing.
  EXPECT_CALL(pdm(), AddObserver).Times(0);
  EXPECT_CALL(pdm(), UpdateProfile).Times(0);
  // The Autofill popup must be reopened when editor dialog is closed.
  EXPECT_CALL(
      driver(),
      RendererShouldTriggerSuggestions(
          queried_form_triggering_field_id_,
          AutofillSuggestionTriggerSource::kShowPromptAfterDialogClosed));

  auto suggestion = Suggestion(PopupItemId::kEditAddressProfile);
  suggestion.payload = Suggestion::Guid(profile.guid());
  external_delegate().DidAcceptSuggestion(
      suggestion, SuggestionPosition{.row = 0},
      AutofillSuggestionTriggerSource::kFormControlElementClicked);
  histogram.ExpectUniqueSample("Autofill.ExtendedMenu.EditAddress", 0, 1);
}

// Test that the editor changes are persisted if the user has canceled editing.
TEST_F(AutofillExternalDelegateUnitTest, UserSavesEdits) {
  IssueOnQuery();

  base::HistogramTester histogram;
  const AutofillProfile profile = test::GetFullProfile();
  pdm().AddProfile(profile);
  EXPECT_CALL(client(), ShowEditAddressProfileDialog(profile, _))
      .WillOnce([](auto profile, auto save_prompt_callback) {
        std::move(save_prompt_callback)
            .Run(AutofillClient::SaveAddressProfileOfferUserDecision::
                     kEditAccepted,
                 profile);
      });
  // Updated Autofill profile must be persisted when user saves changes through
  // the address editor.
  EXPECT_CALL(pdm(), AddObserver(&external_delegate()));
  EXPECT_CALL(pdm(), UpdateProfile(profile));
  // The Autofill popup must be reopened when editor dialog is closed.
  EXPECT_CALL(
      driver(),
      RendererShouldTriggerSuggestions(
          queried_form_triggering_field_id_,
          AutofillSuggestionTriggerSource::kShowPromptAfterDialogClosed));

  auto suggestion = Suggestion(PopupItemId::kEditAddressProfile);
  suggestion.payload = Suggestion::Guid(profile.guid());
  external_delegate().DidAcceptSuggestion(
      suggestion, SuggestionPosition{.row = 0},
      AutofillSuggestionTriggerSource::kFormControlElementClicked);

  external_delegate().OnPersonalDataFinishedProfileTasks();
  histogram.ExpectUniqueSample("Autofill.ExtendedMenu.EditAddress", 1, 1);
}

// Test the situation when database changes take long enough for the user to
// open the address editor for the second time.
TEST_F(AutofillExternalDelegateUnitTest,
       UserOpensEditorTwiceBeforeProfileIsPersisted) {
  IssueOnQuery();

  const AutofillProfile profile = test::GetFullProfile();
  pdm().AddProfile(profile);
  EXPECT_CALL(client(), ShowEditAddressProfileDialog(profile, _))
      .Times(2)
      .WillRepeatedly([](auto profile, auto save_prompt_callback) {
        std::move(save_prompt_callback)
            .Run(AutofillClient::SaveAddressProfileOfferUserDecision::
                     kEditAccepted,
                 profile);
      });
  // PDM observer must be added only once.
  EXPECT_CALL(pdm(), AddObserver(&external_delegate()));
  // Changes to the Autofill profile must be persisted both times.
  EXPECT_CALL(pdm(), UpdateProfile(profile)).Times(2);

  auto suggestion = Suggestion(PopupItemId::kEditAddressProfile);
  suggestion.payload = Suggestion::Guid(profile.guid());

  external_delegate().DidAcceptSuggestion(
      suggestion, SuggestionPosition{.row = 0},
      AutofillSuggestionTriggerSource::kFormControlElementClicked);
  external_delegate().DidAcceptSuggestion(
      suggestion, SuggestionPosition{.row = 0},
      AutofillSuggestionTriggerSource::kFormControlElementClicked);
}

// Test the situation when AutofillExternalDelegate is destroyed before the
// PersonalDataManager observer is notified that all tasks have been processed.
TEST_F(AutofillExternalDelegateUnitTest,
       DelegateIsDestroyedBeforeUpdateIsFinished) {
  IssueOnQuery();

  const AutofillProfile profile = test::GetFullProfile();
  pdm().AddProfile(profile);
  EXPECT_CALL(client(), ShowEditAddressProfileDialog(profile, _))
      .WillOnce([](auto profile, auto save_prompt_callback) {
        std::move(save_prompt_callback)
            .Run(AutofillClient::SaveAddressProfileOfferUserDecision::
                     kEditAccepted,
                 profile);
      });

  EXPECT_CALL(pdm(), AddObserver(&external_delegate()));
  EXPECT_CALL(pdm(), UpdateProfile(profile));

  auto suggestion = Suggestion(PopupItemId::kEditAddressProfile);
  suggestion.payload = Suggestion::Guid(profile.guid());
  external_delegate().DidAcceptSuggestion(
      suggestion, SuggestionPosition{.row = 0},
      AutofillSuggestionTriggerSource::kFormControlElementClicked);

  EXPECT_CALL(pdm(), RemoveObserver(&external_delegate()));
  DestroyAutofillDriver();
}

// Test that the delete dialog is not shown if there's no Autofill profile with
// the provided GUID.
TEST_F(AutofillExternalDelegateUnitTest,
       ShowDeleteDialogForNonexistingProfile) {
  IssueOnQuery();

  const std::string guid = base::Uuid().AsLowercaseString();
  EXPECT_CALL(client(), ShowDeleteAddressProfileDialog).Times(0);
  auto suggestion = Suggestion(PopupItemId::kDeleteAddressProfile);
  suggestion.payload = Suggestion::Guid(guid);

  external_delegate().DidAcceptSuggestion(
      suggestion, SuggestionPosition{.row = 0},
      AutofillSuggestionTriggerSource::kFormControlElementClicked);
}

// Test that the delete dialog is shown for the GUID identifying existing
// Autofill profile.
TEST_F(AutofillExternalDelegateUnitTest, ShowDeleteDialog) {
  IssueOnQuery();

  const AutofillProfile profile = test::GetFullProfile();
  pdm().AddProfile(profile);
  EXPECT_CALL(client(), ShowDeleteAddressProfileDialog(profile, _));
  auto suggestion = Suggestion(PopupItemId::kDeleteAddressProfile);
  suggestion.payload = Suggestion::Guid(profile.guid());

  external_delegate().DidAcceptSuggestion(
      suggestion, SuggestionPosition{.row = 0},
      AutofillSuggestionTriggerSource::kFormControlElementClicked);
}

// Test that the Autofill profile is not deleted when user cancels the deletion
// process.
TEST_F(AutofillExternalDelegateUnitTest, UserCancelsDeletion) {
  IssueOnQuery();

  base::HistogramTester histogram;
  const AutofillProfile profile = test::GetFullProfile();
  pdm().AddProfile(profile);
  EXPECT_CALL(client(), ShowDeleteAddressProfileDialog(profile, _))
      .WillOnce([](auto profile, auto delete_dialog_callback) {
        std::move(delete_dialog_callback).Run(/*user_accepted_delete=*/false);
      });
  // Address profile must remain intact if user cancels deletion process.
  EXPECT_CALL(pdm(), AddObserver).Times(0);
  EXPECT_CALL(pdm(), RemoveByGUID).Times(0);
  // The Autofill popup must be reopened when the delete dialog is closed.
  EXPECT_CALL(
      driver(),
      RendererShouldTriggerSuggestions(
          queried_form_triggering_field_id_,
          AutofillSuggestionTriggerSource::kShowPromptAfterDialogClosed));
  auto suggestion = Suggestion(PopupItemId::kDeleteAddressProfile);
  suggestion.payload = Suggestion::Guid(profile.guid());

  external_delegate().DidAcceptSuggestion(
      suggestion, SuggestionPosition{.row = 0},
      AutofillSuggestionTriggerSource::kFormControlElementClicked);
  histogram.ExpectUniqueSample("Autofill.ExtendedMenu.DeleteAddress", 0, 1);
}

// Test that the correct Autofill profile is deleted when the user accepts the
// deletion process.
TEST_F(AutofillExternalDelegateUnitTest, UserAcceptsDeletion) {
  IssueOnQuery();

  base::HistogramTester histogram;
  const AutofillProfile profile = test::GetFullProfile();
  pdm().AddProfile(profile);
  EXPECT_CALL(client(), ShowDeleteAddressProfileDialog(profile, _))
      .WillOnce([](auto profile, auto delete_dialog_callback) {
        std::move(delete_dialog_callback).Run(/*user_accepted_delete=*/true);
      });
  // Autofill profile must be deleted when user confirms the dialog.
  EXPECT_CALL(pdm(), AddObserver(&external_delegate()));
  EXPECT_CALL(pdm(), RemoveByGUID(profile.guid()));
  // The Autofill popup must be reopened when the delete dialog is closed.
  EXPECT_CALL(
      driver(),
      RendererShouldTriggerSuggestions(
          queried_form_triggering_field_id_,
          AutofillSuggestionTriggerSource::kShowPromptAfterDialogClosed));
  auto suggestion = Suggestion(PopupItemId::kDeleteAddressProfile);
  suggestion.payload = Suggestion::Guid(profile.guid());

  external_delegate().DidAcceptSuggestion(
      suggestion, SuggestionPosition{.row = 0},
      AutofillSuggestionTriggerSource::kFormControlElementClicked);

  external_delegate().OnPersonalDataFinishedProfileTasks();
  histogram.ExpectUniqueSample("Autofill.ExtendedMenu.DeleteAddress", 1, 1);
}

// Test the situation when AutofillExternalDelegate is destroyed before the
// PersonalDataManager observer is notified that all tasks have been processed.
TEST_F(AutofillExternalDelegateUnitTest,
       UserOpensDeleteDialogTwiceBeforeProfileIsDeleted) {
  IssueOnQuery();

  const AutofillProfile profile = test::GetFullProfile();
  pdm().AddProfile(profile);
  EXPECT_CALL(client(), ShowDeleteAddressProfileDialog(profile, _))
      .Times(2)
      .WillRepeatedly([](auto profile, auto delete_dialog_callback) {
        std::move(delete_dialog_callback).Run(/*user_accepted_delete=*/true);
      });
  // PDM observer must be added only once.
  EXPECT_CALL(pdm(), AddObserver);
  // Autofill profile can be deleted both times.
  EXPECT_CALL(pdm(), RemoveByGUID(profile.guid())).Times(2);
  auto suggestion = Suggestion(PopupItemId::kDeleteAddressProfile);
  suggestion.payload = Suggestion::Guid(profile.guid());

  external_delegate().DidAcceptSuggestion(
      suggestion, SuggestionPosition{.row = 0},
      AutofillSuggestionTriggerSource::kFormControlElementClicked);
  external_delegate().DidAcceptSuggestion(
      suggestion, SuggestionPosition{.row = 0},
      AutofillSuggestionTriggerSource::kFormControlElementClicked);
}

// Test that our external delegate called the virtual methods at the right time.
TEST_F(AutofillExternalDelegateUnitTest, TestExternalDelegateVirtualCalls) {
  IssueOnQuery();

  const auto kExpectedSuggestions =
      SuggestionVectorIdsAre(PopupItemId::kAddressEntry,
#if !BUILDFLAG(IS_ANDROID)
                             PopupItemId::kSeparator,
#endif
                             PopupItemId::kAutofillOptions);
  EXPECT_CALL(client(),
              ShowAutofillPopup(PopupOpenArgsAre(kExpectedSuggestions), _));

  // This should call ShowAutofillPopup.
  const AutofillProfile profile = test::GetFullProfile();
  pdm().AddProfile(profile);
  std::vector<Suggestion> autofill_item = {
      Suggestion(PopupItemId::kAddressEntry)};
  autofill_item[0].payload = Suggestion::Guid(profile.guid());
  external_delegate().OnSuggestionsReturned(
      queried_form_triggering_field_id_, autofill_item, kDefaultTriggerSource);

  EXPECT_CALL(manager(), FillOrPreviewProfileForm(
                             mojom::ActionPersistence::kFill,
                             HasQueriedFormId(), HasQueriedFieldId(), _, _));
  EXPECT_CALL(client(),
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));

  // This should trigger a call to hide the popup since we've selected an
  // option.
  external_delegate().DidAcceptSuggestion(
      autofill_item[0], SuggestionPosition{.row = 0}, kDefaultTriggerSource);
}

// Test that data list elements for a node will appear in the Autofill popup.
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateDataList) {
  IssueOnQuery();

  std::vector<SelectOption> data_list_items;
  data_list_items.emplace_back();

  EXPECT_CALL(client(), UpdateAutofillPopupDataListValues(SizeIs(1)));
  external_delegate().SetCurrentDataListValues(data_list_items);

  // This should call ShowAutofillPopup.
  const auto kExpectedSuggestions =
      SuggestionVectorIdsAre(PopupItemId::kDatalistEntry,
#if !BUILDFLAG(IS_ANDROID)
                             PopupItemId::kSeparator,
#endif
                             PopupItemId::kAddressEntry,
#if !BUILDFLAG(IS_ANDROID)
                             PopupItemId::kSeparator,
#endif
                             PopupItemId::kAutofillOptions);
  EXPECT_CALL(client(),
              ShowAutofillPopup(PopupOpenArgsAre(kExpectedSuggestions), _));
  std::vector<Suggestion> autofill_item;
  autofill_item.emplace_back(/*main_text=*/u"", PopupItemId::kAddressEntry);
  external_delegate().OnSuggestionsReturned(
      queried_form_triggering_field_id_, autofill_item, kDefaultTriggerSource);

  // Try calling OnSuggestionsReturned with no Autofill values and ensure
  // the datalist items are still shown.
  EXPECT_CALL(
      client(),
      ShowAutofillPopup(
          PopupOpenArgsAre(SuggestionVectorIdsAre(PopupItemId::kDatalistEntry)),
          _));
  autofill_item.clear();
  external_delegate().OnSuggestionsReturned(
      queried_form_triggering_field_id_, autofill_item, kDefaultTriggerSource);
}

// Test that datalist values can get updated while a popup is showing.
TEST_F(AutofillExternalDelegateUnitTest, UpdateDataListWhileShowingPopup) {
  IssueOnQuery();

  EXPECT_CALL(client(), ShowAutofillPopup).Times(0);

  // Make sure just setting the data list values doesn't cause the popup to
  // appear.
  std::vector<SelectOption> data_list_items;
  data_list_items.emplace_back();

  EXPECT_CALL(client(), UpdateAutofillPopupDataListValues(SizeIs(1)));
  external_delegate().SetCurrentDataListValues(data_list_items);

  // Ensure the popup is displayed.
  const auto kExpectedSuggestions =
      SuggestionVectorIdsAre(PopupItemId::kDatalistEntry,
#if !BUILDFLAG(IS_ANDROID)
                             PopupItemId::kSeparator,
#endif
                             PopupItemId::kAddressEntry,
#if !BUILDFLAG(IS_ANDROID)
                             PopupItemId::kSeparator,
#endif
                             PopupItemId::kAutofillOptions);
  EXPECT_CALL(client(),
              ShowAutofillPopup(PopupOpenArgsAre(kExpectedSuggestions), _));
  std::vector<Suggestion> autofill_item;
  autofill_item.emplace_back();
  autofill_item[0].popup_item_id = PopupItemId::kAddressEntry;
  external_delegate().OnSuggestionsReturned(
      queried_form_triggering_field_id_, autofill_item, kDefaultTriggerSource);

  // This would normally get called from ShowAutofillPopup, but it is mocked so
  // we need to call OnPopupShown ourselves.
  external_delegate().OnPopupShown();

  // Update the current data list and ensure the popup is updated.
  data_list_items.emplace_back();

  EXPECT_CALL(client(), UpdateAutofillPopupDataListValues(SizeIs(2)));
  external_delegate().SetCurrentDataListValues(data_list_items);
}

// Test that we _don't_ de-dupe autofill values against datalist values. We
// keep both with a separator.
TEST_F(AutofillExternalDelegateUnitTest, DuplicateAutofillDatalistValues) {
  IssueOnQuery();

  std::vector<SelectOption> datalist{
      {.value = u"Rick", .content = u"Deckard"},
      {.value = u"Beyonce", .content = u"Knowles"}};
  EXPECT_CALL(client(), UpdateAutofillPopupDataListValues(ElementsAre(
                            AllOf(Field(&SelectOption::value, u"Rick"),
                                  Field(&SelectOption::content, u"Deckard")),
                            AllOf(Field(&SelectOption::value, u"Beyonce"),
                                  Field(&SelectOption::content, u"Knowles")))));
  external_delegate().SetCurrentDataListValues(datalist);

  const auto kExpectedSuggestions = SuggestionVectorIdsAre(
      PopupItemId::kDatalistEntry, PopupItemId::kDatalistEntry,
#if !BUILDFLAG(IS_ANDROID)
      PopupItemId::kSeparator,
#endif
      PopupItemId::kAddressEntry,
#if !BUILDFLAG(IS_ANDROID)
      PopupItemId::kSeparator,
#endif
      PopupItemId::kAutofillOptions);
  EXPECT_CALL(client(),
              ShowAutofillPopup(PopupOpenArgsAre(kExpectedSuggestions), _));

  // Have an Autofill item that is identical to one of the datalist entries.
  std::vector<Suggestion> autofill_item;
  autofill_item.emplace_back();
  autofill_item[0].main_text =
      Suggestion::Text(u"Rick", Suggestion::Text::IsPrimary(true));
  autofill_item[0].labels = {{Suggestion::Text(u"Deckard")}};
  autofill_item[0].popup_item_id = PopupItemId::kAddressEntry;
  external_delegate().OnSuggestionsReturned(
      queried_form_triggering_field_id_, autofill_item, kDefaultTriggerSource);
}

// Test that we de-dupe autocomplete values against datalist values, keeping the
// latter in case of a match.
TEST_F(AutofillExternalDelegateUnitTest, DuplicateAutocompleteDatalistValues) {
  IssueOnQuery();

  std::vector<SelectOption> datalist{
      {.value = u"Rick", .content = u"Deckard"},
      {.value = u"Beyonce", .content = u"Knowles"}};
  EXPECT_CALL(client(), UpdateAutofillPopupDataListValues(ElementsAre(
                            AllOf(Field(&SelectOption::value, u"Rick"),
                                  Field(&SelectOption::content, u"Deckard")),
                            AllOf(Field(&SelectOption::value, u"Beyonce"),
                                  Field(&SelectOption::content, u"Knowles")))));
  external_delegate().SetCurrentDataListValues(datalist);

  const auto kExpectedSuggestions = SuggestionVectorIdsAre(
      // We are expecting only two data list entries.
      PopupItemId::kDatalistEntry, PopupItemId::kDatalistEntry,
#if !BUILDFLAG(IS_ANDROID)
      PopupItemId::kSeparator,
#endif
      PopupItemId::kAutocompleteEntry);
  EXPECT_CALL(client(),
              ShowAutofillPopup(PopupOpenArgsAre(kExpectedSuggestions), _));

  // Have an Autocomplete item that is identical to one of the datalist entries
  // and one that is distinct.
  std::vector<Suggestion> autocomplete_items;
  autocomplete_items.emplace_back();
  autocomplete_items[0].main_text =
      Suggestion::Text(u"Rick", Suggestion::Text::IsPrimary(true));
  autocomplete_items[0].popup_item_id = PopupItemId::kAutocompleteEntry;
  autocomplete_items.emplace_back();
  autocomplete_items[1].main_text =
      Suggestion::Text(u"Cain", Suggestion::Text::IsPrimary(true));
  autocomplete_items[1].popup_item_id = PopupItemId::kAutocompleteEntry;
  external_delegate().OnSuggestionsReturned(queried_form_triggering_field_id_,
                                            autocomplete_items,
                                            kDefaultTriggerSource);
}

// Test that the Autofill popup is able to display warnings explaining why
// Autofill is disabled for a website.
// Regression test for http://crbug.com/247880
TEST_F(AutofillExternalDelegateUnitTest, AutofillWarnings) {
  IssueOnQuery();

  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(client(), ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  // This should call ShowAutofillPopup.
  std::vector<Suggestion> autofill_item;
  autofill_item.emplace_back();
  autofill_item[0].popup_item_id =
      PopupItemId::kInsecureContextPaymentDisabledMessage;
  external_delegate().OnSuggestionsReturned(
      queried_form_triggering_field_id_, autofill_item, kDefaultTriggerSource);

  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(
                  PopupItemId::kInsecureContextPaymentDisabledMessage));
  EXPECT_EQ(open_args.element_bounds, gfx::RectF());
  EXPECT_EQ(open_args.text_direction, base::i18n::UNKNOWN_DIRECTION);
  EXPECT_EQ(open_args.trigger_source, kDefaultTriggerSource);
}

// Test that Autofill warnings are removed if there are also autocomplete
// entries in the vector.
TEST_F(AutofillExternalDelegateUnitTest,
       AutofillWarningsNotShown_WithSuggestions) {
  IssueOnQuery();

  EXPECT_CALL(client(),
              ShowAutofillPopup(PopupOpenArgsAre(SuggestionVectorIdsAre(
                                    PopupItemId::kAutocompleteEntry)),
                                _));
  std::vector<Suggestion> suggestions;
  suggestions.emplace_back();
  suggestions[0].popup_item_id =
      PopupItemId::kInsecureContextPaymentDisabledMessage;
  suggestions.emplace_back();
  suggestions[1].main_text =
      Suggestion::Text(u"Rick", Suggestion::Text::IsPrimary(true));
  suggestions[1].popup_item_id = PopupItemId::kAutocompleteEntry;
  external_delegate().OnSuggestionsReturned(queried_form_triggering_field_id_,
                                            suggestions, kDefaultTriggerSource);
}

// Test that the Autofill delegate doesn't try and fill a form with a
// negative unique id.
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateInvalidUniqueId) {
  // Ensure it doesn't try to preview the negative id.
  EXPECT_CALL(manager(), FillOrPreviewProfileForm).Times(0);
  EXPECT_CALL(manager(), FillCreditCardForm).Times(0);
  EXPECT_CALL(driver(), RendererShouldClearPreviewedForm);
  const Suggestion suggestion{
      PopupItemId::kInsecureContextPaymentDisabledMessage};
  external_delegate().DidSelectSuggestion(suggestion, kDefaultTriggerSource);

  // Ensure it doesn't try to fill the form in with the negative id.
  EXPECT_CALL(client(),
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  EXPECT_CALL(manager(), FillCreditCardForm).Times(0);

  external_delegate().DidAcceptSuggestion(
      suggestion, SuggestionPosition{.row = 0}, kDefaultTriggerSource);
}

// Test that the Autofill delegate still allows previewing and filling
// specifically of the negative ID for PopupItemId::kIbanEntry.
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateFillsIbanEntry) {
  IssueOnQuery();

  EXPECT_CALL(
      client(),
      ShowAutofillPopup(
          PopupOpenArgsAre(SuggestionVectorIdsAre(PopupItemId::kIbanEntry)),
          _));
  std::vector<Suggestion> suggestions;
  const std::u16string masked_iban_value = u"IE12 **** **** **** **56 78";
  const std::u16string unmasked_iban_value = u"IE12 BOFI 9000 0112 3456 78";
  suggestions.emplace_back(/*main_text=*/masked_iban_value,
                           PopupItemId::kIbanEntry);
  suggestions[0].labels = {{Suggestion::Text(u"My doctor's IBAN")}};
  suggestions[0].payload = Suggestion::ValueToFill(unmasked_iban_value);
  external_delegate().OnSuggestionsReturned(queried_form_triggering_field_id_,
                                            suggestions, kDefaultTriggerSource);

  EXPECT_CALL(driver(), RendererShouldClearPreviewedForm());
  EXPECT_CALL(manager(),
              FillOrPreviewField(mojom::ActionPersistence::kPreview,
                                 mojom::TextReplacement::kReplaceAll,
                                 HasQueriedFormId(), HasQueriedFieldId(),
                                 masked_iban_value, PopupItemId::kIbanEntry));
  external_delegate().DidSelectSuggestion(suggestions[0],
                                          kDefaultTriggerSource);
  EXPECT_CALL(client(),
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  EXPECT_CALL(manager(),
              FillOrPreviewField(mojom::ActionPersistence::kFill,
                                 mojom::TextReplacement::kReplaceAll,
                                 HasQueriedFormId(), HasQueriedFieldId(),
                                 unmasked_iban_value, PopupItemId::kIbanEntry));
  external_delegate().DidAcceptSuggestion(
      suggestions[0], SuggestionPosition{.row = 0}, kDefaultTriggerSource);
}

// Test that the Autofill delegate still allows previewing and filling
// specifically of the negative ID for PopupItemId::kMerchantPromoCodeEntry.
TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateFillsMerchantPromoCodeEntry) {
  IssueOnQuery();

  EXPECT_CALL(client(),
              ShowAutofillPopup(PopupOpenArgsAre(SuggestionVectorIdsAre(
                                    PopupItemId::kMerchantPromoCodeEntry)),
                                _));
  std::vector<Suggestion> suggestions;
  const std::u16string promo_code_value = u"PROMOCODE1234";
  suggestions.emplace_back(/*main_text=*/promo_code_value,
                           PopupItemId::kMerchantPromoCodeEntry);
  suggestions[0].main_text.value = promo_code_value;
  suggestions[0].labels = {{Suggestion::Text(u"12.34% off your purchase!")}};
  external_delegate().OnSuggestionsReturned(queried_form_triggering_field_id_,
                                            suggestions, kDefaultTriggerSource);

  EXPECT_CALL(driver(), RendererShouldClearPreviewedForm());
  EXPECT_CALL(manager(),
              FillOrPreviewField(mojom::ActionPersistence::kPreview,
                                 mojom::TextReplacement::kReplaceAll,
                                 HasQueriedFormId(), HasQueriedFieldId(),
                                 promo_code_value,
                                 PopupItemId::kMerchantPromoCodeEntry));
  external_delegate().DidSelectSuggestion(suggestions[0],
                                          kDefaultTriggerSource);
  EXPECT_CALL(client(),
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  EXPECT_CALL(manager(),
              FillOrPreviewField(mojom::ActionPersistence::kFill,
                                 mojom::TextReplacement::kReplaceAll,
                                 HasQueriedFormId(), HasQueriedFieldId(),
                                 promo_code_value,
                                 PopupItemId::kMerchantPromoCodeEntry));

  external_delegate().DidAcceptSuggestion(
      suggestions[0], SuggestionPosition{.row = 0}, kDefaultTriggerSource);
}

// Test that the Autofill delegate routes the merchant promo code suggestions
// footer redirect logic correctly.
TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateMerchantPromoCodeSuggestionsFooter) {
  const GURL gurl{"https://example.com/"};
  EXPECT_CALL(client(), OpenPromoCodeOfferDetailsURL(gurl));

  external_delegate().DidAcceptSuggestion(
      test::CreateAutofillSuggestion(PopupItemId::kSeePromoCodeDetails,
                                     u"baz foo", gurl),
      SuggestionPosition{.row = 0}, kDefaultTriggerSource);
}

// Test that the ClearPreview call is only sent if the form was being previewed
// (i.e. it isn't autofilling a password).
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateClearPreviewedForm) {
  // Ensure selecting a new password entries or Autofill entries will
  // cause any previews to get cleared.
  IssueOnQuery();
  EXPECT_CALL(driver(), RendererShouldClearPreviewedForm());
  external_delegate().DidSelectSuggestion(
      test::CreateAutofillSuggestion(PopupItemId::kAddressEntry, u"baz foo"),
      kDefaultTriggerSource);
  EXPECT_CALL(driver(), RendererShouldClearPreviewedForm());
  EXPECT_CALL(manager(), FillOrPreviewProfileForm(
                             mojom::ActionPersistence::kPreview,
                             HasQueriedFormId(), HasQueriedFieldId(), _, _));
  const AutofillProfile profile = test::GetFullProfile();
  pdm().AddProfile(profile);
  external_delegate().DidSelectSuggestion(
      test::CreateAutofillSuggestion(PopupItemId::kAddressEntry, u"baz foo",
                                     Suggestion::Guid(profile.guid())),
      kDefaultTriggerSource);

  // Ensure selecting an autocomplete entry will cause any previews to
  // get cleared.
  EXPECT_CALL(driver(), RendererShouldClearPreviewedForm());
  EXPECT_CALL(manager(),
              FillOrPreviewField(mojom::ActionPersistence::kPreview,
                                 mojom::TextReplacement::kReplaceAll,
                                 HasQueriedFormId(), HasQueriedFieldId(),
                                 std::u16string(u"baz foo"),
                                 PopupItemId::kAutocompleteEntry));
  external_delegate().DidSelectSuggestion(
      test::CreateAutofillSuggestion(PopupItemId::kAutocompleteEntry,
                                     u"baz foo"),
      kDefaultTriggerSource);

  CreditCard card = test::GetMaskedServerCard();
  pdm().AddCreditCard(card);
  // Ensure selecting a virtual card entry will cause any previews to
  // get cleared.
  EXPECT_CALL(driver(), RendererShouldClearPreviewedForm());
  EXPECT_CALL(manager(), FillOrPreviewCreditCardForm(
                             mojom::ActionPersistence::kPreview,
                             HasQueriedFormId(), HasQueriedFieldId(), _, _));
  Suggestion suggestion(PopupItemId::kVirtualCreditCardEntry);
  suggestion.payload = Suggestion::Guid(card.guid());
  external_delegate().DidSelectSuggestion(suggestion, kDefaultTriggerSource);
}

// Test that the popup is hidden once we are done editing the autofill field.
TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateHidePopupAfterEditing) {
  EXPECT_CALL(client(), ShowAutofillPopup);
  test::GenerateTestAutofillPopup(&external_delegate());

  EXPECT_CALL(client(),
              HideAutofillPopup(autofill::PopupHidingReason::kEndEditing));
  external_delegate().DidEndTextFieldEditing();
}

// Test that the driver is directed to accept the data list after being notified
// that the user accepted the data list suggestion.
TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateAcceptDatalistSuggestion) {
  IssueOnQuery();
  EXPECT_CALL(client(),
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  std::u16string dummy_string(u"baz qux");
  EXPECT_CALL(driver(), RendererShouldAcceptDataListSuggestion(
                            queried_form_triggering_field_id_, dummy_string));

  external_delegate().DidAcceptSuggestion(
      test::CreateAutofillSuggestion(PopupItemId::kDatalistEntry, dummy_string),
      SuggestionPosition{.row = 0}, kDefaultTriggerSource);
}

// Test parameter data for asserting filling method metrics depending on the
// suggestion (`PopupItemId`) accepted.
struct FillingMethodMetricsTestParams {
  const PopupItemId popup_item_id;
  const autofill_metrics::AutofillFillingMethodMetric target_metric;
  const std::string test_name;
};

class FillingMethodMetricsUnitTest
    : public AutofillExternalDelegateUnitTest,
      public ::testing::WithParamInterface<FillingMethodMetricsTestParams> {};

const FillingMethodMetricsTestParams kFillingMethodMetricsTestCases[] = {
    {.popup_item_id = PopupItemId::kAddressEntry,
     .target_metric = autofill_metrics::AutofillFillingMethodMetric::kFullForm,
     .test_name = "addressEntry"},
    {.popup_item_id = PopupItemId::kFillEverythingFromAddressProfile,
     .target_metric = autofill_metrics::AutofillFillingMethodMetric::kFullForm,
     .test_name = "fillEverythingFromAddressProfile"},
    {.popup_item_id = PopupItemId::kFieldByFieldFilling,
     .target_metric =
         autofill_metrics::AutofillFillingMethodMetric::kFieldByFieldFilling,
     .test_name = "fieldByFieldFilling"},
    {.popup_item_id = PopupItemId::kFillFullAddress,
     .target_metric =
         autofill_metrics::AutofillFillingMethodMetric::kGroupFillingAddress,
     .test_name = "fillFullAddress"},
    {.popup_item_id = PopupItemId::kFillFullPhoneNumber,
     .target_metric = autofill_metrics::AutofillFillingMethodMetric::
         kGroupFillingPhoneNumber,
     .test_name = "fillFullPhoneNumber"},
    {.popup_item_id = PopupItemId::kFillFullEmail,
     .target_metric =
         autofill_metrics::AutofillFillingMethodMetric::kGroupFillingEmail,
     .test_name = "fillFullEmail"},
};

// Tests that for a certain `PopupItemId` accepted, the expected
// `AutofillFillingMethodMetric` is recorded.
TEST_P(FillingMethodMetricsUnitTest, RecordFillingMethodForPopupType) {
  IssueOnQuery();
  const FillingMethodMetricsTestParams& params = GetParam();
  const AutofillProfile profile = test::GetFullProfile();
  pdm().AddProfile(profile);
  const Suggestion suggestion =
      params.popup_item_id == PopupItemId::kFieldByFieldFilling
          ? CreateFieldByFieldFillingSuggestion(profile.guid(), NAME_FIRST)
          : test::CreateAutofillSuggestion(params.popup_item_id);
  manager().OnFormsSeen({queried_form_}, {});
  base::HistogramTester histogram_tester;
  external_delegate().DidAcceptSuggestion(
      suggestion, SuggestionPosition{.row = 0}, kDefaultTriggerSource);

  histogram_tester.ExpectUniqueSample("Autofill.FillingMethodUsed",
                                      params.target_metric, 1);
}

INSTANTIATE_TEST_SUITE_P(
    AutofillExternalDelegateUnitTest,
    FillingMethodMetricsUnitTest,
    ::testing::ValuesIn(kFillingMethodMetricsTestCases),
    [](const ::testing::TestParamInfo<FillingMethodMetricsUnitTest::ParamType>&
           info) { return info.param.test_name; });

// Test parameter data for asserting that group filling suggestions
// forward the expected fields to the manager.
struct GroupFillingTestParams {
  const ServerFieldTypeSet field_types_to_fill;
  const PopupItemId popup_item_id;
  const std::string test_name;
};

class GroupFillingUnitTest
    : public AutofillExternalDelegateUnitTest,
      public ::testing::WithParamInterface<GroupFillingTestParams> {};

const GroupFillingTestParams kGroupFillingTestCases[] = {
    {.field_types_to_fill = GetServerFieldTypesOfGroup(FieldTypeGroup::kName),
     .popup_item_id = PopupItemId::kFillFullName,
     .test_name = "_NameFields"},
    {.field_types_to_fill = GetServerFieldTypesOfGroup(FieldTypeGroup::kPhone),
     .popup_item_id = PopupItemId::kFillFullPhoneNumber,
     .test_name = "_PhoneFields"},
    {.field_types_to_fill = GetServerFieldTypesOfGroup(FieldTypeGroup::kEmail),
     .popup_item_id = PopupItemId::kFillFullEmail,
     .test_name = "_EmailAddressFields"},
    {.field_types_to_fill = GetAddressFieldsForGroupFilling(),
     .popup_item_id = PopupItemId::kFillFullAddress,
     .test_name = "_AddressFields"}};

// Tests that the expected server field set is forwarded to the manager
// depending on the chosen suggestion.
TEST_P(GroupFillingUnitTest, GroupFillingTests_FillAndPreview) {
  IssueOnQuery();
  const GroupFillingTestParams& params = GetParam();
  const AutofillProfile profile = test::GetFullProfile();
  pdm().AddProfile(profile);
  const Suggestion suggestion =
      params.popup_item_id == PopupItemId::kFieldByFieldFilling
          ? CreateFieldByFieldFillingSuggestion(profile.guid(), NAME_FIRST)
          : test::CreateAutofillSuggestion(params.popup_item_id, u"baz foo",
                                           Suggestion::Guid(profile.guid()));
  auto expected_source =
#if BUILDFLAG(IS_ANDROID)
      AutofillTriggerSource::kKeyboardAccessory;
#else
      AutofillTriggerSource::kPopup;
#endif
  // Test preview
  EXPECT_CALL(manager(),
              FillOrPreviewProfileForm(
                  mojom::ActionPersistence::kPreview, HasQueriedFormId(),
                  HasQueriedFieldId(), _,
                  EqualsAutofillTriggerDetails(
                      {.trigger_source = expected_source,
                       .field_types_to_fill = params.field_types_to_fill})));
  external_delegate().DidSelectSuggestion(suggestion, kDefaultTriggerSource);

  // Test fill
  EXPECT_CALL(manager(),
              FillOrPreviewProfileForm(
                  mojom::ActionPersistence::kFill, HasQueriedFormId(),
                  HasQueriedFieldId(), _,
                  EqualsAutofillTriggerDetails(
                      {.trigger_source = expected_source,
                       .field_types_to_fill = params.field_types_to_fill})));
  external_delegate().DidAcceptSuggestion(
      suggestion, SuggestionPosition{.row = 0}, kDefaultTriggerSource);
}

INSTANTIATE_TEST_SUITE_P(
    AutofillExternalDelegateUnitTest,
    GroupFillingUnitTest,
    ::testing::ValuesIn(kGroupFillingTestCases),
    [](const ::testing::TestParamInfo<GroupFillingUnitTest::ParamType>& info) {
      return info.param.test_name;
    });

// Test that an accepted autofill suggestion will fill the form.
TEST_F(AutofillExternalDelegateUnitTest, AcceptSuggestion) {
  IssueOnQuery();
  EXPECT_CALL(client(),
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  EXPECT_CALL(manager(), FillOrPreviewProfileForm(
                             mojom::ActionPersistence::kFill,
                             HasQueriedFormId(), HasQueriedFieldId(), _, _));

  const AutofillProfile profile = test::GetFullProfile();
  pdm().AddProfile(profile);
  external_delegate().DidAcceptSuggestion(
      test::CreateAutofillSuggestion(PopupItemId::kAddressEntry, u"John Legend",
                                     Suggestion::Guid(profile.guid())),
      SuggestionPosition{.row = 2}, kDefaultTriggerSource);
}

TEST_F(AutofillExternalDelegateUnitTest,
       AcceptFirstPopupLevelSuggestion_LogSuggestionAcceptedMetric) {
  const AutofillProfile profile = test::GetFullProfile();
  pdm().AddProfile(profile);
  const int suggestion_accepted_row = 2;
  base::HistogramTester histogram_tester;

  external_delegate().DidAcceptSuggestion(
      test::CreateAutofillSuggestion(PopupItemId::kAddressEntry, u"John Legend",
                                     Suggestion::Guid(profile.guid())),
      AutofillPopupDelegate::SuggestionPosition{.row = suggestion_accepted_row},
      kDefaultTriggerSource);

  histogram_tester.ExpectUniqueSample("Autofill.SuggestionAcceptedIndex",
                                      suggestion_accepted_row, 1);
}

TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateAccept_FillEverythingSuggestion_FillAndPreview) {
  IssueOnQuery();
  const AutofillProfile profile = test::GetFullProfile();
  pdm().AddProfile(profile);
  const Suggestion suggestion = test::CreateAutofillSuggestion(
      PopupItemId::kFillEverythingFromAddressProfile, u"John Legend",
      Suggestion::Guid(profile.guid()));

  EXPECT_CALL(client(),
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  // Test fill
  EXPECT_CALL(manager(), FillOrPreviewProfileForm(
                             mojom::ActionPersistence::kFill,
                             HasQueriedFormId(), HasQueriedFieldId(), _, _));

  external_delegate().DidAcceptSuggestion(
      suggestion, SuggestionPosition{.row = 2}, kDefaultTriggerSource);

  // Test preview
  EXPECT_CALL(manager(), FillOrPreviewProfileForm(
                             mojom::ActionPersistence::kPreview,
                             HasQueriedFormId(), HasQueriedFieldId(), _, _));

  external_delegate().DidSelectSuggestion(suggestion, kDefaultTriggerSource);
}

// Tests that when accepting a suggestion, the `AutofillSuggestionTriggerSource`
// is converted to the correct `AutofillTriggerSource`.
TEST_F(AutofillExternalDelegateUnitTest, AcceptSuggestion_TriggerSource) {
  IssueOnQuery();
  const AutofillProfile profile = test::GetFullProfile();
  pdm().AddProfile(profile);
  Suggestion suggestion = test::CreateAutofillSuggestion(
      PopupItemId::kAddressEntry, /*main_text_value=*/u"",
      Suggestion::Guid(profile.guid()));

  // Expect that `kFormControlElementClicked` translates to source `kPopup` or
  // `kKeyboardAccessory`, depending on the platform.
  auto suggestion_source =
      AutofillSuggestionTriggerSource::kFormControlElementClicked;
  auto expected_source =
#if BUILDFLAG(IS_ANDROID)
      AutofillTriggerSource::kKeyboardAccessory;
#else
      AutofillTriggerSource::kPopup;
#endif
  EXPECT_CALL(
      manager(),
      FillOrPreviewProfileForm(
          mojom::ActionPersistence::kFill, HasQueriedFormId(),
          HasQueriedFieldId(), _,
          EqualsAutofillTriggerDetails({.trigger_source = expected_source})));
  external_delegate().DidAcceptSuggestion(
      suggestion, SuggestionPosition{.row = 1}, suggestion_source);

  // Expect that `kManualFallbackAddress` translates to the manual fallback
  // trigger source.
  suggestion_source = AutofillSuggestionTriggerSource::kManualFallbackAddress;
  expected_source = AutofillTriggerSource::kManualFallback;
  EXPECT_CALL(
      manager(),
      FillOrPreviewProfileForm(
          mojom::ActionPersistence::kFill, HasQueriedFormId(),
          HasQueriedFieldId(), _,
          EqualsAutofillTriggerDetails({.trigger_source = expected_source})));
  external_delegate().DidAcceptSuggestion(
      suggestion, SuggestionPosition{.row = 1}, suggestion_source);
}

// Tests that when the suggestion is of type
// `PopupItemId::kFieldByFieldFilling`, we emit the expected metric
// corresponding to which field type was used.
TEST_F(AutofillExternalDelegateUnitTest,
       FieldByFieldFilling_SubPopup_EmitsTypeMetric) {
  const AutofillProfile profile = test::GetFullProfile();
  pdm().AddProfile(profile);
  Suggestion suggestion =
      CreateFieldByFieldFillingSuggestion(profile.guid(), NAME_FIRST);
  IssueOnQuery();
  manager().OnFormsSeen({queried_form_}, {});
  base::HistogramTester histogram_tester;

  external_delegate().DidAcceptSuggestion(
      suggestion, SuggestionPosition{.row = 0, .sub_popup_level = 1},
      AutofillSuggestionTriggerSource::kFormControlElementClicked);

  histogram_tester.ExpectUniqueSample(
      "Autofill.FieldByFieldFilling.FieldTypeUsed",
      autofill_metrics::AutofillFieldByFieldFillingTypes::kNameFirst, 1);
}

TEST_F(AutofillExternalDelegateUnitTest,
       FieldByFieldFilling_RootPopup_DoNotEmitTypeMetric) {
  const AutofillProfile profile = test::GetFullProfile();
  pdm().AddProfile(profile);
  Suggestion suggestion =
      CreateFieldByFieldFillingSuggestion(profile.guid(), NAME_FIRST);
  IssueOnQuery();
  manager().OnFormsSeen({queried_form_}, {});
  base::HistogramTester histogram_tester;

  external_delegate().DidAcceptSuggestion(
      suggestion, SuggestionPosition{.row = 0},
      AutofillSuggestionTriggerSource::kFormControlElementClicked);

  histogram_tester.ExpectUniqueSample(
      "Autofill.FieldByFieldFilling.FieldTypeUsed",
      autofill_metrics::AutofillFieldByFieldFillingTypes::kNameFirst, 0);
}

TEST_F(AutofillExternalDelegateUnitTest,
       FieldByFieldFilling_PreviewCreditCard) {
  const CreditCard local_card = test::GetCreditCard();
  pdm().AddCreditCard(local_card);
  Suggestion suggestion = CreateFieldByFieldFillingSuggestion(
      local_card.guid(), CREDIT_CARD_NAME_FULL);
  IssueOnQuery();
  manager().OnFormsSeen({queried_form_}, {});

  EXPECT_CALL(manager(),
              FillOrPreviewField(mojom::ActionPersistence::kPreview,
                                 mojom::TextReplacement::kReplaceAll,
                                 HasQueriedFormId(), HasQueriedFieldId(),
                                 suggestion.main_text.value,
                                 PopupItemId::kFieldByFieldFilling));

  external_delegate().DidSelectSuggestion(
      suggestion, AutofillSuggestionTriggerSource::kManualFallbackPayments);
}

TEST_F(AutofillExternalDelegateUnitTest, FieldByFieldFilling_FillCreditCard) {
  const CreditCard local_card = test::GetCreditCard();
  pdm().AddCreditCard(local_card);
  Suggestion suggestion = CreateFieldByFieldFillingSuggestion(
      local_card.guid(), CREDIT_CARD_NAME_FULL);
  IssueOnQuery();
  manager().OnFormsSeen({queried_form_}, {});

  EXPECT_CALL(manager(),
              FillOrPreviewField(mojom::ActionPersistence::kFill,
                                 mojom::TextReplacement::kReplaceAll,
                                 HasQueriedFormId(), HasQueriedFieldId(),
                                 suggestion.main_text.value,
                                 PopupItemId::kFieldByFieldFilling));

  external_delegate().DidAcceptSuggestion(
      suggestion, SuggestionPosition{.row = 1},
      AutofillSuggestionTriggerSource::kManualFallbackPayments);
}

// Test parameter data for asserting that the expected set of field types
// is stored in the delegate.
struct GetLastServerTypesToFillForSectionTestParams {
  const absl::optional<ServerFieldTypeSet>
      expected_last_field_types_to_fill_for_section;
  const PopupItemId popup_item_id;
  const absl::optional<Section> section;
  const bool is_preview = false;
  const std::string test_name;
};

class GetLastFieldTypesToFillUnitTest
    : public AutofillExternalDelegateUnitTest,
      public ::testing::WithParamInterface<
          GetLastServerTypesToFillForSectionTestParams> {};

const GetLastServerTypesToFillForSectionTestParams
    kGetLastServerTypesToFillForSectionTesCases[] = {
        // Tests that when `PopupItemId::kAddressEntry` is accepted and
        // therefore the user wanted to fill the whole form. Autofill
        // stores the last targeted fields as `kAllServerFieldTypes`.
        {.expected_last_field_types_to_fill_for_section = kAllServerFieldTypes,
         .popup_item_id = PopupItemId::kAddressEntry,
         .test_name = "_AllServerFields"},
        // Tests that when `PopupItemId::kFieldByFieldFilling`
        // is accepted and therefore the user wanted to fill a single field.
        // The last targeted fields is stored as the triggering field type
        // only, this way the next time the user interacts
        // with the form, they are kept at the same filling granularity.
        {.expected_last_field_types_to_fill_for_section =
             absl::optional<ServerFieldTypeSet>({NAME_FIRST}),
         .popup_item_id = PopupItemId::kFieldByFieldFilling,
         .test_name = "_SingleField"},
        // Tests that when `GetLastFieldTypesToFillForSection` is called for
        // a section for which no information was stored, `absl::nullopt` is
        // returned.
        {.expected_last_field_types_to_fill_for_section = absl::nullopt,
         .popup_item_id = PopupItemId::kCreditCardEntry,
         .test_name = "_EmptySet"},
        {.expected_last_field_types_to_fill_for_section = absl::nullopt,
         .popup_item_id = PopupItemId::kAddressEntry,
         .section = Section::FromAutocomplete({.section = "another-section"}),
         .test_name = "_DoesNotReturnsForNonExistingSection"},
        // Tests that when `PopupItemId::kAddressEntry` is selected
        // (i.e preview mode) we do not store anything as last
        // targeted fields.
        {.expected_last_field_types_to_fill_for_section = absl::nullopt,
         .popup_item_id = PopupItemId::kAddressEntry,
         .is_preview = true,
         .test_name = "_NotStoredDuringPreview"}};

// Tests that the expected set of last field types to fill is stored.
TEST_P(GetLastFieldTypesToFillUnitTest, LastFieldTypesToFillForSection) {
  IssueOnQuery();
  const GetLastServerTypesToFillForSectionTestParams& params = GetParam();
  base::test::ScopedFeatureList features(
      features::kAutofillGranularFillingAvailable);

  const AutofillProfile profile = test::GetFullProfile();
  pdm().AddProfile(profile);
  IssueOnQuery();
  manager().OnFormsSeen({queried_form_}, {});
  ON_CALL(pdm(), IsAutofillProfileEnabled).WillByDefault(Return(true));
  const Suggestion suggestion =
      params.popup_item_id == PopupItemId::kFieldByFieldFilling
          ? CreateFieldByFieldFillingSuggestion(profile.guid(), NAME_FIRST)
          : test::CreateAutofillSuggestion(params.popup_item_id);

  if (!params.is_preview) {
    external_delegate().DidAcceptSuggestion(
        suggestion, SuggestionPosition{.row = 1}, kDefaultTriggerSource);
  } else {
    external_delegate().DidSelectSuggestion(suggestion, kDefaultTriggerSource);
  }

  EXPECT_EQ(
      external_delegate().GetLastFieldTypesToFillForSection(
          params.section.value_or(get_triggering_autofill_field()->section)),
      params.expected_last_field_types_to_fill_for_section);
}

INSTANTIATE_TEST_SUITE_P(
    AutofillExternalDelegateUnitTest,
    GetLastFieldTypesToFillUnitTest,
    ::testing::ValuesIn(kGetLastServerTypesToFillForSectionTesCases),
    [](const ::testing::TestParamInfo<
        GetLastFieldTypesToFillUnitTest::ParamType>& info) {
      return info.param.test_name;
    });

// Mock out an existing plus address autofill suggestion, and ensure that
// choosing it results in the field being filled with its value (as opposed to
// the mocked address used in the creation flow).
TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateFillsExistingPlusAddress) {
  IssueOnQuery();

  base::HistogramTester histogram_tester;

  EXPECT_CALL(client(),
              ShowAutofillPopup(PopupOpenArgsAre(SuggestionVectorIdsAre(
                                    PopupItemId::kFillExistingPlusAddress)),
                                _));
  const std::u16string plus_address = u"test+plus@test.example";
  std::vector<Suggestion> suggestions;
  suggestions.emplace_back(/*main_text=*/plus_address,
                           PopupItemId::kFillExistingPlusAddress);
  // This function tests the filling of existing plus addresses, which is why
  // `OfferPlusAddressCreation` need not be mocked.
  external_delegate().OnSuggestionsReturned(queried_form_triggering_field_id_,
                                            suggestions, kDefaultTriggerSource);

  EXPECT_CALL(driver(), RendererShouldClearPreviewedForm());
  EXPECT_CALL(
      manager(),
      FillOrPreviewField(mojom::ActionPersistence::kPreview,
                         mojom::TextReplacement::kReplaceAll,
                         HasQueriedFormId(), HasQueriedFieldId(), plus_address,
                         PopupItemId::kFillExistingPlusAddress));
  external_delegate().DidSelectSuggestion(suggestions[0],
                                          kDefaultTriggerSource);
  EXPECT_CALL(client(),
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  EXPECT_CALL(
      manager(),
      FillOrPreviewField(mojom::ActionPersistence::kFill,
                         mojom::TextReplacement::kReplaceAll,
                         HasQueriedFormId(), HasQueriedFieldId(), plus_address,
                         PopupItemId::kFillExistingPlusAddress));
  external_delegate().DidAcceptSuggestion(
      suggestions[0], SuggestionPosition{.row = 0}, kDefaultTriggerSource);
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kPlusAddressSuggestionMetric),
      BucketsAre(base::Bucket(
          plus_addresses::PlusAddressMetrics::
              PlusAddressAutofillSuggestionEvent::kExistingPlusAddressChosen,
          1)));
}

// Mock out the new plus address creation flow, and ensure that its completion
// results in the field being filled with the resulting plus address.
TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateOffersPlusAddressCreation) {
  const std::u16string kMockPlusAddressForCreationCallback =
      u"test+1234@test.example";

  IssueOnQuery();

  base::HistogramTester histogram_tester;
  EXPECT_CALL(client(),
              ShowAutofillPopup(PopupOpenArgsAre(SuggestionVectorIdsAre(
                                    PopupItemId::kCreateNewPlusAddress)),
                                _));
  std::vector<Suggestion> suggestions;
  suggestions.emplace_back(/*main_text=*/u"",
                           PopupItemId::kCreateNewPlusAddress);
  external_delegate().OnSuggestionsReturned(queried_form_triggering_field_id_,
                                            suggestions, kDefaultTriggerSource);

  EXPECT_CALL(driver(), RendererShouldClearPreviewedForm());
  external_delegate().DidSelectSuggestion(suggestions[0],
                                          kDefaultTriggerSource);
  EXPECT_CALL(client(),
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  // Mock out the plus address creation logic to ensure it is deterministic and
  // independent of the client implementations in //chrome or //ios.
  EXPECT_CALL(client(), OfferPlusAddressCreation)
      .WillOnce([&](const url::Origin& origin,
                    plus_addresses::PlusAddressCallback callback) {
        std::move(callback).Run(
            base::UTF16ToUTF8(kMockPlusAddressForCreationCallback));
      });
  // `kMockPlusAddressForCreationCallback` is returned in the callback from the
  // mocked `OfferPlusAddressCreation()`. Ensure it is filled (vs, say, the
  // empty text of the suggestion).
  EXPECT_CALL(manager(),
              FillOrPreviewField(mojom::ActionPersistence::kFill,
                                 mojom::TextReplacement::kReplaceAll,
                                 HasQueriedFormId(), HasQueriedFieldId(),
                                 kMockPlusAddressForCreationCallback,
                                 PopupItemId::kCreateNewPlusAddress));
  external_delegate().DidAcceptSuggestion(
      suggestions[0], SuggestionPosition{.row = 0}, kDefaultTriggerSource);
  EXPECT_THAT(
      histogram_tester.GetAllSamples(kPlusAddressSuggestionMetric),
      BucketsAre(base::Bucket(
          plus_addresses::PlusAddressMetrics::
              PlusAddressAutofillSuggestionEvent::kCreateNewPlusAddressChosen,
          1)));
}

// Tests that accepting a Compose suggestion returns a callback that, when run,
// fills the trigger field.
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateOpensComposeAndFills) {
  MockAutofillComposeDelegate compose_delegate;
  ON_CALL(client(), GetComposeDelegate)
      .WillByDefault(Return(&compose_delegate));

  IssueOnQuery();

  // Simulate receiving a Compose suggestion.
  EXPECT_CALL(
      client(),
      ShowAutofillPopup(
          PopupOpenArgsAre(SuggestionVectorIdsAre(PopupItemId::kCompose)), _));
  std::vector<Suggestion> suggestions = {
      Suggestion(/*main_text=*/u"", PopupItemId::kCompose)};
  external_delegate().OnSuggestionsReturned(queried_form_triggering_field_id_,
                                            suggestions, kDefaultTriggerSource);

  // Simulate accepting a Compose suggestion.
  AutofillComposeDelegate::ComposeCallback callback;
  EXPECT_CALL(compose_delegate, OpenCompose).WillOnce(MoveArg<3>(&callback));
  EXPECT_CALL(client(),
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  external_delegate().DidAcceptSuggestion(
      suggestions[0], SuggestionPosition{.row = 0}, kDefaultTriggerSource);
  Mock::VerifyAndClearExpectations(&compose_delegate);
  ASSERT_TRUE(callback);

  const std::u16string kComposeResponse = u"Cucumbers are tasty.";
  EXPECT_CALL(manager(),
              FillOrPreviewField(mojom::ActionPersistence::kFill,
                                 mojom::TextReplacement::kReplaceSelection,
                                 HasQueriedFormId(), HasQueriedFieldId(),
                                 kComposeResponse, PopupItemId::kCompose));
  std::move(callback).Run(kComposeResponse);
}

class AutofillExternalDelegateUnitTest_UndoAutofill
    : public AutofillExternalDelegateUnitTest,
      public testing::WithParamInterface<bool> {
 public:
  bool UndoInsteadOfClear() { return GetParam(); }

 private:
  void SetUp() override {
    UndoInsteadOfClear()
        ? scoped_feature_list_.InitAndEnableFeature(features::kAutofillUndo)
        : scoped_feature_list_.InitAndDisableFeature(features::kAutofillUndo);
    AutofillExternalDelegateUnitTest::SetUp();
  }
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(AutofillExternalDelegateUnitTest,
                         AutofillExternalDelegateUnitTest_UndoAutofill,
                         testing::Bool());

// Test that the driver is directed to clear or undo the form after being
// notified that the user accepted the suggestion to clear or undo the form.
TEST_P(AutofillExternalDelegateUnitTest_UndoAutofill,
       ExternalDelegateUndoAndClearForm) {
  if (UndoInsteadOfClear()) {
    EXPECT_CALL(manager(), UndoAutofill);
  } else {
    EXPECT_CALL(client(),
                HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
    EXPECT_CALL(driver(), RendererShouldClearFilledSection());
  }
  external_delegate().DidAcceptSuggestion(Suggestion(PopupItemId::kClearForm),
                                          SuggestionPosition{.row = 0},
                                          kDefaultTriggerSource);
}

// Test that the driver is directed to undo the form after being notified that
// the user selected the suggestion to undo the form.
TEST_P(AutofillExternalDelegateUnitTest_UndoAutofill,
       ExternalDelegateUndoAndClearPreviewForm) {
  if (UndoInsteadOfClear()) {
    EXPECT_CALL(manager(), UndoAutofill);
  }
  external_delegate().DidSelectSuggestion(Suggestion(PopupItemId::kClearForm),
                                          kDefaultTriggerSource);
}

// Test that autofill client will scan a credit card after use accepted the
// suggestion to scan a credit card.
TEST_F(AutofillExternalDelegateUnitTest, ScanCreditCardMenuItem) {
  EXPECT_CALL(client(), ScanCreditCard(_));
  EXPECT_CALL(client(),
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));

  external_delegate().DidAcceptSuggestion(
      Suggestion(PopupItemId::kScanCreditCard), SuggestionPosition{.row = 0},
      kDefaultTriggerSource);
}

TEST_F(AutofillExternalDelegateUnitTest, ScanCreditCardPromptMetricsTest) {
  // Log that the scan card item was shown, although nothing was selected.
  {
    EXPECT_CALL(manager(), ShouldShowScanCreditCard(_, _))
        .WillOnce(testing::Return(true));
    base::HistogramTester histogram;
    IssueOnQuery();
    IssueOnSuggestionsReturned(queried_form_triggering_field_id_);
    external_delegate().OnPopupShown();
    histogram.ExpectUniqueSample("Autofill.ScanCreditCardPrompt",
                                 AutofillMetrics::SCAN_CARD_ITEM_SHOWN, 1);
  }
  // Log that the scan card item was selected.
  {
    EXPECT_CALL(manager(), ShouldShowScanCreditCard(_, _))
        .WillOnce(testing::Return(true));
    base::HistogramTester histogram;
    IssueOnQuery();
    IssueOnSuggestionsReturned(queried_form_triggering_field_id_);
    external_delegate().OnPopupShown();

    external_delegate().DidAcceptSuggestion(
        Suggestion(PopupItemId::kScanCreditCard), SuggestionPosition{.row = 0},
        kDefaultTriggerSource);

    histogram.ExpectBucketCount("Autofill.ScanCreditCardPrompt",
                                AutofillMetrics::SCAN_CARD_ITEM_SHOWN, 1);
    histogram.ExpectBucketCount("Autofill.ScanCreditCardPrompt",
                                AutofillMetrics::SCAN_CARD_ITEM_SELECTED, 1);
    histogram.ExpectBucketCount("Autofill.ScanCreditCardPrompt",
                                AutofillMetrics::SCAN_CARD_OTHER_ITEM_SELECTED,
                                0);
  }
  // Log that something else was selected.
  {
    EXPECT_CALL(manager(), ShouldShowScanCreditCard(_, _))
        .WillOnce(testing::Return(true));
    base::HistogramTester histogram;
    IssueOnQuery();
    IssueOnSuggestionsReturned(queried_form_triggering_field_id_);
    external_delegate().OnPopupShown();

    external_delegate().DidAcceptSuggestion(Suggestion(PopupItemId::kClearForm),
                                            SuggestionPosition{.row = 0},
                                            kDefaultTriggerSource);

    histogram.ExpectBucketCount("Autofill.ScanCreditCardPrompt",
                                AutofillMetrics::SCAN_CARD_ITEM_SHOWN, 1);
    histogram.ExpectBucketCount("Autofill.ScanCreditCardPrompt",
                                AutofillMetrics::SCAN_CARD_ITEM_SELECTED, 0);
    histogram.ExpectBucketCount("Autofill.ScanCreditCardPrompt",
                                AutofillMetrics::SCAN_CARD_OTHER_ITEM_SELECTED,
                                1);
  }
  // Nothing is logged when the item isn't shown.
  {
    EXPECT_CALL(manager(), ShouldShowScanCreditCard(_, _))
        .WillOnce(testing::Return(false));
    base::HistogramTester histogram;
    IssueOnQuery();
    IssueOnSuggestionsReturned(queried_form_triggering_field_id_);
    external_delegate().OnPopupShown();
    histogram.ExpectTotalCount("Autofill.ScanCreditCardPrompt", 0);
  }
}

MATCHER_P(CreditCardMatches, card, "") {
  return !arg.Compare(card);
}

// Test that autofill manager will fill the credit card form after user scans a
// credit card.
TEST_F(AutofillExternalDelegateUnitTest, FillCreditCardForm) {
  CreditCard card;
  test::SetCreditCardInfo(&card, "Alice", "4111", "1", "3000", "1");
  EXPECT_CALL(manager(), FillCreditCardForm(_, _, CreditCardMatches(card),
                                            std::u16string(), _));
  external_delegate().OnCreditCardScanned(AutofillTriggerSource::kPopup, card);
}

TEST_F(AutofillExternalDelegateUnitTest, IgnoreAutocompleteOffForAutofill) {
  const FormData form;
  FormFieldData field;
  field.is_focusable = true;
  field.should_autocomplete = false;

  external_delegate().OnQuery(form, field, gfx::RectF());

  std::vector<Suggestion> autofill_items;
  autofill_items.emplace_back();
  autofill_items[0].popup_item_id = PopupItemId::kAutocompleteEntry;

  // Ensure the popup tries to show itself, despite autocomplete="off".
  EXPECT_CALL(client(), ShowAutofillPopup);
  EXPECT_CALL(client(), HideAutofillPopup(_)).Times(0);

  external_delegate().OnSuggestionsReturned(field.global_id(), autofill_items,
                                            kDefaultTriggerSource);
}

TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateFillFieldWithValue_Autocomplete) {
  EXPECT_CALL(client(),
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  IssueOnQuery();

  base::HistogramTester histogram_tester;
  std::u16string dummy_autocomplete_string(u"autocomplete");
  EXPECT_CALL(manager(),
              FillOrPreviewField(mojom::ActionPersistence::kFill,
                                 mojom::TextReplacement::kReplaceAll,
                                 HasQueriedFormId(), HasQueriedFieldId(),
                                 dummy_autocomplete_string,
                                 PopupItemId::kAutocompleteEntry));
  EXPECT_CALL(*client().GetMockAutocompleteHistoryManager(),
              OnSingleFieldSuggestionSelected(dummy_autocomplete_string,
                                              PopupItemId::kAutocompleteEntry));

  external_delegate().DidAcceptSuggestion(
      test::CreateAutofillSuggestion(PopupItemId::kAutocompleteEntry,
                                     dummy_autocomplete_string),
      SuggestionPosition{.row = 0}, kDefaultTriggerSource);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SuggestionAcceptedIndex.Autocomplete", 0, 1);
}

TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateFillFieldWithValue_MerchantPromoCode) {
  EXPECT_CALL(client(),
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  IssueOnQuery();

  std::u16string dummy_promo_code_string(u"merchant promo");
  EXPECT_CALL(manager(),
              FillOrPreviewField(mojom::ActionPersistence::kFill,
                                 mojom::TextReplacement::kReplaceAll,
                                 HasQueriedFormId(), HasQueriedFieldId(),
                                 dummy_promo_code_string,
                                 PopupItemId::kMerchantPromoCodeEntry));
  EXPECT_CALL(
      *client().GetMockMerchantPromoCodeManager(),
      OnSingleFieldSuggestionSelected(dummy_promo_code_string,
                                      PopupItemId::kMerchantPromoCodeEntry));

  external_delegate().DidAcceptSuggestion(
      test::CreateAutofillSuggestion(PopupItemId::kMerchantPromoCodeEntry,
                                     dummy_promo_code_string),
      SuggestionPosition{.row = 0}, kDefaultTriggerSource);
}

TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateFillFieldWithValue_Iban) {
  EXPECT_CALL(client(),
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  IssueOnQuery();

  std::u16string masked_iban_value = u"IE12 **** **** **** **56 78";
  std::u16string unmasked_iban_value = u"IE12 BOFI 9000 0112 3456 78";
  EXPECT_CALL(manager(),
              FillOrPreviewField(mojom::ActionPersistence::kFill,
                                 mojom::TextReplacement::kReplaceAll,
                                 HasQueriedFormId(), HasQueriedFieldId(),
                                 unmasked_iban_value, PopupItemId::kIbanEntry));
  EXPECT_CALL(*client().GetMockIbanManager(),
              OnSingleFieldSuggestionSelected(masked_iban_value,
                                              PopupItemId::kIbanEntry));

  external_delegate().DidAcceptSuggestion(
      test::CreateAutofillSuggestion(
          PopupItemId::kIbanEntry, masked_iban_value,
          Suggestion::ValueToFill(unmasked_iban_value)),
      SuggestionPosition{.row = 0}, kDefaultTriggerSource);
}

TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateFillFieldWithValue_FieldByFieldFilling) {
  const AutofillProfile profile = test::GetFullProfile();
  pdm().AddProfile(profile);
  IssueOnQuery();
  manager().OnFormsSeen({queried_form_}, {});
  Suggestion suggestion =
      CreateFieldByFieldFillingSuggestion(profile.guid(), NAME_FIRST);
  EXPECT_CALL(client(),
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  EXPECT_CALL(
      manager(),
      FillOrPreviewField(
          mojom::ActionPersistence::kFill, mojom::TextReplacement::kReplaceAll,
          HasQueriedFormId(), HasQueriedFieldId(),
          profile.GetRawInfo(*suggestion.field_by_field_filling_type_used),
          PopupItemId::kFieldByFieldFilling));

  external_delegate().DidAcceptSuggestion(
      suggestion, SuggestionPosition{.row = 0}, kDefaultTriggerSource);
}

TEST_F(AutofillExternalDelegateUnitTest, ShouldShowGooglePayIcon) {
  IssueOnQuery();

  const auto kExpectedSuggestions =
  // On Desktop, the GPay icon should be stored in the store indicator icon.
#if BUILDFLAG(IS_ANDROID)
      SuggestionVectorIconsAre(Suggestion::Icon::kNoIcon,
                               AnyOf(Suggestion::Icon::kGooglePay,
                                     Suggestion::Icon::kGooglePayDark));
#elif BUILDFLAG(IS_IOS)
      SuggestionVectorIconsAre(Suggestion::Icon::kNoIcon,
                               Suggestion::Icon::kNoIcon,
                               AnyOf(Suggestion::Icon::kGooglePay,
                                     Suggestion::Icon::kGooglePayDark));
#else
      SuggestionVectorStoreIndicatorIconsAre(
          Suggestion::Icon::kNoIcon, Suggestion::Icon::kNoIcon,
          AnyOf(Suggestion::Icon::kGooglePay,
                Suggestion::Icon::kGooglePayDark));
#endif
  EXPECT_CALL(client(),
              ShowAutofillPopup(PopupOpenArgsAre(kExpectedSuggestions), _));
  std::vector<Suggestion> autofill_item;
  autofill_item.emplace_back(/*main_text=*/u"", PopupItemId::kAddressEntry);
  external_delegate().OnSuggestionsReturned(queried_form_triggering_field_id_,
                                            autofill_item,
                                            kDefaultTriggerSource, true);
}

TEST_F(AutofillExternalDelegateUnitTest,
       ShouldNotShowGooglePayIconIfSuggestionsContainLocalCards) {
  IssueOnQuery();

  const auto kExpectedSuggestions =
      SuggestionVectorIconsAre(Suggestion::Icon::kNoIcon,
#if !BUILDFLAG(IS_ANDROID)
                               Suggestion::Icon::kNoIcon,
#endif
                               Suggestion::Icon::kSettings);
  EXPECT_CALL(client(),
              ShowAutofillPopup(PopupOpenArgsAre(kExpectedSuggestions), _));
  std::vector<Suggestion> autofill_item;
  autofill_item.emplace_back(/*main_text=*/u"", PopupItemId::kAddressEntry);
  external_delegate().OnSuggestionsReturned(queried_form_triggering_field_id_,
                                            autofill_item,
                                            kDefaultTriggerSource, false);
}

TEST_F(AutofillExternalDelegateUnitTest, ShouldUseNewSettingName) {
  IssueOnQuery();

  const auto kExpectedSuggestions = SuggestionVectorMainTextsAre(
      Suggestion::Text(std::u16string(), Suggestion::Text::IsPrimary(true)),
#if !BUILDFLAG(IS_ANDROID)
      Suggestion::Text(std::u16string(), Suggestion::Text::IsPrimary(false)),
#endif
      Suggestion::Text(l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE),
                       Suggestion::Text::IsPrimary(true)));
  EXPECT_CALL(client(),
              ShowAutofillPopup(PopupOpenArgsAre(kExpectedSuggestions), _));
  std::vector<Suggestion> autofill_item;
  autofill_item.emplace_back(/*main_text=*/u"", PopupItemId::kAddressEntry);
  autofill_item[0].main_text.is_primary = Suggestion::Text::IsPrimary(true);
  external_delegate().OnSuggestionsReturned(
      queried_form_triggering_field_id_, autofill_item, kDefaultTriggerSource);
}

// Test that browser autofill manager will handle the unmasking request for the
// virtual card after users accept the suggestion to use a virtual card.
TEST_F(AutofillExternalDelegateUnitTest, AcceptVirtualCardOptionItem) {
  IssueOnQuery();
  FormData form;
  CreditCard card = test::GetMaskedServerCard();
  pdm().AddCreditCard(card);
  EXPECT_CALL(manager(), FillOrPreviewCreditCardForm(
                             mojom::ActionPersistence::kFill,
                             HasQueriedFormId(), HasQueriedFieldId(), _, _));
  Suggestion suggestion(PopupItemId::kVirtualCreditCardEntry);
  suggestion.payload = Suggestion::Guid(card.guid());
  external_delegate().DidAcceptSuggestion(
      suggestion, SuggestionPosition{.row = 0}, kDefaultTriggerSource);
}

TEST_F(AutofillExternalDelegateUnitTest, SelectVirtualCardOptionItem) {
  IssueOnQuery();
  CreditCard card = test::GetMaskedServerCard();
  pdm().AddCreditCard(card);
  EXPECT_CALL(manager(), FillOrPreviewCreditCardForm(
                             mojom::ActionPersistence::kPreview,
                             HasQueriedFormId(), HasQueriedFieldId(), _, _));
  Suggestion suggestion(PopupItemId::kVirtualCreditCardEntry);
  suggestion.payload = Suggestion::Guid(card.guid());
  external_delegate().DidSelectSuggestion(suggestion, kDefaultTriggerSource);
}

TEST_F(AutofillExternalDelegateUnitTest,
       ShouldNotShowAutocompleteSuggestionAfterDialogIsClosed) {
  IssueOnQuery();

  EXPECT_CALL(client(), ShowAutofillPopup).Times(0);

  external_delegate().OnSuggestionsReturned(
      queried_form_triggering_field_id_,
      {Suggestion{PopupItemId::kAutocompleteEntry}},
      AutofillSuggestionTriggerSource::kShowPromptAfterDialogClosed);
}

// Tests that the prompt to show account cards shows up when the corresponding
// bit is set, including any suggestions that are passed along and the "Manage"
// row in the footer.
TEST_F(AutofillExternalDelegateCardsFromAccountTest,
       ShouldShowCardsFromAccountOptionWithCards) {
  IssueOnQuery();

  const auto kExpectedSuggestions = SuggestionVectorMainTextsAre(
      Suggestion::Text(std::u16string(), Suggestion::Text::IsPrimary(true)),
      Suggestion::Text(
          l10n_util::GetStringUTF16(IDS_AUTOFILL_SHOW_ACCOUNT_CARDS),
          Suggestion::Text::IsPrimary(true)),
#if !BUILDFLAG(IS_ANDROID)
      Suggestion::Text(std::u16string(), Suggestion::Text::IsPrimary(false)),
#endif
      Suggestion::Text(l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE),
                       Suggestion::Text::IsPrimary(true)));
  EXPECT_CALL(client(),
              ShowAutofillPopup(PopupOpenArgsAre(kExpectedSuggestions), _));
  std::vector<Suggestion> autofill_item;
  autofill_item.emplace_back(/*main_text=*/u"", PopupItemId::kAddressEntry);
  autofill_item[0].main_text.is_primary = Suggestion::Text::IsPrimary(true);
  external_delegate().OnSuggestionsReturned(
      queried_form_triggering_field_id_, autofill_item, kDefaultTriggerSource);
}

// Tests that the prompt to show account cards shows up when the corresponding
// bit is set, even if no suggestions are passed along. The "Manage" row should
// *not* show up in this case.
TEST_F(AutofillExternalDelegateCardsFromAccountTest,
       ShouldShowCardsFromAccountOptionWithoutCards) {
  IssueOnQuery();

  const auto kExpectedSuggestions =
      SuggestionVectorMainTextsAre(Suggestion::Text(
          l10n_util::GetStringUTF16(IDS_AUTOFILL_SHOW_ACCOUNT_CARDS),
          Suggestion::Text::IsPrimary(true)));
  EXPECT_CALL(client(),
              ShowAutofillPopup(PopupOpenArgsAre(kExpectedSuggestions), _));
  external_delegate().OnSuggestionsReturned(queried_form_triggering_field_id_,
                                            std::vector<Suggestion>(),
                                            kDefaultTriggerSource);
}

#if BUILDFLAG(IS_IOS)
// Tests that outdated returned suggestions are discarded.
TEST_F(AutofillExternalDelegateCardsFromAccountTest,
       ShouldDiscardOutdatedSuggestions) {
  FieldGlobalId old_field_id = test::MakeFieldGlobalId();
  FieldGlobalId new_field_id = test::MakeFieldGlobalId();
  client().set_last_queried_field(new_field_id);
  IssueOnQuery();
  EXPECT_CALL(client(), ShowAutofillPopup).Times(0);
  external_delegate().OnSuggestionsReturned(
      old_field_id, std::vector<Suggestion>(), kDefaultTriggerSource);
}
#endif

}  // namespace autofill
