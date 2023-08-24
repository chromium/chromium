// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/i18n/rtl.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/metrics/autofill_metrics.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/browser/ui/suggestion_test_helpers.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#include "components/plus_addresses/plus_address_service.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "url/origin.h"

using testing::_;
using testing::Field;
using testing::Matcher;
using testing::NiceMock;

namespace autofill {

namespace {

Matcher<const AutofillTriggerDetails&> EqualsAutofilltriggerDetails(
    AutofillTriggerDetails details) {
  return AllOf(
      Field(&AutofillTriggerDetails::trigger_source, details.trigger_source),
      Field(&AutofillTriggerDetails::filling_granularity,
            details.filling_granularity));
}

constexpr auto kDefaultTriggerSource =
    AutofillSuggestionTriggerSource::kFormControlElementClicked;

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
              RendererShouldFillFieldWithValue,
              (const FieldGlobalId&, const std::u16string&),
              (override));
  MOCK_METHOD(void,
              RendererShouldPreviewFieldWithValue,
              (const FieldGlobalId&, const std::u16string&),
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
              (const std::vector<std::u16string>& values,
               const std::vector<std::u16string>& labels),
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

class MockBrowserAutofillManager : public BrowserAutofillManager {
 public:
  MockBrowserAutofillManager(AutofillDriver* driver, MockAutofillClient* client)
      : BrowserAutofillManager(driver, client, "en-US") {}
  MockBrowserAutofillManager(const MockBrowserAutofillManager&) = delete;
  MockBrowserAutofillManager& operator=(const MockBrowserAutofillManager&) =
      delete;

  PopupType GetPopupType(const FormData& form,
                         const FormFieldData& field) override {
    return PopupType::kPersonalInformation;
  }

  MOCK_METHOD(bool,
              ShouldShowScanCreditCard,
              (const FormData& form, const FormFieldData& field),
              (override));
  MOCK_METHOD(void,
              OnUserHideSuggestions,
              (const FormData& form, const FormFieldData& field),
              (override));
  MOCK_METHOD(void,
              FillOrPreviewVirtualCardInformation,
              (mojom::AutofillActionPersistence action_persistence,
               const std::string& guid,
               const FormData& form,
               const FormFieldData& field,
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
              (mojom::AutofillActionPersistence action_persistence,
               FormData form,
               const FormFieldData& trigger_field),
              (override));
  MOCK_METHOD(void,
              FillOrPreviewForm,
              (mojom::AutofillActionPersistence action_persistence,
               const FormData& form,
               const FormFieldData& field,
               Suggestion::BackendId backend_id,
               const AutofillTriggerDetails& trigger_details),
              (override));
  MOCK_METHOD(void,
              FillCreditCardFormImpl,
              (const FormData& form,
               const FormFieldData& field,
               const CreditCard& credit_card,
               const std::u16string& cvc,
               const AutofillTriggerDetails& trigger_details),
              (override));

 private:
  bool should_show_cards_from_account_option_ = false;
};

}  // namespace

class AutofillExternalDelegateUnitTest : public testing::Test {
 protected:
  void SetUp() override {
    autofill_driver_ = std::make_unique<NiceMock<MockAutofillDriver>>();
    browser_autofill_manager_ =
        std::make_unique<NiceMock<MockBrowserAutofillManager>>(
            autofill_driver_.get(), &autofill_client_);
    external_delegate_ = std::make_unique<AutofillExternalDelegate>(
        browser_autofill_manager_.get());
  }

  void TearDown() override {
    // Order of destruction is important as BrowserAutofillManager relies on
    // PersonalDataManager to be around when it gets destroyed.
    external_delegate_.reset();
    browser_autofill_manager_.reset();
    autofill_driver_.reset();
  }

  // Issue an OnQuery call.
  void IssueOnQuery() {
    FormData form;
    form.host_frame = form_id_.frame_token;
    form.unique_renderer_id = form_id_.renderer_id;
    FormFieldData field;
    field.host_frame = field_id_.frame_token;
    field.unique_renderer_id = field_id_.renderer_id;
    field.host_form_id = form.unique_renderer_id;
    field.is_focusable = true;
    field.should_autocomplete = true;
    external_delegate_->OnQuery(form, field, gfx::RectF());
  }

  void IssueOnSuggestionsReturned(FieldGlobalId field_id) {
    std::vector<Suggestion> suggestions;
    suggestions.emplace_back();
    suggestions[0].popup_item_id = PopupItemId::kAddressEntry;
    external_delegate_->OnSuggestionsReturned(field_id, suggestions,
                                              kDefaultTriggerSource);
  }

  base::test::TaskEnvironment task_environment_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;

  NiceMock<MockAutofillClient> autofill_client_;
  std::unique_ptr<NiceMock<MockAutofillDriver>> autofill_driver_;
  std::unique_ptr<MockBrowserAutofillManager> browser_autofill_manager_;
  std::unique_ptr<AutofillExternalDelegate> external_delegate_;

  FormGlobalId form_id_ = test::MakeFormGlobalId();
  FieldGlobalId field_id_ = test::MakeFieldGlobalId();
};

// Variant for use in cases when we expect the BrowserAutofillManager would
// normally set the |should_show_cards_from_account_option_| bit.
class AutofillExternalDelegateCardsFromAccountTest
    : public AutofillExternalDelegateUnitTest {
 protected:
  void SetUp() override {
    AutofillExternalDelegateUnitTest::SetUp();
    browser_autofill_manager_->ShowCardsFromAccountOption();
  }
};

// Test that our external delegate called the virtual methods at the right time.
TEST_F(AutofillExternalDelegateUnitTest, TestExternalDelegateVirtualCalls) {
  IssueOnQuery();

  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  // This should call ShowAutofillPopup.
  std::vector<Suggestion> autofill_item;
  autofill_item.emplace_back();
  autofill_item[0].popup_item_id = PopupItemId::kAddressEntry;
  external_delegate_->OnSuggestionsReturned(field_id_, autofill_item,
                                            kDefaultTriggerSource);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(PopupItemId::kAddressEntry,
#if !BUILDFLAG(IS_ANDROID)
                                     PopupItemId::kSeparator,
#endif
                                     PopupItemId::kAutofillOptions));
  EXPECT_EQ(open_args.trigger_source, kDefaultTriggerSource);

  EXPECT_CALL(
      *browser_autofill_manager_,
      FillOrPreviewForm(mojom::AutofillActionPersistence::kFill, _, _, _, _));
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));

  // This should trigger a call to hide the popup since we've selected an
  // option.
  external_delegate_->DidAcceptSuggestion(autofill_item[0], 0,
                                          kDefaultTriggerSource);
}

