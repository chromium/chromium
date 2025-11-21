// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/ui/autofill_external_delegate.h"

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
#include "base/time/time.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_trigger_source.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager_observer.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/autofill_ai/field_filling_entity_util.h"
#include "components/autofill/core/browser/filling/field_filling_util.h"
#include "components/autofill/core/browser/filling/form_filler.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager_test_api.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/foundations/test_autofill_driver.h"
#include "components/autofill/core/browser/foundations/test_browser_autofill_manager.h"
#include "components/autofill/core/browser/foundations/with_test_autofill_client_driver_manager.h"
#include "components/autofill/core/browser/integrators/autofill_ai/mock_autofill_ai_manager.h"
#include "components/autofill/core/browser/integrators/compose/autofill_compose_delegate.h"
#include "components/autofill/core/browser/integrators/compose/mock_autofill_compose_delegate.h"
#include "components/autofill/core/browser/integrators/identity_credential/mock_identity_credential_delegate.h"
#include "components/autofill/core/browser/integrators/one_time_tokens/mock_otp_manager.h"
#include "components/autofill/core/browser/integrators/plus_addresses/autofill_plus_address_delegate.h"
#include "components/autofill/core/browser/integrators/plus_addresses/mock_autofill_plus_address_delegate.h"
#include "components/autofill/core/browser/metrics/autofill_in_devtools_metrics.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/metrics/log_event.h"
#include "components/autofill/core/browser/metrics/payments/save_and_fill_metrics.h"
#include "components/autofill/core/browser/metrics/suggestions_list_metrics.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/mock_iban_access_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/test/mock_bnpl_manager.h"
#include "components/autofill/core/browser/payments/test/mock_save_and_fill_manager.h"
#include "components/autofill/core/browser/payments/test_payments_autofill_client.h"
#include "components/autofill/core/browser/single_field_fillers/mock_single_field_fill_router.h"
#include "components/autofill/core/browser/studies/autofill_experiments.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/suggestions/suggestion_test_helpers.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_form_test_utils.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/test_utils/valuables_data_test_utils.h"
#include "components/autofill/core/browser/ui/suggestion_button_action.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/autofill/core/common/aliases.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_data_test_api.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/autofill/core/common/plus_address_survey_type.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "url/origin.h"

