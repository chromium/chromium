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
#include "base/test/gmock_callback_support.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_compose_delegate.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_form_test_utils.h"
#include "components/autofill/core/browser/autofill_granular_filling_utils.h"
#include "components/autofill/core/browser/autofill_plus_address_delegate.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/metrics/autofill_in_devtools_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/granular_filling_metrics.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"
#include "components/autofill/core/browser/mock_autofill_compose_delegate.h"
#include "components/autofill/core/browser/mock_autofill_plus_address_delegate.h"
#include "components/autofill/core/browser/mock_autofill_prediction_improvements_delegate.h"
#include "components/autofill/core/browser/mock_single_field_form_fill_router.h"
#include "components/autofill/core/browser/payments/mock_iban_access_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/test_payments_autofill_client.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_button_action.h"
#include "components/autofill/core/browser/ui/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/ui/suggestion_test_helpers.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "url/origin.h"

namespace autofill {
namespace {

using base::test::RunOnceCallback;
using test::CreateTestAddressFormData;
using test::CreateTestCreditCardFormData;
using test::CreateTestPersonalInformationFormData;
using test::CreateTestUnclassifiedFormData;
using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Field;
using ::testing::InSequence;
using ::testing::Matcher;
using ::testing::Mock;
using ::testing::MockFunction;
using ::testing::NiceMock;
using ::testing::Property;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SizeIs;
using ::testing::StartsWith;

// Action `SaveArgElementsTo<k>(pointer)` saves the value pointed to by the
// `k`th (0-based) argument of the mock function by moving it to `*pointer`.
ACTION_TEMPLATE(SaveArgElementsTo,
                HAS_1_TEMPLATE_PARAMS(int, k),
                AND_1_VALUE_PARAMS(pointer)) {
  auto span = testing::get<k>(args);
  pointer->assign(span.begin(), span.end());
}

using SuggestionPosition =
    autofill::AutofillSuggestionDelegate::SuggestionMetadata;

constexpr auto kDefaultTriggerSource =
    AutofillSuggestionTriggerSource::kFormControlElementClicked;

// Creates a field by field filling suggestion.
// `guid` is used to set `Suggestion::payload` as
// `Suggestion::Guid(guid)`. This method also sets the
// `Suggestion::field_by_field_filling_type_used` to `fbf_type_used`.
Suggestion CreateFieldByFieldFillingSuggestion(const std::string& guid,
                                               FieldType fbf_type_used) {
  Suggestion suggestion = test::CreateAutofillSuggestion(
      GroupTypeOfFieldType(fbf_type_used) == FieldTypeGroup::kCreditCard
          ? SuggestionType::kCreditCardFieldByFieldFilling
          : SuggestionType::kAddressFieldByFieldFilling,
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

// TODO(crbug.com/40285811): Unify existing `MockCreditCardAccessManager`s in a
// separate file.
class MockCreditCardAccessManager : public CreditCardAccessManager {
 public:
  using CreditCardAccessManager::CreditCardAccessManager;
  MOCK_METHOD(void,
              FetchCreditCard,
              (const CreditCard*,
               CreditCardAccessManager::OnCreditCardFetchedCallback),
              (override));
};

class MockPersonalDataManager : public TestPersonalDataManager {
 public:
  MockPersonalDataManager() = default;
  MOCK_METHOD(void, RemoveByGUID, (const std::string&), (override));
};

class MockAddressDataManager : public TestAddressDataManager {
 public:
  using TestAddressDataManager::TestAddressDataManager;
  MOCK_METHOD(bool, IsAutofillProfileEnabled, (), (const override));
  MOCK_METHOD(void, UpdateProfile, (const AutofillProfile&), (override));
  MOCK_METHOD(void, RemoveProfile, (const std::string&), (override));
};

class MockAutofillDriver : public TestAutofillDriver {
 public:
  using TestAutofillDriver::TestAutofillDriver;
  MockAutofillDriver(const MockAutofillDriver&) = delete;
  MockAutofillDriver& operator=(const MockAutofillDriver&) = delete;
  // Mock methods to enable testability.
  MOCK_METHOD(void,
              RendererShouldAcceptDataListSuggestion,
              (const FieldGlobalId&, const std::u16string&),
              (override));
  MOCK_METHOD(void, RendererShouldClearPreviewedForm, (), (override));
  MOCK_METHOD(void,
              RendererShouldTriggerSuggestions,
              (const FieldGlobalId&, AutofillSuggestionTriggerSource),
              (override));
  MOCK_METHOD(void,
              RendererShouldSetSuggestionAvailability,
              (const FieldGlobalId&, mojom::AutofillSuggestionAvailability),
              (override));
  MOCK_METHOD((base::flat_set<FieldGlobalId>),
              ApplyFormAction,
              (mojom::FormActionType action_type,
               mojom::ActionPersistence action_persistence,
               base::span<const FormFieldData> data,
               const url::Origin& triggered_origin,
               (const base::flat_map<FieldGlobalId, FieldType>&)),
              (override));
  MOCK_METHOD(void,
              ApplyFieldAction,
              (mojom::FieldActionType action_type,
               mojom::ActionPersistence action_persistence,
               const FieldGlobalId& field_id,
               const std::u16string& value),
              (override));
};

class MockPaymentsAutofillClient : public payments::TestPaymentsAutofillClient {
 public:
  explicit MockPaymentsAutofillClient(AutofillClient* client)
      : payments::TestPaymentsAutofillClient(client) {}
  ~MockPaymentsAutofillClient() override = default;

  MOCK_METHOD(void,
              ScanCreditCard,
              (CreditCardScanCallback callback),
              (override));
  MOCK_METHOD(void,
              OpenPromoCodeOfferDetailsURL,
              (const GURL& url),
              (override));
};

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient() {
    set_payments_autofill_client(
        std::make_unique<MockPaymentsAutofillClient>(this));
  }
  MockAutofillClient(const MockAutofillClient&) = delete;
  MockAutofillClient& operator=(const MockAutofillClient&) = delete;
  MOCK_METHOD(AutofillClient::SuggestionUiSessionId,
              ShowAutofillSuggestions,
              (const autofill::AutofillClient::PopupOpenArgs&,
               base::WeakPtr<AutofillSuggestionDelegate>),
              (override));
  MOCK_METHOD(void,
              UpdateAutofillSuggestions,
              (const std::vector<Suggestion>&,
               FillingProduct,
               AutofillSuggestionTriggerSource),
              (override));
  MOCK_METHOD(base::span<const Suggestion>,
              GetAutofillSuggestions,
              (),
              (const override));
  MOCK_METHOD(void,
              UpdateAutofillDataListValues,
              (base::span<const SelectOption> options),
              (override));
  MOCK_METHOD(void,
              HideAutofillSuggestions,
              (SuggestionHidingReason),
              (override));
  MOCK_METHOD(void,
              OfferPlusAddressCreation,
              (const url::Origin&, PlusAddressCallback),
              (override));
  MOCK_METHOD(void,
              ShowPlusAddressAffiliationError,
              (std::u16string, std::u16string, base::OnceClosure),
              (override));
  MOCK_METHOD(void,
              ShowPlusAddressError,
              (AutofillClient::PlusAddressErrorDialogType, base::OnceClosure),
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
  explicit MockBrowserAutofillManager(AutofillDriver* driver)
      : TestBrowserAutofillManager(driver) {
    test_api(*this).set_credit_card_access_manager(
        std::make_unique<NiceMock<MockCreditCardAccessManager>>(
            this, test_api(*this).credit_card_form_event_logger()));
  }
  MockBrowserAutofillManager(const MockBrowserAutofillManager&) = delete;
  MockBrowserAutofillManager& operator=(const MockBrowserAutofillManager&) =
      delete;

  MOCK_METHOD(void,
              OnUserHideSuggestions,
              (const FormData& form, const FormFieldData& field),
              (override));
  MOCK_METHOD(void,
              AuthenticateThenFillCreditCardForm,
              (const FormData& form,
               const FormFieldData& field,
               const CreditCard& credit_card,
               const AutofillTriggerDetails& trigger_details),
              (override));

  bool ShouldShowCardsFromAccountOption(
      const FormData& form,
      const FormFieldData& field,
      AutofillSuggestionTriggerSource trigger_source) const override {
    return should_show_cards_from_account_option_;
  }

  void ShowCardsFromAccountOption() {
    should_show_cards_from_account_option_ = true;
  }
  MOCK_METHOD(void,
              UndoAutofill,
              (mojom::ActionPersistence action_persistence,
               const FormData& form,
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
              FillOrPreviewCreditCardForm,
              (mojom::ActionPersistence action_persistence,
               const FormData& form,
               const FormFieldData& field,
               const CreditCard& credit_card,
               const std::u16string& cvc,
               const AutofillTriggerDetails& trigger_details),
              (override));
  MOCK_METHOD(void,
              FillOrPreviewField,
              (mojom::ActionPersistence,
               mojom::FieldActionType,
               const FormData&,
               const FormFieldData&,
               const std::u16string&,
               SuggestionType,
               std::optional<FieldType>),
              (override));
  MOCK_METHOD(void,
              OnDidFillAddressFormFillingSuggestion,
              (const AutofillProfile&,
               const FormData&,
               const FormFieldData&,
               AutofillTriggerSource),
              (override));

 private:
  bool should_show_cards_from_account_option_ = false;
};

}  // namespace
// The anonymous namespace needs to end here because of `friend`ships between
// the tests and the production code.

class AutofillExternalDelegateUnitTest : public testing::Test {
 protected:
  void SetUp() override {
    client().set_personal_data_manager(
        std::make_unique<NiceMock<MockPersonalDataManager>>());
    pdm().set_address_data_manager(
        std::make_unique<NiceMock<MockAddressDataManager>>());
    autofill_driver_ =
        std::make_unique<NiceMock<MockAutofillDriver>>(&client());
    auto mock_browser_autofill_manager =
        std::make_unique<NiceMock<MockBrowserAutofillManager>>(
            autofill_driver_.get());
    driver().set_autofill_manager(std::move(mock_browser_autofill_manager));
  }

  // Issue an OnQuery call.
  void IssueOnQuery(
      FormData form_data,
      const gfx::Rect& caret_bounds = gfx::Rect(),
      AutofillSuggestionTriggerSource trigger_source = kDefaultTriggerSource) {
    queried_form_ = std::move(form_data);
    manager().OnFormsSeen({queried_form()}, {});
    external_delegate().OnQuery(queried_form(), queried_field(), caret_bounds,
                                trigger_source);
  }

  void IssueOnQuery(
      const gfx::Rect& caret_bounds,
      AutofillSuggestionTriggerSource trigger_source = kDefaultTriggerSource) {
    FormGlobalId form_id = test::MakeFormGlobalId();
    FieldGlobalId field_id = test::MakeFieldGlobalId();
    IssueOnQuery(test::GetFormData({
                     .fields = {{.role = NAME_FIRST,
                                 .host_frame = field_id.frame_token,
                                 .renderer_id = field_id.renderer_id,
                                 .autocomplete_attribute = "given-name"}},
                     .host_frame = form_id.frame_token,
                     .renderer_id = form_id.renderer_id,
                 }),
                 caret_bounds, trigger_source);
  }

  void IssueOnQuery(
      AutofillSuggestionTriggerSource trigger_source = kDefaultTriggerSource) {
    IssueOnQuery(/*caret_bounds=*/gfx::Rect(), trigger_source);
  }
  // Returns the triggering `AutofillField`. This is the only field in the form
  // created in `IssueOnQuery()`.
  AutofillField* get_triggering_autofill_field() {
    return manager().GetAutofillField(queried_form(),
                                      queried_form().fields()[0]);
  }

  Matcher<const FormData&> HasQueriedFormId() {
    return Property(&FormData::global_id, queried_form().global_id());
  }

  Matcher<const FormFieldData&> HasQueriedFieldId() {
    return Property(&FormFieldData::global_id, queried_field().global_id());
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
  MockAddressDataManager& address_data_manager() {
    return static_cast<MockAddressDataManager&>(pdm().address_data_manager());
  }
  MockCreditCardAccessManager& cc_access_manager() {
    return static_cast<MockCreditCardAccessManager&>(
        manager().GetCreditCardAccessManager());
  }

  const FormData& queried_form() {
    CHECK(!queried_form_.fields().empty());
    return queried_form_;
  }

  const FormFieldData& queried_field() {
    return queried_form().fields().front();
  }

  MockPaymentsAutofillClient& payments_client() {
    return static_cast<MockPaymentsAutofillClient&>(
        *client().GetPaymentsAutofillClient());
  }

  // Resets the Autofill driver (and therefore also the manager and the AED).
  void ResetDriver() { autofill_driver_.reset(); }

  void OnSuggestionsReturned(FieldGlobalId field_id,
                             const std::vector<Suggestion>& input_suggestions) {
    autofill_metrics::SuggestionRankingContext context;
    external_delegate().OnSuggestionsReturned(field_id, input_suggestions,
                                              context);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;

  NiceMock<MockAutofillClient> autofill_client_;
  std::unique_ptr<MockAutofillDriver> autofill_driver_;

  // Form containing the triggering field that initialized the external delegate
  // `OnQuery`.
  FormData queried_form_;
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

TEST_F(AutofillExternalDelegateUnitTest, GetMainFillingProduct) {
  IssueOnQuery();

  // Main filling product is not defined before the first call to
  // `OnSuggestionsReturned`.
  EXPECT_EQ(external_delegate().GetMainFillingProduct(), FillingProduct::kNone);

  // Show address suggestion in the popup.
  OnSuggestionsReturned(
      queried_field().global_id(),
      {test::CreateAutofillSuggestion(SuggestionType::kAddressEntry,
                                      u"address suggestion"),
       test::CreateAutofillSuggestion(SuggestionType::kManageAddress,
                                      u"manage addresses")});
  EXPECT_EQ(external_delegate().GetMainFillingProduct(),
            FillingProduct::kAddress);

  // Show create plus address suggestion in the popup.
  OnSuggestionsReturned(
      queried_field().global_id(),
      {test::CreateAutofillSuggestion(SuggestionType::kCreateNewPlusAddress,
                                      u"create new plus address"),
       test::CreateAutofillSuggestion(SuggestionType::kManagePlusAddress,
                                      u"manage address methods")});
  EXPECT_EQ(external_delegate().GetMainFillingProduct(),
            FillingProduct::kPlusAddresses);

  // Show fill plus address suggestion in the popup.
  OnSuggestionsReturned(
      queried_field().global_id(),
      {test::CreateAutofillSuggestion(SuggestionType::kFillExistingPlusAddress,
                                      u"fill existing plus address"),
       test::CreateAutofillSuggestion(SuggestionType::kManagePlusAddress,
                                      u"manage address methods")});
  EXPECT_EQ(external_delegate().GetMainFillingProduct(),
            FillingProduct::kPlusAddresses);

  // Show credit card suggestion in the popup.
  OnSuggestionsReturned(
      queried_field().global_id(),
      {test::CreateAutofillSuggestion(SuggestionType::kCreditCardEntry,
                                      u"credit card suggestion"),
       test::CreateAutofillSuggestion(SuggestionType::kManageCreditCard,
                                      u"manage payment methods")});
  EXPECT_EQ(external_delegate().GetMainFillingProduct(),
            FillingProduct::kCreditCard);

  // Show merchant promo code suggestion in the popup.
  OnSuggestionsReturned(
      queried_field().global_id(),
      {test::CreateAutofillSuggestion(SuggestionType::kMerchantPromoCodeEntry,
                                      u"promo code")});
  EXPECT_EQ(external_delegate().GetMainFillingProduct(),
            FillingProduct::kMerchantPromoCode);

  // Show IBAN suggestion in the popup.
  OnSuggestionsReturned(queried_field().global_id(),
                        {test::CreateAutofillSuggestion(
                            SuggestionType::kIbanEntry, u"fill IBAN")});
  EXPECT_EQ(external_delegate().GetMainFillingProduct(), FillingProduct::kIban);

  // Show password suggestion in the popup.
  OnSuggestionsReturned(queried_field().global_id(),
                        {test::CreateAutofillSuggestion(
                            SuggestionType::kPasswordEntry, u"password")});
  EXPECT_EQ(external_delegate().GetMainFillingProduct(),
            FillingProduct::kPassword);

  // Show compose suggestion in the popup.
  OnSuggestionsReturned(
      queried_field().global_id(),
      {test::CreateAutofillSuggestion(SuggestionType::kComposeResumeNudge,
                                      u"generated text")});
  EXPECT_EQ(external_delegate().GetMainFillingProduct(),
            FillingProduct::kCompose);

  // Show only autocomplete suggestion in the popup.
  OnSuggestionsReturned(
      queried_field().global_id(),
      {test::CreateAutofillSuggestion(SuggestionType::kAutocompleteEntry,
                                      u"autocomplete")});
  EXPECT_EQ(external_delegate().GetMainFillingProduct(),
            FillingProduct::kAutocomplete);

  // Show only datalist suggestion with autocomplete suggestion in the popup.
  OnSuggestionsReturned(
      queried_field().global_id(),
      {test::CreateAutofillSuggestion(SuggestionType::kDatalistEntry,
                                      u"datalist"),
       test::CreateAutofillSuggestion(SuggestionType::kAutocompleteEntry,
                                      u"autocomplete")});
  EXPECT_EQ(external_delegate().GetMainFillingProduct(),
            FillingProduct::kAutocomplete);

  // Show auxiliary helper suggestion in the popup.
  OnSuggestionsReturned(
      queried_field().global_id(),
      {test::CreateAutofillSuggestion(SuggestionType::kUndoOrClear, u"undo")});
  EXPECT_EQ(external_delegate().GetMainFillingProduct(), FillingProduct::kNone);

  // Show auxiliary helper suggestion in the popup.
  OnSuggestionsReturned(
      queried_field().global_id(),
      {test::CreateAutofillSuggestion(SuggestionType::kMixedFormMessage,
                                      u"no autofill available")});
  EXPECT_EQ(external_delegate().GetMainFillingProduct(), FillingProduct::kNone);
}

// Test that the address editor is not shown if there's no Autofill profile with
// the provided GUID.
TEST_F(AutofillExternalDelegateUnitTest, ShowEditorForNonexistingProfile) {
  IssueOnQuery();

  const std::string guid = base::Uuid().AsLowercaseString();
  EXPECT_CALL(client(), ShowEditAddressProfileDialog).Times(0);

  auto suggestion = Suggestion(SuggestionType::kEditAddressProfile);
  suggestion.payload = Suggestion::Guid(guid);
  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 0});
}

// Test that the address editor is shown for the GUID identifying existing
// Autofill profile.
TEST_F(AutofillExternalDelegateUnitTest, ShowEditorForExistingProfile) {
  IssueOnQuery();

  const AutofillProfile profile = test::GetFullProfile();
  pdm().address_data_manager().AddProfile(profile);
  EXPECT_CALL(client(), ShowEditAddressProfileDialog(profile, _));

  auto suggestion = Suggestion(SuggestionType::kEditAddressProfile);
  suggestion.payload = Suggestion::Guid(profile.guid());
  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 0});
}

// Test that the editor changes are not persisted if the user has canceled
// editing.
TEST_F(AutofillExternalDelegateUnitTest, UserCancelsEditing) {
  IssueOnQuery();

  base::HistogramTester histogram;
  const AutofillProfile profile = test::GetFullProfile();
  pdm().address_data_manager().AddProfile(profile);
  EXPECT_CALL(client(), ShowEditAddressProfileDialog(profile, _))
      .WillOnce([](auto profile, auto save_prompt_callback) {
        std::move(save_prompt_callback)
            .Run(AutofillClient::AddressPromptUserDecision::kEditDeclined,
                 profile);
      });
  // No changes should be saved when user cancels editing.
  EXPECT_CALL(address_data_manager(), UpdateProfile).Times(0);
  // The Autofill popup must be reopened when editor dialog is closed.
  EXPECT_CALL(driver(), RendererShouldTriggerSuggestions(
                            queried_field().global_id(),
                            AutofillSuggestionTriggerSource::
                                kShowPromptAfterDialogClosedNonManualFallback));

  auto suggestion = Suggestion(SuggestionType::kEditAddressProfile);
  suggestion.payload = Suggestion::Guid(profile.guid());
  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 0});
  histogram.ExpectUniqueSample("Autofill.ExtendedMenu.EditAddress", 0, 1);
}

// Test that the manual fallback is re-triggered after user closes the edit
// address profile dialog.
TEST_F(AutofillExternalDelegateUnitTest, UserCancelsEditing_ManualFallback) {
  IssueOnQuery(AutofillSuggestionTriggerSource::kManualFallbackAddress);

  base::HistogramTester histogram;
  const AutofillProfile profile = test::GetFullProfile();
  pdm().address_data_manager().AddProfile(profile);
  EXPECT_CALL(client(), ShowEditAddressProfileDialog(profile, _))
      .WillOnce([](auto profile, auto save_prompt_callback) {
        std::move(save_prompt_callback)
            .Run(AutofillClient::AddressPromptUserDecision::kEditDeclined,
                 profile);
      });
  // No changes should be saved when user cancels editing.
  EXPECT_CALL(address_data_manager(), UpdateProfile).Times(0);
  // The Autofill popup must be reopened when editor dialog is closed.
  EXPECT_CALL(driver(),
              RendererShouldTriggerSuggestions(
                  queried_field().global_id(),
                  AutofillSuggestionTriggerSource::kManualFallbackAddress));

  auto suggestion = Suggestion(SuggestionType::kEditAddressProfile);
  suggestion.payload = Suggestion::Guid(profile.guid());
  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 0});
  histogram.ExpectUniqueSample("Autofill.ExtendedMenu.EditAddress", 0, 1);
}