// Test that data list elements for a node will appear in the Autofill popup.
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateDataList) {
  IssueOnQuery();

  std::vector<std::u16string> data_list_items;
  data_list_items.emplace_back();

  EXPECT_CALL(autofill_client_, UpdateAutofillPopupDataListValues(
                                    data_list_items, data_list_items));

  external_delegate_->SetCurrentDataListValues(data_list_items,
                                               data_list_items);

  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  // This should call ShowAutofillPopup.
  std::vector<Suggestion> autofill_item;
  autofill_item.emplace_back();
  autofill_item[0].popup_item_id = PopupItemId::kAddressEntry;
  external_delegate_->OnSuggestionsReturned(field_id_, autofill_item,
                                            kDefaultTriggerSource);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(PopupItemId::kDatalistEntry,
#if !BUILDFLAG(IS_ANDROID)
                                     PopupItemId::kSeparator,
#endif
                                     PopupItemId::kAddressEntry,
#if !BUILDFLAG(IS_ANDROID)
                                     PopupItemId::kSeparator,
#endif
                                     PopupItemId::kAutofillOptions));
  EXPECT_EQ(open_args.trigger_source, kDefaultTriggerSource);

  // Try calling OnSuggestionsReturned with no Autofill values and ensure
  // the datalist items are still shown.

  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  autofill_item.clear();
  external_delegate_->OnSuggestionsReturned(field_id_, autofill_item,
                                            kDefaultTriggerSource);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(PopupItemId::kDatalistEntry));
  EXPECT_EQ(open_args.trigger_source, kDefaultTriggerSource);
}

// Test that datalist values can get updated while a popup is showing.
TEST_F(AutofillExternalDelegateUnitTest, UpdateDataListWhileShowingPopup) {
  IssueOnQuery();

  EXPECT_CALL(autofill_client_, ShowAutofillPopup).Times(0);

  // Make sure just setting the data list values doesn't cause the popup to
  // appear.
  std::vector<std::u16string> data_list_items;
  data_list_items.emplace_back();

  EXPECT_CALL(autofill_client_, UpdateAutofillPopupDataListValues(
                                    data_list_items, data_list_items));

  external_delegate_->SetCurrentDataListValues(data_list_items,
                                               data_list_items);

  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  // Ensure the popup is displayed.
  std::vector<Suggestion> autofill_item;
  autofill_item.emplace_back();
  autofill_item[0].popup_item_id = PopupItemId::kAddressEntry;
  external_delegate_->OnSuggestionsReturned(field_id_, autofill_item,
                                            kDefaultTriggerSource);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(PopupItemId::kDatalistEntry,
#if !BUILDFLAG(IS_ANDROID)
                                     PopupItemId::kSeparator,
#endif
                                     PopupItemId::kAddressEntry,
#if !BUILDFLAG(IS_ANDROID)
                                     PopupItemId::kSeparator,
#endif
                                     PopupItemId::kAutofillOptions));
  EXPECT_EQ(open_args.trigger_source, kDefaultTriggerSource);

  // This would normally get called from ShowAutofillPopup, but it is mocked so
  // we need to call OnPopupShown ourselves.
  external_delegate_->OnPopupShown();

  // Update the current data list and ensure the popup is updated.
  data_list_items.emplace_back();

  EXPECT_CALL(autofill_client_, UpdateAutofillPopupDataListValues(
                                    data_list_items, data_list_items));

  external_delegate_->SetCurrentDataListValues(data_list_items,
                                               data_list_items);
}