namespace autofill {
namespace {

using ::base::test::RunOnceCallback;
using test::CreateTestAddressFormData;
using ::testing::_;
using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Eq;
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

using SuggestionPosition = AutofillSuggestionDelegate::SuggestionMetadata;

constexpr auto kDefaultTriggerSource =
    AutofillSuggestionTriggerSource::kFormControlElementClicked;

template <typename SuggestionsMatcher>
auto PopupOpenArgsAre(
    SuggestionsMatcher suggestions_matcher,
    AutofillSuggestionTriggerSource trigger_source = kDefaultTriggerSource) {
  using PopupOpenArgs = AutofillClient::PopupOpenArgs;
  return AllOf(Field(&PopupOpenArgs::suggestions, suggestions_matcher),
               Field(&PopupOpenArgs::trigger_source, trigger_source));
}

MATCHER_P(OtpPayloadPointeeEq, expected_otp_payload, "") {
  if (const auto* payload_ptr = std::get_if<const OtpFillData*>(&arg)) {
    return *payload_ptr && **payload_ptr == expected_otp_payload;
  }
  return false;
}

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
               (const base::flat_map<FieldGlobalId, FieldType>&),
               (const Section&)),
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
              (const AutofillClient::PopupOpenArgs&,
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
  MOCK_METHOD(void, ShowAutofillSettings, (SuggestionType), (override));
  MOCK_METHOD(AutofillComposeDelegate*, GetComposeDelegate, (), (override));
  MOCK_METHOD(void,
              TriggerPlusAddressUserPerceptionSurvey,
              (plus_addresses::hats::SurveyType),
              (override));
  MOCK_METHOD(IdentityCredentialDelegate*,
              GetIdentityCredentialDelegate,
              (),
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

class TestCreditCardAccessManager : public CreditCardAccessManager {
 public:
  using CreditCardAccessManager::CreditCardAccessManager;
  void PrepareToFetchCreditCard() override {
    // Do nothing for testing.
  }
};

class MockBrowserAutofillManager : public TestBrowserAutofillManager {
 public:
  explicit MockBrowserAutofillManager(AutofillDriver* driver)
      : TestBrowserAutofillManager(driver) {
    test_api(*this).set_credit_card_access_manager(
        std::make_unique<TestCreditCardAccessManager>(this));
    test_api(*this).set_bnpl_manager(
        std::make_unique<testing::NiceMock<MockBnplManager>>(this));
  }
  MockBrowserAutofillManager(const MockBrowserAutofillManager&) = delete;
  MockBrowserAutofillManager& operator=(const MockBrowserAutofillManager&) =
      delete;

  MOCK_METHOD(void,
              UndoAutofill,
              (mojom::ActionPersistence action_persistence,
               const FormData& form,
               const FormFieldData& trigger_field),
              (override));
  MOCK_METHOD(void,
              FillOrPreviewForm,
              (mojom::ActionPersistence,
               const FormData&,
               const FieldGlobalId&,
               const FillingPayload&,
               AutofillTriggerSource),
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
               const FormGlobalId&,
               const FieldGlobalId&,
               AutofillTriggerSource),
              (override));
  MOCK_METHOD(void,
              OnDidFillAddressOnTypingSuggestion,
              (const FieldGlobalId&,
               const std::u16string&,
               FieldType,
               const std::string&),
              (override));
};

class AutofillExternalDelegateTest : public testing::Test,
                                     public WithTestAutofillClientDriverManager<
                                         NiceMock<MockAutofillClient>,
                                         NiceMock<MockAutofillDriver>,
                                         NiceMock<MockBrowserAutofillManager>,
                                         MockPaymentsAutofillClient> {
 protected:
  void SetUp() override {
    InitAutofillClient();
    autofill_client().set_entity_data_manager(
        std::make_unique<EntityDataManager>(
            autofill_client().GetPrefs(),
            autofill_client().GetIdentityManager(),
            autofill_client().GetSyncService(),
            webdata_helper_.autofill_webdata_service(),
            /*history_service=*/nullptr,
            /*strike_database=*/nullptr));
    CreateAutofillDriver();
  }

  // Issue an OnQuery call.
  void IssueOnQuery(
      FormData form_data,
      const gfx::Rect& caret_bounds = gfx::Rect(),
      AutofillSuggestionTriggerSource trigger_source = kDefaultTriggerSource,
      bool update_datalist = false) {
    queried_form_ = std::move(form_data);
    autofill_manager().OnFormsSeen({queried_form()}, {});
    external_delegate().OnQuery(queried_form(), queried_field(), caret_bounds,
                                trigger_source, update_datalist);
  }

  void IssueOnQuery(test::FormDescription form_description) {
    queried_form_ = test::GetFormData(form_description);
    autofill_manager().AddSeenForm(queried_form(),
                                   test::GetHeuristicTypes(form_description),
                                   test::GetServerTypes(form_description));
    external_delegate().OnQuery(queried_form(), queried_field(), gfx::Rect(),
                                kDefaultTriggerSource,
                                /*update_datalist=*/false);
  }

  void IssueOnQuery(
      const gfx::Rect& caret_bounds,
      AutofillSuggestionTriggerSource trigger_source = kDefaultTriggerSource,
      FieldType trigger_field_type = NAME_FIRST,
      const std::string& autocomplete_attribute = "given-name") {
    FormGlobalId form_id = test::MakeFormGlobalId();
    FieldGlobalId field_id = test::MakeFieldGlobalId();
    IssueOnQuery(
        test::GetFormData({
            .fields = {{.role = trigger_field_type,
                        .host_frame = field_id.frame_token,
                        .renderer_id = field_id.renderer_id,
                        .autocomplete_attribute = autocomplete_attribute}},
            .host_frame = form_id.frame_token,
            .renderer_id = form_id.renderer_id,
        }),
        caret_bounds, trigger_source);
  }

  void IssueOnQuery(
      AutofillSuggestionTriggerSource trigger_source = kDefaultTriggerSource,
      FieldType trigger_field_type = NAME_FIRST,
      const std::string& autocomplete_attribute = "given-name") {
    IssueOnQuery(/*caret_bounds=*/gfx::Rect(), trigger_source,
                 trigger_field_type, autocomplete_attribute);
  }

  void IssueOnQuery(std::vector<SelectOption> datalist_options) {
    FormGlobalId form_id = test::MakeFormGlobalId();
    FieldGlobalId field_id = test::MakeFieldGlobalId();
    IssueOnQuery(
        test::GetFormData({
            .fields = {{.role = NAME_FIRST,
                        .host_frame = field_id.frame_token,
                        .renderer_id = field_id.renderer_id,
                        .autocomplete_attribute = "given-name",
                        .datalist_options = std::move(datalist_options)}},
            .host_frame = form_id.frame_token,
            .renderer_id = form_id.renderer_id,
        }),
        /*caret_bounds=*/gfx::Rect(), kDefaultTriggerSource,
        /*update_datalist=*/true);
  }

  // Returns the triggering `AutofillField`. This is the only field in the form
  // created in `IssueOnQuery()`.
  AutofillField* get_triggering_autofill_field() {
    return autofill_manager().GetAutofillField(
        queried_form().global_id(), queried_form().fields()[0].global_id());
  }

  Matcher<const FormData&> HasQueriedFormId() {
    return Property(&FormData::global_id, queried_form().global_id());
  }

  Matcher<const FormFieldData&> HasQueriedFieldId() {
    return Property(&FormFieldData::global_id, queried_field().global_id());
  }

  Matcher<const FieldGlobalId&> IsQueriedFieldId() {
    return Eq(queried_field().global_id());
  }

  AutofillExternalDelegate& external_delegate() {
    return *test_api(autofill_manager()).external_delegate();
  }
  PersonalDataManager& pdm() {
    return autofill_client().GetPersonalDataManager();
  }

  const FormData& queried_form() {
    CHECK(!queried_form_.fields().empty());
    return queried_form_;
  }

  const FormFieldData& queried_field() {
    return queried_form().fields().front();
  }

  void OnSuggestionsReturned(FieldGlobalId field_id,
                             const std::vector<Suggestion>& input_suggestions) {
    external_delegate().OnSuggestionsReturned(field_id, input_suggestions);
  }

  AutofillWebDataServiceTestHelper& webdata_helper() { return webdata_helper_; }

 private:
  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;

  AutofillWebDataServiceTestHelper webdata_helper_{
      std::make_unique<EntityTable>()};

  // Form containing the triggering field that initialized the external delegate
  // `OnQuery`.
  FormData queried_form_;
};

TEST_F(AutofillExternalDelegateTest, GetMainFillingProduct) {
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

  // Show BNPL suggestion in the popup.
  OnSuggestionsReturned(queried_field().global_id(),
                        {test::CreateAutofillSuggestion(
                            SuggestionType::kBnplEntry, u"BNPL suggestion")});
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

  // Show only datalist suggestion in the popup.
  OnSuggestionsReturned(
      queried_field().global_id(),
      {test::CreateAutofillSuggestion(SuggestionType::kDatalistEntry,
                                      u"datalist")});
  EXPECT_EQ(external_delegate().GetMainFillingProduct(),
            FillingProduct::kDataList);

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

  // Show save and fill suggestion in the popup.
  OnSuggestionsReturned(queried_field().global_id(),
                        {test::CreateAutofillSuggestion(
                            SuggestionType::kSaveAndFillCreditCardEntry,
                            u"save and fill suggestion")});
  EXPECT_EQ(external_delegate().GetMainFillingProduct(),
            FillingProduct::kCreditCard);
}

// Test that our external delegate called the virtual methods at the right time.
TEST_F(AutofillExternalDelegateTest, TestExternalDelegateVirtualCalls) {
  IssueOnQuery();

  const auto kExpectedSuggestions =
      SuggestionVectorIdsAre(SuggestionType::kAddressEntry);
  EXPECT_CALL(
      autofill_client(),
      ShowAutofillSuggestions(PopupOpenArgsAre(kExpectedSuggestions), _));

  // This should call ShowAutofillSuggestions.
  const AutofillProfile profile = test::GetFullProfile();
  pdm().address_data_manager().AddProfile(profile);
  std::vector<Suggestion> autofill_item = {
      Suggestion(SuggestionType::kAddressEntry)};
  autofill_item[0].payload =
      Suggestion::AutofillProfilePayload(Suggestion::Guid(profile.guid()));
  OnSuggestionsReturned(queried_field().global_id(), autofill_item);

  EXPECT_CALL(autofill_manager(),
              FillOrPreviewForm(mojom::ActionPersistence::kFill,
                                HasQueriedFormId(), IsQueriedFieldId(), _, _));
  EXPECT_CALL(
      autofill_client(),
      HideAutofillSuggestions(SuggestionHidingReason::kAcceptSuggestion));

  // This should trigger a call to hide the popup since we've selected an
  // option.
  external_delegate().DidAcceptSuggestion(autofill_item[0],
                                          SuggestionPosition{.row = 0});
}

// Test that data list elements for a node will appear in the Autofill popup.
TEST_F(AutofillExternalDelegateTest, ExternalDelegateDataList) {
  std::vector<SelectOption> data_list_items;
  data_list_items.emplace_back();

  EXPECT_CALL(autofill_client(), UpdateAutofillDataListValues(SizeIs(1)));
  IssueOnQuery(data_list_items);

  // This should call ShowAutofillSuggestions.
  const auto kExpectedSuggestions =
      SuggestionVectorIdsAre(SuggestionType::kDatalistEntry,
#if !BUILDFLAG(IS_ANDROID)
                             SuggestionType::kSeparator,
#endif
                             SuggestionType::kAddressEntry);
  EXPECT_CALL(
      autofill_client(),
      ShowAutofillSuggestions(PopupOpenArgsAre(kExpectedSuggestions), _));
  std::vector<Suggestion> autofill_item;
  autofill_item.emplace_back(/*main_text=*/u"", SuggestionType::kAddressEntry);
  OnSuggestionsReturned(queried_field().global_id(), autofill_item);

  // Try calling OnSuggestionsReturned with no Autofill values and ensure
  // the datalist items are still shown.
  EXPECT_CALL(autofill_client(),
              ShowAutofillSuggestions(PopupOpenArgsAre(SuggestionVectorIdsAre(
                                          SuggestionType::kDatalistEntry)),
                                      _));
  autofill_item.clear();
  OnSuggestionsReturned(queried_field().global_id(), autofill_item);
}

// Test that datalist values can get updated while a popup is showing.
TEST_F(AutofillExternalDelegateTest, UpdateDataListWhileShowingPopup) {
  EXPECT_CALL(autofill_client(), ShowAutofillSuggestions).Times(0);

  // Make sure just setting the data list values doesn't cause the popup to
  // appear.
  std::vector<SelectOption> data_list_items;
  data_list_items.emplace_back();

  EXPECT_CALL(autofill_client(), UpdateAutofillDataListValues(SizeIs(1)));
  IssueOnQuery(data_list_items);

  // Ensure the popup is displayed.
  const auto kExpectedSuggestions =
      SuggestionVectorIdsAre(SuggestionType::kDatalistEntry,
#if !BUILDFLAG(IS_ANDROID)
                             SuggestionType::kSeparator,
#endif
                             SuggestionType::kAddressEntry);
  EXPECT_CALL(
      autofill_client(),
      ShowAutofillSuggestions(PopupOpenArgsAre(kExpectedSuggestions), _));
  std::vector<Suggestion> autofill_item;
  autofill_item.emplace_back(SuggestionType::kAddressEntry);
  OnSuggestionsReturned(queried_field().global_id(), autofill_item);

  // This would normally get called from ShowAutofillSuggestions, but it is
  // mocked so we need to call OnSuggestionsShown ourselves.
  external_delegate().OnSuggestionsShown(autofill_item);

  // Update the current data list and ensure the popup is updated.
  data_list_items.emplace_back();

  EXPECT_CALL(autofill_client(), UpdateAutofillDataListValues(SizeIs(2)));
  IssueOnQuery(data_list_items);
}

// Test that we _don't_ de-dupe autofill values against datalist values. We
// keep both with a separator.
TEST_F(AutofillExternalDelegateTest, DuplicateAutofillDatalistValues) {
  std::vector<SelectOption> datalist{{.value = u"Rick", .text = u"Deckard"},
                                     {.value = u"Beyonce", .text = u"Knowles"}};
  EXPECT_CALL(autofill_client(),
              UpdateAutofillDataListValues(
                  ElementsAre(AllOf(Field(&SelectOption::value, u"Rick"),
                                    Field(&SelectOption::text, u"Deckard")),
                              AllOf(Field(&SelectOption::value, u"Beyonce"),
                                    Field(&SelectOption::text, u"Knowles")))));
  IssueOnQuery(datalist);

  const auto kExpectedSuggestions = SuggestionVectorIdsAre(
      SuggestionType::kDatalistEntry, SuggestionType::kDatalistEntry,
#if !BUILDFLAG(IS_ANDROID)
      SuggestionType::kSeparator,
#endif
      SuggestionType::kAddressEntry);
  EXPECT_CALL(
      autofill_client(),
      ShowAutofillSuggestions(PopupOpenArgsAre(kExpectedSuggestions), _));

  // Have an Autofill item that is identical to one of the datalist entries.
  std::vector<Suggestion> autofill_item;
  autofill_item.emplace_back(SuggestionType::kAddressEntry);
  autofill_item[0].main_text =
      Suggestion::Text(u"Rick", Suggestion::Text::IsPrimary(true));
  autofill_item[0].labels = {{Suggestion::Text(u"Deckard")}};
  OnSuggestionsReturned(queried_field().global_id(), autofill_item);
}

// Test that we de-dupe autocomplete values against datalist values, keeping the
// latter in case of a match.
TEST_F(AutofillExternalDelegateTest, DuplicateAutocompleteDatalistValues) {
  std::vector<SelectOption> datalist{{.value = u"Rick", .text = u"Deckard"},
                                     {.value = u"Beyonce", .text = u"Knowles"}};
  EXPECT_CALL(autofill_client(),
              UpdateAutofillDataListValues(
                  ElementsAre(AllOf(Field(&SelectOption::value, u"Rick"),
                                    Field(&SelectOption::text, u"Deckard")),
                              AllOf(Field(&SelectOption::value, u"Beyonce"),
                                    Field(&SelectOption::text, u"Knowles")))));
  IssueOnQuery(datalist);

  const auto kExpectedSuggestions = SuggestionVectorIdsAre(
      // We are expecting only two data list entries.
      SuggestionType::kDatalistEntry, SuggestionType::kDatalistEntry,
#if !BUILDFLAG(IS_ANDROID)
      SuggestionType::kSeparator,
#endif
      SuggestionType::kAutocompleteEntry);
  EXPECT_CALL(
      autofill_client(),
      ShowAutofillSuggestions(PopupOpenArgsAre(kExpectedSuggestions), _));

  // Have an Autocomplete item that is identical to one of the datalist entries
  // and one that is distinct.
  std::vector<Suggestion> autocomplete_items;
  autocomplete_items.emplace_back(SuggestionType::kAutocompleteEntry);
  autocomplete_items[0].main_text =
      Suggestion::Text(u"Rick", Suggestion::Text::IsPrimary(true));
  autocomplete_items.emplace_back(SuggestionType::kAutocompleteEntry);
  autocomplete_items[1].main_text =
      Suggestion::Text(u"Cain", Suggestion::Text::IsPrimary(true));
  OnSuggestionsReturned(queried_field().global_id(), autocomplete_items);
}

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
// Test that `BnplManager::OnSuggestionsShown` will not be called if the
// suggestion list doesn't contain a credit card entry.
TEST_F(AutofillExternalDelegateTest,
       BnplSuggestionsNotShownWithoutCreditCardEntry) {
  EXPECT_CALL(*autofill_manager().GetPaymentsBnplManager(), OnSuggestionsShown)
      .Times(0);

  const std::vector<Suggestion> suggestions = {
      test::CreateAutofillSuggestion(SuggestionType::kAddressEntry),
      test::CreateAutofillSuggestion(SuggestionType::kSeparator),
      test::CreateAutofillSuggestion(SuggestionType::kManageCreditCard)};

  external_delegate().OnSuggestionsShown(suggestions);
}

// Test that `BnplManager::OnSuggestionsShown` will be called if the
// suggestion list contains a credit card entry.
TEST_F(AutofillExternalDelegateTest, BnplSuggestionsShownWithCreditCardEntry) {
  EXPECT_CALL(*autofill_manager().GetPaymentsBnplManager(), OnSuggestionsShown)
      .Times(1);

  const std::vector<Suggestion> suggestions = {
      test::CreateAutofillSuggestion(SuggestionType::kCreditCardEntry),
      test::CreateAutofillSuggestion(SuggestionType::kSeparator),
      test::CreateAutofillSuggestion(SuggestionType::kManageCreditCard)};

  external_delegate().OnSuggestionsShown(suggestions);
}

// Tests that the Autofill delegate fills a form with a VCN when a suggestion
// containing a BNPL entry is accepted, and the user completes the flow.
TEST_F(AutofillExternalDelegateTest, AcceptedBnplEntry_FormIsFilled) {
  IssueOnQuery();
  CreditCard card = test::GetVirtualCard();
  card.set_issuer_id(kBnplAffirmIssuerId);

  const std::optional<int64_t> expected_amount = 50'000'000;

  EXPECT_CALL(*autofill_manager().GetPaymentsBnplManager(),
              OnDidAcceptBnplSuggestion(expected_amount, _))
      .WillOnce(RunOnceCallback<1>(card));
  EXPECT_CALL(
      autofill_manager(),
      FillOrPreviewForm(mojom::ActionPersistence::kFill, HasQueriedFormId(),
                        IsQueriedFieldId(), _, AutofillTriggerSource::kPopup));

  Suggestion::PaymentsPayload payments_payload;
  payments_payload.extracted_amount_in_micros = expected_amount;
  external_delegate().DidAcceptSuggestion(
      test::CreateAutofillSuggestion(SuggestionType::kBnplEntry,
                                     /*main_text_value=*/u"BNPL suggestion",
                                     payments_payload),
      {});
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

// Test that the Autofill popup is able to display warnings explaining why
// Autofill is disabled for a website.
// Regression test for http://crbug.com/247880
TEST_F(AutofillExternalDelegateTest, AutofillWarnings) {
  IssueOnQuery();

  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client(), ShowAutofillSuggestions)
      .WillOnce(DoAll(SaveArg<0>(&open_args),
                      Return(AutofillClient::SuggestionUiSessionId())));

  // This should call ShowAutofillSuggestions.
  std::vector<Suggestion> autofill_item;
  autofill_item.emplace_back(
      SuggestionType::kInsecureContextPaymentDisabledMessage);
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
TEST_F(AutofillExternalDelegateTest, AutofillWarningsNotShown_WithSuggestions) {
  IssueOnQuery();

  EXPECT_CALL(autofill_client(),
              ShowAutofillSuggestions(PopupOpenArgsAre(SuggestionVectorIdsAre(
                                          SuggestionType::kAutocompleteEntry)),
                                      _));
  std::vector<Suggestion> suggestions;
  suggestions.emplace_back(
      SuggestionType::kInsecureContextPaymentDisabledMessage);
  suggestions.emplace_back(SuggestionType::kAutocompleteEntry);
  suggestions[1].main_text =
      Suggestion::Text(u"Rick", Suggestion::Text::IsPrimary(true));
  OnSuggestionsReturned(queried_field().global_id(), suggestions);
}

// Test that the Autofill delegate doesn't try and fill a form with a
// negative unique id.
TEST_F(AutofillExternalDelegateTest, ExternalDelegateInvalidUniqueId) {
  IssueOnQuery();
  // Ensure it doesn't try to preview the negative id.
  EXPECT_CALL(autofill_manager(), FillOrPreviewForm).Times(0);
  EXPECT_CALL(autofill_driver(), RendererShouldClearPreviewedForm);
  const Suggestion suggestion{
      SuggestionType::kInsecureContextPaymentDisabledMessage};
  external_delegate().DidSelectSuggestion(suggestion);

  // Ensure it doesn't try to fill the form in with the negative id.
  EXPECT_CALL(
      autofill_client(),
      HideAutofillSuggestions(SuggestionHidingReason::kAcceptSuggestion));
  EXPECT_CALL(autofill_manager(), FillOrPreviewForm).Times(0);

  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 0});
}

// Test that the Autofill delegate still allows previewing and filling
// specifically of the negative ID for SuggestionType::kIbanEntry.
TEST_F(AutofillExternalDelegateTest, ExternalDelegateFillsIbanEntry) {
  IssueOnQuery();

  EXPECT_CALL(
      autofill_client(),
      ShowAutofillSuggestions(
          PopupOpenArgsAre(SuggestionVectorIdsAre(SuggestionType::kIbanEntry)),
          _));
  std::vector<Suggestion> suggestions;
  Iban iban = test::GetLocalIban();
  suggestions.emplace_back(/*main_text=*/u"My doctor's IBAN",
                           SuggestionType::kIbanEntry);
  suggestions[0].labels = {
      {Suggestion::Text(iban.GetIdentifierStringForAutofillDisplay())}};
  suggestions[0].payload = Suggestion::Guid(iban.guid());
  OnSuggestionsReturned(queried_field().global_id(), suggestions);

  EXPECT_CALL(autofill_driver(), RendererShouldClearPreviewedForm());
  EXPECT_CALL(autofill_manager(),
              FillOrPreviewField(mojom::ActionPersistence::kPreview,
                                 mojom::FieldActionType::kReplaceAll,
                                 HasQueriedFormId(), HasQueriedFieldId(),
                                 iban.GetIdentifierStringForAutofillDisplay(),
                                 SuggestionType::kIbanEntry,
                                 std::optional(IBAN_VALUE)));
  external_delegate().DidSelectSuggestion(suggestions[0]);
  EXPECT_CALL(
      autofill_client(),
      HideAutofillSuggestions(SuggestionHidingReason::kAcceptSuggestion));
  EXPECT_CALL(autofill_manager(),
              FillOrPreviewField(mojom::ActionPersistence::kFill,
                                 mojom::FieldActionType::kReplaceAll,
                                 HasQueriedFormId(), HasQueriedFieldId(),
                                 iban.value(), SuggestionType::kIbanEntry,
                                 std::optional(IBAN_VALUE)));
  Suggestion suggestion(u"My doctor's IBAN", SuggestionType::kIbanEntry);
  suggestion.payload = Suggestion::Guid(iban.guid());
  suggestion.labels = {
      {Suggestion::Text(iban.GetIdentifierStringForAutofillDisplay())}};
  EXPECT_CALL(*payments_autofill_client().GetIbanManager(),
              OnSingleFieldSuggestionSelected(suggestion));
  ON_CALL(*payments_autofill_client().GetIbanAccessManager(), FetchValue)
      .WillByDefault([iban](const Suggestion::Payload& payload,
                            IbanAccessManager::OnIbanFetchedCallback callback) {
        std::move(callback).Run(iban.value());
      });

  external_delegate().DidAcceptSuggestion(suggestions[0],
                                          SuggestionPosition{.row = 0});
}

// Test that the Autofill delegate still allows previewing and filling
// specifically of the negative ID for
// SuggestionType::kMerchantPromoCodeEntry.
TEST_F(AutofillExternalDelegateTest,
       ExternalDelegateFillsMerchantPromoCodeEntry) {
  IssueOnQuery();

  EXPECT_CALL(
      autofill_client(),
      ShowAutofillSuggestions(PopupOpenArgsAre(SuggestionVectorIdsAre(
                                  SuggestionType::kMerchantPromoCodeEntry)),
                              _));
  std::vector<Suggestion> suggestions;
  const std::u16string promo_code_value = u"PROMOCODE1234";
  suggestions.emplace_back(/*main_text=*/promo_code_value,
                           SuggestionType::kMerchantPromoCodeEntry);
  suggestions[0].main_text.value = promo_code_value;
  suggestions[0].labels = {{Suggestion::Text(u"12.34% off your purchase!")}};
  OnSuggestionsReturned(queried_field().global_id(), suggestions);

  EXPECT_CALL(autofill_driver(), RendererShouldClearPreviewedForm());
  EXPECT_CALL(autofill_manager(),
              FillOrPreviewField(mojom::ActionPersistence::kPreview,
                                 mojom::FieldActionType::kReplaceAll,
                                 HasQueriedFormId(), HasQueriedFieldId(),
                                 promo_code_value,
                                 SuggestionType::kMerchantPromoCodeEntry,
                                 std::optional(MERCHANT_PROMO_CODE)));
  external_delegate().DidSelectSuggestion(suggestions[0]);
  EXPECT_CALL(
      autofill_client(),
      HideAutofillSuggestions(SuggestionHidingReason::kAcceptSuggestion));
  EXPECT_CALL(autofill_manager(),
              FillOrPreviewField(mojom::ActionPersistence::kFill,
                                 mojom::FieldActionType::kReplaceAll,
                                 HasQueriedFormId(), HasQueriedFieldId(),
                                 promo_code_value,
                                 SuggestionType::kMerchantPromoCodeEntry,
                                 std::optional(MERCHANT_PROMO_CODE)));

  external_delegate().DidAcceptSuggestion(suggestions[0],
                                          SuggestionPosition{.row = 0});
}

// Test that the Autofill delegate allows previewing `kLoyaltyCardEntry`
// suggestions.
TEST_F(AutofillExternalDelegateTest, ExternalDelegatePreviewsLoyaltyCardEntry) {
  IssueOnQuery();

  EXPECT_CALL(autofill_client(),
              ShowAutofillSuggestions(PopupOpenArgsAre(SuggestionVectorIdsAre(
                                          SuggestionType::kLoyaltyCardEntry)),
                                      _));
  std::vector<Suggestion> suggestions;
  const std::u16string loyalty_card_value = u"LOYALTYCARD1234";
  suggestions.emplace_back(/*main_text=*/loyalty_card_value,
                           SuggestionType::kLoyaltyCardEntry);
  suggestions[0].main_text.value = loyalty_card_value;
  OnSuggestionsReturned(queried_field().global_id(), suggestions);

  EXPECT_CALL(autofill_driver(), RendererShouldClearPreviewedForm());
  EXPECT_CALL(
      autofill_manager(),
      FillOrPreviewField(mojom::ActionPersistence::kPreview,
                         mojom::FieldActionType::kReplaceAll,
                         HasQueriedFormId(), HasQueriedFieldId(),
                         loyalty_card_value, SuggestionType::kLoyaltyCardEntry,
                         std::optional(LOYALTY_MEMBERSHIP_ID)));
  external_delegate().DidSelectSuggestion(suggestions[0]);
}

// Test that the Autofill delegate allows filling `kLoyaltyCardEntry`
// suggestions.
TEST_F(AutofillExternalDelegateTest, ExternalDelegateFillsLoyaltyCardEntry) {
  IssueOnQuery();

  EXPECT_CALL(autofill_client(),
              ShowAutofillSuggestions(PopupOpenArgsAre(SuggestionVectorIdsAre(
                                          SuggestionType::kLoyaltyCardEntry)),
                                      _));
  LoyaltyCard loyalty_card = test::CreateLoyaltyCard();
  std::vector<Suggestion> suggestions;
  const std::u16string full_loyalty_card_value = u"LOYALTYCARD1234";
  suggestions.emplace_back(/*main_text=*/full_loyalty_card_value,
                           SuggestionType::kLoyaltyCardEntry);
  suggestions[0].main_text.value = full_loyalty_card_value;
  suggestions[0].payload = Suggestion::Guid(loyalty_card.id().value());
  OnSuggestionsReturned(queried_field().global_id(), suggestions);

  EXPECT_CALL(autofill_driver(), RendererShouldClearPreviewedForm());
  EXPECT_CALL(autofill_manager(),
              FillOrPreviewField(mojom::ActionPersistence::kPreview,
                                 mojom::FieldActionType::kReplaceAll,
                                 HasQueriedFormId(), HasQueriedFieldId(),
                                 full_loyalty_card_value,
                                 SuggestionType::kLoyaltyCardEntry,
                                 std::optional(LOYALTY_MEMBERSHIP_ID)));
  external_delegate().DidSelectSuggestion(suggestions[0]);

  EXPECT_CALL(
      autofill_client(),
      HideAutofillSuggestions(SuggestionHidingReason::kAcceptSuggestion));
  EXPECT_CALL(autofill_manager(),
              FillOrPreviewField(mojom::ActionPersistence::kFill,
                                 mojom::FieldActionType::kReplaceAll,
                                 HasQueriedFormId(), HasQueriedFieldId(),
                                 full_loyalty_card_value,
                                 SuggestionType::kLoyaltyCardEntry,
                                 std::optional(LOYALTY_MEMBERSHIP_ID)));

  external_delegate().DidAcceptSuggestion(suggestions[0],
                                          SuggestionPosition{.row = 0});
}

TEST_F(AutofillExternalDelegateTest, AcceptManageLoyaltyCards) {
  Suggestion manage_suggestion =
      Suggestion(u"Manage cards", SuggestionType::kManageLoyaltyCard);
  EXPECT_CALL(autofill_client(),
              ShowAutofillSettings(SuggestionType::kManageLoyaltyCard));
  external_delegate().DidAcceptSuggestion(manage_suggestion, {});
}

// Test that the Autofill delegate routes the merchant promo code suggestions
// footer redirect logic correctly.
TEST_F(AutofillExternalDelegateTest,
       ExternalDelegateMerchantPromoCodeSuggestionsFooter) {
  IssueOnQuery();
  const GURL gurl{"https://example.com/"};
  EXPECT_CALL(payments_autofill_client(), OpenPromoCodeOfferDetailsURL(gurl));

  external_delegate().DidAcceptSuggestion(
      test::CreateAutofillSuggestion(SuggestionType::kSeePromoCodeDetails,
                                     u"baz foo", gurl),
      SuggestionPosition{.row = 0});
}

// Test that the ClearPreview call is only sent if the form was being previewed
// (i.e. it isn't autofilling a password).
TEST_F(AutofillExternalDelegateTest, ExternalDelegateClearPreviewedForm) {
  // Ensure selecting a new password entries or Autofill entries will
  // cause any previews to get cleared.
  IssueOnQuery();
  EXPECT_CALL(autofill_driver(), RendererShouldClearPreviewedForm());
  external_delegate().DidSelectSuggestion(test::CreateAutofillSuggestion(
      SuggestionType::kAddressEntry, u"baz foo"));
  EXPECT_CALL(autofill_driver(), RendererShouldClearPreviewedForm());
  EXPECT_CALL(autofill_manager(),
              FillOrPreviewForm(mojom::ActionPersistence::kPreview,
                                HasQueriedFormId(), IsQueriedFieldId(), _, _));
  const AutofillProfile profile = test::GetFullProfile();
  pdm().address_data_manager().AddProfile(profile);
  external_delegate().DidSelectSuggestion(test::CreateAutofillSuggestion(
      SuggestionType::kAddressEntry, u"baz foo",
      Suggestion::AutofillProfilePayload(Suggestion::Guid(profile.guid()))));

  // Ensure selecting an autocomplete entry will cause any previews to
  // get cleared.
  EXPECT_CALL(autofill_driver(), RendererShouldClearPreviewedForm());
  EXPECT_CALL(autofill_manager(),
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
  EXPECT_CALL(autofill_driver(), RendererShouldClearPreviewedForm());
  EXPECT_CALL(autofill_manager(),
              FillOrPreviewForm(mojom::ActionPersistence::kPreview,
                                HasQueriedFormId(), IsQueriedFieldId(), _, _));
  Suggestion suggestion(SuggestionType::kVirtualCreditCardEntry);
  suggestion.payload = Suggestion::Guid(card.guid());
  external_delegate().DidSelectSuggestion(suggestion);
}

// Test that the popup is hidden once we are done editing the autofill field.
TEST_F(AutofillExternalDelegateTest, ExternalDelegateHidePopupAfterEditing) {
  EXPECT_CALL(autofill_client(), ShowAutofillSuggestions);
  test::GenerateTestAutofillPopup(&external_delegate());

  EXPECT_CALL(autofill_client(),
              HideAutofillSuggestions(SuggestionHidingReason::kEndEditing));
  external_delegate().DidEndTextFieldEditing();
}

// Test that the driver is directed to accept the data list after being notified
// that the user accepted the data list suggestion.
TEST_F(AutofillExternalDelegateTest, ExternalDelegateAcceptDatalistSuggestion) {
  IssueOnQuery();
  EXPECT_CALL(
      autofill_client(),
      HideAutofillSuggestions(SuggestionHidingReason::kAcceptSuggestion));
  std::u16string dummy_string(u"baz qux");
  EXPECT_CALL(autofill_driver(),
              RendererShouldAcceptDataListSuggestion(
                  queried_field().global_id(), dummy_string));

  external_delegate().DidAcceptSuggestion(
      test::CreateAutofillSuggestion(SuggestionType::kDatalistEntry,
                                     dummy_string),
      SuggestionPosition{.row = 0});
}

// Test that a11y autofill availability is set to `kAutofillAvailable` when
// the popup is open with regular autofill suggestions.
TEST_F(AutofillExternalDelegateTest, AutofillSuggestionAvailability_Autofill) {
  IssueOnQuery();

  std::vector<Suggestion> suggestions = {
      Suggestion(u"Suggestion main_text", SuggestionType::kAddressEntry)};
  OnSuggestionsReturned(queried_field().global_id(), suggestions);

  EXPECT_CALL(autofill_driver(),
              RendererShouldSetSuggestionAvailability(
                  queried_field().global_id(),
                  mojom::AutofillSuggestionAvailability::kAutofillAvailable));

  external_delegate().OnSuggestionsShown(suggestions);
}

// Test that a11y autofill availability is set to `kAutofillAvailable` when
// the popup is open with the `kFillAutofillAi` suggestion.
TEST_F(AutofillExternalDelegateTest,
       AutofillSuggestionAvailabilityFillAutofillAi) {
  IssueOnQuery();

  std::vector<Suggestion> suggestions = {
      Suggestion(u"Autofill with AI", SuggestionType::kFillAutofillAi)};
  OnSuggestionsReturned(queried_field().global_id(), suggestions);

  EXPECT_CALL(autofill_driver(),
              RendererShouldSetSuggestionAvailability(
                  queried_field().global_id(),
                  mojom::AutofillSuggestionAvailability::kAutofillAvailable));

  external_delegate().OnSuggestionsShown(suggestions);
}

// Test that a11y autofill availability is set to `kAutocompleteAvailable` when
// the popup is open with autocomplete suggestions.
TEST_F(AutofillExternalDelegateTest,
       AutofillSuggestionAvailability_Autocomplete) {
  IssueOnQuery();

  std::vector<Suggestion> suggestions = {
      Suggestion(u"Suggestion main_text", SuggestionType::kAutocompleteEntry)};
  OnSuggestionsReturned(queried_field().global_id(), suggestions);

  EXPECT_CALL(
      autofill_driver(),
      RendererShouldSetSuggestionAvailability(
          queried_field().global_id(),
          mojom::AutofillSuggestionAvailability::kAutocompleteAvailable));

  external_delegate().OnSuggestionsShown(suggestions);
}

// Test that an accepted autofill suggestion will fill the form.
TEST_F(AutofillExternalDelegateTest, AcceptSuggestion) {
  IssueOnQuery();
  EXPECT_CALL(
      autofill_client(),
      HideAutofillSuggestions(SuggestionHidingReason::kAcceptSuggestion));
  EXPECT_CALL(autofill_manager(),
              FillOrPreviewForm(mojom::ActionPersistence::kFill,
                                HasQueriedFormId(), IsQueriedFieldId(), _, _));

  const AutofillProfile profile = test::GetFullProfile();
  pdm().address_data_manager().AddProfile(profile);
  external_delegate().DidAcceptSuggestion(
      test::CreateAutofillSuggestion(
          SuggestionType::kAddressEntry, u"John Legend",
          Suggestion::AutofillProfilePayload(Suggestion::Guid(profile.guid()))),
      SuggestionPosition{.row = 2});
}

TEST_F(AutofillExternalDelegateTest,
       TestAddressSuggestionShown_MetricsEmitted) {
  base::HistogramTester histogram_tester;
  autofill_client().set_test_addresses({test::GetFullProfile()});
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
TEST_F(AutofillExternalDelegateTest, TestAddressSuggestion_FillAndPreview) {
  IssueOnQuery();
  const AutofillProfile profile = test::GetFullProfile();
  autofill_client().set_test_addresses({profile});
  const Suggestion suggestion = test::CreateAutofillSuggestion(
      SuggestionType::kDevtoolsTestAddressEntry, u"John Legend",
      Suggestion::AutofillProfilePayload(Suggestion::Guid(profile.guid())));
  base::HistogramTester histogram_tester;

  // Test preview.
  EXPECT_CALL(autofill_manager(),
              FillOrPreviewForm(mojom::ActionPersistence::kPreview,
                                HasQueriedFormId(), IsQueriedFieldId(), _, _));
  external_delegate().DidSelectSuggestion(suggestion);

  // Test fill.
  EXPECT_CALL(autofill_manager(),
              FillOrPreviewForm(mojom::ActionPersistence::kFill,
                                HasQueriedFormId(), IsQueriedFieldId(), _, _));
  EXPECT_CALL(
      autofill_client(),
      HideAutofillSuggestions(SuggestionHidingReason::kAcceptSuggestion));
  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 0});
  histogram_tester.ExpectUniqueSample(
      "Autofill.TestAddressesEvent",
      autofill_metrics::AutofillInDevtoolsTestAddressesEvents::
          kTestAddressesSuggestionSelected,
      1);
}

// Test that an accepted test verified email autofill suggestion will fill the
// form and that the delegate gets notified.
TEST_F(AutofillExternalDelegateTest, TestVerifiedEmailSuggestion_Preview) {
  IssueOnQuery();
  const Suggestion suggestion = test::CreateAutofillSuggestion(
      SuggestionType::kIdentityCredential, u"John Legend",
      Suggestion::IdentityCredentialPayload());

  // Test preview.
  EXPECT_CALL(autofill_manager(),
              FillOrPreviewForm(mojom::ActionPersistence::kPreview,
                                HasQueriedFormId(), IsQueriedFieldId(), _, _));
  external_delegate().DidSelectSuggestion(suggestion);
}

// Test that an accepted test verified email autofill suggestion will fill the
// form and that the delegate gets notified.
TEST_F(AutofillExternalDelegateTest, TestVerifiedEmailSuggestion_Fill) {
  IssueOnQuery();
  const Suggestion suggestion = test::CreateAutofillSuggestion(
      SuggestionType::kIdentityCredential, u"John Legend",
      Suggestion::IdentityCredentialPayload());

  // Set up a mock identity credential delegate.
  MockIdentityCredentialDelegate mock;
  ON_CALL(autofill_client(), GetIdentityCredentialDelegate)
      .WillByDefault(Return(&mock));

  // Expect that the form filler gets notified.
  EXPECT_CALL(autofill_manager(),
              FillOrPreviewForm(mojom::ActionPersistence::kFill,
                                HasQueriedFormId(), IsQueriedFieldId(), _, _));
  // Expect that the delegate gets notified and pretend that the user has
  // accepted the prompt.
  EXPECT_CALL(mock, NotifySuggestionAccepted(_, /*show_modal=*/true, _))
      .WillOnce(RunOnceCallback<2>(/*accepted=*/true));

  // Test fill.
  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 0});
}