// Test that the editor changes are persisted if the user has canceled editing.
TEST_F(AutofillExternalDelegateUnitTest, UserSavesEdits) {
  IssueOnQuery();

  base::HistogramTester histogram;
  const AutofillProfile profile = test::GetFullProfile();
  pdm().address_data_manager().AddProfile(profile);
  EXPECT_CALL(client(), ShowEditAddressProfileDialog(profile, _))
      .WillOnce([](auto profile, auto save_prompt_callback) {
        std::move(save_prompt_callback)
            .Run(AutofillClient::AddressPromptUserDecision::kEditAccepted,
                 profile);
      });
  // Updated Autofill profile must be persisted when user saves changes through
  // the address editor.
  EXPECT_CALL(address_data_manager(), UpdateProfile(profile));
  // The Autofill popup must be reopened when editor dialog is closed.
  EXPECT_CALL(driver(), RendererShouldTriggerSuggestions(
                            queried_field().global_id(),
                            AutofillSuggestionTriggerSource::
                                kShowPromptAfterDialogClosedNonManualFallback));

  auto suggestion = Suggestion(SuggestionType::kEditAddressProfile);
  suggestion.payload = Suggestion::Guid(profile.guid());
  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 0});

  external_delegate().OnAddressDataChanged();
  histogram.ExpectUniqueSample("Autofill.ExtendedMenu.EditAddress", 1, 1);
}

// Test the situation when database changes take long enough for the user to
// open the address editor for the second time.
TEST_F(AutofillExternalDelegateUnitTest,
       UserOpensEditorTwiceBeforeProfileIsPersisted) {
  IssueOnQuery();

  const AutofillProfile profile = test::GetFullProfile();
  pdm().address_data_manager().AddProfile(profile);
  EXPECT_CALL(client(), ShowEditAddressProfileDialog(profile, _))
      .Times(2)
      .WillRepeatedly([](auto profile, auto save_prompt_callback) {
        std::move(save_prompt_callback)
            .Run(AutofillClient::AddressPromptUserDecision::kEditAccepted,
                 profile);
      });
  // Changes to the Autofill profile must be persisted both times.
  EXPECT_CALL(address_data_manager(), UpdateProfile(profile)).Times(2);

  auto suggestion = Suggestion(SuggestionType::kEditAddressProfile);
  suggestion.payload = Suggestion::Guid(profile.guid());

  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 0});
  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 0});
}

// Test the situation when AutofillExternalDelegate is destroyed before the
// AddressDataManager observer is notified that all tasks have been processed.
TEST_F(AutofillExternalDelegateUnitTest,
       DelegateIsDestroyedBeforeUpdateIsFinished) {
  IssueOnQuery();

  const AutofillProfile profile = test::GetFullProfile();
  pdm().address_data_manager().AddProfile(profile);
  EXPECT_CALL(client(), ShowEditAddressProfileDialog(profile, _))
      .WillOnce([](auto profile, auto save_prompt_callback) {
        std::move(save_prompt_callback)
            .Run(AutofillClient::AddressPromptUserDecision::kEditAccepted,
                 profile);
      });

  EXPECT_CALL(address_data_manager(), UpdateProfile(profile));

  auto suggestion = Suggestion(SuggestionType::kEditAddressProfile);
  suggestion.payload = Suggestion::Guid(profile.guid());
  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 0});
  DestroyAutofillDriver();
}

// Test that the delete dialog is not shown if there's no Autofill profile with
// the provided GUID.
TEST_F(AutofillExternalDelegateUnitTest,
       ShowDeleteDialogForNonexistingProfile) {
  IssueOnQuery();

  const std::string guid = base::Uuid().AsLowercaseString();
  EXPECT_CALL(client(), ShowDeleteAddressProfileDialog).Times(0);
  auto suggestion = Suggestion(SuggestionType::kDeleteAddressProfile);
  suggestion.payload = Suggestion::Guid(guid);

  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 0});
}

// Test that the delete dialog is shown for the GUID identifying existing
// Autofill profile.
TEST_F(AutofillExternalDelegateUnitTest, ShowDeleteDialog) {
  IssueOnQuery();

  const AutofillProfile profile = test::GetFullProfile();
  pdm().address_data_manager().AddProfile(profile);
  EXPECT_CALL(client(), ShowDeleteAddressProfileDialog(profile, _));
  auto suggestion = Suggestion(SuggestionType::kDeleteAddressProfile);
  suggestion.payload = Suggestion::Guid(profile.guid());

  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 0});
}

// Test that the Autofill profile is not deleted when user cancels the deletion
// process.
TEST_F(AutofillExternalDelegateUnitTest, UserCancelsDeletion) {
  IssueOnQuery();

  base::HistogramTester histogram;
  const AutofillProfile profile = test::GetFullProfile();
  pdm().address_data_manager().AddProfile(profile);
  EXPECT_CALL(client(), ShowDeleteAddressProfileDialog(profile, _))
      .WillOnce([](auto profile, auto delete_dialog_callback) {
        std::move(delete_dialog_callback).Run(/*user_accepted_delete=*/false);
      });
  // Address profile must remain intact if user cancels deletion process.
  EXPECT_CALL(address_data_manager(), RemoveProfile).Times(0);
  // The Autofill popup must be reopened when the delete dialog is closed.
  EXPECT_CALL(driver(), RendererShouldTriggerSuggestions(
                            queried_field().global_id(),
                            AutofillSuggestionTriggerSource::
                                kShowPromptAfterDialogClosedNonManualFallback));
  auto suggestion = Suggestion(SuggestionType::kDeleteAddressProfile);
  suggestion.payload = Suggestion::Guid(profile.guid());

  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 0});
  histogram.ExpectUniqueSample("Autofill.ProfileDeleted.ExtendedMenu", 0, 1);
  histogram.ExpectUniqueSample("Autofill.ProfileDeleted.Any", 0, 1);
}

// Test that the manual fallback is re-triggered after user closes the edit
// address profile dialog.
TEST_F(AutofillExternalDelegateUnitTest, UserCancelsDeletion_ManualFallback) {
  IssueOnQuery(AutofillSuggestionTriggerSource::kManualFallbackAddress);

  base::HistogramTester histogram;
  const AutofillProfile profile = test::GetFullProfile();
  pdm().address_data_manager().AddProfile(profile);
  EXPECT_CALL(client(), ShowDeleteAddressProfileDialog(profile, _))
      .WillOnce([](auto profile, auto delete_dialog_callback) {
        std::move(delete_dialog_callback).Run(/*user_accepted_delete=*/false);
      });
  // Address profile must remain intact if user cancels deletion process.
  EXPECT_CALL(address_data_manager(), RemoveProfile).Times(0);
  // The Autofill popup must be reopened when the delete dialog is closed.
  EXPECT_CALL(driver(),
              RendererShouldTriggerSuggestions(
                  queried_field().global_id(),
                  AutofillSuggestionTriggerSource::kManualFallbackAddress));
  auto suggestion = Suggestion(SuggestionType::kDeleteAddressProfile);
  suggestion.payload = Suggestion::Guid(profile.guid());

  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 0});
  histogram.ExpectUniqueSample("Autofill.ProfileDeleted.ExtendedMenu", 0, 1);
  histogram.ExpectUniqueSample("Autofill.ProfileDeleted.Any", 0, 1);
}

// Test that the correct Autofill profile is deleted when the user accepts the
// deletion process.
TEST_F(AutofillExternalDelegateUnitTest, UserAcceptsDeletion) {
  IssueOnQuery();

  base::HistogramTester histogram;
  const AutofillProfile profile = test::GetFullProfile();
  pdm().address_data_manager().AddProfile(profile);
  EXPECT_CALL(client(), ShowDeleteAddressProfileDialog(profile, _))
      .WillOnce([](auto profile, auto delete_dialog_callback) {
        std::move(delete_dialog_callback).Run(/*user_accepted_delete=*/true);
      });
  // Autofill profile must be deleted when user confirms the dialog.
  EXPECT_CALL(address_data_manager(), RemoveProfile(profile.guid()));
  // The Autofill popup must be reopened when the delete dialog is closed.
  EXPECT_CALL(driver(), RendererShouldTriggerSuggestions(
                            queried_field().global_id(),
                            AutofillSuggestionTriggerSource::
                                kShowPromptAfterDialogClosedNonManualFallback));
  auto suggestion = Suggestion(SuggestionType::kDeleteAddressProfile);
  suggestion.payload = Suggestion::Guid(profile.guid());

  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 0});

  external_delegate().OnAddressDataChanged();
  histogram.ExpectUniqueSample("Autofill.ProfileDeleted.ExtendedMenu", 1, 1);
  histogram.ExpectUniqueSample("Autofill.ProfileDeleted.Any", 1, 1);
}

// Test the situation when AutofillExternalDelegate is destroyed before the
// AddressDataManager observer is notified that all tasks have been processed.
TEST_F(AutofillExternalDelegateUnitTest,
       UserOpensDeleteDialogTwiceBeforeProfileIsDeleted) {
  IssueOnQuery();

  const AutofillProfile profile = test::GetFullProfile();
  pdm().address_data_manager().AddProfile(profile);
  EXPECT_CALL(client(), ShowDeleteAddressProfileDialog(profile, _))
      .Times(2)
      .WillRepeatedly([](auto profile, auto delete_dialog_callback) {
        std::move(delete_dialog_callback).Run(/*user_accepted_delete=*/true);
      });
  // ADM observer must be added only once.
  // Autofill profile can be deleted both times.
  EXPECT_CALL(address_data_manager(), RemoveProfile(profile.guid())).Times(2);
  auto suggestion = Suggestion(SuggestionType::kDeleteAddressProfile);
  suggestion.payload = Suggestion::Guid(profile.guid());

  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 0});
  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 0});
}

// Test that our external delegate called the virtual methods at the right time.
TEST_F(AutofillExternalDelegateUnitTest, TestExternalDelegateVirtualCalls) {
  IssueOnQuery();

  const auto kExpectedSuggestions =
      SuggestionVectorIdsAre(SuggestionType::kAddressEntry);
  EXPECT_CALL(client(), ShowAutofillSuggestions(
                            PopupOpenArgsAre(kExpectedSuggestions), _));

  // This should call ShowAutofillSuggestions.
  const AutofillProfile profile = test::GetFullProfile();
  pdm().address_data_manager().AddProfile(profile);
  std::vector<Suggestion> autofill_item = {
      Suggestion(SuggestionType::kAddressEntry)};
  autofill_item[0].payload = Suggestion::Guid(profile.guid());
  OnSuggestionsReturned(queried_field().global_id(), autofill_item);

  EXPECT_CALL(manager(), FillOrPreviewProfileForm(
                             mojom::ActionPersistence::kFill,
                             HasQueriedFormId(), HasQueriedFieldId(), _, _));
  EXPECT_CALL(client(), HideAutofillSuggestions(
                            SuggestionHidingReason::kAcceptSuggestion));

  // This should trigger a call to hide the popup since we've selected an
  // option.
  external_delegate().DidAcceptSuggestion(autofill_item[0],
                                          SuggestionPosition{.row = 0});
}

// Test that data list elements for a node will appear in the Autofill popup.
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateDataList) {
  IssueOnQuery();

  std::vector<SelectOption> data_list_items;
  data_list_items.emplace_back();

  EXPECT_CALL(client(), UpdateAutofillDataListValues(SizeIs(1)));
  external_delegate().SetCurrentDataListValues(data_list_items);

  // This should call ShowAutofillSuggestions.
  const auto kExpectedSuggestions =
      SuggestionVectorIdsAre(SuggestionType::kDatalistEntry,
#if !BUILDFLAG(IS_ANDROID)
                             SuggestionType::kSeparator,
#endif
                             SuggestionType::kAddressEntry);
  EXPECT_CALL(client(), ShowAutofillSuggestions(
                            PopupOpenArgsAre(kExpectedSuggestions), _));
  std::vector<Suggestion> autofill_item;
  autofill_item.emplace_back(/*main_text=*/u"", SuggestionType::kAddressEntry);
  OnSuggestionsReturned(queried_field().global_id(), autofill_item);

  // Try calling OnSuggestionsReturned with no Autofill values and ensure
  // the datalist items are still shown.
  EXPECT_CALL(client(),
              ShowAutofillSuggestions(PopupOpenArgsAre(SuggestionVectorIdsAre(
                                          SuggestionType::kDatalistEntry)),
                                      _));
  autofill_item.clear();
  OnSuggestionsReturned(queried_field().global_id(), autofill_item);
}

// Test that datalist values can get updated while a popup is showing.
TEST_F(AutofillExternalDelegateUnitTest, UpdateDataListWhileShowingPopup) {
  IssueOnQuery();

  EXPECT_CALL(client(), ShowAutofillSuggestions).Times(0);

  // Make sure just setting the data list values doesn't cause the popup to
  // appear.
  std::vector<SelectOption> data_list_items;
  data_list_items.emplace_back();

  EXPECT_CALL(client(), UpdateAutofillDataListValues(SizeIs(1)));
  external_delegate().SetCurrentDataListValues(data_list_items);

  // Ensure the popup is displayed.
  const auto kExpectedSuggestions =
      SuggestionVectorIdsAre(SuggestionType::kDatalistEntry,
#if !BUILDFLAG(IS_ANDROID)
                             SuggestionType::kSeparator,
#endif
                             SuggestionType::kAddressEntry);
  EXPECT_CALL(client(), ShowAutofillSuggestions(
                            PopupOpenArgsAre(kExpectedSuggestions), _));
  std::vector<Suggestion> autofill_item;
  autofill_item.emplace_back();
  autofill_item[0].type = SuggestionType::kAddressEntry;
  OnSuggestionsReturned(queried_field().global_id(), autofill_item);

  // This would normally get called from ShowAutofillSuggestions, but it is
  // mocked so we need to call OnSuggestionsShown ourselves.
  external_delegate().OnSuggestionsShown(autofill_item);

  // Update the current data list and ensure the popup is updated.
  data_list_items.emplace_back();

  EXPECT_CALL(client(), UpdateAutofillDataListValues(SizeIs(2)));
  external_delegate().SetCurrentDataListValues(data_list_items);
}