// Test that we _don't_ de-dupe autofill values against datalist values. We
// keep both with a separator.
TEST_F(AutofillExternalDelegateUnitTest, DuplicateAutofillDatalistValues) {
  IssueOnQuery();

  std::vector<std::u16string> data_list_values{u"Rick", u"Beyonce"};
  std::vector<std::u16string> data_list_labels{u"Deckard", u"Knowles"};

  EXPECT_CALL(autofill_client_, UpdateAutofillPopupDataListValues(
                                    data_list_values, data_list_labels));

  external_delegate_->SetCurrentDataListValues(data_list_values,
                                               data_list_labels);

  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  // Have an Autofill item that is identical to one of the datalist entries.
  std::vector<Suggestion> autofill_item;
  autofill_item.emplace_back();
  autofill_item[0].main_text =
      Suggestion::Text(u"Rick", Suggestion::Text::IsPrimary(true));
  autofill_item[0].labels = {{Suggestion::Text(u"Deckard")}};
  autofill_item[0].popup_item_id = PopupItemId::kAddressEntry;
  external_delegate_->OnSuggestionsReturned(field_id_, autofill_item,
                                            kDefaultTriggerSource);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(PopupItemId::kDatalistEntry,
                                     PopupItemId::kDatalistEntry,
#if !BUILDFLAG(IS_ANDROID)
                                     PopupItemId::kSeparator,
#endif
                                     PopupItemId::kAddressEntry,
#if !BUILDFLAG(IS_ANDROID)
                                     PopupItemId::kSeparator,
#endif
                                     PopupItemId::kAutofillOptions));
  EXPECT_EQ(open_args.trigger_source, kDefaultTriggerSource);
}

// Test that we de-dupe autocomplete values against datalist values, keeping the
// latter in case of a match.
TEST_F(AutofillExternalDelegateUnitTest, DuplicateAutocompleteDatalistValues) {
  IssueOnQuery();

  std::vector<std::u16string> data_list_values{u"Rick", u"Beyonce"};
  std::vector<std::u16string> data_list_labels{u"Deckard", u"Knowles"};

  EXPECT_CALL(autofill_client_, UpdateAutofillPopupDataListValues(
                                    data_list_values, data_list_labels));

  external_delegate_->SetCurrentDataListValues(data_list_values,
                                               data_list_labels);

  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

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
  external_delegate_->OnSuggestionsReturned(field_id_, autocomplete_items,
                                            kDefaultTriggerSource);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(
                  // We are expecting only two data list entries.
                  PopupItemId::kDatalistEntry, PopupItemId::kDatalistEntry,
#if !BUILDFLAG(IS_ANDROID)
                  PopupItemId::kSeparator,
#endif
                  PopupItemId::kAutocompleteEntry));
  EXPECT_EQ(open_args.trigger_source, kDefaultTriggerSource);
}

// Test that the Autofill popup is able to display warnings explaining why
// Autofill is disabled for a website.
// Regression test for http://crbug.com/247880
TEST_F(AutofillExternalDelegateUnitTest, AutofillWarnings) {
  IssueOnQuery();

  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  // This should call ShowAutofillPopup.
  std::vector<Suggestion> autofill_item;
  autofill_item.emplace_back();
  autofill_item[0].popup_item_id =
      PopupItemId::kInsecureContextPaymentDisabledMessage;
  external_delegate_->OnSuggestionsReturned(field_id_, autofill_item,
                                            kDefaultTriggerSource);

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

  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  // This should call ShowAutofillPopup.
  std::vector<Suggestion> suggestions;
  suggestions.emplace_back();
  suggestions[0].popup_item_id =
      PopupItemId::kInsecureContextPaymentDisabledMessage;
  suggestions.emplace_back();
  suggestions[1].main_text =
      Suggestion::Text(u"Rick", Suggestion::Text::IsPrimary(true));
  suggestions[1].popup_item_id = PopupItemId::kAutocompleteEntry;
  external_delegate_->OnSuggestionsReturned(field_id_, suggestions,
                                            kDefaultTriggerSource);

  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(PopupItemId::kAutocompleteEntry));
  EXPECT_EQ(open_args.trigger_source, kDefaultTriggerSource);
}

// Test that the Autofill delegate doesn't try and fill a form with a
// negative unique id.
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateInvalidUniqueId) {
  // Ensure it doesn't try to preview the negative id.
  EXPECT_CALL(*browser_autofill_manager_, FillOrPreviewForm(_, _, _, _, _))
      .Times(0);
  EXPECT_CALL(*autofill_driver_, RendererShouldClearPreviewedForm());
  const Suggestion suggestion{
      PopupItemId::kInsecureContextPaymentDisabledMessage};
  external_delegate_->DidSelectSuggestion(suggestion, kDefaultTriggerSource);

  // Ensure it doesn't try to fill the form in with the negative id.
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  EXPECT_CALL(*browser_autofill_manager_, FillOrPreviewForm(_, _, _, _, _))
      .Times(0);

  external_delegate_->DidAcceptSuggestion(suggestion, /*position=*/0,
                                          kDefaultTriggerSource);
}

// Test that the Autofill delegate still allows previewing and filling
// specifically of the negative ID for PopupItemId::kIbanEntry.
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateFillsIbanEntry) {
  IssueOnQuery();

  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  // This should call ShowAutofillPopup.
  std::vector<Suggestion> suggestions;
  suggestions.emplace_back();
  std::u16string masked_iban_value = u"IE12 **** **** **** **56 78";
  std::u16string unmasked_iban_value = u"IE12 BOFI 9000 0112 3456 78";
  suggestions[0].main_text.value = masked_iban_value;
  suggestions[0].labels = {{Suggestion::Text(u"My doctor's IBAN")}};
  suggestions[0].payload = Suggestion::ValueToFill(unmasked_iban_value);
  suggestions[0].popup_item_id = PopupItemId::kIbanEntry;
  external_delegate_->OnSuggestionsReturned(field_id_, suggestions,
                                            kDefaultTriggerSource);

  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(PopupItemId::kIbanEntry));

  EXPECT_CALL(*autofill_driver_, RendererShouldClearPreviewedForm());
  EXPECT_CALL(*autofill_driver_, RendererShouldPreviewFieldWithValue(
                                     field_id_, masked_iban_value));
  external_delegate_->DidSelectSuggestion(suggestions[0],
                                          kDefaultTriggerSource);
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  EXPECT_CALL(*autofill_driver_,
              RendererShouldFillFieldWithValue(field_id_, unmasked_iban_value));
  external_delegate_->DidAcceptSuggestion(suggestions[0], 0,
                                          kDefaultTriggerSource);
}

