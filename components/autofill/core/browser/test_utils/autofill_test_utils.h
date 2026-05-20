// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_UTILS_AUTOFILL_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_UTILS_AUTOFILL_TEST_UTILS_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/payments/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/payments/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/data_model/payments/credit_card_benefit.h"
#include "components/autofill/core/browser/data_model/payments/credit_card_cloud_token_data.h"
#include "components/autofill/core/browser/data_model/payments/ewallet.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/browser/suggestions/suggestion.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/browser/test_utils/autofill_testing_pref_service.h"
#include "components/autofill/core/common/autofill_test_utils.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync/protocol/autofill_specifics.pb.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace autofill {

class AutofillExternalDelegate;
class AutofillProfile;
class BankAccount;
class FormData;
class FormFieldData;
struct FormDataPredictions;
struct FormFieldDataPredictions;
class PaymentsAutofillTable;
class TestPersonalDataManager;

// Defined by pair-wise equality of all members.
bool operator==(const FormFieldDataPredictions& a,
                const FormFieldDataPredictions& b);

// Holds iff the underlying FormDatas sans field values are equal and the
// remaining members are pairwise equal.
bool operator==(const FormDataPredictions& a, const FormDataPredictions& b);

// Common utilities shared amongst Autofill tests.
namespace test {

inline constexpr base::Time kJanuary2017 =
    base::Time::FromSecondsSinceUnixEpoch(1484505871);
inline constexpr base::Time kJune2017 =
    base::Time::FromSecondsSinceUnixEpoch(1497552271);

// A compound data type that contains the type, the value and the verification
// status for a form group entry (an AutofillProfile).
struct FormGroupValue {
  FieldType type;
  std::string value;
  VerificationStatus verification_status = VerificationStatus::kNoStatus;
};

// Convenience declaration for multiple FormGroup values.
using FormGroupValues = std::vector<FormGroupValue>;

// Helper function to set values and verification statuses to a form group.
void SetFormGroupValues(FormGroup& form_group,
                        const std::vector<FormGroupValue>& values);

// Helper function to verify the expectation of values and verification
// statuses in a form group. If |ignore_status| is set, status checking is
// omitted.
void VerifyFormGroupValues(const FormGroup& form_group,
                           const std::vector<FormGroupValue>& values,
                           bool ignore_status = false);

inline constexpr char kEmptyOrigin[] = "";

// The following methods return a PrefService that can be used for
// Autofill-related testing in contexts where the PrefService would otherwise
// have to be constructed manually (e.g., in unit tests within Autofill core
// code). The returned PrefService has had Autofill preferences registered on
// its associated registry.
std::unique_ptr<AutofillTestingPrefService> PrefServiceForTesting();
std::unique_ptr<PrefService> PrefServiceForTesting(
    user_prefs::PrefRegistrySyncable* registry);

// Returns a `FormData` corresponding to a simple address form. Use `unique_id`
// to ensure that the form has its own signature.
[[nodiscard]] FormData CreateTestAddressFormData(
    std::string_view unique_id = "");

// Returns a `FormData` corresponding to a simple one-time-password form.
[[nodiscard]] FormData CreateTestOtpFormData(const char* unique_id = nullptr);

// Returns a `FormData` corresponding to a simple sign-up form that also
// accepts a passkey.
[[nodiscard]] FormData CreateTestHybridSignUpFormData(
    const char* unique_id = nullptr);

// Returns a full profile with valid info according to rules for Canada.
AutofillProfile GetFullValidProfileForCanada();

// Returns a profile full of dummy info.
AutofillProfile GetFullProfile(
    AddressCountryCode country_code = AddressCountryCode("US"));

// Returns a profile full of dummy info, different to the above.
AutofillProfile GetFullProfile2(
    AddressCountryCode country_code = AddressCountryCode("US"));

// Returns a profile full of dummy info, different to the above.
AutofillProfile GetFullCanadianProfile();

// Returns an incomplete profile of dummy info.
AutofillProfile GetIncompleteProfile1();

// Returns an incomplete profile of dummy info, different to the above.
AutofillProfile GetIncompleteProfile2();

// Sets the `profile`s record type and initial creator to match `category`.
void SetProfileCategory(
    AutofillProfile& profile,
    autofill_metrics::AutofillProfileRecordTypeCategory category);

// Returns the stripped (without characters representing whitespace) value of
// the given `value`.
std::string GetStrippedValue(const char* value);

// Returns a local IBAN full of dummy info.
Iban GetLocalIban();

// Returns a local IBAN full of dummy info, different from the above.
Iban GetLocalIban2();

// Returns server-based IBANs full of dummy info.
Iban GetServerIban();
Iban GetServerIban2();
Iban GetServerIban3();

// Returns a credit card full of dummy info.
CreditCard GetCreditCard();

// Returns a credit card full of dummy info, different to the above.
CreditCard GetCreditCard2();

// Returns an expired credit card full of fake info.
CreditCard GetExpiredCreditCard();

// Returns an incomplete credit card full of fake info with card holder's name
// missing.
CreditCard GetIncompleteCreditCard();

// Returns a masked server card full of dummy info.
CreditCard GetMaskedServerCard();
CreditCard GetMaskedServerCard2();
CreditCard GetMaskedServerCardWithNonLegacyId();
CreditCard GetMaskedServerCardWithLegacyId();
CreditCard GetMaskedServerCardVisa();
CreditCard GetMaskedServerCardAmex();
CreditCard GetMaskedServerCardWithNickname();
CreditCard GetMaskedServerCardEnrolledIntoVirtualCardNumber();
CreditCard GetMaskedServerCardEnrolledIntoRuntimeRetrieval();

// Returns a full server card full of dummy info.
CreditCard GetFullServerCard();

// Returns a virtual card full of dummy info.
CreditCard GetVirtualCard();

// Returns a randomly generated credit card of |record_type|. Note that the
// card is not guaranteed to be valid/sane from a card validation standpoint.
CreditCard GetRandomCreditCard(CreditCard::RecordType record_type);

// Returns a copy of `credit_card` with `cvc` set as specified.
CreditCard WithCvc(CreditCard credit_card, std::u16string cvc = u"123");

// Returns a `credit_card` with its record type set to full server card.
CreditCard AsFullServerCard(CreditCard credit_card);

// Returns a `credit_card` with its record type set to virtual card.
CreditCard AsVirtualCard(CreditCard credit_card);

// Returns a credit card cloud token data full of dummy info.
CreditCardCloudTokenData GetCreditCardCloudTokenData1();

// Returns a credit card cloud token data full of dummy info, different from the
// one above.
CreditCardCloudTokenData GetCreditCardCloudTokenData2();

// Returns an Autofill card-linked offer data full of dummy info. Use
// |offer_id| to optionally set the offer id.
AutofillOfferData GetCardLinkedOfferData1(int64_t offer_id = 111);

// Returns an Autofill card-linked offer data full of dummy info, different from
// the one above. Use |offer_id| to optionally set the offer id.
AutofillOfferData GetCardLinkedOfferData2(int64_t offer_id = 222);

// Returns an Autofill promo code offer data full of dummy info, using |origin|
// if provided and expired if |is_expired| is true. Use |offer_id| to optionally
// set the offer id.
AutofillOfferData GetPromoCodeOfferData(
    GURL origin = GURL("http://www.example.com"),
    bool is_expired = false,
    int64_t offer_id = 333);

// Return an Usage Data with dummy info specifically for a Virtual Card.
VirtualCardUsageData GetVirtualCardUsageData1();
VirtualCardUsageData GetVirtualCardUsageData2();

// For each type in `types`, this function creates a challenge option with dummy
// info that has the specific type.
std::vector<CardUnmaskChallengeOption> GetCardUnmaskChallengeOptions(
    const std::vector<CardUnmaskChallengeOptionType>& types);

// Each Get returns an active CreditCardBenefit with dummy info.
// One getter for each benefit type.
CreditCardFlatRateBenefit GetActiveCreditCardFlatRateBenefit();
CreditCardCategoryBenefit GetActiveCreditCardCategoryBenefit();
CreditCardCategoryBenefit CreateCreditCardCategoryBenefit(
    CreditCardBenefitBase::BenefitId benefit_id,
    CreditCardBenefitBase::LinkedCardInstrumentId linked_card_instrument_id,
    CreditCardCategoryBenefit::BenefitCategory benefit_category,
    std::u16string benefit_description);
CreditCardMerchantBenefit GetActiveCreditCardMerchantBenefit();

// Returns a set of merchant origin webpages used for a merchant credit card
// benefit.
base::flat_set<url::Origin> GetOriginsForMerchantBenefit();

// Prevents kAccountNameEmail profile from being created.
void HideAccountNameEmailProfile(PrefService* pref_service,
                                 const AccountInfo& info);

// Adds `card` with a set `issuer_id`, `benefit` and `benefit_source` to
// `personal_data`. Also configures a category benefit with the
// `optimization_guide`.
void SetUpCreditCardAndBenefitData(
    CreditCard& card,
    const std::string& issuer_id,
    const CreditCardBenefit& benefit,
    const std::string& benefit_source,
    TestPersonalDataManager& personal_data,
    AutofillOptimizationGuideDecider* optimization_guide);

struct SetProfileInfoOptions {
  SetProfileInfoOptions();
  SetProfileInfoOptions(const SetProfileInfoOptions&);
  SetProfileInfoOptions(SetProfileInfoOptions&&);
  SetProfileInfoOptions& operator=(const SetProfileInfoOptions&);
  SetProfileInfoOptions& operator=(SetProfileInfoOptions&&);
  ~SetProfileInfoOptions();