// Test that an accepted verified email autofill suggestion will not fill the
// form if the user later rejects the prompt.
TEST_F(AutofillExternalDelegateTest,
       TestVerifiedEmailSuggestion_PromptRejectedNoFill) {
  IssueOnQuery();
  const Suggestion suggestion = test::CreateAutofillSuggestion(
      SuggestionType::kIdentityCredential, u"John Legend",
      Suggestion::IdentityCredentialPayload());

  // Set up a mock identity credential delegate.
  MockIdentityCredentialDelegate mock;
  ON_CALL(autofill_client(), GetIdentityCredentialDelegate)
      .WillByDefault(Return(&mock));

  // Expect that the delegate gets notified and pretend that the user has
  // rejected the prompt.
  EXPECT_CALL(mock, NotifySuggestionAccepted(_, /*show_modal=*/true, _))
      .WillOnce(RunOnceCallback<2>(/*accepted=*/false));

  // Test fill.
  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 0});
}

TEST_F(AutofillExternalDelegateTest,
       AcceptFirstPopupLevelSuggestion_LogSuggestionAcceptedMetric) {
  IssueOnQuery();
  const AutofillProfile profile = test::GetFullProfile();
  pdm().address_data_manager().AddProfile(profile);
  const int suggestion_accepted_row = 2;
  base::HistogramTester histogram_tester;

  external_delegate().DidAcceptSuggestion(
      test::CreateAutofillSuggestion(
          SuggestionType::kAddressEntry, u"John Legend",
          Suggestion::AutofillProfilePayload(Suggestion::Guid(profile.guid()))),
      AutofillSuggestionDelegate::SuggestionMetadata{
          .row = suggestion_accepted_row});

  histogram_tester.ExpectUniqueSample("Autofill.SuggestionAcceptedIndex",
                                      suggestion_accepted_row, 1);
}