// Test that the Autofill delegate still allows previewing and filling
// specifically of the negative ID for PopupItemId::kMerchantPromoCodeEntry.
TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateFillsMerchantPromoCodeEntry) {
  IssueOnQuery();

  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  // This should call ShowAutofillPopup.
  std::vector<Suggestion> suggestions;
  suggestions.emplace_back();
  std::u16string promo_code_value = u"PROMOCODE1234";
  suggestions[0].main_text.value = promo_code_value;
  suggestions[0].labels = {{Suggestion::Text(u"12.34% off your purchase!")}};
  suggestions[0].popup_item_id = PopupItemId::kMerchantPromoCodeEntry;
  external_delegate_->OnSuggestionsReturned(field_id_, suggestions,
                                            kDefaultTriggerSource);

  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(PopupItemId::kMerchantPromoCodeEntry));

  EXPECT_CALL(*autofill_driver_, RendererShouldClearPreviewedForm());
  EXPECT_CALL(*autofill_driver_,
              RendererShouldPreviewFieldWithValue(field_id_, promo_code_value));
  external_delegate_->DidSelectSuggestion(suggestions[0],
                                          kDefaultTriggerSource);
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  EXPECT_CALL(*autofill_driver_,
              RendererShouldFillFieldWithValue(field_id_, promo_code_value));

  external_delegate_->DidAcceptSuggestion(suggestions[0], 0,
                                          kDefaultTriggerSource);
}

// Test that the Autofill delegate routes the merchant promo code suggestions
// footer redirect logic correctly.
TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateMerchantPromoCodeSuggestionsFooter) {
  const GURL gurl{"https://example.com/"};
  EXPECT_CALL(autofill_client_, OpenPromoCodeOfferDetailsURL(gurl));

  external_delegate_->DidAcceptSuggestion(
      test::CreateAutofillSuggestion(PopupItemId::kSeePromoCodeDetails,
                                     u"baz foo", gurl),
      0, kDefaultTriggerSource);
}

// Test that the ClearPreview call is only sent if the form was being previewed
// (i.e. it isn't autofilling a password).
TEST_F(AutofillExternalDelegateUnitTest, ExternalDelegateClearPreviewedForm) {
  // Ensure selecting a new password entries or Autofill entries will
  // cause any previews to get cleared.
  IssueOnQuery();
  EXPECT_CALL(*autofill_driver_, RendererShouldClearPreviewedForm());
  external_delegate_->DidSelectSuggestion(
      test::CreateAutofillSuggestion(PopupItemId::kPasswordEntry, u"baz foo"),
      kDefaultTriggerSource);
  EXPECT_CALL(*autofill_driver_, RendererShouldClearPreviewedForm());
  EXPECT_CALL(*browser_autofill_manager_,
              FillOrPreviewForm(mojom::AutofillActionPersistence::kPreview, _,
                                _, _, _));
  external_delegate_->DidSelectSuggestion(
      test::CreateAutofillSuggestion(PopupItemId::kAddressEntry, u"baz foo"),
      kDefaultTriggerSource);

  // Ensure selecting an autocomplete entry will cause any previews to
  // get cleared.
  EXPECT_CALL(*autofill_driver_, RendererShouldClearPreviewedForm());
  EXPECT_CALL(*autofill_driver_, RendererShouldPreviewFieldWithValue(
                                     field_id_, std::u16string(u"baz foo")));
  external_delegate_->DidSelectSuggestion(
      test::CreateAutofillSuggestion(PopupItemId::kAutocompleteEntry,
                                     u"baz foo"),
      kDefaultTriggerSource);

  // Ensure selecting a virtual card entry will cause any previews to
  // get cleared.
  EXPECT_CALL(*autofill_driver_, RendererShouldClearPreviewedForm());
  EXPECT_CALL(*browser_autofill_manager_,
              FillOrPreviewVirtualCardInformation(
                  mojom::AutofillActionPersistence::kPreview, _, _, _, _));
  external_delegate_->DidSelectSuggestion(
      test::CreateAutofillSuggestion(PopupItemId::kVirtualCreditCardEntry),
      kDefaultTriggerSource);
}

// Test that the popup is hidden once we are done editing the autofill field.
TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateHidePopupAfterEditing) {
  EXPECT_CALL(autofill_client_, ShowAutofillPopup);
  test::GenerateTestAutofillPopup(external_delegate_.get());

  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(autofill::PopupHidingReason::kEndEditing));
  external_delegate_->DidEndTextFieldEditing();
}