// Test that we _don't_ de-dupe autofill values against datalist values. We
// keep both with a separator.
TEST_F(AutofillExternalDelegateUnitTest, DuplicateAutofillDatalistValues) {
  IssueOnQuery();

  std::vector<SelectOption> datalist{{.value = u"Rick", .text = u"Deckard"},
                                     {.value = u"Beyonce", .text = u"Knowles"}};
  EXPECT_CALL(client(), UpdateAutofillDataListValues(ElementsAre(
                            AllOf(Field(&SelectOption::value, u"Rick"),
                                  Field(&SelectOption::text, u"Deckard")),
                            AllOf(Field(&SelectOption::value, u"Beyonce"),
                                  Field(&SelectOption::text, u"Knowles")))));
  external_delegate().SetCurrentDataListValues(datalist);

  const auto kExpectedSuggestions = SuggestionVectorIdsAre(
      SuggestionType::kDatalistEntry, SuggestionType::kDatalistEntry,
#if !BUILDFLAG(IS_ANDROID)
      SuggestionType::kSeparator,
#endif
      SuggestionType::kAddressEntry);
  EXPECT_CALL(client(), ShowAutofillSuggestions(
                            PopupOpenArgsAre(kExpectedSuggestions), _));

  // Have an Autofill item that is identical to one of the datalist entries.
  std::vector<Suggestion> autofill_item;
  autofill_item.emplace_back();
  autofill_item[0].main_text =
      Suggestion::Text(u"Rick", Suggestion::Text::IsPrimary(true));
  autofill_item[0].labels = {{Suggestion::Text(u"Deckard")}};
  autofill_item[0].type = SuggestionType::kAddressEntry;
  OnSuggestionsReturned(queried_field().global_id(), autofill_item);
}

// Test that we de-dupe autocomplete values against datalist values, keeping the
// latter in case of a match.
TEST_F(AutofillExternalDelegateUnitTest, DuplicateAutocompleteDatalistValues) {
  IssueOnQuery();

  std::vector<SelectOption> datalist{{.value = u"Rick", .text = u"Deckard"},
                                     {.value = u"Beyonce", .text = u"Knowles"}};
  EXPECT_CALL(client(), UpdateAutofillDataListValues(ElementsAre(
                            AllOf(Field(&SelectOption::value, u"Rick"),
                                  Field(&SelectOption::text, u"Deckard")),
                            AllOf(Field(&SelectOption::value, u"Beyonce"),
                                  Field(&SelectOption::text, u"Knowles")))));
  external_delegate().SetCurrentDataListValues(datalist);

  const auto kExpectedSuggestions = SuggestionVectorIdsAre(
      // We are expecting only two data list entries.
      SuggestionType::kDatalistEntry, SuggestionType::kDatalistEntry,
#if !BUILDFLAG(IS_ANDROID)
      SuggestionType::kSeparator,
#endif
      SuggestionType::kAutocompleteEntry);
  EXPECT_CALL(client(), ShowAutofillSuggestions(
                            PopupOpenArgsAre(kExpectedSuggestions), _));

  // Have an Autocomplete item that is identical to one of the datalist entries
  // and one that is distinct.
  std::vector<Suggestion> autocomplete_items;
  autocomplete_items.emplace_back();
  autocomplete_items[0].main_text =
      Suggestion::Text(u"Rick", Suggestion::Text::IsPrimary(true));
  autocomplete_items[0].type = SuggestionType::kAutocompleteEntry;
  autocomplete_items.emplace_back();
  autocomplete_items[1].main_text =
      Suggestion::Text(u"Cain", Suggestion::Text::IsPrimary(true));
  autocomplete_items[1].type = SuggestionType::kAutocompleteEntry;
  OnSuggestionsReturned(queried_field().global_id(), autocomplete_items);
}

// Test that the Autofill popup is able to display warnings explaining why
// Autofill is disabled for a website.
// Regression test for http://crbug.com/247880
TEST_F(AutofillExternalDelegateUnitTest, AutofillWarnings) {
  IssueOnQuery();

  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(client(), ShowAutofillSuggestions)
      .WillOnce(DoAll(SaveArg<0>(&open_args),
                      Return(AutofillClient::SuggestionUiSessionId())));

  // This should call ShowAutofillSuggestions.
  std::vector<Suggestion> autofill_item;
  autofill_item.emplace_back();
  autofill_item[0].type =
      SuggestionType::kInsecureContextPaymentDisabledMessage;
  OnSuggestionsReturned(queried_field().global_id(), autofill_item);

  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(
                  SuggestionType::kInsecureContextPaymentDisabledMessage));
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
              ShowAutofillSuggestions(PopupOpenArgsAre(SuggestionVectorIdsAre(
                                          SuggestionType::kAutocompleteEntry)),
                                      _));
  std::vector<Suggestion> suggestions;
  suggestions.emplace_back();
  suggestions[0].type = SuggestionType::kInsecureContextPaymentDisabledMessage;
  suggestions.emplace_back();
  suggestions[1].main_text =
      Suggestion::Text(u"Rick", Suggestion::Text::IsPrimary(true));
  suggestions[1].type = SuggestionType::kAutocompleteEntry;
  OnSuggestionsReturned(queried_field().global_id(), suggestions);
}

// Test that the Autofill delegate doesn't try and fill a form with a
// negative unique id.
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateInvalidUniqueId) {
  IssueOnQuery();
  // Ensure it doesn't try to preview the negative id.
  EXPECT_CALL(manager(), FillOrPreviewProfileForm).Times(0);
  EXPECT_CALL(manager(), FillOrPreviewCreditCardForm).Times(0);
  EXPECT_CALL(driver(), RendererShouldClearPreviewedForm);
  const Suggestion suggestion{
      SuggestionType::kInsecureContextPaymentDisabledMessage};
  external_delegate().DidSelectSuggestion(suggestion);

  // Ensure it doesn't try to fill the form in with the negative id.
  EXPECT_CALL(client(), HideAutofillSuggestions(
                            SuggestionHidingReason::kAcceptSuggestion));
  EXPECT_CALL(manager(), FillOrPreviewCreditCardForm).Times(0);

  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 0});
}

// Test that the Autofill delegate still allows previewing and filling
// specifically of the negative ID for SuggestionType::kIbanEntry.
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateFillsIbanEntry) {
  IssueOnQuery();

  EXPECT_CALL(
      client(),
      ShowAutofillSuggestions(
          PopupOpenArgsAre(SuggestionVectorIdsAre(SuggestionType::kIbanEntry)),
          _));
  std::vector<Suggestion> suggestions;
  Iban iban = test::GetLocalIban();
  suggestions.emplace_back(
      /*main_text=*/iban.GetIdentifierStringForAutofillDisplay(),
      SuggestionType::kIbanEntry);
  suggestions[0].labels = {{Suggestion::Text(u"My doctor's IBAN")}};
  suggestions[0].payload = Suggestion::Guid(iban.guid());
  OnSuggestionsReturned(queried_field().global_id(), suggestions);

  EXPECT_CALL(driver(), RendererShouldClearPreviewedForm());
  EXPECT_CALL(manager(),
              FillOrPreviewField(mojom::ActionPersistence::kPreview,
                                 mojom::FieldActionType::kReplaceAll,
                                 HasQueriedFormId(), HasQueriedFieldId(),
                                 iban.GetIdentifierStringForAutofillDisplay(),
                                 SuggestionType::kIbanEntry,
                                 std::optional(IBAN_VALUE)));
  external_delegate().DidSelectSuggestion(suggestions[0]);
  EXPECT_CALL(client(), HideAutofillSuggestions(
                            SuggestionHidingReason::kAcceptSuggestion));
  EXPECT_CALL(manager(),
              FillOrPreviewField(mojom::ActionPersistence::kFill,
                                 mojom::FieldActionType::kReplaceAll,
                                 HasQueriedFormId(), HasQueriedFieldId(),
                                 iban.value(), SuggestionType::kIbanEntry,
                                 std::optional(IBAN_VALUE)));
  Suggestion suggestion(iban.GetIdentifierStringForAutofillDisplay(),
                        SuggestionType::kIbanEntry);
  suggestion.payload = Suggestion::Guid(iban.guid());
  suggestion.labels = {{Suggestion::Text(u"My doctor's IBAN")}};
  EXPECT_CALL(*client().GetPaymentsAutofillClient()->GetIbanManager(),
              OnSingleFieldSuggestionSelected(suggestion));
  ON_CALL(*client().GetPaymentsAutofillClient()->GetIbanAccessManager(),
          FetchValue)
      .WillByDefault([iban](const Suggestion::BackendId& backend_id,
                            IbanAccessManager::OnIbanFetchedCallback callback) {
        std::move(callback).Run(iban.value());
      });

  external_delegate().DidAcceptSuggestion(suggestions[0],
                                          SuggestionPosition{.row = 0});
}

// Test that the Autofill delegate still allows previewing and filling
// specifically of the negative ID for
// SuggestionType::kMerchantPromoCodeEntry.
TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateFillsMerchantPromoCodeEntry) {
  IssueOnQuery();

  EXPECT_CALL(client(), ShowAutofillSuggestions(
                            PopupOpenArgsAre(SuggestionVectorIdsAre(
                                SuggestionType::kMerchantPromoCodeEntry)),
                            _));
  std::vector<Suggestion> suggestions;
  const std::u16string promo_code_value = u"PROMOCODE1234";
  suggestions.emplace_back(/*main_text=*/promo_code_value,
                           SuggestionType::kMerchantPromoCodeEntry);
  suggestions[0].main_text.value = promo_code_value;
  suggestions[0].labels = {{Suggestion::Text(u"12.34% off your purchase!")}};
  OnSuggestionsReturned(queried_field().global_id(), suggestions);

  EXPECT_CALL(driver(), RendererShouldClearPreviewedForm());
  EXPECT_CALL(manager(),
              FillOrPreviewField(mojom::ActionPersistence::kPreview,
                                 mojom::FieldActionType::kReplaceAll,
                                 HasQueriedFormId(), HasQueriedFieldId(),
                                 promo_code_value,
                                 SuggestionType::kMerchantPromoCodeEntry,
                                 std::optional(MERCHANT_PROMO_CODE)));
  external_delegate().DidSelectSuggestion(suggestions[0]);
  EXPECT_CALL(client(), HideAutofillSuggestions(
                            SuggestionHidingReason::kAcceptSuggestion));
  EXPECT_CALL(manager(),
              FillOrPreviewField(mojom::ActionPersistence::kFill,
                                 mojom::FieldActionType::kReplaceAll,
                                 HasQueriedFormId(), HasQueriedFieldId(),
                                 promo_code_value,
                                 SuggestionType::kMerchantPromoCodeEntry,
                                 std::optional(MERCHANT_PROMO_CODE)));

  external_delegate().DidAcceptSuggestion(suggestions[0],
                                          SuggestionPosition{.row = 0});
}

// Test that the Autofill delegate routes the merchant promo code suggestions
// footer redirect logic correctly.
TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateMerchantPromoCodeSuggestionsFooter) {
  IssueOnQuery();
  const GURL gurl{"https://example.com/"};
  EXPECT_CALL(payments_client(), OpenPromoCodeOfferDetailsURL(gurl));

  external_delegate().DidAcceptSuggestion(
      test::CreateAutofillSuggestion(SuggestionType::kSeePromoCodeDetails,
                                     u"baz foo", gurl),
      SuggestionPosition{.row = 0});
}

// Test that the ClearPreview call is only sent if the form was being previewed
// (i.e. it isn't autofilling a password).
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateClearPreviewedForm) {
  // Ensure selecting a new password entries or Autofill entries will
  // cause any previews to get cleared.
  IssueOnQuery();
  EXPECT_CALL(driver(), RendererShouldClearPreviewedForm());
  external_delegate().DidSelectSuggestion(test::CreateAutofillSuggestion(
      SuggestionType::kAddressEntry, u"baz foo"));
  EXPECT_CALL(driver(), RendererShouldClearPreviewedForm());
  EXPECT_CALL(manager(), FillOrPreviewProfileForm(
                             mojom::ActionPersistence::kPreview,
                             HasQueriedFormId(), HasQueriedFieldId(), _, _));
  const AutofillProfile profile = test::GetFullProfile();
  pdm().address_data_manager().AddProfile(profile);
  external_delegate().DidSelectSuggestion(
      test::CreateAutofillSuggestion(SuggestionType::kAddressEntry, u"baz foo",
                                     Suggestion::Guid(profile.guid())));

  // Ensure selecting an autocomplete entry will cause any previews to
  // get cleared.
  EXPECT_CALL(driver(), RendererShouldClearPreviewedForm());
  EXPECT_CALL(manager(),
              FillOrPreviewField(mojom::ActionPersistence::kPreview,
                                 mojom::FieldActionType::kReplaceAll,
                                 HasQueriedFormId(), HasQueriedFieldId(),
                                 std::u16string(u"baz foo"),
                                 SuggestionType::kAutocompleteEntry,
                                 std::optional<FieldType>()));
  external_delegate().DidSelectSuggestion(test::CreateAutofillSuggestion(
      SuggestionType::kAutocompleteEntry, u"baz foo"));

  CreditCard card = test::GetMaskedServerCard();
  pdm().payments_data_manager().AddCreditCard(card);
  // Ensure selecting a virtual card entry will cause any previews to
  // get cleared.
  EXPECT_CALL(driver(), RendererShouldClearPreviewedForm());
  EXPECT_CALL(manager(), FillOrPreviewCreditCardForm(
                             mojom::ActionPersistence::kPreview,
                             HasQueriedFormId(), HasQueriedFieldId(), _, _, _));
  Suggestion suggestion(SuggestionType::kVirtualCreditCardEntry);
  suggestion.payload = Suggestion::Guid(card.guid());
  external_delegate().DidSelectSuggestion(suggestion);
}

// Test that the popup is hidden once we are done editing the autofill field.
TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateHidePopupAfterEditing) {
  EXPECT_CALL(client(), ShowAutofillSuggestions);
  test::GenerateTestAutofillPopup(&external_delegate());

  EXPECT_CALL(client(), HideAutofillSuggestions(
                            autofill::SuggestionHidingReason::kEndEditing));
  external_delegate().DidEndTextFieldEditing();
}

// Test that the driver is directed to accept the data list after being notified
// that the user accepted the data list suggestion.
TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateAcceptDatalistSuggestion) {
  IssueOnQuery();
  EXPECT_CALL(client(), HideAutofillSuggestions(
                            SuggestionHidingReason::kAcceptSuggestion));
  std::u16string dummy_string(u"baz qux");
  EXPECT_CALL(driver(), RendererShouldAcceptDataListSuggestion(
                            queried_field().global_id(), dummy_string));

  external_delegate().DidAcceptSuggestion(
      test::CreateAutofillSuggestion(SuggestionType::kDatalistEntry,
                                     dummy_string),
      SuggestionPosition{.row = 0});
}

// Test that a11y autofill availability is set to `kNoSuggestions` when
// the popup is open and if it was manually triggered on an unclassified field.
TEST_F(AutofillExternalDelegateUnitTest,
       AutofillSuggestionAvailability_ManuallFallback) {
  IssueOnQuery(AutofillSuggestionTriggerSource::kManualFallbackAddress);
  get_triggering_autofill_field()->SetTypeTo(AutofillType(UNKNOWN_TYPE));

  std::vector<Suggestion> suggestions = {test::CreateAutofillSuggestion(
      SuggestionType::kAddressEntry, u"Suggestion main_text")};
  OnSuggestionsReturned(queried_field().global_id(), suggestions);

  EXPECT_CALL(driver(),
              RendererShouldSetSuggestionAvailability(
                  queried_field().global_id(),
                  mojom::AutofillSuggestionAvailability::kNoSuggestions));

  external_delegate().OnSuggestionsShown(suggestions);
}

// Test that a11y autofill availability is set to `kAutofillAvailable` when
// the popup is open with regular autofill suggestions.
TEST_F(AutofillExternalDelegateUnitTest,
       AutofillSuggestionAvailability_Autofill) {
  IssueOnQuery();

  std::vector<Suggestion> suggestions = {
      Suggestion(u"Suggestion main_text", SuggestionType::kAddressEntry)};
  OnSuggestionsReturned(queried_field().global_id(), suggestions);

  EXPECT_CALL(driver(),
              RendererShouldSetSuggestionAvailability(
                  queried_field().global_id(),
                  mojom::AutofillSuggestionAvailability::kAutofillAvailable));

  external_delegate().OnSuggestionsShown(suggestions);
}

// Test that a11y autofill availability is set to `kAutocompleteAvailable` when
// the popup is open with autocomplete suggestions.
TEST_F(AutofillExternalDelegateUnitTest,
       AutofillSuggestionAvailability_Autocomplete) {
  IssueOnQuery();

  std::vector<Suggestion> suggestions = {
      Suggestion(u"Suggestion main_text", SuggestionType::kAutocompleteEntry)};
  OnSuggestionsReturned(queried_field().global_id(), suggestions);

  EXPECT_CALL(
      driver(),
      RendererShouldSetSuggestionAvailability(
          queried_field().global_id(),
          mojom::AutofillSuggestionAvailability::kAutocompleteAvailable));

  external_delegate().OnSuggestionsShown(suggestions);
}