// Tests that when accepting a suggestion, the `AutofillSuggestionTriggerSource`
// is converted to the correct `AutofillTriggerSource`.
TEST_F(AutofillExternalDelegateTest, AcceptSuggestion_TriggerSource) {
  // Expect that `kFormControlElementClicked` translates to source `kPopup` or
  // `kKeyboardAccessory`, depending on the platform.
  const AutofillProfile profile = test::GetFullProfile();
  pdm().address_data_manager().AddProfile(profile);
  Suggestion suggestion = test::CreateAutofillSuggestion(
      SuggestionType::kAddressEntry, /*main_text_value=*/u"",
      Suggestion::AutofillProfilePayload(Suggestion::Guid(profile.guid())));

  IssueOnQuery(AutofillSuggestionTriggerSource::kFormControlElementClicked);
  auto expected_source =
#if BUILDFLAG(IS_ANDROID)
      AutofillTriggerSource::kKeyboardAccessory;
#else
      AutofillTriggerSource::kPopup;
#endif
  EXPECT_CALL(
      autofill_manager(),
      FillOrPreviewForm(mojom::ActionPersistence::kFill, HasQueriedFormId(),
                        IsQueriedFieldId(), _, expected_source));
  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 1});

  // Expect that `kManualFallbackPlusAddresses` translates to the manual
  // fallback trigger source.
  IssueOnQuery(AutofillSuggestionTriggerSource::kManualFallbackPlusAddresses);
  expected_source = AutofillTriggerSource::kManualFallback;
  EXPECT_CALL(
      autofill_manager(),
      FillOrPreviewForm(mojom::ActionPersistence::kFill, HasQueriedFormId(),
                        IsQueriedFieldId(), _, expected_source));
  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 1});
}