// Test that the driver is directed to accept the data list after being notified
// that the user accepted the data list suggestion.
TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateAcceptDatalistSuggestion) {
  IssueOnQuery();
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  std::u16string dummy_string(u"baz qux");
  EXPECT_CALL(*autofill_driver_,
              RendererShouldAcceptDataListSuggestion(field_id_, dummy_string));

  external_delegate_->DidAcceptSuggestion(
      test::CreateAutofillSuggestion(PopupItemId::kDatalistEntry, dummy_string),
      0, kDefaultTriggerSource);
}

// Test that an accepted autofill suggestion will fill the form.
TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateAcceptAutofillSuggestion) {
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  EXPECT_CALL(
      *browser_autofill_manager_,
      FillOrPreviewForm(mojom::AutofillActionPersistence::kFill, _, _, _, _));

  external_delegate_->DidAcceptSuggestion(
      test::CreateAutofillSuggestion(PopupItemId::kAddressEntry,
                                     u"John Legend"),
      2, kDefaultTriggerSource);  // Row 2
}

TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateAcceptFillEverythingSuggestion) {
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  EXPECT_CALL(
      *browser_autofill_manager_,
      FillOrPreviewForm(mojom::AutofillActionPersistence::kFill, _, _, _, _));

  external_delegate_->DidAcceptSuggestion(
      test::CreateAutofillSuggestion(
          PopupItemId::kFillEverythingFromAddressProfile, u"John Legend"),
      2, kDefaultTriggerSource);  // Row 2
}

// Tests that when accepting a suggestion, the `AutofillSuggestionTriggerSource`
// is converted to the correct `AutofillTriggerSource`.
TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateAcceptAutofillSuggestion_TriggerSource) {
  Suggestion suggestion =
      test::CreateAutofillSuggestion(PopupItemId::kAddressEntry);

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
      *browser_autofill_manager_,
      FillOrPreviewForm(
          mojom::AutofillActionPersistence::kFill, _, _, _,
          EqualsAutofilltriggerDetails({.trigger_source = expected_source})));
  external_delegate_->DidAcceptSuggestion(suggestion, /*position=*/1,
                                          suggestion_source);

  // Expect that `kManualFallbackForAutocompleteUnrecognized` translates to
  // trigger source of the same name.
  suggestion_source = AutofillSuggestionTriggerSource::
      kManualFallbackForAutocompleteUnrecognized;
  expected_source =
      AutofillTriggerSource::kManualFallbackForAutocompleteUnrecognized;
  EXPECT_CALL(
      *browser_autofill_manager_,
      FillOrPreviewForm(
          mojom::AutofillActionPersistence::kFill, _, _, _,
          EqualsAutofilltriggerDetails({.trigger_source = expected_source})));
  external_delegate_->DidAcceptSuggestion(suggestion, /*position=*/1,
                                          suggestion_source);
}

// Mock out an existing plus address autofill suggestion, and ensure that
// choosing it results in the field being filled with its value (as opposed to
// the mocked address used in the creation flow).
TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateFillsExistingPlusAddress) {
  IssueOnQuery();

  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  // This should call ShowAutofillPopup.
  std::vector<Suggestion> suggestions;
  suggestions.emplace_back();
  // This function tests the filling of existing plus addresses, which is why
  // `OfferPlusAddressCreation` need not be mocked.
  std::u16string plus_address = u"test+plus@test.example";
  suggestions[0].main_text.value = plus_address;
  suggestions[0].popup_item_id = PopupItemId::kFillExistingPlusAddress;
  external_delegate_->OnSuggestionsReturned(field_id_, suggestions,
                                            kDefaultTriggerSource);

  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(PopupItemId::kFillExistingPlusAddress));

  EXPECT_CALL(*autofill_driver_, RendererShouldClearPreviewedForm());
  EXPECT_CALL(*autofill_driver_,
              RendererShouldPreviewFieldWithValue(field_id_, plus_address));
  external_delegate_->DidSelectSuggestion(suggestions[0],
                                          kDefaultTriggerSource);
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  EXPECT_CALL(*autofill_driver_,
              RendererShouldFillFieldWithValue(field_id_, plus_address));
  external_delegate_->DidAcceptSuggestion(suggestions[0], 0,
                                          kDefaultTriggerSource);
}

// Mock out the new plus address creation flow, and ensure that its completion
// results in the field being filled with the resulting plus address.
TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateOffersPlusAddressCreation) {
  const std::u16string kMockPlusAddressForCreationCallback =
      u"test+1234@test.example";

  IssueOnQuery();
  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  // This should call ShowAutofillPopup.
  std::vector<Suggestion> suggestions;
  suggestions.emplace_back();
  suggestions[0].popup_item_id = PopupItemId::kCreateNewPlusAddress;
  external_delegate_->OnSuggestionsReturned(field_id_, suggestions,
                                            kDefaultTriggerSource);

  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIdsAre(PopupItemId::kCreateNewPlusAddress));

  EXPECT_CALL(*autofill_driver_, RendererShouldClearPreviewedForm());
  external_delegate_->DidSelectSuggestion(suggestions[0],
                                          kDefaultTriggerSource);
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  // Mock out the plus address creation logic to ensure it is deterministic and
  // independent of the client implementations in //chrome or //ios.
  EXPECT_CALL(autofill_client_, OfferPlusAddressCreation)
      .WillOnce([&](const url::Origin& origin,
                    plus_addresses::PlusAddressCallback callback) {
        std::move(callback).Run(
            base::UTF16ToUTF8(kMockPlusAddressForCreationCallback));
      });
  // `kMockPlusAddressForCreationCallback` is returned in the callback from the
  // mocked `OfferPlusAddressCreation()`. Ensure it is filled (vs, say, the
  // empty text of the suggestion).
  EXPECT_CALL(*autofill_driver_,
              RendererShouldFillFieldWithValue(
                  field_id_, kMockPlusAddressForCreationCallback));
  external_delegate_->DidAcceptSuggestion(suggestions[0], 0,
                                          kDefaultTriggerSource);
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
    EXPECT_CALL(*browser_autofill_manager_, UndoAutofill);
  } else {
    EXPECT_CALL(autofill_client_,
                HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
    EXPECT_CALL(*autofill_driver_, RendererShouldClearFilledSection());
  }
  external_delegate_->DidAcceptSuggestion(Suggestion(PopupItemId::kClearForm),
                                          0, kDefaultTriggerSource);
}