// Test parameter data for asserting filling method metrics depending on the
// suggestion (`SuggestionType`) accepted.
struct FillingMethodMetricsTestParams {
  const SuggestionType type;
  const FillingMethod target_metric;
  const std::string test_name;
};

class FillingMethodMetricsUnitTest
    : public AutofillExternalDelegateUnitTest,
      public ::testing::WithParamInterface<FillingMethodMetricsTestParams> {};

const FillingMethodMetricsTestParams kFillingMethodMetricsTestCases[] = {
    {.type = SuggestionType::kAddressEntry,
     .target_metric = FillingMethod::kFullForm,
     .test_name = "addressEntry"},
    {.type = SuggestionType::kFillEverythingFromAddressProfile,
     .target_metric = FillingMethod::kFullForm,
     .test_name = "fillEverythingFromAddressProfile"},
    {.type = SuggestionType::kAddressFieldByFieldFilling,
     .target_metric = FillingMethod::kFieldByFieldFilling,
     .test_name = "fieldByFieldFilling"},
    {.type = SuggestionType::kFillFullAddress,
     .target_metric = FillingMethod::kGroupFillingAddress,
     .test_name = "fillFullAddress"},
    {.type = SuggestionType::kFillFullPhoneNumber,
     .target_metric = FillingMethod::kGroupFillingPhoneNumber,
     .test_name = "fillFullPhoneNumber"},
    {.type = SuggestionType::kFillFullEmail,
     .target_metric = FillingMethod::kGroupFillingEmail,
     .test_name = "fillFullEmail"},
};

// Tests that for a certain `SuggestionType` accepted, the expected
// `FillingMethod` is recorded.
TEST_P(FillingMethodMetricsUnitTest, RecordFillingMethodForPopupType) {
  IssueOnQuery();
  const FillingMethodMetricsTestParams& params = GetParam();
  const AutofillProfile profile = test::GetFullProfile();
  pdm().address_data_manager().AddProfile(profile);
  const Suggestion suggestion =
      params.type == SuggestionType::kAddressFieldByFieldFilling
          ? CreateFieldByFieldFillingSuggestion(profile.guid(), NAME_FIRST)
          : test::CreateAutofillSuggestion(params.type);
  base::HistogramTester histogram_tester;

  // Field-by-field filling is the only filling method that can fill
  // unclassified fields.
  if (params.target_metric == FillingMethod::kFieldByFieldFilling) {
    get_triggering_autofill_field()->SetTypeTo(AutofillType(UNKNOWN_TYPE));
    external_delegate().DidAcceptSuggestion(suggestion,
                                            SuggestionPosition{.row = 0});

    histogram_tester.ExpectUniqueSample(
        "Autofill.FillingMethodUsed.Address."
        "TriggeringFieldDoesNotMatchFillingProduct",
        params.target_metric, 1);

    get_triggering_autofill_field()->SetTypeTo(AutofillType(NAME_FIRST));
    // Now the field is classified as an address field, assert the expected
    // metric.
  }
  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 0});

  histogram_tester.ExpectUniqueSample(
      "Autofill.FillingMethodUsed.Address."
      "TriggeringFieldMatchesFillingProduct",
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
  const FieldTypeSet field_types_to_fill;
  const SuggestionType type;
  const std::string test_name;
};

class GroupFillingUnitTest
    : public AutofillExternalDelegateUnitTest,
      public ::testing::WithParamInterface<GroupFillingTestParams> {};

const GroupFillingTestParams kGroupFillingTestCases[] = {
    {.field_types_to_fill = GetFieldTypesOfGroup(FieldTypeGroup::kName),
     .type = SuggestionType::kFillFullName,
     .test_name = "_NameFields"},
    {.field_types_to_fill = GetFieldTypesOfGroup(FieldTypeGroup::kPhone),
     .type = SuggestionType::kFillFullPhoneNumber,
     .test_name = "_PhoneFields"},
    {.field_types_to_fill = GetFieldTypesOfGroup(FieldTypeGroup::kEmail),
     .type = SuggestionType::kFillFullEmail,
     .test_name = "_EmailAddressFields"},
    {.field_types_to_fill = GetAddressFieldsForGroupFilling(),
     .type = SuggestionType::kFillFullAddress,
     .test_name = "_AddressFields"}};

// Tests that the expected server field set is forwarded to the manager
// depending on the chosen suggestion.
TEST_P(GroupFillingUnitTest, GroupFillingTests_FillAndPreview) {
  IssueOnQuery();
  const GroupFillingTestParams& params = GetParam();
  const AutofillProfile profile = test::GetFullProfile();
  pdm().address_data_manager().AddProfile(profile);
  const Suggestion suggestion =
      params.type == SuggestionType::kAddressFieldByFieldFilling
          ? CreateFieldByFieldFillingSuggestion(profile.guid(), NAME_FIRST)
          : test::CreateAutofillSuggestion(params.type, u"baz foo",
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
  external_delegate().DidSelectSuggestion(suggestion);

  // Test fill
  EXPECT_CALL(manager(),
              FillOrPreviewProfileForm(
                  mojom::ActionPersistence::kFill, HasQueriedFormId(),
                  HasQueriedFieldId(), _,
                  EqualsAutofillTriggerDetails(
                      {.trigger_source = expected_source,
                       .field_types_to_fill = params.field_types_to_fill})));
  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 0});
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
  EXPECT_CALL(client(), HideAutofillSuggestions(
                            SuggestionHidingReason::kAcceptSuggestion));
  EXPECT_CALL(manager(), FillOrPreviewProfileForm(
                             mojom::ActionPersistence::kFill,
                             HasQueriedFormId(), HasQueriedFieldId(), _, _));

  const AutofillProfile profile = test::GetFullProfile();
  pdm().address_data_manager().AddProfile(profile);
  external_delegate().DidAcceptSuggestion(
      test::CreateAutofillSuggestion(SuggestionType::kAddressEntry,
                                     u"John Legend",
                                     Suggestion::Guid(profile.guid())),
      SuggestionPosition{.row = 2});
}

TEST_F(AutofillExternalDelegateUnitTest,
       TestAddressSuggestionShown_MetricsEmitted) {
  base::HistogramTester histogram_tester;
  client().set_test_addresses({test::GetFullProfile()});
  IssueOnQuery();
  std::vector<Suggestion> suggestions = {test::CreateAutofillSuggestion(
      SuggestionType::kDevtoolsTestAddresses, u"Devtools")};
  OnSuggestionsReturned(queried_field().global_id(), suggestions);
  external_delegate().OnSuggestionsShown(suggestions);
  histogram_tester.ExpectUniqueSample(
      "Autofill.TestAddressesEvent",
      autofill_metrics::AutofillInDevtoolsTestAddressesEvents::
          kTestAddressesSuggestionShown,
      1);
}

// Test that an accepted test address autofill suggestion will fill the form.
TEST_F(AutofillExternalDelegateUnitTest, TestAddressSuggestion_FillAndPreview) {
  IssueOnQuery();
  const AutofillProfile profile = test::GetFullProfile();
  client().set_test_addresses({profile});
  const Suggestion suggestion = test::CreateAutofillSuggestion(
      SuggestionType::kDevtoolsTestAddressEntry, u"John Legend",
      Suggestion::Guid(profile.guid()));
  base::HistogramTester histogram_tester;

  // Test preview.
  EXPECT_CALL(manager(), FillOrPreviewProfileForm(
                             mojom::ActionPersistence::kPreview,
                             HasQueriedFormId(), HasQueriedFieldId(), _, _));
  external_delegate().DidSelectSuggestion(suggestion);

  // Test fill.
  EXPECT_CALL(manager(), FillOrPreviewProfileForm(
                             mojom::ActionPersistence::kFill,
                             HasQueriedFormId(), HasQueriedFieldId(), _, _));
  EXPECT_CALL(client(), HideAutofillSuggestions(
                            SuggestionHidingReason::kAcceptSuggestion));
  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 0});
  histogram_tester.ExpectUniqueSample(
      "Autofill.TestAddressesEvent",
      autofill_metrics::AutofillInDevtoolsTestAddressesEvents::
          kTestAddressesSuggestionSelected,
      1);
}

TEST_F(AutofillExternalDelegateUnitTest,
       AcceptFirstPopupLevelSuggestion_LogSuggestionAcceptedMetric) {
  IssueOnQuery();
  const AutofillProfile profile = test::GetFullProfile();
  pdm().address_data_manager().AddProfile(profile);
  const int suggestion_accepted_row = 2;
  base::HistogramTester histogram_tester;

  external_delegate().DidAcceptSuggestion(
      test::CreateAutofillSuggestion(SuggestionType::kAddressEntry,
                                     u"John Legend",
                                     Suggestion::Guid(profile.guid())),
      AutofillSuggestionDelegate::SuggestionMetadata{
          .row = suggestion_accepted_row});

  histogram_tester.ExpectUniqueSample("Autofill.SuggestionAcceptedIndex",
                                      suggestion_accepted_row, 1);
}

TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateAccept_FillEverythingSuggestion_FillAndPreview) {
  IssueOnQuery();
  const AutofillProfile profile = test::GetFullProfile();
  pdm().address_data_manager().AddProfile(profile);
  const Suggestion suggestion = test::CreateAutofillSuggestion(
      SuggestionType::kFillEverythingFromAddressProfile, u"John Legend",
      Suggestion::Guid(profile.guid()));

  // Test preview.
  EXPECT_CALL(manager(), FillOrPreviewProfileForm(
                             mojom::ActionPersistence::kPreview,
                             HasQueriedFormId(), HasQueriedFieldId(), _, _));
  external_delegate().DidSelectSuggestion(suggestion);

  // Test fill.
  EXPECT_CALL(manager(), FillOrPreviewProfileForm(
                             mojom::ActionPersistence::kFill,
                             HasQueriedFormId(), HasQueriedFieldId(), _, _));
  EXPECT_CALL(client(), HideAutofillSuggestions(
                            SuggestionHidingReason::kAcceptSuggestion));
  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 2});
}

// Tests that when accepting a suggestion, the `AutofillSuggestionTriggerSource`
// is converted to the correct `AutofillTriggerSource`.
TEST_F(AutofillExternalDelegateUnitTest, AcceptSuggestion_TriggerSource) {
  // Expect that `kFormControlElementClicked` translates to source `kPopup` or
  // `kKeyboardAccessory`, depending on the platform.
  const AutofillProfile profile = test::GetFullProfile();
  pdm().address_data_manager().AddProfile(profile);
  Suggestion suggestion = test::CreateAutofillSuggestion(
      SuggestionType::kAddressEntry, /*main_text_value=*/u"",
      Suggestion::Guid(profile.guid()));

  IssueOnQuery(AutofillSuggestionTriggerSource::kFormControlElementClicked);
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
  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 1});

  // Expect that `kManualFallbackAddress` translates to the manual fallback
  // trigger source.
  IssueOnQuery(AutofillSuggestionTriggerSource::kManualFallbackAddress);
  expected_source = AutofillTriggerSource::kManualFallback;
  EXPECT_CALL(
      manager(),
      FillOrPreviewProfileForm(
          mojom::ActionPersistence::kFill, HasQueriedFormId(),
          HasQueriedFieldId(), _,
          EqualsAutofillTriggerDetails({.trigger_source = expected_source})));
  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 1});
}

// Tests that when the suggestion is of type
// `SuggestionType::kAddressFieldByFieldFilling`, we emit the expected metric
// corresponding to which field type was used.
TEST_F(AutofillExternalDelegateUnitTest,
       FieldByFieldFilling_SubPopup_EmitsTypeMetric) {
  const AutofillProfile profile = test::GetFullProfile();
  pdm().address_data_manager().AddProfile(profile);
  Suggestion suggestion =
      CreateFieldByFieldFillingSuggestion(profile.guid(), NAME_FIRST);
  IssueOnQuery();
  base::HistogramTester histogram_tester;

  external_delegate().DidAcceptSuggestion(
      suggestion, SuggestionPosition{.row = 0, .sub_popup_level = 1});

  histogram_tester.ExpectUniqueSample(
      "Autofill.FieldByFieldFilling.FieldTypeUsed.Address."
      "TriggeringFieldMatchesFillingProduct",
      autofill_metrics::AutofillFieldByFieldFillingTypes::kNameFirst, 1);
}

TEST_F(AutofillExternalDelegateUnitTest,
       FieldByFieldFilling_RootPopup_DoNotEmitTypeMetric) {
  const AutofillProfile profile = test::GetFullProfile();
  pdm().address_data_manager().AddProfile(profile);
  Suggestion suggestion =
      CreateFieldByFieldFillingSuggestion(profile.guid(), NAME_FIRST);
  IssueOnQuery();
  base::HistogramTester histogram_tester;

  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 0});

  histogram_tester.ExpectUniqueSample(
      "Autofill.FieldByFieldFilling.FieldTypeUsed.Address."
      "TriggeringFieldMatchesFillingProduct",
      autofill_metrics::AutofillFieldByFieldFillingTypes::kNameFirst, 0);
}

TEST_F(AutofillExternalDelegateUnitTest,
       FieldByFieldFilling_PreviewCreditCard) {
  const CreditCard local_card = test::GetCreditCard();
  pdm().payments_data_manager().AddCreditCard(local_card);
  Suggestion suggestion = CreateFieldByFieldFillingSuggestion(
      local_card.guid(), CREDIT_CARD_NAME_FULL);
  IssueOnQuery(AutofillSuggestionTriggerSource::kManualFallbackPayments);

  EXPECT_CALL(manager(),
              FillOrPreviewField(mojom::ActionPersistence::kPreview,
                                 mojom::FieldActionType::kReplaceAll,
                                 HasQueriedFormId(), HasQueriedFieldId(),
                                 suggestion.main_text.value,
                                 SuggestionType::kCreditCardFieldByFieldFilling,
                                 std::optional(CREDIT_CARD_NAME_FULL)));

  external_delegate().DidSelectSuggestion(suggestion);
}

TEST_F(AutofillExternalDelegateUnitTest,
       FieldByFieldFilling_FillCreditCardName) {
  const CreditCard local_card = test::GetCreditCard();
  pdm().payments_data_manager().AddCreditCard(local_card);
  Suggestion suggestion = CreateFieldByFieldFillingSuggestion(
      local_card.guid(), CREDIT_CARD_NAME_FULL);
  IssueOnQuery(AutofillSuggestionTriggerSource::kManualFallbackPayments);

  EXPECT_CALL(cc_access_manager(), FetchCreditCard).Times(0);
  EXPECT_CALL(manager(),
              FillOrPreviewField(mojom::ActionPersistence::kFill,
                                 mojom::FieldActionType::kReplaceAll,
                                 HasQueriedFormId(), HasQueriedFieldId(),
                                 suggestion.main_text.value,
                                 SuggestionType::kCreditCardFieldByFieldFilling,
                                 std::optional(CREDIT_CARD_NAME_FULL)));

  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 1});
}

TEST_F(AutofillExternalDelegateUnitTest,
       FieldByFieldFilling_FillCreditCardNumber_FetchingFailed) {
  const CreditCard server_card = test::GetMaskedServerCard();
  pdm().payments_data_manager().AddCreditCard(server_card);
  Suggestion suggestion = CreateFieldByFieldFillingSuggestion(
      server_card.guid(), CREDIT_CARD_NUMBER);
  IssueOnQuery(AutofillSuggestionTriggerSource::kManualFallbackPayments);

  EXPECT_CALL(cc_access_manager(), FetchCreditCard)
      .WillOnce(
          [&server_card](
              const CreditCard* credit_card,
              CreditCardAccessManager::OnCreditCardFetchedCallback callback) {
            EXPECT_EQ(*credit_card, server_card);
            std::move(callback).Run(
                /*result=*/CreditCardFetchResult::kTransientError,
                /*credit_card=*/nullptr);
          });
  EXPECT_CALL(manager(), FillOrPreviewField).Times(0);
  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 1});
}

TEST_F(AutofillExternalDelegateUnitTest,
       FieldByFieldFilling_FillCreditCardNumber_Fetched) {
  const CreditCard server_card = test::GetMaskedServerCard();
  pdm().payments_data_manager().AddCreditCard(server_card);
  Suggestion suggestion = CreateFieldByFieldFillingSuggestion(
      server_card.guid(), CREDIT_CARD_NUMBER);
  IssueOnQuery(AutofillSuggestionTriggerSource::kManualFallbackPayments);

  const CreditCard unlocked_card = test::GetFullServerCard();
  EXPECT_CALL(cc_access_manager(), FetchCreditCard)
      .WillOnce(
          [&server_card, &unlocked_card](
              const CreditCard* credit_card,
              CreditCardAccessManager::OnCreditCardFetchedCallback callback) {
            EXPECT_EQ(*credit_card, server_card);
            std::move(callback).Run(
                /*result=*/CreditCardFetchResult::kSuccess,
                /*credit_card=*/&unlocked_card);
          });
  EXPECT_CALL(
      manager(),
      FillOrPreviewField(
          mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
          HasQueriedFormId(), HasQueriedFieldId(),
          unlocked_card.GetInfo(CREDIT_CARD_NUMBER, pdm().app_locale()),
          SuggestionType::kCreditCardFieldByFieldFilling,
          std::optional(CREDIT_CARD_NUMBER)));
  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 1});
}