// Tests that on selecting and accepting a `kFillAutofillAi` suggestion with
// `Suggestion::AutofillAiPayload` payload previews and fills the form,
// respectively.
TEST_F(AutofillExternalDelegateTest, FillAutofillAiFillsFullForm) {
  base::test::ScopedFeatureList scoped_feature_list{
      features::kAutofillAiWithDataSchema};

  EntityInstance passport = test::GetPassportEntityInstance({.number = u"123"});
  autofill_client().GetEntityDataManager()->AddOrUpdateEntityInstance(passport);
  webdata_helper().WaitUntilIdle();
  IssueOnQuery({.fields = {{.role = NAME_FIRST},
                           {.role = NAME_LAST},
                           {.role = PASSPORT_NUMBER},
                           {.role = IBAN_VALUE},
                           {.role = UNKNOWN_TYPE}}});

  Suggestion fill_suggestion(SuggestionType::kFillAutofillAi);
  fill_suggestion.payload = Suggestion::AutofillAiPayload(passport.guid());

  EXPECT_CALL(autofill_manager(),
              FillOrPreviewForm(mojom::ActionPersistence::kPreview,
                                HasQueriedFormId(), IsQueriedFieldId(), _,
                                AutofillTriggerSource::kAutofillAi));
  external_delegate().DidSelectSuggestion(fill_suggestion);

  EXPECT_CALL(autofill_manager(),
              FillOrPreviewForm(mojom::ActionPersistence::kFill,
                                HasQueriedFormId(), IsQueriedFieldId(), _,
                                AutofillTriggerSource::kAutofillAi));
  external_delegate().DidAcceptSuggestion(fill_suggestion, {});
}

TEST_F(AutofillExternalDelegateTest, AcceptManageAutofillAi) {
  Suggestion manage_suggestion =
      Suggestion(u"Manage information", SuggestionType::kManageAutofillAi);
  EXPECT_CALL(autofill_client(),
              ShowAutofillSettings(SuggestionType::kManageAutofillAi));
  external_delegate().DidAcceptSuggestion(manage_suggestion, {});
}

TEST_F(AutofillExternalDelegateTest, AcceptedOtpSuggestion) {
  IssueOnQuery();

  std::u16string otp_value = u"123456";
  OtpFillData otp_fill_data;
  otp_fill_data[queried_field().global_id()] = otp_value;

  // Expect that suggestion trigger source translates to source `kPopup` or
  // `kKeyboardAccessory`, depending on the platform.
  auto expected_source =
#if BUILDFLAG(IS_ANDROID)
      AutofillTriggerSource::kKeyboardAccessory;
#else
      AutofillTriggerSource::kPopup;
#endif
  EXPECT_CALL(
      autofill_manager(),
      FillOrPreviewForm(mojom::ActionPersistence::kFill, HasQueriedFormId(),
                        IsQueriedFieldId(), OtpPayloadPointeeEq(otp_fill_data),
                        expected_source));
  external_delegate().DidAcceptSuggestion(
      test::CreateAutofillSuggestion(SuggestionType::kOneTimePasswordEntry,
                                     /*main_text_value=*/otp_value),
      {});
}

class AutofillExternalDelegatePlusAddressTest
    : public AutofillExternalDelegateTest {
 public:
  AutofillExternalDelegatePlusAddressTest() = default;

  void SetUp() override {
    AutofillExternalDelegateTest::SetUp();
    autofill_client().set_plus_address_delegate(
        std::make_unique<NiceMock<MockAutofillPlusAddressDelegate>>());
  }

 protected:
  MockAutofillPlusAddressDelegate& plus_address_delegate() {
    return static_cast<MockAutofillPlusAddressDelegate&>(
        *autofill_client().GetPlusAddressDelegate());
  }

  const std::vector<Suggestion>& suggestions() const { return suggestions_; }

 private:
  // The currently shown suggestions. Kept as a member since
  // `GetAutofillSuggestions` returns a span.
  std::vector<Suggestion> suggestions_;
};

// Mock out an existing plus address autofill suggestion, and ensure that
// choosing it results in the field being filled with its value (as opposed to
// the mocked address used in the creation flow).
TEST_F(AutofillExternalDelegatePlusAddressTest,
       ExternalDelegateFillsExistingPlusAddress) {
  // Trigger the popup on an email field.
  IssueOnQuery(kDefaultTriggerSource, EMAIL_ADDRESS, "email");

  base::HistogramTester histogram_tester;

  EXPECT_CALL(
      autofill_client(),
      ShowAutofillSuggestions(PopupOpenArgsAre(SuggestionVectorIdsAre(
                                  SuggestionType::kFillExistingPlusAddress)),
                              _));
  const std::u16string plus_address = u"test+plus@test.example";
  std::vector<Suggestion> suggestions;
  suggestions.emplace_back(/*main_text=*/plus_address,
                           SuggestionType::kFillExistingPlusAddress);
  OnSuggestionsReturned(queried_field().global_id(), suggestions);

  EXPECT_CALL(autofill_driver(), RendererShouldClearPreviewedForm());
  EXPECT_CALL(
      autofill_manager(),
      FillOrPreviewField(mojom::ActionPersistence::kPreview,
                         mojom::FieldActionType::kReplaceAll,
                         HasQueriedFormId(), HasQueriedFieldId(), plus_address,
                         SuggestionType::kFillExistingPlusAddress,
                         std::optional(EMAIL_ADDRESS)));
  external_delegate().DidSelectSuggestion(suggestions[0]);
  EXPECT_CALL(
      autofill_client(),
      HideAutofillSuggestions(SuggestionHidingReason::kAcceptSuggestion));
  EXPECT_CALL(plus_address_delegate(),
              RecordAutofillSuggestionEvent(
                  MockAutofillPlusAddressDelegate::SuggestionEvent::
                      kExistingPlusAddressChosen));
  EXPECT_CALL(plus_address_delegate(), DidFillPlusAddress);
  EXPECT_CALL(
      autofill_client(),
      TriggerPlusAddressUserPerceptionSurvey(
          plus_addresses::hats::SurveyType::kDidChoosePlusAddressOverEmail));
  EXPECT_CALL(
      autofill_manager(),
      FillOrPreviewField(mojom::ActionPersistence::kFill,
                         mojom::FieldActionType::kReplaceAll,
                         HasQueriedFormId(), HasQueriedFieldId(), plus_address,
                         SuggestionType::kFillExistingPlusAddress,
                         std::optional(EMAIL_ADDRESS)));
  external_delegate().DidAcceptSuggestion(suggestions[0],
                                          SuggestionPosition{.row = 0});
}