// Test that the driver is directed to undo the form after being notified that
// the user selected the suggestion to undo the form.
TEST_P(AutofillExternalDelegateUnitTest_UndoAutofill,
       ExternalDelegateUndoAndClearPreviewForm) {
  if (UndoInsteadOfClear()) {
    EXPECT_CALL(*browser_autofill_manager_, UndoAutofill);
  }
  external_delegate_->DidSelectSuggestion(Suggestion(PopupItemId::kClearForm),
                                          kDefaultTriggerSource);
}

// Test that autofill client will scan a credit card after use accepted the
// suggestion to scan a credit card.
TEST_F(AutofillExternalDelegateUnitTest, ScanCreditCardMenuItem) {
  EXPECT_CALL(autofill_client_, ScanCreditCard(_));
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));

  external_delegate_->DidAcceptSuggestion(
      Suggestion(PopupItemId::kScanCreditCard), 0, kDefaultTriggerSource);
}

TEST_F(AutofillExternalDelegateUnitTest, ScanCreditCardPromptMetricsTest) {
  // Log that the scan card item was shown, although nothing was selected.
  {
    EXPECT_CALL(*browser_autofill_manager_, ShouldShowScanCreditCard(_, _))
        .WillOnce(testing::Return(true));
    base::HistogramTester histogram;
    IssueOnQuery();
    IssueOnSuggestionsReturned(field_id_);
    external_delegate_->OnPopupShown();
    histogram.ExpectUniqueSample("Autofill.ScanCreditCardPrompt",
                                 AutofillMetrics::SCAN_CARD_ITEM_SHOWN, 1);
  }
  // Log that the scan card item was selected.
  {
    EXPECT_CALL(*browser_autofill_manager_, ShouldShowScanCreditCard(_, _))
        .WillOnce(testing::Return(true));
    base::HistogramTester histogram;
    IssueOnQuery();
    IssueOnSuggestionsReturned(field_id_);
    external_delegate_->OnPopupShown();

    external_delegate_->DidAcceptSuggestion(
        Suggestion(PopupItemId::kScanCreditCard), 0, kDefaultTriggerSource);

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
    EXPECT_CALL(*browser_autofill_manager_, ShouldShowScanCreditCard(_, _))
        .WillOnce(testing::Return(true));
    base::HistogramTester histogram;
    IssueOnQuery();
    IssueOnSuggestionsReturned(field_id_);
    external_delegate_->OnPopupShown();

    external_delegate_->DidAcceptSuggestion(Suggestion(PopupItemId::kClearForm),
                                            0, kDefaultTriggerSource);

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
    EXPECT_CALL(*browser_autofill_manager_, ShouldShowScanCreditCard(_, _))
        .WillOnce(testing::Return(false));
    base::HistogramTester histogram;
    IssueOnQuery();
    IssueOnSuggestionsReturned(field_id_);
    external_delegate_->OnPopupShown();
    histogram.ExpectTotalCount("Autofill.ScanCreditCardPrompt", 0);
  }
}

MATCHER_P(CreditCardMatches, card, "") {
  return !arg.Compare(card);
}

// Test that autofill manager will fill the credit card form after user scans a
// credit card.
TEST_F(AutofillExternalDelegateUnitTest, FillCreditCardFormImpl) {
  CreditCard card;
  test::SetCreditCardInfo(&card, "Alice", "4111", "1", "3000", "1");
  EXPECT_CALL(*browser_autofill_manager_,
              FillCreditCardFormImpl(_, _, CreditCardMatches(card),
                                     std::u16string(), _));
  external_delegate_->OnCreditCardScanned(AutofillTriggerSource::kPopup, card);
}

TEST_F(AutofillExternalDelegateUnitTest, IgnoreAutocompleteOffForAutofill) {
  const FormData form;
  FormFieldData field;
  field.is_focusable = true;
  field.should_autocomplete = false;

  external_delegate_->OnQuery(form, field, gfx::RectF());

  std::vector<Suggestion> autofill_items;
  autofill_items.emplace_back();
  autofill_items[0].popup_item_id = PopupItemId::kAutocompleteEntry;

  // Ensure the popup tries to show itself, despite autocomplete="off".
  EXPECT_CALL(autofill_client_, ShowAutofillPopup);
  EXPECT_CALL(autofill_client_, HideAutofillPopup(_)).Times(0);

  external_delegate_->OnSuggestionsReturned(field.global_id(), autofill_items,
                                            kDefaultTriggerSource);
}

TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateFillFieldWithValue_Autocomplete) {
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  IssueOnQuery();

  base::HistogramTester histogram_tester;
  std::u16string dummy_autocomplete_string(u"autocomplete");
  EXPECT_CALL(*autofill_driver_, RendererShouldFillFieldWithValue(
                                     field_id_, dummy_autocomplete_string));
  EXPECT_CALL(*autofill_client_.GetMockAutocompleteHistoryManager(),
              OnSingleFieldSuggestionSelected(dummy_autocomplete_string,
                                              PopupItemId::kAutocompleteEntry));

  external_delegate_->DidAcceptSuggestion(
      test::CreateAutofillSuggestion(PopupItemId::kAutocompleteEntry,
                                     dummy_autocomplete_string),
      0, kDefaultTriggerSource);

  histogram_tester.ExpectUniqueSample(
      "Autofill.SuggestionAcceptedIndex.Autocomplete", 0, 1);
}

TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateFillFieldWithValue_MerchantPromoCode) {
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  IssueOnQuery();

  std::u16string dummy_promo_code_string(u"merchant promo");
  EXPECT_CALL(*autofill_driver_, RendererShouldFillFieldWithValue(
                                     field_id_, dummy_promo_code_string));
  EXPECT_CALL(
      *autofill_client_.GetMockMerchantPromoCodeManager(),
      OnSingleFieldSuggestionSelected(dummy_promo_code_string,
                                      PopupItemId::kMerchantPromoCodeEntry));

  external_delegate_->DidAcceptSuggestion(
      test::CreateAutofillSuggestion(PopupItemId::kMerchantPromoCodeEntry,
                                     dummy_promo_code_string),
      0, kDefaultTriggerSource);
}

TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateFillFieldWithValue_Iban) {
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  IssueOnQuery();

  std::u16string masked_iban_value = u"IE12 **** **** **** **56 78";
  std::u16string unmasked_iban_value = u"IE12 BOFI 9000 0112 3456 78";
  EXPECT_CALL(*autofill_driver_,
              RendererShouldFillFieldWithValue(field_id_, unmasked_iban_value));
  EXPECT_CALL(*autofill_client_.GetMockIbanManager(),
              OnSingleFieldSuggestionSelected(masked_iban_value,
                                              PopupItemId::kIbanEntry));

  external_delegate_->DidAcceptSuggestion(
      test::CreateAutofillSuggestion(
          PopupItemId::kIbanEntry, masked_iban_value,
          Suggestion::ValueToFill(unmasked_iban_value)),
      /*position=*/0, kDefaultTriggerSource);
}

TEST_F(AutofillExternalDelegateUnitTest,
       ExternalDelegateFillFieldWithValueFieldByFieldFilling) {
  EXPECT_CALL(autofill_client_,
              HideAutofillPopup(PopupHidingReason::kAcceptSuggestion));
  IssueOnQuery();

  base::HistogramTester histogram_tester;
  const std::u16string dummy_field_by_field_string = u"field by field";
  EXPECT_CALL(*autofill_driver_, RendererShouldFillFieldWithValue(
                                     field_id_, dummy_field_by_field_string));

  external_delegate_->DidAcceptSuggestion(
      test::CreateAutofillSuggestion(PopupItemId::kFieldByFieldFilling,
                                     dummy_field_by_field_string),
      /*position=*/0, kDefaultTriggerSource);
}

TEST_F(AutofillExternalDelegateUnitTest, ShouldShowGooglePayIcon) {
  IssueOnQuery();

  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  std::vector<Suggestion> autofill_item;
  autofill_item.emplace_back();
  autofill_item[0].popup_item_id = PopupItemId::kAddressEntry;

  // This should call ShowAutofillPopup.
  external_delegate_->OnSuggestionsReturned(field_id_, autofill_item,
                                            kDefaultTriggerSource, true);

  // On Desktop, the GPay icon should be stored in the store indicator icon.
#if BUILDFLAG(IS_ANDROID)
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIconsAre(std::string(),
                                       testing::StartsWith("googlePay")));
#elif BUILDFLAG(IS_IOS)
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorIconsAre(std::string(), std::string(),
                                       testing::StartsWith("googlePay")));
#else
  EXPECT_THAT(open_args.suggestions, SuggestionVectorStoreIndicatorIconsAre(
                                         std::string(), std::string(),
                                         testing::StartsWith("googlePay")));
#endif
  EXPECT_EQ(open_args.trigger_source, kDefaultTriggerSource);
}

TEST_F(AutofillExternalDelegateUnitTest,
       ShouldNotShowGooglePayIconIfSuggestionsContainLocalCards) {
  IssueOnQuery();

  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  std::vector<Suggestion> autofill_item;
  autofill_item.emplace_back();
  autofill_item[0].popup_item_id = PopupItemId::kAddressEntry;

  // This should call ShowAutofillPopup.
  external_delegate_->OnSuggestionsReturned(field_id_, autofill_item,
                                            kDefaultTriggerSource, false);
  EXPECT_THAT(open_args.suggestions, SuggestionVectorIconsAre(std::string(),
#if !BUILDFLAG(IS_ANDROID)
                                                              std::string(),
#endif
                                                              "settingsIcon"));
  EXPECT_EQ(open_args.trigger_source, kDefaultTriggerSource);
}