TEST_F(AutofillExternalDelegateUnitTest,
       VirtualCreditCard_ManualFallback_CreditCardForm_Preview) {
  const CreditCard enrolled_card =
      test::GetMaskedServerCardEnrolledIntoVirtualCardNumber();
  pdm().payments_data_manager().AddCreditCard(enrolled_card);
  Suggestion suggestion =
      test::CreateAutofillSuggestion(SuggestionType::kVirtualCreditCardEntry,
                                     /*main_text_value=*/u"Virtual credit card",
                                     Suggestion::Guid(enrolled_card.guid()));
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  manager().OnFormsSeen({form}, {});
  external_delegate().OnQuery(
      form, form.fields()[0],
      /*caret_bounds=*/gfx::Rect(),
      AutofillSuggestionTriggerSource::kManualFallbackPayments);

  EXPECT_CALL(cc_access_manager(), FetchCreditCard).Times(0);
  EXPECT_CALL(manager(), FillOrPreviewCreditCardForm(
                             mojom::ActionPersistence::kPreview,
                             Property(&FormData::global_id, form.global_id()),
                             Property(&FormFieldData::global_id,
                                      form.fields()[0].global_id()),
                             _, _, _));
  EXPECT_CALL(manager(), FillOrPreviewField).Times(0);

  external_delegate().DidSelectSuggestion(suggestion);
}

TEST_F(AutofillExternalDelegateUnitTest,
       VirtualCreditCard_ManualFallback_NonCreditCardForm_NoPreview) {
  const CreditCard enrolled_card =
      test::GetMaskedServerCardEnrolledIntoVirtualCardNumber();
  pdm().payments_data_manager().AddCreditCard(enrolled_card);

  Suggestion suggestion =
      test::CreateAutofillSuggestion(SuggestionType::kVirtualCreditCardEntry,
                                     /*main_text_value=*/u"Virtual credit card",
                                     Suggestion::Guid(enrolled_card.guid()));
  IssueOnQuery(AutofillSuggestionTriggerSource::kManualFallbackPayments);
  EXPECT_CALL(cc_access_manager(), FetchCreditCard).Times(0);
  EXPECT_CALL(manager(), AuthenticateThenFillCreditCardForm).Times(0);
  EXPECT_CALL(manager(), FillOrPreviewField).Times(0);

  external_delegate().DidSelectSuggestion(suggestion);
}

TEST_F(AutofillExternalDelegateUnitTest,
       VirtualCreditCard_ManualFallback_CreditCardForm_FullFormFilling) {
  const CreditCard enrolled_card =
      test::GetMaskedServerCardEnrolledIntoVirtualCardNumber();
  pdm().payments_data_manager().AddCreditCard(enrolled_card);
  Suggestion suggestion =
      test::CreateAutofillSuggestion(SuggestionType::kVirtualCreditCardEntry,
                                     /*main_text_value=*/u"Virtual credit card",
                                     Suggestion::Guid(enrolled_card.guid()));
  FormData form =
      CreateTestCreditCardFormData(/*is_https=*/true, /*use_month_type=*/false);
  manager().OnFormsSeen({form}, {});
  external_delegate().OnQuery(
      form, form.fields()[0],
      /*caret_bounds=*/gfx::Rect(),

      AutofillSuggestionTriggerSource::kManualFallbackPayments);

  EXPECT_CALL(manager(), AuthenticateThenFillCreditCardForm(
                             Property(&FormData::global_id, form.global_id()),
                             Property(&FormFieldData::global_id,
                                      form.fields()[0].global_id()),
                             _, _));
  EXPECT_CALL(manager(), FillOrPreviewField).Times(0);

  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 0});
}

TEST_F(AutofillExternalDelegateUnitTest,
       VirtualCreditCard_ManualFallback_FetchingFails) {
  const CreditCard enrolled_card =
      test::GetMaskedServerCardEnrolledIntoVirtualCardNumber();
  pdm().payments_data_manager().AddCreditCard(enrolled_card);
  Suggestion suggestion =
      test::CreateAutofillSuggestion(SuggestionType::kVirtualCreditCardEntry,
                                     /*main_text_value=*/u"Virtual credit card",
                                     Suggestion::Guid(enrolled_card.guid()));
  IssueOnQuery(AutofillSuggestionTriggerSource::kManualFallbackPayments);

  EXPECT_CALL(cc_access_manager(), FetchCreditCard)
      .WillOnce(
          [&enrolled_card](
              const CreditCard* credit_card,
              CreditCardAccessManager::OnCreditCardFetchedCallback callback) {
            CreditCard expected_card =
                CreditCard::CreateVirtualCard(enrolled_card);
            EXPECT_EQ(*credit_card, expected_card);
            std::move(callback).Run(
                /*result=*/CreditCardFetchResult::kTransientError,
                /*credit_card=*/nullptr);
          });
  EXPECT_CALL(manager(), AuthenticateThenFillCreditCardForm).Times(0);
  EXPECT_CALL(manager(), FillOrPreviewField).Times(0);

  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 0});
}

TEST_F(AutofillExternalDelegateUnitTest,
       VirtualCreditCard_ManualFallback_FetchingSucceeds) {
  const CreditCard enrolled_card =
      test::GetMaskedServerCardEnrolledIntoVirtualCardNumber();
  pdm().payments_data_manager().AddCreditCard(enrolled_card);
  Suggestion suggestion =
      test::CreateAutofillSuggestion(SuggestionType::kVirtualCreditCardEntry,
                                     /*main_text_value=*/u"Virtual credit card",
                                     Suggestion::Guid(enrolled_card.guid()));
  IssueOnQuery(AutofillSuggestionTriggerSource::kManualFallbackPayments);

  const CreditCard unlocked_card = test::GetFullServerCard();
  EXPECT_CALL(cc_access_manager(), FetchCreditCard)
      .WillOnce(
          [&unlocked_card, &enrolled_card](
              const CreditCard* credit_card,
              CreditCardAccessManager::OnCreditCardFetchedCallback callback) {
            CreditCard expected_card =
                CreditCard::CreateVirtualCard(enrolled_card);
            EXPECT_EQ(*credit_card, expected_card);
            std::move(callback).Run(
                /*result=*/CreditCardFetchResult::kSuccess,
                /*credit_card=*/&unlocked_card);
          });
  EXPECT_CALL(manager(), AuthenticateThenFillCreditCardForm).Times(0);
  EXPECT_CALL(manager(), FillOrPreviewField).Times(0);

  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 0});
}

// Tests that on acceptance of a `kRetrievePredictionImprovements` suggestion,
// the `AutofillPredictionImprovementsDelegate::OnClickedTriggerSuggestion()`
// event handler is called.
TEST_F(AutofillExternalDelegateUnitTest,
       DidAcceptRetrievePredictionImprovementsSuggestionCallsEventHandler) {
  EXPECT_CALL(*client().GetAutofillPredictionImprovementsDelegate(),
              OnClickedTriggerSuggestion);
  external_delegate().DidAcceptSuggestion(
      Suggestion(u"Autocomplete",
                 SuggestionType::kRetrievePredictionImprovements),
      {});
}

// Tests that on acceptance of a `kFillPredictionImprovements` suggestion with
// `Suggestion::PredictionImprovementsPayload` payload, the full form is filled
// accordingly.
TEST_F(AutofillExternalDelegateUnitTest,
       DidAcceptFillPredictionImprovementsFillsFullForm) {
  FormData form = CreateTestAddressFormData();
  ASSERT_GT(form.fields().size(), 0UL);
  const std::u16string value_to_fill = u"John";
  FormFieldData* field_to_fill = form.FindFieldByNameForTest(u"firstname");
  ASSERT_TRUE(field_to_fill);

  manager().OnFormsSeen({form}, {});
  external_delegate().OnQuery(
      form, *field_to_fill,
      /*caret_bounds=*/gfx::Rect(),
      AutofillSuggestionTriggerSource::kPredictionImprovements);
  Suggestion fill_suggestion =
      Suggestion(u"Autocomplete", SuggestionType::kFillPredictionImprovements);
  fill_suggestion.payload = Suggestion::PredictionImprovementsPayload(
      {{field_to_fill->global_id(), value_to_fill}}, {NAME_FIRST}, {});

  std::vector<FormFieldData> filled_fields;
  EXPECT_CALL(driver(), ApplyFormAction)
      .WillOnce(DoAll(SaveArgElementsTo<2>(&filled_fields),
                      Return(std::vector<FieldGlobalId>{})));
  external_delegate().DidAcceptSuggestion(fill_suggestion, {});

  EXPECT_THAT(filled_fields,
              ElementsAre(AllOf(
                  Property("global_id", &FormFieldData::global_id,
                           field_to_fill->global_id()),
                  Property("value", &FormFieldData::value, value_to_fill))));
}

// Tests that on acceptance of a `kFillPredictionImprovements` suggestion with
// `Suggestion::ValueToFill` payload, the queried field is filled.
TEST_F(AutofillExternalDelegateUnitTest,
       DidAcceptFillPredictionImprovementsFillsSingleField) {
  IssueOnQuery();
  ASSERT_GT(queried_form().fields().size(), 0UL);
  const std::u16string value_to_fill = u"John";

  Suggestion fill_suggestion =
      Suggestion(u"Autocomplete", SuggestionType::kFillPredictionImprovements);
  fill_suggestion.payload = Suggestion::ValueToFill(value_to_fill);

  EXPECT_CALL(
      manager(),
      FillOrPreviewField(mojom::ActionPersistence::kFill,
                         mojom::FieldActionType::kReplaceAll,
                         HasQueriedFormId(), HasQueriedFieldId(), value_to_fill,
                         SuggestionType::kFillPredictionImprovements, _));
  external_delegate().DidAcceptSuggestion(fill_suggestion, {});
}

// Tests that the `AutofillPredictionImprovementsDelegate` is notified when the
// `kPredictionImprovementsLoadingState` suggestion is shown.
TEST_F(AutofillExternalDelegateUnitTest,
       OnPredictionImprovementsLoadingStateShownNotifiesDelegate) {
  FormData form = CreateTestAddressFormData();
  ASSERT_GT(form.fields().size(), 0UL);
  const std::u16string value_to_fill = u"John";
  FormFieldData* field_to_fill = form.FindFieldByNameForTest(u"firstname");
  ASSERT_TRUE(field_to_fill);

  manager().OnFormsSeen({form}, {});
  external_delegate().OnQuery(
      form, *field_to_fill,
      /*caret_bounds=*/gfx::Rect(),
      AutofillSuggestionTriggerSource::kPredictionImprovements);
  EXPECT_CALL(*client().GetAutofillPredictionImprovementsDelegate(),
              OnLoadingSuggestionShown);
  external_delegate().OnSuggestionsShown(std::vector<Suggestion>{
      Suggestion(SuggestionType::kPredictionImprovementsLoadingState)});
}

// Test parameter data for asserting that the expected set of field types
// is stored in the delegate.
struct GetLastFieldTypesToFillForSectionTestParams {
  SuggestionType last_accepted_address_suggestion_for_section;
  const SuggestionType type;
  const std::optional<Section> section;
  const bool is_preview = false;
  const std::string test_name;
};

class GetLastFieldTypesToFillUnitTest
    : public AutofillExternalDelegateUnitTest,
      public ::testing::WithParamInterface<
          GetLastFieldTypesToFillForSectionTestParams> {};

const GetLastFieldTypesToFillForSectionTestParams
    kGetLastFieldTypesToFillForSectionTesCases[] = {
        // Tests that when `SuggestionType::kAddressEntry` is accepted and
        // therefore the user wanted to fill the whole form. Autofill
        // stores the last targeted fields as `kAllFieldTypes`.
        {.last_accepted_address_suggestion_for_section =
             SuggestionType::kAddressEntry,
         .type = SuggestionType::kAddressEntry,
         .test_name = "_AllFields"},
        // Tests that when `SuggestionType::kAddressFieldByFieldFilling`
        // is accepted and therefore the user wanted to fill a single field.
        // The last targeted fields is stored as the triggering field type
        // only, this way the next time the user interacts
        // with the form, they are kept at the same filling granularity.
        {.last_accepted_address_suggestion_for_section =
             SuggestionType::kAddressFieldByFieldFilling,
         .type = SuggestionType::kAddressFieldByFieldFilling,
         .test_name = "_SingleField"},
        // Tests that when `GetLastFieldTypesToFillForSection` is called for
        // a section for which no information was stored, `std::nullopt` is
        // returned.
        {.last_accepted_address_suggestion_for_section =
             SuggestionType::kAddressEntry,
         .type = SuggestionType::kCreditCardEntry,
         .test_name = "_EmptySet"},
        {.last_accepted_address_suggestion_for_section =
             SuggestionType::kAddressEntry,
         .type = SuggestionType::kAddressEntry,
         .section = Section::FromAutocomplete({.section = "another-section"}),
         .test_name = "_DoesNotReturnsForNonExistingSection"},
        // Tests that when `SuggestionType::kFillFullAddress` is selected
        // (i.e preview) we do not store anything as last accepted suggestion.
        {.last_accepted_address_suggestion_for_section =
             SuggestionType::kAddressEntry,
         .type = SuggestionType::kFillFullAddress,
         .is_preview = true,
         .test_name = "_NotStoredDuringPreview"}};

// Tests that the expected set of last field types to fill is stored.
TEST_P(GetLastFieldTypesToFillUnitTest, LastFieldTypesToFillForSection) {
  IssueOnQuery();
  const GetLastFieldTypesToFillForSectionTestParams& params = GetParam();
  base::test::ScopedFeatureList features(
      features::kAutofillGranularFillingAvailable);

  const AutofillProfile profile = test::GetFullProfile();
  pdm().address_data_manager().AddProfile(profile);
  IssueOnQuery();
  ON_CALL(address_data_manager(), IsAutofillProfileEnabled)
      .WillByDefault(Return(true));
  const Suggestion suggestion =
      params.type == SuggestionType::kAddressFieldByFieldFilling
          ? CreateFieldByFieldFillingSuggestion(profile.guid(), NAME_FIRST)
          : test::CreateAutofillSuggestion(params.type);

  if (!params.is_preview) {
    external_delegate().DidAcceptSuggestion(
        suggestion, SuggestionPosition{.row = 1, .sub_popup_level = 1});
  } else {
    external_delegate().DidSelectSuggestion(suggestion);
  }

  EXPECT_EQ(
      external_delegate().GetLastAcceptedSuggestionToFillForSection(
          params.section.value_or(get_triggering_autofill_field()->section())),
      params.last_accepted_address_suggestion_for_section);
}

INSTANTIATE_TEST_SUITE_P(
    AutofillExternalDelegateUnitTest,
    GetLastFieldTypesToFillUnitTest,
    ::testing::ValuesIn(kGetLastFieldTypesToFillForSectionTesCases),
    [](const ::testing::TestParamInfo<
        GetLastFieldTypesToFillUnitTest::ParamType>& info) {
      return info.param.test_name;
    });

class AutofillExternalDelegatePlusAddressUnitTest
    : public AutofillExternalDelegateUnitTest {
 public:
  AutofillExternalDelegatePlusAddressUnitTest() = default;

  void SetUp() override {
    AutofillExternalDelegateUnitTest::SetUp();
    client().set_plus_address_delegate(
        std::make_unique<NiceMock<MockAutofillPlusAddressDelegate>>());
  }

 protected:
  MockAutofillPlusAddressDelegate& plus_address_delegate() {
    return static_cast<MockAutofillPlusAddressDelegate&>(
        *client().GetPlusAddressDelegate());
  }

  const std::vector<Suggestion>& suggestions() const { return suggestions_; }

  void ShowPlusAddressInlineSuggestion(
      std::optional<std::u16string> plus_address) {
    IssueOnQuery();

    suggestions_.emplace_back(/*main_text=*/u"Create plus address",
                              SuggestionType::kCreateNewPlusAddressInline);
    suggestions_.back().payload = Suggestion::PlusAddressPayload(plus_address);
    OnSuggestionsReturned(queried_field().global_id(), suggestions_);
    ON_CALL(client(), GetAutofillSuggestions)
        .WillByDefault(Return(base::span<const Suggestion>(suggestions_)));
    client().set_suggestion_ui_session_id(
        AutofillClient::SuggestionUiSessionId(123));
  }

 private:
  // The currently shown suggestions. Kept as a member since
  // `GetAutofillSuggestions` returns a span.
  std::vector<Suggestion> suggestions_;
};

// Mock out an existing plus address autofill suggestion, and ensure that
// choosing it results in the field being filled with its value (as opposed to
// the mocked address used in the creation flow).
TEST_F(AutofillExternalDelegatePlusAddressUnitTest,
       ExternalDelegateFillsExistingPlusAddress) {
  IssueOnQuery();

  base::HistogramTester histogram_tester;

  EXPECT_CALL(client(), ShowAutofillSuggestions(
                            PopupOpenArgsAre(SuggestionVectorIdsAre(
                                SuggestionType::kFillExistingPlusAddress)),
                            _));
  const std::u16string plus_address = u"test+plus@test.example";
  std::vector<Suggestion> suggestions;
  suggestions.emplace_back(/*main_text=*/plus_address,
                           SuggestionType::kFillExistingPlusAddress);
  // This function tests the filling of existing plus addresses, which is why
  // `OfferPlusAddressCreation` need not be mocked.
  OnSuggestionsReturned(queried_field().global_id(), suggestions);

  EXPECT_CALL(driver(), RendererShouldClearPreviewedForm());
  EXPECT_CALL(
      manager(),
      FillOrPreviewField(mojom::ActionPersistence::kPreview,
                         mojom::FieldActionType::kReplaceAll,
                         HasQueriedFormId(), HasQueriedFieldId(), plus_address,
                         SuggestionType::kFillExistingPlusAddress,
                         std::optional(EMAIL_ADDRESS)));
  external_delegate().DidSelectSuggestion(suggestions[0]);
  EXPECT_CALL(client(), HideAutofillSuggestions(
                            SuggestionHidingReason::kAcceptSuggestion));
  EXPECT_CALL(plus_address_delegate(),
              RecordAutofillSuggestionEvent(
                  MockAutofillPlusAddressDelegate::SuggestionEvent::
                      kExistingPlusAddressChosen));
  EXPECT_CALL(
      manager(),
      FillOrPreviewField(mojom::ActionPersistence::kFill,
                         mojom::FieldActionType::kReplaceAll,
                         HasQueriedFormId(), HasQueriedFieldId(), plus_address,
                         SuggestionType::kFillExistingPlusAddress,
                         std::optional(EMAIL_ADDRESS)));
  external_delegate().DidAcceptSuggestion(suggestions[0],
                                          SuggestionPosition{.row = 0});
}