// Tests the scenario when the user chooses an email suggestion over the plus
// address suggestion.
TEST_F(AutofillExternalDelegatePlusAddressTest,
       EmailSuggestionIsFilledWhenPlusAddressIsSuggested) {
  // Trigger the popup on an email field.
  IssueOnQuery(kDefaultTriggerSource, EMAIL_ADDRESS, "email");

  base::HistogramTester histogram_tester;

  EXPECT_CALL(
      autofill_client(),
      ShowAutofillSuggestions(PopupOpenArgsAre(SuggestionVectorIdsAre(
                                  SuggestionType::kAddressEntry,
                                  SuggestionType::kFillExistingPlusAddress)),
                              _));
  const AutofillProfile profile = test::GetFullProfile();
  pdm().address_data_manager().AddProfile(profile);
  const std::u16string email = u"example@gmail.com";
  std::vector<Suggestion> suggestions;
  suggestions.emplace_back(/*main_text=*/email, SuggestionType::kAddressEntry);
  suggestions[0].payload =
      Suggestion::AutofillProfilePayload(Suggestion::Guid(profile.guid()));
  suggestions.emplace_back(/*main_text=*/u"test+plus@test.example",
                           SuggestionType::kFillExistingPlusAddress);
  OnSuggestionsReturned(queried_field().global_id(), suggestions);

  EXPECT_CALL(autofill_driver(), RendererShouldClearPreviewedForm());
  EXPECT_CALL(autofill_manager(),
              FillOrPreviewForm(mojom::ActionPersistence::kPreview,
                                HasQueriedFormId(), IsQueriedFieldId(), _, _));
  external_delegate().DidSelectSuggestion(suggestions[0]);
  EXPECT_CALL(
      autofill_client(),
      HideAutofillSuggestions(SuggestionHidingReason::kAcceptSuggestion));
  EXPECT_CALL(plus_address_delegate(),
              RecordAutofillSuggestionEvent(
                  MockAutofillPlusAddressDelegate::SuggestionEvent::
                      kExistingPlusAddressChosen))
      .Times(0);
  EXPECT_CALL(
      autofill_client(),
      TriggerPlusAddressUserPerceptionSurvey(
          plus_addresses::hats::SurveyType::kDidChooseEmailOverPlusAddress));
  EXPECT_CALL(autofill_manager(),
              FillOrPreviewForm(mojom::ActionPersistence::kFill,
                                HasQueriedFormId(), IsQueriedFieldId(), _, _));
  external_delegate().DidAcceptSuggestion(suggestions[0],
                                          SuggestionPosition{.row = 0});
}

// Tests the scenario when the user triggers plus address suggestions manually
// from the context menu and no email suggestions are shown.
TEST_F(AutofillExternalDelegatePlusAddressTest,
       AcceptsManuallyTriggeredPlusAddressFillingSuggestion) {
  // Trigger the popup on an email field.
  IssueOnQuery(AutofillSuggestionTriggerSource::kManualFallbackPlusAddresses);

  base::HistogramTester histogram_tester;

  EXPECT_CALL(
      autofill_client(),
      ShowAutofillSuggestions(
          PopupOpenArgsAre(
              SuggestionVectorIdsAre(SuggestionType::kFillExistingPlusAddress),
              AutofillSuggestionTriggerSource::kManualFallbackPlusAddresses),
          _));
  const std::u16string plus_address = u"test+plus@test.example";
  std::vector<Suggestion> suggestions;
  suggestions.emplace_back(/*main_text=*/plus_address,
                           SuggestionType::kFillExistingPlusAddress);
  OnSuggestionsReturned(queried_field().global_id(), suggestions);

  EXPECT_CALL(autofill_driver(), RendererShouldClearPreviewedForm());
  EXPECT_CALL(
      autofill_manager(),
      FillOrPreviewField(mojom::ActionPersistence::kPreview,
                         mojom::FieldActionType::kReplaceAll,
                         HasQueriedFormId(), HasQueriedFieldId(), plus_address,
                         SuggestionType::kFillExistingPlusAddress,
                         std::optional(EMAIL_ADDRESS)));
  external_delegate().DidSelectSuggestion(suggestions[0]);
  EXPECT_CALL(
      autofill_client(),
      HideAutofillSuggestions(SuggestionHidingReason::kAcceptSuggestion));
  EXPECT_CALL(plus_address_delegate(),
              RecordAutofillSuggestionEvent(
                  MockAutofillPlusAddressDelegate::SuggestionEvent::
                      kExistingPlusAddressChosen));
  EXPECT_CALL(plus_address_delegate(), DidFillPlusAddress);
  EXPECT_CALL(autofill_client(), TriggerPlusAddressUserPerceptionSurvey(
                                     plus_addresses::hats::SurveyType::
                                         kFilledPlusAddressViaManualFallack));
  EXPECT_CALL(
      autofill_manager(),
      FillOrPreviewField(mojom::ActionPersistence::kFill,
                         mojom::FieldActionType::kReplaceAll,
                         HasQueriedFormId(), HasQueriedFieldId(), plus_address,
                         SuggestionType::kFillExistingPlusAddress,
                         std::optional(EMAIL_ADDRESS)));
  external_delegate().DidAcceptSuggestion(suggestions[0],
                                          SuggestionPosition{.row = 0});
}

// Tests that displaying an address suggestion that contains a plus address
// email override records the corresponding user action.
TEST_F(AutofillExternalDelegatePlusAddressTest,
       PlusAddressEmailOverrideUserAction) {
  IssueOnQuery();
  base::UserActionTester user_action_tester;
  Suggestion suggestion(SuggestionType::kAddressEntry);
  suggestion.payload = Suggestion::AutofillProfilePayload(
      Suggestion::Guid("123"), u"test_override");

  std::vector<Suggestion> suggestions = {suggestion};
  OnSuggestionsReturned(queried_field().global_id(), suggestions);

  external_delegate().OnSuggestionsShown(suggestions);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "PlusAddresses.AddressFillSuggestionShown"),
            1);
}

TEST_F(AutofillExternalDelegateTest,
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

  EXPECT_CALL(autofill_client(),
              ShowAutofillSuggestions(
                  AllOf(Field(&AutofillClient::PopupOpenArgs::element_bounds,
                              gfx::RectF(caret_bounds)),
                        Field(&AutofillClient::PopupOpenArgs::anchor_type,
                              PopupAnchorType::kCaret)),
                  _));
  MockAutofillComposeDelegate compose_delegate;
  ON_CALL(autofill_client(), GetComposeDelegate)
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
    AutofillExternalDelegateTest,
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
      autofill_client(),
      ShowAutofillSuggestions(
          Field(&AutofillClient::PopupOpenArgs::element_bounds, field_bounds),
          _));
  MockAutofillComposeDelegate compose_delegate;
  ON_CALL(autofill_client(), GetComposeDelegate)
      .WillByDefault(Return(&compose_delegate));
  ON_CALL(compose_delegate, ShouldAnchorNudgeOnCaret)
      .WillByDefault(Return(false));

  // This should call ShowAutofillSuggestions.
  OnSuggestionsReturned(queried_field().global_id(),
                        {Suggestion(SuggestionType::kComposeProactiveNudge)});
}

TEST_F(
    AutofillExternalDelegateTest,
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
      autofill_client(),
      ShowAutofillSuggestions(
          Field(&AutofillClient::PopupOpenArgs::element_bounds, field_bounds),
          _));
  MockAutofillComposeDelegate compose_delegate;
  ON_CALL(autofill_client(), GetComposeDelegate)
      .WillByDefault(Return(&compose_delegate));
  ON_CALL(compose_delegate, ShouldAnchorNudgeOnCaret)
      .WillByDefault(Return(true));

  // This should call ShowAutofillSuggestions.
  OnSuggestionsReturned(queried_field().global_id(),
                        {Suggestion(SuggestionType::kComposeProactiveNudge)});
}

TEST_F(
    AutofillExternalDelegateTest,
    NonComposeSuggestion_NonComposeProactiveNudge_DoNotForwardsCaretBoundsToClient) {
  IssueOnQuery(gfx::Rect(/*width=*/123, /*height=*/123));

  const PopupAnchorType default_anchor_type =
#if BUILDFLAG(IS_ANDROID)
      PopupAnchorType::kKeyboardAccessory;
#else
      PopupAnchorType::kField;
#endif
  EXPECT_CALL(autofill_client(),
              ShowAutofillSuggestions(
                  AllOf(Field(&AutofillClient::PopupOpenArgs::element_bounds,
                              gfx::RectF(/*width=*/0, /*height=*/0)),
                        Field(&AutofillClient::PopupOpenArgs::anchor_type,
                              default_anchor_type)),
                  _));
  MockAutofillComposeDelegate compose_delegate;
  ON_CALL(autofill_client(), GetComposeDelegate)
      .WillByDefault(Return(&compose_delegate));
  ON_CALL(compose_delegate, ShouldAnchorNudgeOnCaret)
      .WillByDefault(Return(true));

  // This should call ShowAutofillSuggestions.
  OnSuggestionsReturned(queried_field().global_id(),
                        {Suggestion(SuggestionType::kAutocompleteEntry)});
}

// Tests that accepting a Compose suggestion returns a callback that, when run,
// fills the trigger field.
TEST_F(AutofillExternalDelegateTest, ExternalDelegateOpensComposeAndFills) {
  MockAutofillComposeDelegate compose_delegate;
  ON_CALL(autofill_client(), GetComposeDelegate)
      .WillByDefault(Return(&compose_delegate));

  IssueOnQuery();

  // Simulate receiving a Compose suggestion.
  EXPECT_CALL(autofill_client(),
              ShowAutofillSuggestions(PopupOpenArgsAre(SuggestionVectorIdsAre(
                                          SuggestionType::kComposeResumeNudge)),
                                      _));
  std::vector<Suggestion> suggestions = {
      Suggestion(SuggestionType::kComposeResumeNudge)};
  OnSuggestionsReturned(queried_field().global_id(), suggestions);

  // Simulate accepting a Compose suggestion.
  EXPECT_CALL(
      compose_delegate,
      OpenCompose(_, queried_field().global_id(),
                  AutofillComposeDelegate::UiEntryPoint::kAutofillPopup));
  EXPECT_CALL(
      autofill_client(),
      HideAutofillSuggestions(SuggestionHidingReason::kAcceptSuggestion));
  external_delegate().DidAcceptSuggestion(suggestions[0],
                                          SuggestionPosition{.row = 0});
}

TEST_F(AutofillExternalDelegateTest,
       Compose_AcceptDisable_CallsComposeDelegate) {
  MockAutofillComposeDelegate compose_delegate;
  ON_CALL(autofill_client(), GetComposeDelegate)
      .WillByDefault(Return(&compose_delegate));

  IssueOnQuery();

  // Simulate accepting a Compose `SuggestionType::kComposeDisable`
  // suggestion.
  EXPECT_CALL(compose_delegate, DisableCompose);
  external_delegate().DidAcceptSuggestion(
      Suggestion(SuggestionType::kComposeDisable),
      SuggestionPosition{.row = 0});
}