TEST_F(AutofillExternalDelegateUnitTest, ShouldUseNewSettingName) {
  IssueOnQuery();

  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  std::vector<Suggestion> autofill_item;
  autofill_item.emplace_back();
  autofill_item[0].popup_item_id = PopupItemId::kAddressEntry;
  autofill_item[0].main_text.is_primary = Suggestion::Text::IsPrimary(true);

  // This should call ShowAutofillPopup.
  external_delegate_->OnSuggestionsReturned(field_id_, autofill_item,
                                            kDefaultTriggerSource);
  EXPECT_THAT(
      open_args.suggestions,
      SuggestionVectorMainTextsAre(
          Suggestion::Text(std::u16string(), Suggestion::Text::IsPrimary(true)),
#if !BUILDFLAG(IS_ANDROID)
          Suggestion::Text(std::u16string(),
                           Suggestion::Text::IsPrimary(false)),
#endif
          Suggestion::Text(l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE),
                           Suggestion::Text::IsPrimary(true))));
  EXPECT_EQ(open_args.trigger_source, kDefaultTriggerSource);
}

// Test that browser autofill manager will handle the unmasking request for the
// virtual card after users accept the suggestion to use a virtual card.
TEST_F(AutofillExternalDelegateUnitTest, AcceptVirtualCardOptionItem) {
  FormData form;
  EXPECT_CALL(*browser_autofill_manager_,
              FillOrPreviewVirtualCardInformation(
                  mojom::AutofillActionPersistence::kFill, _, _, _, _));
  external_delegate_->DidAcceptSuggestion(
      Suggestion(PopupItemId::kVirtualCreditCardEntry), 0,
      kDefaultTriggerSource);
}

TEST_F(AutofillExternalDelegateUnitTest, SelectVirtualCardOptionItem) {
  EXPECT_CALL(*browser_autofill_manager_,
              FillOrPreviewVirtualCardInformation(
                  mojom::AutofillActionPersistence::kPreview, _, _, _, _));
  external_delegate_->DidSelectSuggestion(
      Suggestion(PopupItemId::kVirtualCreditCardEntry), kDefaultTriggerSource);
}

// Tests that the prompt to show account cards shows up when the corresponding
// bit is set, including any suggestions that are passed along and the "Manage"
// row in the footer.
TEST_F(AutofillExternalDelegateCardsFromAccountTest,
       ShouldShowCardsFromAccountOptionWithCards) {
  IssueOnQuery();

  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  std::vector<Suggestion> autofill_item;
  autofill_item.emplace_back();
  autofill_item[0].popup_item_id = PopupItemId::kAddressEntry;
  autofill_item[0].main_text.is_primary = Suggestion::Text::IsPrimary(true);

  external_delegate_->OnSuggestionsReturned(field_id_, autofill_item,
                                            kDefaultTriggerSource);
  EXPECT_THAT(
      open_args.suggestions,
      SuggestionVectorMainTextsAre(
          Suggestion::Text(std::u16string(), Suggestion::Text::IsPrimary(true)),
          Suggestion::Text(
              l10n_util::GetStringUTF16(IDS_AUTOFILL_SHOW_ACCOUNT_CARDS),
              Suggestion::Text::IsPrimary(true)),
#if !BUILDFLAG(IS_ANDROID)
          Suggestion::Text(std::u16string(),
                           Suggestion::Text::IsPrimary(false)),
#endif
          Suggestion::Text(l10n_util::GetStringUTF16(IDS_AUTOFILL_MANAGE),
                           Suggestion::Text::IsPrimary(true))));
  EXPECT_EQ(open_args.trigger_source, kDefaultTriggerSource);
}

// Tests that the prompt to show account cards shows up when the corresponding
// bit is set, even if no suggestions are passed along. The "Manage" row should
// *not* show up in this case.
TEST_F(AutofillExternalDelegateCardsFromAccountTest,
       ShouldShowCardsFromAccountOptionWithoutCards) {
  IssueOnQuery();

  AutofillClient::PopupOpenArgs open_args;
  EXPECT_CALL(autofill_client_, ShowAutofillPopup)
      .WillOnce(testing::SaveArg<0>(&open_args));

  external_delegate_->OnSuggestionsReturned(
      field_id_, std::vector<Suggestion>(), kDefaultTriggerSource);
  EXPECT_THAT(open_args.suggestions,
              SuggestionVectorMainTextsAre(Suggestion::Text(
                  l10n_util::GetStringUTF16(IDS_AUTOFILL_SHOW_ACCOUNT_CARDS),
                  Suggestion::Text::IsPrimary(true))));
  EXPECT_EQ(open_args.trigger_source, kDefaultTriggerSource);
}

#if BUILDFLAG(IS_IOS)
// Tests that outdated returned suggestions are discarded.
TEST_F(AutofillExternalDelegateCardsFromAccountTest,
       ShouldDiscardOutdatedSuggestions) {
  FieldGlobalId old_field_id = test::MakeFieldGlobalId();
  FieldGlobalId new_field_id = test::MakeFieldGlobalId();
  autofill_client_.set_last_queried_field(new_field_id);
  IssueOnQuery();
  EXPECT_CALL(autofill_client_, ShowAutofillPopup).Times(0);
  external_delegate_->OnSuggestionsReturned(
      old_field_id, std::vector<Suggestion>(), kDefaultTriggerSource);
}
#endif

}  // namespace autofill