// Mock out the new plus address creation flow, and ensure that its completion
// results in the field being filled with the resulting plus address.
TEST_F(AutofillExternalDelegatePlusAddressUnitTest,
       ExternalDelegateOffersPlusAddressCreation) {
  const std::u16string kMockPlusAddressForCreationCallback =
      u"test+1234@test.example";

  IssueOnQuery();

  base::HistogramTester histogram_tester;
  EXPECT_CALL(client(), ShowAutofillSuggestions(
                            PopupOpenArgsAre(SuggestionVectorIdsAre(
                                SuggestionType::kCreateNewPlusAddress)),
                            _));
  std::vector<Suggestion> suggestions;
  suggestions.emplace_back(/*main_text=*/u"",
                           SuggestionType::kCreateNewPlusAddress);
  OnSuggestionsReturned(queried_field().global_id(), suggestions);

  EXPECT_CALL(driver(), RendererShouldClearPreviewedForm());
  external_delegate().DidSelectSuggestion(suggestions[0]);
  EXPECT_CALL(client(), HideAutofillSuggestions(
                            SuggestionHidingReason::kAcceptSuggestion));
  EXPECT_CALL(plus_address_delegate(),
              RecordAutofillSuggestionEvent(
                  MockAutofillPlusAddressDelegate::SuggestionEvent::
                      kCreateNewPlusAddressChosen));

  // Mock out the plus address creation logic to ensure it is deterministic and
  // independent of the client implementations in //chrome or //ios.
  EXPECT_CALL(client(), OfferPlusAddressCreation)
      .WillOnce([&](const url::Origin& origin, PlusAddressCallback callback) {
        std::move(callback).Run(
            base::UTF16ToUTF8(kMockPlusAddressForCreationCallback));
      });
  // `kMockPlusAddressForCreationCallback` is returned in the callback from the
  // mocked `OfferPlusAddressCreation()`. Ensure it is filled (vs, say, the
  // empty text of the suggestion).
  EXPECT_CALL(manager(),
              FillOrPreviewField(mojom::ActionPersistence::kFill,
                                 mojom::FieldActionType::kReplaceAll,
                                 HasQueriedFormId(), HasQueriedFieldId(),
                                 kMockPlusAddressForCreationCallback,
                                 SuggestionType::kCreateNewPlusAddress,
                                 std::optional(EMAIL_ADDRESS)));
  external_delegate().DidAcceptSuggestion(suggestions[0],
                                          SuggestionPosition{.row = 0});
}

// Tests that showing a plus address inline suggestion calls
// `AutofillPlusAddressDelegate` with a callback that updates the Autofill
// popup.
TEST_F(AutofillExternalDelegatePlusAddressUnitTest,
       PlusAddressInlineSuggestionShown) {
  ShowPlusAddressInlineSuggestion(std::nullopt);

  {
    InSequence s;
    std::vector<Suggestion> updated_suggestions = suggestions();
    updated_suggestions[0].payload =
        Suggestion::PlusAddressPayload(u"test+plus@test.example");
    EXPECT_CALL(plus_address_delegate(),
                OnShowedInlineSuggestion(
                    _, base::span<const Suggestion>(suggestions()), _))
        .WillOnce(RunOnceCallback<2>(updated_suggestions,
                                     AutofillSuggestionTriggerSource::
                                         kPlusAddressUpdatedInBrowserProcess));
    EXPECT_CALL(client(),
                UpdateAutofillSuggestions(
                    updated_suggestions, FillingProduct::kPlusAddresses,
                    AutofillSuggestionTriggerSource::
                        kPlusAddressUpdatedInBrowserProcess));
  }

  external_delegate().OnSuggestionsShown(suggestions());
}

// Tests that selecting an inline plus address suggestion previews the value
// stored in the payload.
TEST_F(AutofillExternalDelegatePlusAddressUnitTest,
       PlusAddressInlineSuggestionSelected) {
  const std::u16string plus_address = u"test+plus@test.example";
  ShowPlusAddressInlineSuggestion(plus_address);

  EXPECT_CALL(driver(), RendererShouldClearPreviewedForm());
  EXPECT_CALL(
      manager(),
      FillOrPreviewField(mojom::ActionPersistence::kPreview,
                         mojom::FieldActionType::kReplaceAll,
                         HasQueriedFormId(), HasQueriedFieldId(), plus_address,
                         SuggestionType::kCreateNewPlusAddressInline,
                         std::optional(EMAIL_ADDRESS)));
  external_delegate().DidSelectSuggestion(suggestions()[0]);
}

// Tests that selecting an inline plus address suggestion with an empty address
// value does not preview anything.
TEST_F(AutofillExternalDelegatePlusAddressUnitTest,
       PlusAddressInlineSuggestionSelectedWithNoAddress) {
  ShowPlusAddressInlineSuggestion(std::nullopt);

  EXPECT_CALL(driver(), RendererShouldClearPreviewedForm());
  EXPECT_CALL(manager(), FillOrPreviewField).Times(0);
  external_delegate().DidSelectSuggestion(suggestions()[0]);
}

// Tests that triggering the extra button action on a plus address inline
// suggestion informs the plus address delegate and passes a callback that can
// be used to update the Autofill suggestions.
TEST_F(AutofillExternalDelegatePlusAddressUnitTest,
       PlusAddressExtraButtonAction) {
  ShowPlusAddressInlineSuggestion(u"test+plus@test.example");

  {
    InSequence s;

    std::vector<Suggestion> updated_suggestions = suggestions();
    updated_suggestions.back().payload = Suggestion::PlusAddressPayload();
    EXPECT_CALL(driver(), RendererShouldClearPreviewedForm);
    EXPECT_CALL(plus_address_delegate(),
                OnClickedRefreshInlineSuggestion(
                    _, base::span<const Suggestion>(suggestions()),
                    /*current_suggestion_index=*/0, _))
        .WillOnce(RunOnceCallback<3>(updated_suggestions,
                                     AutofillSuggestionTriggerSource::
                                         kPlusAddressUpdatedInBrowserProcess));
    EXPECT_CALL(client(),
                UpdateAutofillSuggestions(
                    updated_suggestions, FillingProduct::kPlusAddresses,
                    AutofillSuggestionTriggerSource::
                        kPlusAddressUpdatedInBrowserProcess));
  }

  external_delegate().DidPerformButtonActionForSuggestion(
      suggestions()[0], SuggestionButtonAction());
}

// Tests that triggering the extra button action on a plus address error
// suggestion informs the plus address delegate and passes a callback that can
// be used to update the Autofill suggestions.
TEST_F(AutofillExternalDelegatePlusAddressUnitTest,
       PlusAddressExtraButtonActionForErrorSuggestion) {
  IssueOnQuery();

  const std::u16string plus_address = u"test+plus@test.example";
  std::vector<Suggestion> suggestions;
  suggestions.emplace_back(/*main_text=*/u"Error reserving",
                           SuggestionType::kPlusAddressError);
  OnSuggestionsReturned(queried_field().global_id(), suggestions);
  ON_CALL(client(), GetAutofillSuggestions)
      .WillByDefault(Return(base::span<const Suggestion>(suggestions)));
  client().set_suggestion_ui_session_id(
      AutofillClient::SuggestionUiSessionId(123));

  {
    InSequence s;

    std::vector<Suggestion> updated_suggestions;
    updated_suggestions.emplace_back(
        /*main_text=*/u"Create plus address",
        SuggestionType::kCreateNewPlusAddressInline);
    updated_suggestions.back().payload =
        Suggestion::PlusAddressPayload(plus_address);
    EXPECT_CALL(plus_address_delegate(),
                OnClickedRefreshInlineSuggestion(
                    _, base::span<const Suggestion>(suggestions),
                    /*current_suggestion_index=*/0, _))
        .WillOnce(RunOnceCallback<3>(updated_suggestions,
                                     AutofillSuggestionTriggerSource::
                                         kPlusAddressUpdatedInBrowserProcess));
    EXPECT_CALL(client(),
                UpdateAutofillSuggestions(
                    updated_suggestions, FillingProduct::kPlusAddresses,
                    AutofillSuggestionTriggerSource::
                        kPlusAddressUpdatedInBrowserProcess));
  }

  external_delegate().DidPerformButtonActionForSuggestion(
      suggestions[0], SuggestionButtonAction());
}

// Tests that running the update callback is a no-op if the session id of the
// suggestions UI has changed since the update callback was requested.
TEST_F(AutofillExternalDelegatePlusAddressUnitTest,
       PlusAddressExtraButtonActionUiSessionIdChanged) {
  ShowPlusAddressInlineSuggestion(u"test+plus@test.example");

  base::OnceCallback<void(std::vector<Suggestion>,
                          AutofillSuggestionTriggerSource)>
      update_callback;
  EXPECT_CALL(client(), UpdateAutofillSuggestions).Times(0);
  EXPECT_CALL(plus_address_delegate(),
              OnClickedRefreshInlineSuggestion(
                  _, base::span<const Suggestion>(suggestions()),
                  /*current_suggestion_index=*/0, _))
      .WillOnce(MoveArg<3>(&update_callback));

  client().set_suggestion_ui_session_id(
      AutofillClient::SuggestionUiSessionId(3));
  external_delegate().DidPerformButtonActionForSuggestion(
      suggestions()[0], SuggestionButtonAction());
  ASSERT_TRUE(update_callback);

  // Now simulate that the popup has a new session id.
  client().set_suggestion_ui_session_id(
      AutofillClient::SuggestionUiSessionId(4));
  std::move(update_callback)
      .Run(
          suggestions(),
          AutofillSuggestionTriggerSource::kPlusAddressUpdatedInBrowserProcess);
}

// Tests that running the update callback is safe even after AED has been
// destroyed.
TEST_F(AutofillExternalDelegatePlusAddressUnitTest,
       PlusAddressExtraButtonActionIsAlwaysSafeToCall) {
  ShowPlusAddressInlineSuggestion(u"test+plus@test.example");

  base::OnceCallback<void(std::vector<Suggestion>,
                          AutofillSuggestionTriggerSource)>
      update_callback;
  EXPECT_CALL(client(), UpdateAutofillSuggestions).Times(0);
  EXPECT_CALL(plus_address_delegate(),
              OnClickedRefreshInlineSuggestion(
                  _, base::span<const Suggestion>(suggestions()),
                  /*current_suggestion_index=*/0, _))
      .WillOnce(MoveArg<3>(&update_callback));

  external_delegate().DidPerformButtonActionForSuggestion(
      suggestions()[0], SuggestionButtonAction());
  ASSERT_TRUE(update_callback);
  ResetDriver();
  std::move(update_callback)
      .Run(
          suggestions(),
          AutofillSuggestionTriggerSource::kPlusAddressUpdatedInBrowserProcess);
}

// Tests that triggering the extra button action on a plus address inline
// suggestion informs the plus address delegate and passes a callback that can
// be used to update the Autofill suggestions.
TEST_F(AutofillExternalDelegatePlusAddressUnitTest, PlusAddressInlineAccepted) {
  const std::u16string plus_address = u"test+plus@test.example";
  ShowPlusAddressInlineSuggestion(plus_address);

  using UpdateSuggestionsCallback =
      AutofillPlusAddressDelegate::UpdateSuggestionsCallback;
  using HideSuggestionsCallback =
      AutofillPlusAddressDelegate::HideSuggestionsCallback;
  UpdateSuggestionsCallback update_callback;
  HideSuggestionsCallback hide_callback;
  PlusAddressCallback filling_callback;
  std::vector<Suggestion> updated_suggestions = suggestions();
  updated_suggestions.back().is_loading = Suggestion::IsLoading(true);
  MockFunction<void()> check;
  {
    InSequence s;

    // `MoveArg` only supports moving out a single argument and cannot be
    // combined via `DoAll` - therefore use a helper.
    EXPECT_CALL(plus_address_delegate(),
                OnAcceptedInlineSuggestion(
                    _, base::span<const Suggestion>(suggestions()),
                    /*current_suggestion_index=*/0, _, _, _, _, _, _))
        .WillOnce(
            [&](const url::Origin& primary_main_frame_origin,
                base::span<const Suggestion> current_suggestions,
                size_t current_suggestion_index,
                UpdateSuggestionsCallback update_suggestions_callback,
                HideSuggestionsCallback hide_suggestions_callback,
                PlusAddressCallback fill_field_callback,
                AutofillPlusAddressDelegate::ShowAffiliationErrorDialogCallback,
                AutofillPlusAddressDelegate::ShowErrorDialogCallback,
                base::OnceClosure reshow_suggestions) {
              update_callback = std::move(update_suggestions_callback);
              hide_callback = std::move(hide_suggestions_callback);
              filling_callback = std::move(fill_field_callback);
            });
    EXPECT_CALL(client(),
                UpdateAutofillSuggestions(
                    updated_suggestions, FillingProduct::kPlusAddresses,
                    AutofillSuggestionTriggerSource::
                        kPlusAddressUpdatedInBrowserProcess));
    EXPECT_CALL(check, Call);
    EXPECT_CALL(client(), HideAutofillSuggestions(
                              SuggestionHidingReason::kAcceptSuggestion));
    EXPECT_CALL(check, Call);
    EXPECT_CALL(manager(),
                FillOrPreviewField(mojom::ActionPersistence::kFill,
                                   mojom::FieldActionType::kReplaceAll,
                                   HasQueriedFormId(), HasQueriedFieldId(),
                                   plus_address,
                                   SuggestionType::kCreateNewPlusAddressInline,
                                   std::optional(EMAIL_ADDRESS)));
  }

  external_delegate().DidAcceptSuggestion(suggestions()[0],
                                          SuggestionPosition{.row = 0});
  ASSERT_TRUE(update_callback);
  ASSERT_TRUE(hide_callback);
  ASSERT_TRUE(filling_callback);

  std::move(update_callback)
      .Run(
          updated_suggestions,
          AutofillSuggestionTriggerSource::kPlusAddressUpdatedInBrowserProcess);
  check.Call();
  std::move(hide_callback).Run(SuggestionHidingReason::kAcceptSuggestion);
  check.Call();
  std::move(filling_callback).Run(base::UTF16ToUTF8(plus_address));
}

// Tests that `OnAcceptedInlineSuggestion` gets passed a
// `ShowAffiliationErrorDialogCallback` that, when run, triggers showing a plus
// address affiliation error dialog in `AutofillClient`. If that dialog is
// accepted, the affiliated plus address is filled.
TEST_F(AutofillExternalDelegatePlusAddressUnitTest,
       PlusAddressInlineAcceptedAffiliationError) {
  ShowPlusAddressInlineSuggestion(u"test+plus@test.example");
  const std::u16string affiliated_domain = u"https://bar.com";
  const std::u16string affiliated_plus_address = u"foo@bar.com";

  AutofillPlusAddressDelegate::ShowAffiliationErrorDialogCallback
      show_affiliation_error_callback;
  EXPECT_CALL(plus_address_delegate(),
              OnAcceptedInlineSuggestion(
                  _, base::span<const Suggestion>(suggestions()),
                  /*current_suggestion_index=*/0, _, _, _, _, _, _))
      .WillOnce(MoveArg<6>(&show_affiliation_error_callback));
  // Simulate accepting the dialog.
  EXPECT_CALL(client(), ShowPlusAddressAffiliationError(
                            affiliated_domain, affiliated_plus_address, _))
      .WillOnce(RunOnceCallback<2>());
  EXPECT_CALL(manager(),
              FillOrPreviewField(mojom::ActionPersistence::kFill,
                                 mojom::FieldActionType::kReplaceAll,
                                 HasQueriedFormId(), HasQueriedFieldId(),
                                 affiliated_plus_address,
                                 SuggestionType::kCreateNewPlusAddressInline,
                                 std::optional(EMAIL_ADDRESS)));

  external_delegate().DidAcceptSuggestion(suggestions()[0],
                                          SuggestionPosition{.row = 0});
  ASSERT_TRUE(show_affiliation_error_callback);
  // Simulate showing the affiliation error dialog.
  std::move(show_affiliation_error_callback)
      .Run(affiliated_domain, affiliated_plus_address);
}

// Tests that `OnAcceptedInlineSuggestion` gets passed a
// `ShowErrorDialogCallback` that, when run, triggers showing a plus address
// error dialog in `AutofillClient`.
TEST_F(AutofillExternalDelegatePlusAddressUnitTest,
       PlusAddressInlineAcceptedQuotaError) {
  ShowPlusAddressInlineSuggestion(u"test+plus@test.example");

  AutofillPlusAddressDelegate::ShowErrorDialogCallback show_error_callback;
  EXPECT_CALL(plus_address_delegate(),
              OnAcceptedInlineSuggestion(
                  _, base::span<const Suggestion>(suggestions()),
                  /*current_suggestion_index=*/0, _, _, _, _, _, _))
      .WillOnce(MoveArg<7>(&show_error_callback));
  EXPECT_CALL(
      client(),
      ShowPlusAddressError(
          AutofillClient::PlusAddressErrorDialogType::kQuotaExhausted, _));

  external_delegate().DidAcceptSuggestion(suggestions()[0],
                                          SuggestionPosition{.row = 0});
  ASSERT_TRUE(show_error_callback);
  std::move(show_error_callback)
      .Run(AutofillClient::PlusAddressErrorDialogType::kQuotaExhausted,
           base::DoNothing());
}