  std::string guid;
  std::string first_name;
  std::string middle_name;
  std::string last_name;
  std::string full_name;
  std::string email;
  std::string company;
  std::string address1;
  std::string address2;
  std::string dependent_locality;
  std::string city;
  std::string state;
  std::string zipcode;
  std::string country;
  std::string phone;
  VerificationStatus status = VerificationStatus::kObserved;
};

class SetProfileInfoOptionsBuilder {
 public:
  SetProfileInfoOptionsBuilder();
  SetProfileInfoOptionsBuilder(const SetProfileInfoOptionsBuilder&);
  SetProfileInfoOptionsBuilder& operator=(const SetProfileInfoOptionsBuilder&);
  ~SetProfileInfoOptionsBuilder();

  SetProfileInfoOptionsBuilder& with_guid(std::string_view guid);
  SetProfileInfoOptionsBuilder& with_first_name(std::string_view first_name);
  SetProfileInfoOptionsBuilder& with_middle_name(std::string_view middle_name);
  SetProfileInfoOptionsBuilder& with_last_name(std::string_view last_name);
  SetProfileInfoOptionsBuilder& with_full_name(std::string_view full_name);
  SetProfileInfoOptionsBuilder& with_email(std::string_view email);
  SetProfileInfoOptionsBuilder& with_company(std::string_view company);
  SetProfileInfoOptionsBuilder& with_address1(std::string_view address1);
  SetProfileInfoOptionsBuilder& with_address2(std::string_view address2);
  SetProfileInfoOptionsBuilder& with_dependent_locality(
      std::string_view dependent_locality);
  SetProfileInfoOptionsBuilder& with_city(std::string_view city);
  SetProfileInfoOptionsBuilder& with_state(std::string_view state);
  SetProfileInfoOptionsBuilder& with_zipcode(std::string_view zipcode);
  SetProfileInfoOptionsBuilder& with_country(std::string_view country);
  SetProfileInfoOptionsBuilder& with_phone(std::string_view phone);
  SetProfileInfoOptionsBuilder& with_status(VerificationStatus status);

