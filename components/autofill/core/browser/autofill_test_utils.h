// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_TEST_UTILS_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_TEST_UTILS_H_

#include <memory>
#include <string>
#include <vector>

#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_cloud_token_data.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/payments/card_unmask_challenge_option.h"
#include "components/autofill/core/browser/proto/api_v1.pb.h"
#include "components/autofill/core/browser/proto/server.pb.h"
#include "components/autofill/core/browser/ui/popup_item_ids.h"
#include "components/autofill/core/browser/ui/suggestion.h"
#include "components/autofill/core/common/autofill_test_utils.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}  // namespace user_prefs

namespace autofill {

class AutofillExternalDelegate;
class AutofillProfile;
class AutofillTable;
struct FormData;
struct FormFieldData;
struct FormDataPredictions;
struct FormFieldDataPredictions;

// Defined by pair-wise equality of all members.
bool operator==(const FormFieldDataPredictions& a,
                const FormFieldDataPredictions& b);

inline bool operator!=(const FormFieldDataPredictions& a,
                       const FormFieldDataPredictions& b) {
  return !(a == b);
}

// Holds iff the underlying FormDatas sans field values are equal and the
// remaining members are pairwise equal.
bool operator==(const FormDataPredictions& a, const FormDataPredictions& b);

inline bool operator!=(const FormDataPredictions& a,
                       const FormDataPredictions& b) {
  return !(a == b);
}

// Common utilities shared amongst Autofill tests.
namespace test {

// A compound data type that contains the type, the value and the verification
// status for a form group entry (an AutofillProfile).
struct FormGroupValue {
  ServerFieldType type;
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
std::unique_ptr<PrefService> PrefServiceForTesting();
std::unique_ptr<PrefService> PrefServiceForTesting(
    user_prefs::PrefRegistrySyncable* registry);

// Populates `form` with data corresponding to a simple address form.
// Note that this actually appends fields to the form data, which can be useful
// for building up more complex test forms. Another version of the function is
// provided in case the caller wants the vector of expected field `types`. Use
// `unique_id` optionally ensure that each form has its own signature.
[[nodiscard]] FormData CreateTestAddressFormData(
    const char* unique_id = nullptr);
void CreateTestAddressFormData(FormData* form, const char* unique_id = nullptr);
void CreateTestAddressFormData(FormData* form,
                               std::vector<ServerFieldTypeSet>* types,
                               const char* unique_id = nullptr);

// Returns a full profile with valid info according to rules for Canada.
AutofillProfile GetFullValidProfileForCanada();

// Returns a profile full of dummy info.
AutofillProfile GetFullProfile();

// Returns a profile full of dummy info, different to the above.
AutofillProfile GetFullProfile2();

// Returns a profile full of dummy info, different to the above.
AutofillProfile GetFullCanadianProfile();

// Returns an incomplete profile of dummy info.
AutofillProfile GetIncompleteProfile1();

// Returns an incomplete profile of dummy info, different to the above.
AutofillProfile GetIncompleteProfile2();

// Returns a server profile full of dummy info.
AutofillProfile GetServerProfile();

// Returns a server profile full of dummy info, different to the above.
AutofillProfile GetServerProfile2();

// Sets the `profile`s source and initial creator to match `category`.
void SetProfileCategory(
    AutofillProfile& profile,
    autofill_metrics::AutofillProfileSourceCategory category);

// Returns the stripped (without characters representing whitespace) value of
// the given `value`.
std::string GetStrippedValue(const char* value);

// Returns an IBAN full of dummy info.
Iban GetIban();

// Returns an IBAN full of dummy info, different to the above.
Iban GetIban2();

// Returns an IBAN full of dummy info, different to the above and without
// nickname.
Iban GetIbanWithoutNickname();

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
CreditCard GetMaskedServerCardWithCvc();
CreditCard GetMaskedServerCardWithNonLegacyId();
CreditCard GetMaskedServerCardWithLegacyId();
CreditCard GetMaskedServerCardVisa();
CreditCard GetMaskedServerCardAmex();
CreditCard GetMaskedServerCardWithNickname();
CreditCard GetMaskedServerCardEnrolledIntoVirtualCardNumber();

// Returns a full server card full of dummy info.
CreditCard GetFullServerCard();

// Returns a virtual card full of dummy info.
CreditCard GetVirtualCard();

// Returns a randomly generated credit card of |record_type|. Note that the
// card is not guaranteed to be valid/sane from a card validation standpoint.
CreditCard GetRandomCreditCard(CreditCard::RecordType record_Type);

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

// A unit testing utility that is common to a number of the Autofill unit
// tests.  |SetProfileInfo| provides a quick way to populate a profile with
// c-strings.
void SetProfileInfo(AutofillProfile* profile,
                    const char* first_name,
                    const char* middle_name,
                    const char* last_name,
                    const char* email,
                    const char* company,
                    const char* address1,
                    const char* address2,
                    const char* dependent_locality,
                    const char* city,
                    const char* state,
                    const char* zipcode,
                    const char* country,
                    const char* phone,
                    bool finalize = true,
                    VerificationStatus status = VerificationStatus::kObserved);

// This one doesn't require the |dependent_locality|.
void SetProfileInfo(AutofillProfile* profile,
                    const char* first_name,
                    const char* middle_name,
                    const char* last_name,
                    const char* email,
                    const char* company,
                    const char* address1,
                    const char* address2,
                    const char* city,
                    const char* state,
                    const char* zipcode,
                    const char* country,
                    const char* phone,
                    bool finalize = true,
                    VerificationStatus status = VerificationStatus::kObserved);

void SetProfileInfoWithGuid(AutofillProfile* profile,
                            const char* guid,
                            const char* first_name,
                            const char* middle_name,
                            const char* last_name,
                            const char* email,
                            const char* company,
                            const char* address1,
                            const char* address2,
                            const char* city,
                            const char* state,
                            const char* zipcode,
                            const char* country,
                            const char* phone,
                            bool finalize = true,
                            VerificationStatus = VerificationStatus::kObserved);

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

// TODO(isherman): We should do this automatically for all tests, not manually
// on a per-test basis: http://crbug.com/57221
// Disables or mocks out code that would otherwise reach out to system services.
// Revert this configuration with |ReenableSystemServices|.
void DisableSystemServices(PrefService* prefs);

// Undoes the mocking set up by |DisableSystemServices|
void ReenableSystemServices();

// Sets |cards| for |table|. |cards| may contain full, unmasked server cards,
// whereas AutofillTable::SetServerCreditCards can only contain masked cards.
void SetServerCreditCards(AutofillTable* table,
                          const std::vector<CreditCard>& cards);

// Adds an element at the end of |possible_field_types| and
// |possible_field_types_validities| given |possible_type| and their
// corresponding |validity_state|.
void InitializePossibleTypesAndValidities(
    std::vector<ServerFieldTypeSet>& possible_field_types,
    std::vector<ServerFieldTypeValidityStatesMap>&
        possible_field_types_validities,
    const std::vector<ServerFieldType>& possible_type,
    const std::vector<AutofillDataModel::ValidityState>& validity_state = {});

// Fills the upload |field| with the information passed by parameter. If the
// value of a const char* parameter is NULL, the corresponding attribute won't
// be set at all, as opposed to being set to empty string.
void FillUploadField(AutofillUploadContents::Field* field,
                     unsigned signature,
                     const char* name,
                     const char* control_type,
                     const char* autocomplete,
                     unsigned autofill_type,
                     unsigned validity_state = 0);

void FillUploadField(AutofillUploadContents::Field* field,
                     unsigned signature,
                     const char* name,
                     const char* control_type,
                     const char* autocomplete,
                     const std::vector<unsigned>& autofill_type,
                     const std::vector<unsigned>& validity_state = {});

void FillUploadField(AutofillUploadContents::Field* field,
                     unsigned signature,
                     const char* name,
                     const char* control_type,
                     const char* autocomplete,
                     unsigned autofill_type,
                     const std::vector<unsigned>& validity_states);

// Creates the structure of signatures that would be encoded by
// FormStructure::EncodeUploadRequest() and FormStructure::EncodeQueryRequest()
// and consumed by FormStructure::ParseQueryResponse() and
// FormStructure::ParseApiQueryResponse().
//
// Perhaps a neater way would be to move this to TestFormStructure.
std::vector<FormSignature> GetEncodedSignatures(const FormStructure& form);
std::vector<FormSignature> GetEncodedSignatures(
    const std::vector<FormStructure*>& forms);

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
::autofill::AutofillQueryResponse::FormSuggestion::FieldSuggestion::
    FieldPrediction
    CreateFieldPrediction(ServerFieldType type,
                          ::autofill::AutofillQueryResponse::FormSuggestion::
                              FieldSuggestion::FieldPrediction::Source source);

// Creates a `FieldPrediction` instance, with a plausible value for `source()`.
::autofill::AutofillQueryResponse::FormSuggestion::FieldSuggestion::
    FieldPrediction
    CreateFieldPrediction(ServerFieldType type, bool is_override = false);

void AddFieldPredictionToForm(
    const autofill::FormFieldData& field_data,
    ServerFieldType field_type,
    ::autofill::AutofillQueryResponse_FormSuggestion* form_suggestion,
    bool is_override = false);

void AddFieldPredictionsToForm(
    const autofill::FormFieldData& field_data,
    const std::vector<ServerFieldType>& field_types,
    ::autofill::AutofillQueryResponse_FormSuggestion* form_suggestion);

void AddFieldPredictionsToForm(
    const autofill::FormFieldData& field_data,
    const std::vector<::autofill::AutofillQueryResponse::FormSuggestion::
                          FieldSuggestion::FieldPrediction>& field_predictions,
    ::autofill::AutofillQueryResponse_FormSuggestion* form_suggestion);

Suggestion CreateAutofillSuggestion(
    PopupItemId popup_item_id,
    const std::u16string& main_text_value = std::u16string(),
    const Suggestion::Payload& payload = Suggestion::Payload());

}  // namespace test
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_AUTOFILL_TEST_UTILS_H_