// Tests that `OnAcceptedInlineSuggestion` gets passed a closure that, when run,
// triggers reshowing the plus address suggestions.
TEST_F(AutofillExternalDelegatePlusAddressUnitTest,
       PlusAddressInlineAcceptedReshowSuggestions) {
  ShowPlusAddressInlineSuggestion(u"test+plus@test.example");

  base::OnceClosure reshow_suggestions;
  EXPECT_CALL(plus_address_delegate(),
              OnAcceptedInlineSuggestion(
                  _, base::span<const Suggestion>(suggestions()),
                  /*current_suggestion_index=*/0, _, _, _, _, _, _))
      .WillOnce(MoveArg<8>(&reshow_suggestions));
  EXPECT_CALL(
      driver(),
      RendererShouldTriggerSuggestions(
          queried_field().global_id(),
          AutofillSuggestionTriggerSource::kManualFallbackPlusAddresses));

  external_delegate().DidAcceptSuggestion(suggestions()[0],
                                          SuggestionPosition{.row = 0});
  ASSERT_TRUE(reshow_suggestions);
  std::move(reshow_suggestions).Run();
}

TEST_F(
    AutofillExternalDelegateUnitTest,
    PredictionImprovements_DidPerformButtonAction_ThumbsUpFeedbackIsForwardedToDelegate) {
  IssueOnQuery();

  // TODO(crbug.com/362468426): Update comment in case it is decided that
  // feedback will be its own suggestion.
  EXPECT_CALL(
      *client().GetAutofillPredictionImprovementsDelegate(),
      UserFeedbackReceived(
          AutofillPredictionImprovementsDelegate::UserFeedback::kThumbsUp));

  external_delegate().DidPerformButtonActionForSuggestion(
      Suggestion(SuggestionType::kPredictionImprovementsFeedback),
      PredictionImprovementsButtonActions::kThumbsUpClicked);
}

TEST_F(
    AutofillExternalDelegateUnitTest,
    PredictionImprovements_DidPerformButtonAction_ThumbsDownFeedbackIsForwardedToDelegate) {
  IssueOnQuery();

  // TODO(crbug.com/362468426): Update comment in case it is decided that
  // feedback will be its own suggestion.
  EXPECT_CALL(
      *client().GetAutofillPredictionImprovementsDelegate(),
      UserFeedbackReceived(
          AutofillPredictionImprovementsDelegate::UserFeedback::kThumbsDown));

  external_delegate().DidPerformButtonActionForSuggestion(
      Suggestion(SuggestionType::kPredictionImprovementsFeedback),
      PredictionImprovementsButtonActions::kThumbsDownClicked);
}

TEST_F(
    AutofillExternalDelegateUnitTest,
    PredictionImprovements_DidPerformButtonAction_LearnMoreIsForwardedToDelegate) {
  IssueOnQuery();

  // TODO(crbug.com/362468426): Update comment in case it is decided that
  // feedback will be its own suggestion.
  EXPECT_CALL(*client().GetAutofillPredictionImprovementsDelegate(),
              UserClickedLearnMore());

  external_delegate().DidPerformButtonActionForSuggestion(
      Suggestion(SuggestionType::kPredictionImprovementsFeedback),
      PredictionImprovementsButtonActions::kLearnMoreClicked);
}

TEST_F(AutofillExternalDelegateUnitTest,
       ComposeSuggestion_ComposeProactiveNudge_ForwardsCaretBoundsToClient) {
  const gfx::Rect caret_bounds = gfx::Rect(/*width=*/1, /*height=*/3);
  FormGlobalId form_id = test::MakeFormGlobalId();
  FieldGlobalId field_id = test::MakeFieldGlobalId();
  FormData form_data = test::GetFormData({
      .fields = {{.role = NAME_FIRST,
                  .host_frame = field_id.frame_token,
                  .renderer_id = field_id.renderer_id,
                  .autocomplete_attribute = "given-name"}},
      .host_frame = form_id.frame_token,
      .renderer_id = form_id.renderer_id,
  });
  // make sure the field bounds contain the caret.
  test_api(form_data).field(0).set_bounds(gfx::RectF(
      /*x=*/0, /*y=*/0, caret_bounds.width() * 2, caret_bounds.height() * 2));

  IssueOnQuery(std::move(form_data), caret_bounds);

  EXPECT_CALL(client(),
              ShowAutofillSuggestions(
                  AllOf(Field(&AutofillClient::PopupOpenArgs::element_bounds,
                              gfx::RectF(caret_bounds)),
                        Field(&AutofillClient::PopupOpenArgs::anchor_type,
                              PopupAnchorType::kCaret)),
                  _));
  MockAutofillComposeDelegate compose_delegate;
  ON_CALL(client(), GetComposeDelegate)
      .WillByDefault(Return(&compose_delegate));
  ON_CALL(compose_delegate, ShouldAnchorNudgeOnCaret)
      .WillByDefault(Return(true));

  // This should call ShowAutofillSuggestions.
  OnSuggestionsReturned(queried_field().global_id(),
                        {Suggestion(SuggestionType::kComposeProactiveNudge)});
}

// Even though the `SuggestionType` is correct and the bounds are valid. The
// caret bounds are not used because the
// `ComposeDelegate::ShouldAnchorNudgeOnCaret()` is returning false.
TEST_F(
    AutofillExternalDelegateUnitTest,
    ComposeSuggestion_ComposeProactiveNudge_ShouldAnchorNudgeOnCaretReturnsFalse_DoNotForwardsCaretBoundsToClient) {
  const gfx::Rect caret_bounds = gfx::Rect(/*width=*/1, /*height=*/3);
  FormGlobalId form_id = test::MakeFormGlobalId();
  FieldGlobalId field_id = test::MakeFieldGlobalId();
  FormData form_data = test::GetFormData({
      .fields = {{.role = NAME_FIRST,
                  .host_frame = field_id.frame_token,
                  .renderer_id = field_id.renderer_id,
                  .autocomplete_attribute = "given-name"}},
      .host_frame = form_id.frame_token,
      .renderer_id = form_id.renderer_id,
  });
  // make sure the field bounds contain the caret.
  const gfx::RectF field_bounds = gfx::RectF(
      /*x=*/0, /*y=*/0, caret_bounds.width() * 2, caret_bounds.height() * 2);
  test_api(form_data).field(0).set_bounds(field_bounds);

  IssueOnQuery(std::move(form_data), caret_bounds);

  EXPECT_CALL(
      client(),
      ShowAutofillSuggestions(
          Field(&AutofillClient::PopupOpenArgs::element_bounds, field_bounds),
          _));
  MockAutofillComposeDelegate compose_delegate;
  ON_CALL(client(), GetComposeDelegate)
      .WillByDefault(Return(&compose_delegate));
  ON_CALL(compose_delegate, ShouldAnchorNudgeOnCaret)
      .WillByDefault(Return(false));

  // This should call ShowAutofillSuggestions.
  OnSuggestionsReturned(queried_field().global_id(),
                        {Suggestion(SuggestionType::kComposeProactiveNudge)});
}

TEST_F(
    AutofillExternalDelegateUnitTest,
    ComposeSuggestion_ComposeProactiveNudge_CaretOutsideField_DoNotSendCaretBoundsToClient) {
  const gfx::Rect caret_bounds = gfx::Rect(/*width=*/1, /*height=*/3);
  FormGlobalId form_id = test::MakeFormGlobalId();
  FieldGlobalId field_id = test::MakeFieldGlobalId();
  FormData form_data = test::GetFormData({
      .fields = {{.role = NAME_FIRST,
                  .host_frame = field_id.frame_token,
                  .renderer_id = field_id.renderer_id,
                  .autocomplete_attribute = "given-name"}},
      .host_frame = form_id.frame_token,
      .renderer_id = form_id.renderer_id,
  });
  // make sure the field bounds do not contain the caret.
  const gfx::RectF field_bounds = gfx::RectF(
      /*x=*/caret_bounds.x() + caret_bounds.width() + 1,
      /*y=*/caret_bounds.y() + caret_bounds.height() + 1, caret_bounds.width(),
      caret_bounds.height());
  test_api(form_data).field(0).set_bounds(field_bounds);

  IssueOnQuery(std::move(form_data), caret_bounds);

  EXPECT_CALL(
      client(),
      ShowAutofillSuggestions(
          Field(&AutofillClient::PopupOpenArgs::element_bounds, field_bounds),
          _));
  MockAutofillComposeDelegate compose_delegate;
  ON_CALL(client(), GetComposeDelegate)
      .WillByDefault(Return(&compose_delegate));
  ON_CALL(compose_delegate, ShouldAnchorNudgeOnCaret)
      .WillByDefault(Return(true));

  // This should call ShowAutofillSuggestions.
  OnSuggestionsReturned(queried_field().global_id(),
                        {Suggestion(SuggestionType::kComposeProactiveNudge)});
}

TEST_F(
    AutofillExternalDelegateUnitTest,
    NonComposeSuggestion_NonComposeProactiveNudge_DoNotForwardsCaretBoundsToClient) {
  IssueOnQuery(gfx::Rect(/*width=*/123, /*height=*/123));

  const PopupAnchorType default_anchor_type =
#if BUILDFLAG(IS_ANDROID)
      PopupAnchorType::kKeyboardAccessory;
#else
      PopupAnchorType::kField;
#endif
  EXPECT_CALL(client(),
              ShowAutofillSuggestions(
                  AllOf(Field(&AutofillClient::PopupOpenArgs::element_bounds,
                              gfx::RectF(/*width=*/0, /*height=*/0)),
                        Field(&AutofillClient::PopupOpenArgs::anchor_type,
                              default_anchor_type)),
                  _));
  MockAutofillComposeDelegate compose_delegate;
  ON_CALL(client(), GetComposeDelegate)
      .WillByDefault(Return(&compose_delegate));
  ON_CALL(compose_delegate, ShouldAnchorNudgeOnCaret)
      .WillByDefault(Return(true));

  // This should call ShowAutofillSuggestions.
  OnSuggestionsReturned(queried_field().global_id(),
                        {Suggestion(SuggestionType::kAutocompleteEntry)});
}

// Tests that accepting a Compose suggestion returns a callback that, when run,
// fills the trigger field.
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateOpensComposeAndFills) {
  MockAutofillComposeDelegate compose_delegate;
  ON_CALL(client(), GetComposeDelegate)
      .WillByDefault(Return(&compose_delegate));

  IssueOnQuery();

  // Simulate receiving a Compose suggestion.
  EXPECT_CALL(client(),
              ShowAutofillSuggestions(PopupOpenArgsAre(SuggestionVectorIdsAre(
                                          SuggestionType::kComposeResumeNudge)),
                                      _));
  std::vector<Suggestion> suggestions = {
      Suggestion(SuggestionType::kComposeResumeNudge)};
  OnSuggestionsReturned(queried_field().global_id(), suggestions);

  // Simulate accepting a Compose suggestion.
  EXPECT_CALL(
      compose_delegate,
      OpenCompose(_, queried_field().renderer_form_id(),
                  queried_field().global_id(),
                  AutofillComposeDelegate::UiEntryPoint::kAutofillPopup));
  EXPECT_CALL(client(), HideAutofillSuggestions(
                            SuggestionHidingReason::kAcceptSuggestion));
  external_delegate().DidAcceptSuggestion(suggestions[0],
                                          SuggestionPosition{.row = 0});
}

TEST_F(AutofillExternalDelegateUnitTest,
       Compose_AcceptDisable_CallsComposeDelegate) {
  MockAutofillComposeDelegate compose_delegate;
  ON_CALL(client(), GetComposeDelegate)
      .WillByDefault(Return(&compose_delegate));

  IssueOnQuery();

  // Simulate accepting a Compose `SuggestionType::kComposeDisable`
  // suggestion.
  EXPECT_CALL(compose_delegate, DisableCompose);
  external_delegate().DidAcceptSuggestion(
      Suggestion(SuggestionType::kComposeDisable),
      SuggestionPosition{.row = 0});
}

TEST_F(AutofillExternalDelegateUnitTest,
       Compose_AcceptGoToSettings_CallsComposeDelegate) {
  MockAutofillComposeDelegate compose_delegate;
  ON_CALL(client(), GetComposeDelegate)
      .WillByDefault(Return(&compose_delegate));

  IssueOnQuery();

  // Simulate accepting a Compose `SuggestionType::kComposeGoToSettings`
  // suggestion.
  EXPECT_CALL(compose_delegate, GoToSettings);
  external_delegate().DidAcceptSuggestion(
      Suggestion(SuggestionType::kComposeGoToSettings),
      SuggestionPosition{.row = 0});
}

TEST_F(AutofillExternalDelegateUnitTest,
       Compose_AcceptNeverShowOnThisWebsiteAgain_CallsComposeDelegate) {
  MockAutofillComposeDelegate compose_delegate;
  ON_CALL(client(), GetComposeDelegate)
      .WillByDefault(Return(&compose_delegate));

  IssueOnQuery();

  // Simulate accepting a Compose
  // `SuggestionType::kComposeNeverShowOnThisSiteAgain` suggestion.
  EXPECT_CALL(compose_delegate, NeverShowComposeForOrigin);
  external_delegate().DidAcceptSuggestion(
      Suggestion(SuggestionType::kComposeNeverShowOnThisSiteAgain),
      SuggestionPosition{.row = 0});
}

#if !BUILDFLAG(IS_IOS)
// Test that the driver is directed to clear or undo the form after being
// notified that the user accepted the suggestion to clear or undo the form.
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateUndoForm) {
  IssueOnQuery();
  EXPECT_CALL(manager(), UndoAutofill);
  external_delegate().DidAcceptSuggestion(
      Suggestion(SuggestionType::kUndoOrClear), SuggestionPosition{.row = 0});
}

// Test that the driver is directed to undo the form after being notified that
// the user selected the suggestion to undo the form.
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateUndoPreviewForm) {
  IssueOnQuery();
  EXPECT_CALL(manager(), UndoAutofill);
  external_delegate().DidSelectSuggestion(
      Suggestion(SuggestionType::kUndoOrClear));
}
#endif

// Test that autofill client will scan a credit card after use accepted the
// suggestion to scan a credit card.
TEST_F(AutofillExternalDelegateUnitTest, ScanCreditCardMenuItem) {
  IssueOnQuery();
  EXPECT_CALL(payments_client(), ScanCreditCard);
  EXPECT_CALL(client(), HideAutofillSuggestions(
                            SuggestionHidingReason::kAcceptSuggestion));

  external_delegate().DidAcceptSuggestion(
      Suggestion(SuggestionType::kScanCreditCard),
      SuggestionPosition{.row = 0});
}

TEST_F(AutofillExternalDelegateUnitTest,
       ScanCreditCardMetrics_SuggestionShown) {
  base::HistogramTester histogram;
  IssueOnQuery();
  std::vector<Suggestion> suggestions = {
      Suggestion(SuggestionType::kScanCreditCard)};
  OnSuggestionsReturned(queried_field().global_id(), suggestions);
  external_delegate().OnSuggestionsShown(suggestions);

  histogram.ExpectUniqueSample("Autofill.ScanCreditCardPrompt",
                               AutofillMetrics::SCAN_CARD_ITEM_SHOWN, 1);
}

TEST_F(AutofillExternalDelegateUnitTest,
       ScanCreditCardMetrics_SuggestionAccepted) {
  base::HistogramTester histogram;
  IssueOnQuery();
  std::vector<Suggestion> suggestions = {
      Suggestion(SuggestionType::kScanCreditCard)};
  OnSuggestionsReturned(queried_field().global_id(), suggestions);
  external_delegate().OnSuggestionsShown(suggestions);

  external_delegate().DidAcceptSuggestion(
      Suggestion(SuggestionType::kScanCreditCard),
      SuggestionPosition{.row = 0});

  histogram.ExpectBucketCount("Autofill.ScanCreditCardPrompt",
                              AutofillMetrics::SCAN_CARD_ITEM_SHOWN, 1);
  histogram.ExpectBucketCount("Autofill.ScanCreditCardPrompt",
                              AutofillMetrics::SCAN_CARD_ITEM_SELECTED, 1);
  histogram.ExpectBucketCount("Autofill.ScanCreditCardPrompt",
                              AutofillMetrics::SCAN_CARD_OTHER_ITEM_SELECTED,
                              0);
}

TEST_F(AutofillExternalDelegateUnitTest,
       ScanCreditCardMetrics_DifferentSuggestionAccepted) {
  base::HistogramTester histogram;
  IssueOnQuery();
  std::vector<Suggestion> suggestions = {
      Suggestion(SuggestionType::kScanCreditCard)};
  OnSuggestionsReturned(queried_field().global_id(), suggestions);
  external_delegate().OnSuggestionsShown(suggestions);

  external_delegate().DidAcceptSuggestion(
      Suggestion(SuggestionType::kCreditCardEntry),
      SuggestionPosition{.row = 0});

  histogram.ExpectBucketCount("Autofill.ScanCreditCardPrompt",
                              AutofillMetrics::SCAN_CARD_ITEM_SHOWN, 1);
  histogram.ExpectBucketCount("Autofill.ScanCreditCardPrompt",
                              AutofillMetrics::SCAN_CARD_ITEM_SELECTED, 0);
  histogram.ExpectBucketCount("Autofill.ScanCreditCardPrompt",
                              AutofillMetrics::SCAN_CARD_OTHER_ITEM_SELECTED,
                              1);
}