  [[nodiscard]] SetProfileInfoOptions Build();

 private:
  SetProfileInfoOptions options_;
};

// A unit testing utility that is common to a number of the Autofill unit
// tests.  |SetProfileInfo| provides a quick way to populate a profile.
void SetProfileInfo(AutofillProfile* profile,
                    SetProfileInfoOptions options,
                    bool finalize = true);

// A unit testing utility that is common to a number of the Autofill unit
// tests.  |SetCreditCardInfo| provides a quick way to populate a credit card
// with c-strings.
void SetCreditCardInfo(CreditCard* credit_card,
                       const char* name_on_card,
                       const char* card_number,
                       const char* expiration_month,
                       const char* expiration_year,
                       const std::string& billing_address_id,
                       const std::u16string& cvc = u"");

// Same as SetCreditCardInfo() but returns CreditCard object.
CreditCard CreateCreditCardWithInfo(const char* name_on_card,
                                    const char* card_number,
                                    const char* expiration_month,
                                    const char* expiration_year,
                                    const std::string& billing_address_id,
                                    const std::u16string& cvc = u"");

// Sets |cards| for |table|. |cards| may contain full, unmasked server cards,
// whereas PaymentsAutofillTable::SetServerCreditCards can only contain masked
// cards.
void SetServerCreditCards(PaymentsAutofillTable* table,
                          const std::vector<CreditCard>& cards);

// Adds `possible_types` at the end of `possible_field_types`.
void InitializePossibleTypes(std::vector<FieldTypeSet>& possible_field_types,
                             const std::vector<FieldType>& possible_types);

// Fills the upload |field| with the information passed by parameter.
void FillUploadField(AutofillUploadContents::Field* field,
                     unsigned signature,
                     unsigned autofill_type);

void FillUploadField(AutofillUploadContents::Field* field,
                     unsigned signature,
                     const std::vector<unsigned>& autofill_type);

// Creates the structure of signatures that would be encoded by
// `EncodeUploadRequest()` and `EncodeAutofillPageQueryRequest()`
// and consumed by `ParseServerPredictionsFromQueryResponse()`.
//
// Perhaps a neater way would be to move this to TestFormStructure.
std::vector<FormSignature> GetEncodedSignatures(const FormStructure& form);
std::vector<FormSignature> GetEncodedSignatures(
    const std::vector<raw_ref<FormStructure>>& forms);
std::vector<FormSignature> GetEncodedSignatures(
    base::span<const FormData> forms);

std::vector<FormSignature> GetEncodedAlternativeSignatures(
    const FormStructure& form);
std::vector<FormSignature> GetEncodedAlternativeSignatures(
    const std::vector<raw_ref<FormStructure>>& forms);

// Calls the required functions on the given external delegate to cause the
// delegate to display a popup.
void GenerateTestAutofillPopup(
    AutofillExternalDelegate* autofill_external_delegate);

std::string ObfuscatedCardDigitsAsUTF8(const std::string& str,
                                       int obfuscation_length);

// Returns 2-digit month string, like "02", "10".
std::string NextMonth();
std::string LastYear();
std::string NextYear();
std::string TenYearsFromNow();

// Creates a `FieldPrediction` instance.
AutofillQueryResponse::FormSuggestion::FieldSuggestion::FieldPrediction
CreateFieldPrediction(FieldType type,
                      AutofillQueryResponse::FormSuggestion::FieldSuggestion::
                          FieldPrediction::Source source);

// Creates a `FieldPrediction` instance, with a plausible value for `source()`.
AutofillQueryResponse::FormSuggestion::FieldSuggestion::FieldPrediction
CreateFieldPrediction(FieldType type, bool is_override = false);

void AddFieldPredictionToForm(
    const FormFieldData& field_data,
    FieldType field_type,
    AutofillQueryResponse_FormSuggestion* form_suggestion,
    bool is_override = false);

void AddFieldPredictionsToForm(
    const FormFieldData& field_data,
    const std::vector<FieldType>& field_types,
    AutofillQueryResponse_FormSuggestion* form_suggestion);

void AddFieldPredictionsToForm(
    const FormFieldData& field_data,
    const std::vector<AutofillQueryResponse::FormSuggestion::FieldSuggestion::
                          FieldPrediction>& field_predictions,
    AutofillQueryResponse_FormSuggestion* form_suggestion);

Suggestion CreateAutofillSuggestion(
    SuggestionType type,
    const std::u16string& main_text_value = std::u16string(),
    const Suggestion::Payload& payload = Suggestion::Payload());

Suggestion CreateAutofillSuggestion(SuggestionType type,
                                    const std::u16string& main_text_value,
                                    const std::u16string& minor_text_value,
                                    bool has_deactivated_style);

// Returns a bank account enabled for Pix with fake data.
BankAccount CreatePixBankAccount(int64_t instrument_id);

// Returns an eWallet account with fake data.
Ewallet CreateEwalletAccount(int64_t instrument_id);

// Returns a payment instrument with a bank account filled with fake data.
sync_pb::PaymentInstrument CreatePaymentInstrumentWithBankAccount(
    int64_t instrument_id);

// Returns a payment instrument with an IBAN filled with fake data.
sync_pb::PaymentInstrument CreatePaymentInstrumentWithIban(
    int64_t instrument_id);

// Returns a payment instrument with an eWallet account filled with fake data.
sync_pb::PaymentInstrument CreatePaymentInstrumentWithEwalletAccount(
    int64_t instrument_id);

// Returns a payment instrument with a linked BNPL issuer based on the data
// provided.
sync_pb::PaymentInstrument CreatePaymentInstrumentWithLinkedBnplIssuer(
    int64_t instrument_id,
    std::string issuer_id,
    std::string currency,
    uint64_t min_price_in_micros,
    uint64_t max_price_in_micros,
    std::vector<sync_pb::PaymentInstrument_ActionRequired> actions_required =
        {});

// Returns a linked BNPL issuer with fake data.
BnplIssuer GetTestLinkedBnplIssuer(
    autofill::BnplIssuer::IssuerId issuer_id =
        autofill::BnplIssuer::IssuerId::kBnplAffirm,
    DenseSet<PaymentInstrument::ActionRequired> actions_required =
        DenseSet<PaymentInstrument::ActionRequired>());

// Returns an unlinked BNPL issuer with fake data.
BnplIssuer GetTestUnlinkedBnplIssuer();

// Returns a payment instrument creation option with a BNPL issuer filled with
// fake data using `id` as the `PaymentInstrumentCreationOption.id`.
sync_pb::PaymentInstrumentCreationOption
CreatePaymentInstrumentCreationOptionWithBnplIssuer(const std::string& id);

// For the key metrics as used for different data types, this struct allows to
// define expectations. The values are marked optional. `std::nullopt` means
// that no value was recorded to the histogram.
struct SingleSubmissionKeyMetricExpectations {
  std::optional<bool> readiness;
  std::optional<bool> acceptance;
  std::optional<bool> assistance;
  std::optional<bool> correctness;
};

void VerifySingleSubmissionKeyMetricExpectations(
    const base::HistogramTester& histogram_tester,
    std::string_view form_type_name,
    const SingleSubmissionKeyMetricExpectations& expectations);

}  // namespace test
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_TEST_UTILS_AUTOFILL_TEST_UTILS_H_