TEST_F(AutofillExternalDelegateTest,
       Compose_AcceptGoToSettings_CallsComposeDelegate) {
  MockAutofillComposeDelegate compose_delegate;
  ON_CALL(autofill_client(), GetComposeDelegate)
      .WillByDefault(Return(&compose_delegate));

  IssueOnQuery();

  // Simulate accepting a Compose `SuggestionType::kComposeGoToSettings`
  // suggestion.
  EXPECT_CALL(compose_delegate, GoToSettings);
  external_delegate().DidAcceptSuggestion(
      Suggestion(SuggestionType::kComposeGoToSettings),
      SuggestionPosition{.row = 0});
}

TEST_F(AutofillExternalDelegateTest,
       AcceptSaveAndFillCreditCardSuggestion_CallsSaveAndFillManager) {
  IssueOnQuery();

  EXPECT_CALL(*payments_autofill_client().GetSaveAndFillManager(),
              OnDidAcceptCreditCardSaveAndFillSuggestion(_));
  external_delegate().DidAcceptSuggestion(
      test::CreateAutofillSuggestion(
          SuggestionType::kSaveAndFillCreditCardEntry),
      SuggestionPosition{.row = 0});
}

TEST_F(AutofillExternalDelegateTest, AcceptedSaveAndFillEntry_FillForm) {
  IssueOnQuery();
  CreditCard card = test::GetCreditCard();

  EXPECT_CALL(*payments_autofill_client().GetSaveAndFillManager(),
              OnDidAcceptCreditCardSaveAndFillSuggestion)
      .WillOnce([&](MockSaveAndFillManager::FillCardCallback callback) {
        std::move(callback).Run(card);
      });
  EXPECT_CALL(autofill_manager(),
              FillOrPreviewForm(mojom::ActionPersistence::kFill,
                                HasQueriedFormId(), IsQueriedFieldId(), _,
                                AutofillTriggerSource::kCreditCardSaveAndFill));

  external_delegate().DidAcceptSuggestion(
      test::CreateAutofillSuggestion(
          SuggestionType::kSaveAndFillCreditCardEntry),
      SuggestionPosition{.row = 0});
}

TEST_F(AutofillExternalDelegateTest, SaveAndFillMetrics_SuggestionAccepted) {
  base::HistogramTester histogram;
  IssueOnQuery();

  external_delegate().DidAcceptSuggestion(
      test::CreateAutofillSuggestion(
          SuggestionType::kSaveAndFillCreditCardEntry),
      SuggestionPosition{.row = 0});

  histogram.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard.SaveAndFill",
      autofill_metrics::SaveAndFillFormEvent::kSuggestionAccepted, 1);
}