TEST_F(AutofillExternalDelegateUnitTest,
       ScanCreditCardMetrics_SuggestionNotShown) {
  base::HistogramTester histogram;
  IssueOnQuery();
  OnSuggestionsReturned(queried_field().global_id(), {});
  external_delegate().OnSuggestionsShown({});
  histogram.ExpectTotalCount("Autofill.ScanCreditCardPrompt", 0);
}

TEST_F(AutofillExternalDelegateUnitTest, AutocompleteShown_MetricsEmitted) {
  base::HistogramTester histogram;
  IssueOnQuery();
  std::vector<Suggestion> suggestions = {test::CreateAutofillSuggestion(
      SuggestionType::kAutocompleteEntry, u"autocomplete")};
  OnSuggestionsReturned(queried_field().global_id(), suggestions);
  external_delegate().OnSuggestionsShown(suggestions);
  histogram.ExpectBucketCount("Autocomplete.Events2",
                              AutofillMetrics::AUTOCOMPLETE_SUGGESTIONS_SHOWN,
                              1);
}

MATCHER_P(CreditCardMatches, card, "") {
  return !arg.Compare(card);
}

// Test that autofill manager will fill the credit card form after user scans a
// credit card.
TEST_F(AutofillExternalDelegateUnitTest, FillCreditCardForm) {
  CreditCard card;
  test::SetCreditCardInfo(&card, "Alice", "4111", "1", "3000", "1");
  EXPECT_CALL(manager(), FillOrPreviewCreditCardForm(
                             mojom::ActionPersistence::kFill, _, _,
                             CreditCardMatches(card), std::u16string(), _));
  external_delegate().OnCreditCardScanned(AutofillTriggerSource::kPopup, card);
}

TEST_F(AutofillExternalDelegateUnitTest, IgnoreAutocompleteOffForAutofill) {
  const FormData form;
  FormFieldData field;
  field.set_is_focusable(true);
  field.set_should_autocomplete(false);

  external_delegate().OnQuery(form, field, /*caret_bounds=*/gfx::Rect(),
                              kDefaultTriggerSource);

  std::vector<Suggestion> autofill_items;
  autofill_items.emplace_back();
  autofill_items[0].type = SuggestionType::kAutocompleteEntry;

  // Ensure the popup tries to show itself, despite autocomplete="off".
  EXPECT_CALL(client(), ShowAutofillSuggestions);
  EXPECT_CALL(client(), HideAutofillSuggestions(_)).Times(0);

  OnSuggestionsReturned(field.global_id(), autofill_items);
}

TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateFillFieldWithValue_Autocomplete) {
  EXPECT_CALL(client(), HideAutofillSuggestions(
                            SuggestionHidingReason::kAcceptSuggestion));
  IssueOnQuery();

  base::HistogramTester histogram_tester;
  std::u16string dummy_autocomplete_string(u"autocomplete");
  Suggestion suggestion(SuggestionType::kAutocompleteEntry);
  suggestion.main_text.value = dummy_autocomplete_string;
  EXPECT_CALL(
      manager(),
      FillOrPreviewField(
          mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
          HasQueriedFormId(), HasQueriedFieldId(), dummy_autocomplete_string,
          SuggestionType::kAutocompleteEntry, std::optional<FieldType>()));
  EXPECT_CALL(*client().GetMockAutocompleteHistoryManager(),
              OnSingleFieldSuggestionSelected(suggestion));

  external_delegate().DidAcceptSuggestion(
      test::CreateAutofillSuggestion(SuggestionType::kAutocompleteEntry,
                                     dummy_autocomplete_string),
      SuggestionPosition{.row = 0});

  histogram_tester.ExpectUniqueSample(
      "Autofill.SuggestionAcceptedIndex.Autocomplete", 0, 1);
}

TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateFillFieldWithValue_MerchantPromoCode) {
  EXPECT_CALL(client(), HideAutofillSuggestions(
                            SuggestionHidingReason::kAcceptSuggestion));
  IssueOnQuery();

  std::u16string dummy_promo_code_string(u"merchant promo");
  Suggestion suggestion(SuggestionType::kMerchantPromoCodeEntry);
  suggestion.main_text.value = dummy_promo_code_string;
  EXPECT_CALL(manager(),
              FillOrPreviewField(mojom::ActionPersistence::kFill,
                                 mojom::FieldActionType::kReplaceAll,
                                 HasQueriedFormId(), HasQueriedFieldId(),
                                 dummy_promo_code_string,
                                 SuggestionType::kMerchantPromoCodeEntry,
                                 std::optional(MERCHANT_PROMO_CODE)));
  EXPECT_CALL(
      *client().GetPaymentsAutofillClient()->GetMockMerchantPromoCodeManager(),
      OnSingleFieldSuggestionSelected(suggestion));

  external_delegate().DidAcceptSuggestion(
      test::CreateAutofillSuggestion(SuggestionType::kMerchantPromoCodeEntry,
                                     dummy_promo_code_string),
      SuggestionPosition{.row = 0});
}

TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateFillFieldWithValue_Iban) {
  EXPECT_CALL(client(), HideAutofillSuggestions(
                            SuggestionHidingReason::kAcceptSuggestion));
  IssueOnQuery();

  Iban iban = test::GetLocalIban();
  Suggestion suggestion(SuggestionType::kIbanEntry);
  suggestion.main_text.value = iban.GetIdentifierStringForAutofillDisplay();
  suggestion.payload = Suggestion::Guid(iban.guid());
  EXPECT_CALL(manager(),
              FillOrPreviewField(mojom::ActionPersistence::kFill,
                                 mojom::FieldActionType::kReplaceAll,
                                 HasQueriedFormId(), HasQueriedFieldId(),
                                 iban.value(), SuggestionType::kIbanEntry,
                                 std::optional(IBAN_VALUE)));
  EXPECT_CALL(*client().GetPaymentsAutofillClient()->GetIbanManager(),
              OnSingleFieldSuggestionSelected(suggestion));

  ON_CALL(*client().GetPaymentsAutofillClient()->GetIbanAccessManager(),
          FetchValue)
      .WillByDefault([iban](const Suggestion::BackendId& backend_id,
                            IbanAccessManager::OnIbanFetchedCallback callback) {
        std::move(callback).Run(iban.value());
      });
  external_delegate().DidAcceptSuggestion(
      test::CreateAutofillSuggestion(
          SuggestionType::kIbanEntry,
          iban.GetIdentifierStringForAutofillDisplay(),
          Suggestion::Guid(iban.guid())),
      SuggestionPosition{.row = 0});
}

TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateFillFieldWithValue_FieldByFieldFilling) {
  const AutofillProfile profile = test::GetFullProfile();
  pdm().address_data_manager().AddProfile(profile);
  IssueOnQuery();
  Suggestion suggestion =
      CreateFieldByFieldFillingSuggestion(profile.guid(), NAME_FIRST);
  EXPECT_CALL(client(), HideAutofillSuggestions(
                            SuggestionHidingReason::kAcceptSuggestion));
  EXPECT_CALL(
      manager(),
      FillOrPreviewField(
          mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
          HasQueriedFormId(), HasQueriedFieldId(),
          profile.GetRawInfo(*suggestion.field_by_field_filling_type_used),
          SuggestionType::kAddressFieldByFieldFilling,
          std::optional(NAME_FIRST)));
  EXPECT_CALL(manager(), OnDidFillAddressFormFillingSuggestion(
                             Property(&AutofillProfile::guid, profile.guid()),
                             HasQueriedFormId(), HasQueriedFieldId(), _));

  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 0});
}

// Test that browser autofill manager will handle the unmasking request for the
// virtual card after users accept the suggestion to use a virtual card.
TEST_F(AutofillExternalDelegateUnitTest, AcceptVirtualCardOptionItem) {
  IssueOnQuery();
  FormData form;
  CreditCard card = test::GetMaskedServerCard();
  pdm().payments_data_manager().AddCreditCard(card);
  EXPECT_CALL(manager(), AuthenticateThenFillCreditCardForm(
                             HasQueriedFormId(), HasQueriedFieldId(), _, _));
  Suggestion suggestion(SuggestionType::kVirtualCreditCardEntry);
  suggestion.payload = Suggestion::Guid(card.guid());
  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 0});
}

TEST_F(AutofillExternalDelegateUnitTest, SelectVirtualCardOptionItem) {
  IssueOnQuery();
  CreditCard card = test::GetMaskedServerCard();
  pdm().payments_data_manager().AddCreditCard(card);
  EXPECT_CALL(manager(), FillOrPreviewCreditCardForm(
                             mojom::ActionPersistence::kPreview,
                             HasQueriedFormId(), HasQueriedFieldId(), _, _, _));
  Suggestion suggestion(SuggestionType::kVirtualCreditCardEntry);
  suggestion.payload = Suggestion::Guid(card.guid());
  external_delegate().DidSelectSuggestion(suggestion);
}

class AutofillExternalDelegate_RemoveSuggestionTest
    : public AutofillExternalDelegateUnitTest,
      public ::testing::WithParamInterface<SuggestionType> {
 public:
  void SetUp() override {
    AutofillExternalDelegateUnitTest::SetUp();
    test_api(manager()).set_single_field_form_fill_router(
        std::make_unique<MockSingleFieldFormFillRouter>(
            client().GetMockAutocompleteHistoryManager(), nullptr, nullptr));
  }

  MockSingleFieldFormFillRouter& single_field_form_fill_router() {
    return static_cast<MockSingleFieldFormFillRouter&>(
        test_api(manager()).single_field_form_fill_router());
  }
};

const SuggestionType kRemoveSuggestionTestCases[] = {
    SuggestionType::kAddressEntry,
    SuggestionType::kFillFullAddress,
    SuggestionType::kFillFullName,
    SuggestionType::kFillFullEmail,
    SuggestionType::kFillFullPhoneNumber,
    SuggestionType::kAddressFieldByFieldFilling,
    SuggestionType::kCreditCardFieldByFieldFilling,
    SuggestionType::kCreditCardEntry,
    SuggestionType::kAutocompleteEntry,
    SuggestionType::kPasswordEntry,
};

INSTANTIATE_TEST_SUITE_P(AutofillExternalDelegateUnitTest,
                         AutofillExternalDelegate_RemoveSuggestionTest,
                         ::testing::ValuesIn(kRemoveSuggestionTestCases));

TEST_P(AutofillExternalDelegate_RemoveSuggestionTest, RemoveSuggestion) {
  const AutofillProfile profile = test::GetFullProfile();
  const Suggestion& suggestion = test::CreateAutofillSuggestion(
      GetParam(), u"autofill suggestion", Suggestion::Guid(profile.guid()));
  pdm().address_data_manager().AddProfile(profile);

  if (suggestion.type == SuggestionType::kAutocompleteEntry) {
    EXPECT_CALL(single_field_form_fill_router(),
                OnRemoveCurrentSingleFieldSuggestion);
  } else if (suggestion.type != SuggestionType::kPasswordEntry) {
    // Passwords entries cannot be deleted. Since all the remaining ones are
    // address or credit card, we can expect that pdm is called.
    EXPECT_CALL(pdm(), RemoveByGUID);
  }
  bool result = external_delegate().RemoveSuggestion(suggestion);

  // Password entries are the only ones from the test set that cannot be
  // deleted.
  EXPECT_EQ(result, suggestion.type != SuggestionType::kPasswordEntry);
}

TEST_F(AutofillExternalDelegateCardsFromAccountTest,
       ShowCardsFromAccountMetrics) {
  using Event = autofill_metrics::ShowCardsFromGoogleAccountButtonEvent;
  static constexpr std::string_view kUmaName =
      "Autofill.ButterForPayments.ShowCardsFromGoogleAccountButtonEvents";
  base::HistogramTester histogram_tester;

  auto show_suggestions = [&]() {
    std::vector<Suggestion> suggestions = {
        Suggestion(SuggestionType::kShowAccountCards)};
    OnSuggestionsReturned(queried_field().global_id(), suggestions);
    external_delegate().OnSuggestionsShown(suggestions);
  };
  IssueOnQuery();

  show_suggestions();
  EXPECT_THAT(histogram_tester.GetAllSamples(kUmaName),
              BucketsAre(base::Bucket(Event::kButtonAppeared, 1),
                         base::Bucket(Event::kButtonAppearedOnce, 1)));

  show_suggestions();
  EXPECT_THAT(histogram_tester.GetAllSamples(kUmaName),
              BucketsAre(base::Bucket(Event::kButtonAppeared, 2),
                         base::Bucket(Event::kButtonAppearedOnce, 1)));
}

TEST_F(AutofillExternalDelegateUnitTest,
       RecordSuggestionTypeOnSuggestionAccepted) {
  base::HistogramTester histogram_tester;
  IssueOnQuery();

  const AutofillProfile profile = test::GetFullProfile();
  pdm().address_data_manager().AddProfile(profile);

  external_delegate().DidAcceptSuggestion(
      test::CreateAutofillSuggestion(SuggestionType::kAddressEntry),
      SuggestionPosition{.row = 0});

  histogram_tester.ExpectUniqueSample("Autofill.Suggestions.AcceptedType",
                                      SuggestionType::kAddressEntry, 1);
}

// Tests that setting `is_update` to true in
// `AttemptToDisplayAutofillSuggestions` leads to a call to
// `AutofillClient::UpdateAutofillSuggestions`.
TEST_F(AutofillExternalDelegateUnitTest, UpdateSuggestions) {
  IssueOnQuery();

  std::vector<Suggestion> suggestions1 = {Suggestion(u"Some suggestion")};
  std::vector<Suggestion> suggestions2 = {Suggestion(u"Other suggestion")};

  {
    InSequence s;
    EXPECT_CALL(client(), ShowAutofillSuggestions);
    EXPECT_CALL(client(), UpdateAutofillSuggestions(
                              suggestions2, FillingProduct::kAutocomplete,
                              AutofillSuggestionTriggerSource::kUnspecified));
  }

  OnSuggestionsReturned(queried_field().global_id(), suggestions1);
  external_delegate().AttemptToDisplayAutofillSuggestionsForTest(
      suggestions2, /*suggestion_ranking_context=*/std::nullopt,
      AutofillSuggestionTriggerSource::kUnspecified, /*is_update=*/true);
}

// TODO(crbug.com/41483208): Add test case where 'Show cards from your Google
// account' button is clicked. Encountered issues with test sync setup when
// attempting to make it.

#if BUILDFLAG(IS_IOS)
// Tests that outdated returned suggestions are discarded.
TEST_F(AutofillExternalDelegateCardsFromAccountTest,
       ShouldDiscardOutdatedSuggestions) {
  FieldGlobalId old_field_id = test::MakeFieldGlobalId();
  FieldGlobalId new_field_id = test::MakeFieldGlobalId();
  client().set_last_queried_field(new_field_id);
  IssueOnQuery();
  EXPECT_CALL(client(), ShowAutofillSuggestions).Times(0);
  OnSuggestionsReturned(old_field_id, std::vector<Suggestion>());
}
#endif

// Tests logging with the new ranking algorithm experiment for Autofill
// suggestions enabled.
class AutofillExternalDelegateUnitTestWithNewSuggestionRankingAlgorithm
    : public AutofillExternalDelegateUnitTest {
 public:
  void SetUp() override {
    AutofillExternalDelegateUnitTest::SetUp();
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {features::kAutofillEnableRankingFormulaCreditCards,
         features::kAutofillEnableRankingFormulaAddressProfiles},
        /*disabled_features=*/{});
  }

  void SetUpRankingDifferenceAndSelectCreditCard(
      autofill_metrics::SuggestionRankingContext::RelativePosition
          relative_position) {
    // Set up SuggestionRankingContext.
    autofill_metrics::SuggestionRankingContext context;
    context.suggestion_rankings_difference_map = {
        {Suggestion::Guid(base::Uuid().AsLowercaseString()),
         relative_position}};

    // Simulate showing and selecting a credit card suggestion.
    external_delegate().OnSuggestionsReturned(
        queried_field().global_id(),
        {Suggestion(SuggestionType::kCreditCardEntry)}, context);
    external_delegate().DidAcceptSuggestion(
        Suggestion(SuggestionType::kCreditCardEntry),
        SuggestionPosition{.row = 0});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that the SuggestionRankingDifference metric is logged after a credit
// card suggestion is accepted.
TEST_F(AutofillExternalDelegateUnitTestWithNewSuggestionRankingAlgorithm,
       SuggestionAccepted_LogSuggestionRankingDifference_CreditCard) {
  base::HistogramTester histogram;
  IssueOnQuery();
  SetUpRankingDifferenceAndSelectCreditCard(
      autofill_metrics::SuggestionRankingContext::RelativePosition::
          kRankedLower);
  histogram.ExpectBucketCount(
      base::StrCat({"Autofill.SuggestionAccepted.SuggestionRankingDifference."
                    "CreditCard"}),
      autofill_metrics::SuggestionRankingContext::RelativePosition::
          kRankedLower,
      1);
}

// Test that the SuggestionRankingDifference metric is not logged after a credit
// card suggestion is accepted if the rankings are the same in both algorithms.
TEST_F(
    AutofillExternalDelegateUnitTestWithNewSuggestionRankingAlgorithm,
    SuggestionAccepted_LogSuggestionRankingDifference_NotLoggedWhenRankingsAreTheSame) {
  base::HistogramTester histogram;
  IssueOnQuery();
  histogram.ExpectBucketCount(
      base::StrCat({"Autofill.SuggestionAccepted.SuggestionRankingDifference."
                    "CreditCard"}),
      autofill_metrics::SuggestionRankingContext::RelativePosition::kRankedSame,
      0);
}

}  // namespace autofill