TEST_F(AutofillExternalDelegateTest,
       Compose_AcceptNeverShowOnThisWebsiteAgain_CallsComposeDelegate) {
  MockAutofillComposeDelegate compose_delegate;
  ON_CALL(autofill_client(), GetComposeDelegate)
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
TEST_F(AutofillExternalDelegateTest, ExternalDelegateUndoForm) {
  IssueOnQuery();
  EXPECT_CALL(autofill_manager(), UndoAutofill);
  external_delegate().DidAcceptSuggestion(
      Suggestion(SuggestionType::kUndoOrClear), SuggestionPosition{.row = 0});
}

// Test that the driver is directed to undo the form after being notified that
// the user selected the suggestion to undo the form.
TEST_F(AutofillExternalDelegateTest, ExternalDelegateUndoPreviewForm) {
  IssueOnQuery();
  EXPECT_CALL(autofill_manager(), UndoAutofill);
  external_delegate().DidSelectSuggestion(
      Suggestion(SuggestionType::kUndoOrClear));
}
#endif

// Test that autofill client will scan a credit card after use accepted the
// suggestion to scan a credit card.
TEST_F(AutofillExternalDelegateTest, ScanCreditCardMenuItem) {
  IssueOnQuery();
  EXPECT_CALL(payments_autofill_client(), ScanCreditCard);
  EXPECT_CALL(
      autofill_client(),
      HideAutofillSuggestions(SuggestionHidingReason::kAcceptSuggestion));

  external_delegate().DidAcceptSuggestion(
      Suggestion(SuggestionType::kScanCreditCard),
      SuggestionPosition{.row = 0});
}

TEST_F(AutofillExternalDelegateTest, ScanCreditCardMetrics_SuggestionShown) {
  base::HistogramTester histogram;
  IssueOnQuery();
  std::vector<Suggestion> suggestions = {
      Suggestion(SuggestionType::kScanCreditCard)};
  OnSuggestionsReturned(queried_field().global_id(), suggestions);
  external_delegate().OnSuggestionsShown(suggestions);

  histogram.ExpectUniqueSample("Autofill.ScanCreditCardPrompt",
                               AutofillMetrics::SCAN_CARD_ITEM_SHOWN, 1);
}

TEST_F(AutofillExternalDelegateTest, ScanCreditCardMetrics_SuggestionAccepted) {
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

TEST_F(AutofillExternalDelegateTest,
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

TEST_F(AutofillExternalDelegateTest, ScanCreditCardMetrics_SuggestionNotShown) {
  base::HistogramTester histogram;
  IssueOnQuery();
  OnSuggestionsReturned(queried_field().global_id(), {});
  external_delegate().OnSuggestionsShown({});
  histogram.ExpectTotalCount("Autofill.ScanCreditCardPrompt", 0);
}

TEST_F(AutofillExternalDelegateTest, AutocompleteShown_MetricsEmitted) {
  base::HistogramTester histogram;
  IssueOnQuery();
  std::vector<Suggestion> suggestions = {test::CreateAutofillSuggestion(
      SuggestionType::kAutocompleteEntry, u"autocomplete")};
  OnSuggestionsReturned(queried_field().global_id(), suggestions);
  external_delegate().OnSuggestionsShown(suggestions);
  histogram.ExpectBucketCount("Autocomplete.Events3",
                              AutofillMetrics::AUTOCOMPLETE_SUGGESTIONS_SHOWN,
                              1);
}

TEST_F(AutofillExternalDelegateTest, ScanCreditCard_FillForm) {
  CreditCard card = test::GetCreditCard();
  EXPECT_CALL(payments_autofill_client(), ScanCreditCard)
      .WillOnce(
          [&](MockPaymentsAutofillClient::CreditCardScanCallback callback) {
            std::move(callback).Run(card);
          });
  EXPECT_CALL(autofill_manager(),
              FillOrPreviewForm(mojom::ActionPersistence::kFill, _, _, _, _));
  external_delegate().DidAcceptSuggestion(
      test::CreateAutofillSuggestion(SuggestionType::kScanCreditCard), {});
}

TEST_F(AutofillExternalDelegateTest, IgnoreAutocompleteOffForAutofill) {
  const FormData form;
  FormFieldData field;
  field.set_is_focusable(true);
  field.set_should_autocomplete(false);

  external_delegate().OnQuery(form, field, /*caret_bounds=*/gfx::Rect(),
                              kDefaultTriggerSource, /*update_datalist=*/false);

  std::vector<Suggestion> autofill_items;
  autofill_items.emplace_back(SuggestionType::kAutocompleteEntry);

  // Ensure the popup tries to show itself, despite autocomplete="off".
  EXPECT_CALL(autofill_client(), ShowAutofillSuggestions);
  EXPECT_CALL(autofill_client(), HideAutofillSuggestions(_)).Times(0);

  OnSuggestionsReturned(field.global_id(), autofill_items);
}

TEST_F(AutofillExternalDelegateTest,
       ExternalDelegateFillFieldWithValue_Autocomplete) {
  EXPECT_CALL(
      autofill_client(),
      HideAutofillSuggestions(SuggestionHidingReason::kAcceptSuggestion));
  IssueOnQuery();

  base::HistogramTester histogram_tester;
  std::u16string dummy_autocomplete_string(u"autocomplete");
  Suggestion suggestion(SuggestionType::kAutocompleteEntry);
  suggestion.main_text.value = dummy_autocomplete_string;
  EXPECT_CALL(
      autofill_manager(),
      FillOrPreviewField(
          mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
          HasQueriedFormId(), HasQueriedFieldId(), dummy_autocomplete_string,
          SuggestionType::kAutocompleteEntry, std::optional<FieldType>()));
  MockAutocompleteHistoryManager* autocomplete_history_manager =
      static_cast<MockAutocompleteHistoryManager*>(
          autofill_client().GetAutocompleteHistoryManager());
  EXPECT_CALL(*autocomplete_history_manager,
              OnSingleFieldSuggestionSelected(suggestion));

  external_delegate().DidAcceptSuggestion(
      test::CreateAutofillSuggestion(SuggestionType::kAutocompleteEntry,
                                     dummy_autocomplete_string),
      SuggestionPosition{.row = 0});

  histogram_tester.ExpectUniqueSample(
      "Autofill.SuggestionAcceptedIndex.Autocomplete", 0, 1);
}

TEST_F(AutofillExternalDelegateTest,
       ExternalDelegateFillFieldWithValue_AutofillAddressOnTyping) {
  EXPECT_CALL(
      autofill_client(),
      HideAutofillSuggestions(SuggestionHidingReason::kAcceptSuggestion));
  const AutofillProfile profile = test::GetFullProfile();
  pdm().address_data_manager().AddProfile(profile);
  IssueOnQuery();

  std::u16string dummy_autofill_on_typing_string(u"Jon doe");
  Suggestion suggestion = test::CreateAutofillSuggestion(
      SuggestionType::kAddressEntryOnTyping, dummy_autofill_on_typing_string,
      Suggestion::AutofillProfilePayload(Suggestion::Guid(profile.guid())));
  suggestion.field_by_field_filling_type_used = NAME_FULL;
  // Simulate that the user has typed the first 3 characters of their full name.
  get_triggering_autofill_field()->set_value(
      dummy_autofill_on_typing_string.substr(0, 3));
  base::HistogramTester histogram_tester;

  EXPECT_CALL(
      autofill_manager(),
      FillOrPreviewField(
          mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
          HasQueriedFormId(), HasQueriedFieldId(),
          profile.GetRawInfo(*suggestion.field_by_field_filling_type_used),
          SuggestionType::kAddressEntryOnTyping, std::optional(NAME_FULL)));
  EXPECT_CALL(autofill_manager(), OnDidFillAddressFormFillingSuggestion)
      .Times(0);
  EXPECT_CALL(
      autofill_manager(),
      OnDidFillAddressOnTypingSuggestion(
          IsQueriedFieldId(),
          profile.GetRawInfo(*suggestion.field_by_field_filling_type_used),
          NAME_FULL, profile.guid()));

  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 0});

  histogram_tester.ExpectUniqueSample("Autofill.Suggestions.AcceptedType",
                                      SuggestionType::kAddressEntryOnTyping, 1);
  histogram_tester.ExpectUniqueSample(
      "Autofill.AddressSuggestionOnTyping.AddressFieldTypeUsed", NAME_FULL, 1);
  // Note that the triggeting field is classified.
  histogram_tester.ExpectUniqueSample(
      "Autofill.AddressSuggestionOnTypingAcceptance.FieldClassication", true,
      1);
  // Above it was simulated that the user typed 3 characters in the field.
  histogram_tester.ExpectBucketCount(
      "Autofill.AddressSuggestionOnTypingAcceptance.NumberOfCharactersTyped", 3,
      1);
}

TEST_F(AutofillExternalDelegateTest,
       ExternalDelegateFillFieldWithValue_MerchantPromoCode) {
  EXPECT_CALL(
      autofill_client(),
      HideAutofillSuggestions(SuggestionHidingReason::kAcceptSuggestion));
  IssueOnQuery();

  std::u16string dummy_promo_code_string(u"merchant promo");
  Suggestion suggestion(SuggestionType::kMerchantPromoCodeEntry);
  suggestion.main_text.value = dummy_promo_code_string;
  EXPECT_CALL(autofill_manager(),
              FillOrPreviewField(mojom::ActionPersistence::kFill,
                                 mojom::FieldActionType::kReplaceAll,
                                 HasQueriedFormId(), HasQueriedFieldId(),
                                 dummy_promo_code_string,
                                 SuggestionType::kMerchantPromoCodeEntry,
                                 std::optional(MERCHANT_PROMO_CODE)));
  EXPECT_CALL(*payments_autofill_client().GetMerchantPromoCodeManager(),
              OnSingleFieldSuggestionSelected(suggestion));

  external_delegate().DidAcceptSuggestion(
      test::CreateAutofillSuggestion(SuggestionType::kMerchantPromoCodeEntry,
                                     dummy_promo_code_string),
      SuggestionPosition{.row = 0});
}

TEST_F(AutofillExternalDelegateTest, ExternalDelegateFillFieldWithValue_Iban) {
  EXPECT_CALL(
      autofill_client(),
      HideAutofillSuggestions(SuggestionHidingReason::kAcceptSuggestion));
  IssueOnQuery();

  Iban iban = test::GetLocalIban();
  Suggestion suggestion(SuggestionType::kIbanEntry);
  suggestion.main_text.value = iban.GetIdentifierStringForAutofillDisplay();
  suggestion.payload = Suggestion::Guid(iban.guid());
  EXPECT_CALL(autofill_manager(),
              FillOrPreviewField(mojom::ActionPersistence::kFill,
                                 mojom::FieldActionType::kReplaceAll,
                                 HasQueriedFormId(), HasQueriedFieldId(),
                                 iban.value(), SuggestionType::kIbanEntry,
                                 std::optional(IBAN_VALUE)));
  EXPECT_CALL(*payments_autofill_client().GetIbanManager(),
              OnSingleFieldSuggestionSelected(suggestion));

  ON_CALL(*payments_autofill_client().GetIbanAccessManager(), FetchValue)
      .WillByDefault([iban](const Suggestion::Payload& payload,
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

TEST_F(AutofillExternalDelegateTest,
       ExternalDelegateFillFieldWithValue_FieldByFieldFilling) {
  const AutofillProfile profile = test::GetFullProfile();
  pdm().address_data_manager().AddProfile(profile);
  IssueOnQuery();
  Suggestion suggestion = test::CreateAutofillSuggestion(
      SuggestionType::kAddressFieldByFieldFilling, u"field by field",
      Suggestion::AutofillProfilePayload(Suggestion::Guid(profile.guid())));
  suggestion.field_by_field_filling_type_used = NAME_FIRST;
  EXPECT_CALL(
      autofill_client(),
      HideAutofillSuggestions(SuggestionHidingReason::kAcceptSuggestion));
  EXPECT_CALL(
      autofill_manager(),
      FillOrPreviewField(
          mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
          HasQueriedFormId(), HasQueriedFieldId(),
          profile.GetRawInfo(*suggestion.field_by_field_filling_type_used),
          SuggestionType::kAddressFieldByFieldFilling,
          std::optional(NAME_FIRST)));
  EXPECT_CALL(autofill_manager(),
              OnDidFillAddressFormFillingSuggestion(
                  Property(&AutofillProfile::guid, profile.guid()),
                  queried_form().global_id(), IsQueriedFieldId(), _));

  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 0});
}

// Test that browser autofill manager will handle the unmasking request for the
// virtual card after users accept the suggestion to use a virtual card.
TEST_F(AutofillExternalDelegateTest, AcceptVirtualCardOptionItem) {
  IssueOnQuery();
  FormData form;
  CreditCard card = test::GetMaskedServerCard();
  pdm().payments_data_manager().AddCreditCard(card);
  EXPECT_CALL(autofill_manager(),
              FillOrPreviewForm(mojom::ActionPersistence::kFill,
                                HasQueriedFormId(), IsQueriedFieldId(), _, _));
  Suggestion suggestion(SuggestionType::kVirtualCreditCardEntry);
  suggestion.payload = Suggestion::Guid(card.guid());
  external_delegate().DidAcceptSuggestion(suggestion,
                                          SuggestionPosition{.row = 0});
}

TEST_F(AutofillExternalDelegateTest, SelectVirtualCardOptionItem) {
  IssueOnQuery();
  CreditCard card = test::GetMaskedServerCard();
  pdm().payments_data_manager().AddCreditCard(card);
  EXPECT_CALL(autofill_manager(),
              FillOrPreviewForm(mojom::ActionPersistence::kPreview,
                                HasQueriedFormId(), IsQueriedFieldId(), _, _));
  Suggestion suggestion(SuggestionType::kVirtualCreditCardEntry);
  suggestion.payload = Suggestion::Guid(card.guid());
  external_delegate().DidSelectSuggestion(suggestion);
}

TEST_F(AutofillExternalDelegateTest, RemoveSuggestion_Autocomplete) {
  auto mock_single_field_fill_router =
      std::make_unique<MockSingleFieldFillRouter>(
          autofill_client().GetAutocompleteHistoryManager(), nullptr, nullptr);
  EXPECT_CALL(*mock_single_field_fill_router,
              OnRemoveCurrentSingleFieldSuggestion);
  autofill_client().set_single_field_fill_router(
      std::move(mock_single_field_fill_router));
  EXPECT_TRUE(external_delegate().RemoveSuggestion(
      Suggestion(u"autocomplete", SuggestionType::kAutocompleteEntry)));
}

TEST_F(AutofillExternalDelegateTest, RemoveSuggestion_Address) {
  const AutofillProfile profile = test::GetFullProfile();
  pdm().address_data_manager().AddProfile(profile);
  ASSERT_TRUE(pdm().address_data_manager().GetProfileByGUID(profile.guid()));
  EXPECT_TRUE(external_delegate().RemoveSuggestion(
      test::CreateAutofillSuggestion(SuggestionType::kAddressEntry, u"address",
                                     Suggestion::AutofillProfilePayload(
                                         Suggestion::Guid(profile.guid())))));
  EXPECT_FALSE(pdm().address_data_manager().GetProfileByGUID(profile.guid()));
}

TEST_F(AutofillExternalDelegateTest, RemoveSuggestion_AddressFieldByField) {
  const AutofillProfile profile = test::GetFullProfile();
  pdm().address_data_manager().AddProfile(profile);
  ASSERT_TRUE(pdm().address_data_manager().GetProfileByGUID(profile.guid()));
  EXPECT_TRUE(
      external_delegate().RemoveSuggestion(test::CreateAutofillSuggestion(
          SuggestionType::kAddressFieldByFieldFilling, u"address",
          Suggestion::AutofillProfilePayload(
              Suggestion::Guid(profile.guid())))));
  EXPECT_FALSE(pdm().address_data_manager().GetProfileByGUID(profile.guid()));
}

TEST_F(AutofillExternalDelegateTest, RemoveSuggestion_LocalCard) {
  const CreditCard local_card = test::GetCreditCard();
  pdm().payments_data_manager().AddCreditCard(local_card);
  ASSERT_TRUE(
      pdm().payments_data_manager().GetCreditCardByGUID(local_card.guid()));
  EXPECT_TRUE(external_delegate().RemoveSuggestion(
      test::CreateAutofillSuggestion(SuggestionType::kCreditCardEntry, u"card",
                                     Suggestion::Guid(local_card.guid()))));
  EXPECT_FALSE(
      pdm().payments_data_manager().GetCreditCardByGUID(local_card.guid()));
}

// Tests that server cards are not removed.
TEST_F(AutofillExternalDelegateTest, RemoveSuggestion_ServerCard) {
  const CreditCard server_card = test::GetMaskedServerCard();
  TestPaymentsDataManager& paydm =
      static_cast<TestPaymentsDataManager&>(pdm().payments_data_manager());
  paydm.SetAutofillWalletImportEnabled(true);
  paydm.AddServerCreditCard(server_card);
  ASSERT_TRUE(
      pdm().payments_data_manager().GetCreditCardByGUID(server_card.guid()));
  EXPECT_FALSE(external_delegate().RemoveSuggestion(
      test::CreateAutofillSuggestion(SuggestionType::kCreditCardEntry, u"card",
                                     Suggestion::Guid(server_card.guid()))));
  EXPECT_TRUE(
      pdm().payments_data_manager().GetCreditCardByGUID(server_card.guid()));
}

TEST_F(AutofillExternalDelegateTest, RecordSuggestionTypeOnSuggestionAccepted) {
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
TEST_F(AutofillExternalDelegateTest, UpdateSuggestions) {
  IssueOnQuery();

  std::vector<Suggestion> suggestions1 = {
      Suggestion(u"Some suggestion", SuggestionType::kAutocompleteEntry)};
  std::vector<Suggestion> suggestions2 = {
      Suggestion(u"Other suggestion", SuggestionType::kAutocompleteEntry)};

  {
    InSequence s;
    EXPECT_CALL(autofill_client(), ShowAutofillSuggestions);
    EXPECT_CALL(autofill_client(),
                UpdateAutofillSuggestions(
                    suggestions2, FillingProduct::kAutocomplete,
                    AutofillSuggestionTriggerSource::kUnspecified));
  }

  OnSuggestionsReturned(queried_field().global_id(), suggestions1);
  external_delegate().AttemptToDisplayAutofillSuggestionsForTest(
      suggestions2, AutofillSuggestionTriggerSource::kUnspecified,
      /*is_update=*/true);
}

// TODO(crbug.com/41483208): Add test case where 'Show cards from your Google
// account' button is clicked. Encountered issues with test sync setup when
// attempting to make it.

#if BUILDFLAG(IS_IOS)
// Tests that outdated returned suggestions are discarded.
TEST_F(AutofillExternalDelegateTest, ShouldDiscardOutdatedSuggestions) {
  FieldGlobalId old_field_id = test::MakeFieldGlobalId();
  FieldGlobalId new_field_id = test::MakeFieldGlobalId();
  autofill_client().set_last_queried_field(new_field_id);
  IssueOnQuery();
  EXPECT_CALL(autofill_client(), ShowAutofillSuggestions).Times(0);
  OnSuggestionsReturned(old_field_id, std::vector<Suggestion>());
}
#endif

}  // namespace

}  // namespace autofill
