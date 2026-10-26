// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/autofill_data_provider.h"

#include <string_view>
#include <vector>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager.h"
#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager_test_api.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/field_filling_util.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_data_type_util.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_search_result.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator_util.h"
#include "components/autofill/core/browser/test_utils/autofill_test_util.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_util.h"
#include "components/autofill/core/browser/test_utils/valuables_data_test_util.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/personal_context/proto/features/at_memory.pb.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::UnorderedElementsAre;

namespace {

Matcher<EntryMetadata> IsMetadata(MemoryDataType type,
                                  const std::u16string& value) {
  return AllOf(
      Field(&EntryMetadata::type, Eq(type)),
      Field(&EntryMetadata::type_name, Eq(GetMemoryDataTypeNameForI18n(type))),
      Field(&EntryMetadata::value, Eq(value)));
}

Matcher<MemorySearchResult> IsMemorySearchResult(
    const std::u16string& value,
    const std::u16string& type_name,
    Matcher<std::vector<EntryMetadata>> metadata_matcher,
    bool is_obfuscated = false,
    std::variant<std::monostate, std::string, int64_t> identifier =
        std::monostate()) {
  return AllOf(Field(&MemorySearchResult::value, Eq(value)),
               Field(&MemorySearchResult::type_name, Eq(type_name)),
               Field(&MemorySearchResult::is_obfuscated, Eq(is_obfuscated)),
               Field(&MemorySearchResult::metadata_list, metadata_matcher),
               Field(&MemorySearchResult::identifier, Eq(identifier)));
}

std::vector<MemorySearchResult> RetrieveAllHelper(
    AutofillDataProvider& retriever,
    MemoryDataType type) {
  base::test::TestFuture<std::vector<MemorySearchResult>> future;
  retriever.RetrieveAll({type}, future.GetCallback());
  return future.Take();
}

std::vector<MemorySearchResult> RetrieveAllByContentHelper(
    AutofillDataProvider& retriever,
    std::u16string_view query) {
  base::test::TestFuture<std::vector<MemorySearchResult>> future;
  retriever.RetrieveAllByContent(query, future.GetCallback());
  return future.Take();
}

class AutofillDataProviderTest : public testing::Test {
 public:
  AutofillDataProviderTest()
      : webdata_helper_(std::make_unique<EntityTable>()) {
    client_.SetAutofillProfileEnabled(true);
    client_.GetPersonalDataManager()
        .test_payments_data_manager()
        .SetAutofillPaymentMethodsEnabled(true);

    auto entity_data_manager = std::make_unique<EntityDataManager>(
        client_.GetPrefs(),
        /*identity_manager=*/nullptr, &sync_service_,
        webdata_helper_.autofill_webdata_service(),
        /*history_service=*/nullptr,
        /*pcontext_manager=*/nullptr,
        /*strike_database=*/nullptr,
        /*variation_country_code=*/GeoIpCountryCode("US"));
    entity_data_manager_ = entity_data_manager.get();
    client_.set_entity_data_manager(std::move(entity_data_manager));

    retriever_ = std::make_unique<AutofillDataProvider>(
        &client_.GetPersonalDataManager(), client_.GetEntityDataManager(),
        client_.GetValuablesDataManager());
  }

  void WaitForDatabase() { webdata_helper_.WaitUntilIdle(); }

  AutofillDataProvider& retriever() { return *retriever_; }
  TestAutofillClient& client() { return client_; }
  EntityDataManager& entity_data_manager() { return *entity_data_manager_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  AutofillWebDataServiceTestHelper webdata_helper_;
  syncer::TestSyncService sync_service_;
  TestAutofillClient client_;
  raw_ptr<EntityDataManager> entity_data_manager_;
  std::unique_ptr<AutofillDataProvider> retriever_;
};

// Tests that RetrieveAll returns an empty list when no data is available
TEST_F(AutofillDataProviderTest, RetrieveAll_Empty) {
  EXPECT_THAT(RetrieveAllHelper(retriever(), MemoryDataType::kAddressCity),
              IsEmpty());
}

// Tests that RetrieveAll fetches and formats address-related data from
// PersonalDataManager.
TEST_F(AutofillDataProviderTest, RetrieveAll_AddressData) {
  AutofillProfile profile = test::GetFullProfile();
  profile.set_guid(test::MakeGuid(1));
  client().GetPersonalDataManager().address_data_manager().AddProfile(profile);

  EXPECT_THAT(
      RetrieveAllHelper(retriever(), MemoryDataType::kAddressCity),
      UnorderedElementsAre(IsMemorySearchResult(
          u"Elysium", u"City",
          UnorderedElementsAre(
              IsMetadata(MemoryDataType::kNameFull, u"John H. Doe"),
              IsMetadata(MemoryDataType::kAddressStreetAddress,
                         u"666 Erebus St.\nApt 8"),
              IsMetadata(MemoryDataType::kAddressState, u"CA"),
              IsMetadata(MemoryDataType::kAddressZip, u"91111"),
              IsMetadata(MemoryDataType::kAddressCountry, u"United States"),
              IsMetadata(MemoryDataType::kCompanyName, u"Underworld"),
              IsMetadata(MemoryDataType::kEmail, u"johndoe@hades.com"),
              IsMetadata(MemoryDataType::kPhone, u"16502111111")),
          /*is_obfuscated=*/false, test::MakeGuid(1))));

  EXPECT_THAT(
      RetrieveAllHelper(retriever(), MemoryDataType::kAddressZip),
      UnorderedElementsAre(IsMemorySearchResult(
          u"91111", u"Zip",
          UnorderedElementsAre(
              IsMetadata(MemoryDataType::kNameFull, u"John H. Doe"),
              IsMetadata(MemoryDataType::kAddressStreetAddress,
                         u"666 Erebus St.\nApt 8"),
              IsMetadata(MemoryDataType::kAddressCity, u"Elysium"),
              IsMetadata(MemoryDataType::kAddressState, u"CA"),
              IsMetadata(MemoryDataType::kAddressCountry, u"United States"),
              IsMetadata(MemoryDataType::kCompanyName, u"Underworld"),
              IsMetadata(MemoryDataType::kEmail, u"johndoe@hades.com"),
              IsMetadata(MemoryDataType::kPhone, u"16502111111")),
          /*is_obfuscated=*/false, test::MakeGuid(1))));

  EXPECT_THAT(
      RetrieveAllHelper(retriever(), MemoryDataType::kAddressState),
      UnorderedElementsAre(IsMemorySearchResult(
          u"CA", u"State",
          UnorderedElementsAre(
              IsMetadata(MemoryDataType::kNameFull, u"John H. Doe"),
              IsMetadata(MemoryDataType::kAddressStreetAddress,
                         u"666 Erebus St.\nApt 8"),
              IsMetadata(MemoryDataType::kAddressCity, u"Elysium"),
              IsMetadata(MemoryDataType::kAddressZip, u"91111"),
              IsMetadata(MemoryDataType::kAddressCountry, u"United States"),
              IsMetadata(MemoryDataType::kCompanyName, u"Underworld"),
              IsMetadata(MemoryDataType::kEmail, u"johndoe@hades.com"),
              IsMetadata(MemoryDataType::kPhone, u"16502111111")),
          /*is_obfuscated=*/false, test::MakeGuid(1))));

  EXPECT_THAT(RetrieveAllHelper(retriever(), MemoryDataType::kAddressCountry),
              UnorderedElementsAre(IsMemorySearchResult(
                  u"United States", u"Country",
                  UnorderedElementsAre(
                      IsMetadata(MemoryDataType::kNameFull, u"John H. Doe"),
                      IsMetadata(MemoryDataType::kAddressStreetAddress,
                                 u"666 Erebus St.\nApt 8"),
                      IsMetadata(MemoryDataType::kAddressCity, u"Elysium"),
                      IsMetadata(MemoryDataType::kAddressState, u"CA"),
                      IsMetadata(MemoryDataType::kAddressZip, u"91111"),
                      IsMetadata(MemoryDataType::kCompanyName, u"Underworld"),
                      IsMetadata(MemoryDataType::kEmail, u"johndoe@hades.com"),
                      IsMetadata(MemoryDataType::kPhone, u"16502111111")),
                  /*is_obfuscated=*/false, test::MakeGuid(1))));

  EXPECT_THAT(
      RetrieveAllHelper(retriever(), MemoryDataType::kNameFull),
      UnorderedElementsAre(IsMemorySearchResult(
          u"John H. Doe", u"Name",
          UnorderedElementsAre(
              IsMetadata(MemoryDataType::kAddressStreetAddress,
                         u"666 Erebus St.\nApt 8"),
              IsMetadata(MemoryDataType::kAddressCity, u"Elysium"),
              IsMetadata(MemoryDataType::kAddressState, u"CA"),
              IsMetadata(MemoryDataType::kAddressZip, u"91111"),
              IsMetadata(MemoryDataType::kAddressCountry, u"United States"),
              IsMetadata(MemoryDataType::kCompanyName, u"Underworld"),
              IsMetadata(MemoryDataType::kEmail, u"johndoe@hades.com"),
              IsMetadata(MemoryDataType::kPhone, u"16502111111")),
          /*is_obfuscated=*/false, test::MakeGuid(1))));

  EXPECT_THAT(
      RetrieveAllHelper(retriever(), MemoryDataType::kEmail),
      UnorderedElementsAre(IsMemorySearchResult(
          u"johndoe@hades.com", u"Email",
          UnorderedElementsAre(
              IsMetadata(MemoryDataType::kNameFull, u"John H. Doe"),
              IsMetadata(MemoryDataType::kAddressStreetAddress,
                         u"666 Erebus St.\nApt 8"),
              IsMetadata(MemoryDataType::kAddressCity, u"Elysium"),
              IsMetadata(MemoryDataType::kAddressState, u"CA"),
              IsMetadata(MemoryDataType::kAddressZip, u"91111"),
              IsMetadata(MemoryDataType::kAddressCountry, u"United States"),
              IsMetadata(MemoryDataType::kCompanyName, u"Underworld"),
              IsMetadata(MemoryDataType::kPhone, u"16502111111")),
          /*is_obfuscated=*/false, test::MakeGuid(1))));

  EXPECT_THAT(
      RetrieveAllHelper(retriever(), MemoryDataType::kPhone),
      UnorderedElementsAre(IsMemorySearchResult(
          u"16502111111", u"Phone",
          UnorderedElementsAre(
              IsMetadata(MemoryDataType::kNameFull, u"John H. Doe"),
              IsMetadata(MemoryDataType::kAddressStreetAddress,
                         u"666 Erebus St.\nApt 8"),
              IsMetadata(MemoryDataType::kAddressCity, u"Elysium"),
              IsMetadata(MemoryDataType::kAddressState, u"CA"),
              IsMetadata(MemoryDataType::kAddressZip, u"91111"),
              IsMetadata(MemoryDataType::kAddressCountry, u"United States"),
              IsMetadata(MemoryDataType::kCompanyName, u"Underworld"),
              IsMetadata(MemoryDataType::kEmail, u"johndoe@hades.com")),
          /*is_obfuscated=*/false, test::MakeGuid(1))));

  // Requesting for address should return only the full address.
  EXPECT_THAT(
      RetrieveAllHelper(retriever(), MemoryDataType::kAddressFull),
      UnorderedElementsAre(IsMemorySearchResult(
          u"Underworld, 666 Erebus St., Apt 8, Elysium, CA 91111, "
          u"United States",
          u"Address",
          UnorderedElementsAre(
              IsMetadata(MemoryDataType::kNameFull, u"John H. Doe"),
              IsMetadata(MemoryDataType::kAddressStreetAddress,
                         u"666 Erebus St.\nApt 8"),
              IsMetadata(MemoryDataType::kAddressCity, u"Elysium"),
              IsMetadata(MemoryDataType::kAddressZip, u"91111"),
              IsMetadata(MemoryDataType::kAddressState, u"CA"),
              IsMetadata(MemoryDataType::kAddressCountry, u"United States"),
              IsMetadata(MemoryDataType::kCompanyName, u"Underworld"),
              IsMetadata(MemoryDataType::kEmail, u"johndoe@hades.com"),
              IsMetadata(MemoryDataType::kPhone, u"16502111111")),
          /*is_obfuscated=*/false, test::MakeGuid(1))));
}

// Tests that RetrieveAll correctly fetches and formats IBAN data.
TEST_F(AutofillDataProviderTest, RetrieveAll_IbanData) {
  Iban iban = test::GetLocalIban();
  iban.set_nickname(u"My IBAN");
  client().GetPersonalDataManager().test_payments_data_manager().AddIbanForTest(
      std::make_unique<Iban>(iban));

  std::vector<MemorySearchResult> results =
      RetrieveAllHelper(retriever(), MemoryDataType::kIban);
  EXPECT_THAT(results, UnorderedElementsAre(IsMemorySearchResult(
                           GetObfuscatedIban(iban.value()), u"IBAN",
                           UnorderedElementsAre(IsMetadata(
                               MemoryDataType::kIbanNickname, u"My IBAN")),
                           /*is_obfuscated=*/true, iban.guid())));
}

// Tests that RetrieveAll returns server IBANs, whose full value is unknown.
TEST_F(AutofillDataProviderTest, RetrieveAll_ServerIbanData) {
  const Iban iban = test::GetServerIban();
  client().GetPersonalDataManager().test_payments_data_manager().AddIbanForTest(
      std::make_unique<Iban>(iban));

  std::vector<MemorySearchResult> results =
      RetrieveAllHelper(retriever(), MemoryDataType::kIban);
  EXPECT_THAT(results,
              UnorderedElementsAre(IsMemorySearchResult(
                  iban.GetIdentifierStringForAutofillDisplay(), u"IBAN",
                  UnorderedElementsAre(IsMetadata(MemoryDataType::kIbanNickname,
                                                  iban.nickname())),
                  /*is_obfuscated=*/true, iban.instrument_id())));
}

// Tests that RetrieveAll correctly fetches and formats credit card data.
TEST_F(AutofillDataProviderTest, RetrieveAll_CreditCardData) {
  CreditCard credit_card = test::WithCvc(test::GetCreditCard(), u"123");
  credit_card.SetExpirationYear(2030);
  credit_card.SetExpirationMonth(10);
  credit_card.SetNickname(u"My Credit Card");
  client().GetPersonalDataManager().test_payments_data_manager().AddCreditCard(
      credit_card);

  std::vector<MemorySearchResult> number_results =
      RetrieveAllHelper(retriever(), MemoryDataType::kCreditCardNumber);
  EXPECT_THAT(
      number_results,
      UnorderedElementsAre(IsMemorySearchResult(
          credit_card.ObfuscatedNumberWithVisibleLastFourDigits(),
          GetMemoryDataTypeNameForI18n(MemoryDataType::kCreditCardNumber),
          UnorderedElementsAre(
              IsMetadata(MemoryDataType::kCreditCardNameOnCard,
                         credit_card.GetRawInfo(CREDIT_CARD_NAME_FULL)),
              IsMetadata(
                  MemoryDataType::kCreditCardExpirationDate,
                  credit_card.GetRawInfo(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR)),
              IsMetadata(MemoryDataType::kCreditCardNickname,
                         u"My Credit Card"),
              IsMetadata(MemoryDataType::kCreditCardSecurityCode,
                         std::u16string(3, kMidlineEllipsisPlainDot))),
          /*is_obfuscated=*/true, credit_card.guid())));

  std::vector<MemorySearchResult> cvc_results =
      RetrieveAllHelper(retriever(), MemoryDataType::kCreditCardSecurityCode);
  EXPECT_THAT(
      cvc_results,
      UnorderedElementsAre(IsMemorySearchResult(
          std::u16string(3, kMidlineEllipsisPlainDot),
          GetMemoryDataTypeNameForI18n(MemoryDataType::kCreditCardSecurityCode),
          UnorderedElementsAre(
              IsMetadata(MemoryDataType::kCreditCardNameOnCard,
                         credit_card.GetRawInfo(CREDIT_CARD_NAME_FULL)),
              IsMetadata(
                  MemoryDataType::kCreditCardExpirationDate,
                  credit_card.GetRawInfo(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR)),
              IsMetadata(MemoryDataType::kCreditCardNickname,
                         u"My Credit Card"),
              IsMetadata(
                  MemoryDataType::kCreditCardNumber,
                  credit_card.ObfuscatedNumberWithVisibleLastFourDigits())),
          /*is_obfuscated=*/true, credit_card.guid())));

  std::vector<MemorySearchResult> name_results =
      RetrieveAllHelper(retriever(), MemoryDataType::kCreditCardNameOnCard);
  EXPECT_THAT(
      name_results,
      UnorderedElementsAre(IsMemorySearchResult(
          credit_card.GetRawInfo(CREDIT_CARD_NAME_FULL),
          GetMemoryDataTypeNameForI18n(MemoryDataType::kCreditCardNameOnCard),
          UnorderedElementsAre(
              IsMetadata(
                  MemoryDataType::kCreditCardExpirationDate,
                  credit_card.GetRawInfo(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR)),
              IsMetadata(MemoryDataType::kCreditCardNickname,
                         u"My Credit Card"),
              IsMetadata(
                  MemoryDataType::kCreditCardNumber,
                  credit_card.ObfuscatedNumberWithVisibleLastFourDigits()),
              IsMetadata(MemoryDataType::kCreditCardSecurityCode,
                         std::u16string(3, kMidlineEllipsisPlainDot))),
          /*is_obfuscated=*/false, credit_card.guid())));

  std::vector<MemorySearchResult> exp_results =
      RetrieveAllHelper(retriever(), MemoryDataType::kCreditCardExpirationDate);
  EXPECT_THAT(
      exp_results,
      UnorderedElementsAre(IsMemorySearchResult(
          credit_card.GetRawInfo(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR),
          GetMemoryDataTypeNameForI18n(
              MemoryDataType::kCreditCardExpirationDate),
          UnorderedElementsAre(
              IsMetadata(MemoryDataType::kCreditCardNameOnCard,
                         credit_card.GetRawInfo(CREDIT_CARD_NAME_FULL)),
              IsMetadata(MemoryDataType::kCreditCardNickname,
                         u"My Credit Card"),
              IsMetadata(
                  MemoryDataType::kCreditCardNumber,
                  credit_card.ObfuscatedNumberWithVisibleLastFourDigits()),
              IsMetadata(MemoryDataType::kCreditCardSecurityCode,
                         std::u16string(3, kMidlineEllipsisPlainDot))),
          /*is_obfuscated=*/false, credit_card.guid())));
}

// Tests that `RetrieveAll` omits `kCreditCardSecurityCode` and
// `kCreditCardNumber` metadata when they are empty.
TEST_F(AutofillDataProviderTest, RetrieveAll_CreditCardData_EmptyFields) {
  CreditCard credit_card;
  credit_card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Test User");
  credit_card.SetExpirationYear(2030);
  credit_card.SetExpirationMonth(10);
  client().GetPersonalDataManager().test_payments_data_manager().AddCreditCard(
      credit_card);

  std::vector<MemorySearchResult> name_results =
      RetrieveAllHelper(retriever(), MemoryDataType::kCreditCardNameOnCard);
  // There should be no CVC entry, nor credit card number since they were empty.
  EXPECT_THAT(
      name_results,
      UnorderedElementsAre(IsMemorySearchResult(
          credit_card.GetRawInfo(CREDIT_CARD_NAME_FULL),
          GetMemoryDataTypeNameForI18n(MemoryDataType::kCreditCardNameOnCard),
          UnorderedElementsAre(IsMetadata(
              MemoryDataType::kCreditCardExpirationDate,
              credit_card.GetRawInfo(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR))),
          /*is_obfuscated=*/false, credit_card.guid())));
}

// Tests that `RetrieveAll` obfuscates CVC with the number of dots matching the
// length of the unobfuscated CVC.
TEST_F(AutofillDataProviderTest,
       RetrieveAll_CreditCardData_CvcObfuscationLength) {
  // Card with 4-digit CVC (e.g. American Express with CVC 1234).
  CreditCard amex_card_with_cvc =
      test::WithCvc(test::GetCreditCard2(), u"1234");
  amex_card_with_cvc.SetExpirationYear(2030);
  amex_card_with_cvc.SetExpirationMonth(10);
  client().GetPersonalDataManager().test_payments_data_manager().AddCreditCard(
      amex_card_with_cvc);

  // American Express card without stored CVC (shouldn't be offered).
  CreditCard amex_card_without_cvc = test::GetCreditCard2();
  amex_card_without_cvc.set_guid(test::MakeGuid(2));
  amex_card_without_cvc.SetExpirationYear(2030);
  amex_card_without_cvc.SetExpirationMonth(10);
  client().GetPersonalDataManager().test_payments_data_manager().AddCreditCard(
      amex_card_without_cvc);

  // Direct retrieval for `kCreditCardSecurityCode` should return 4 dots for
  // card with 4-digit CVC.
  std::vector<MemorySearchResult> cvc_results =
      RetrieveAllHelper(retriever(), MemoryDataType::kCreditCardSecurityCode);
  EXPECT_THAT(
      cvc_results,
      UnorderedElementsAre(IsMemorySearchResult(
          std::u16string(4, kMidlineEllipsisPlainDot),
          GetMemoryDataTypeNameForI18n(MemoryDataType::kCreditCardSecurityCode),
          UnorderedElementsAre(
              IsMetadata(MemoryDataType::kCreditCardNameOnCard,
                         amex_card_with_cvc.GetRawInfo(CREDIT_CARD_NAME_FULL)),
              IsMetadata(MemoryDataType::kCreditCardExpirationDate,
                         amex_card_with_cvc.GetRawInfo(
                             CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR)),
              IsMetadata(MemoryDataType::kCreditCardNumber,
                         amex_card_with_cvc
                             .ObfuscatedNumberWithVisibleLastFourDigits())),
          /*is_obfuscated=*/true, amex_card_with_cvc.guid())));

  // Metadata retrieval for `kCreditCardNumber` should include CVC metadata
  // with 4 dots for the card with stored 4-digit CVC, and omit CVC metadata
  // for the card without a stored CVC.
  std::vector<MemorySearchResult> number_results =
      RetrieveAllHelper(retriever(), MemoryDataType::kCreditCardNumber);
  EXPECT_THAT(
      number_results,
      UnorderedElementsAre(
          IsMemorySearchResult(
              amex_card_with_cvc.ObfuscatedNumberWithVisibleLastFourDigits(),
              GetMemoryDataTypeNameForI18n(MemoryDataType::kCreditCardNumber),
              UnorderedElementsAre(
                  IsMetadata(
                      MemoryDataType::kCreditCardNameOnCard,
                      amex_card_with_cvc.GetRawInfo(CREDIT_CARD_NAME_FULL)),
                  IsMetadata(MemoryDataType::kCreditCardExpirationDate,
                             amex_card_with_cvc.GetRawInfo(
                                 CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR)),
                  IsMetadata(MemoryDataType::kCreditCardSecurityCode,
                             std::u16string(4, kMidlineEllipsisPlainDot))),
              /*is_obfuscated=*/true, amex_card_with_cvc.guid()),
          IsMemorySearchResult(
              amex_card_without_cvc.ObfuscatedNumberWithVisibleLastFourDigits(),
              GetMemoryDataTypeNameForI18n(MemoryDataType::kCreditCardNumber),
              UnorderedElementsAre(
                  IsMetadata(
                      MemoryDataType::kCreditCardNameOnCard,
                      amex_card_without_cvc.GetRawInfo(CREDIT_CARD_NAME_FULL)),
                  IsMetadata(MemoryDataType::kCreditCardExpirationDate,
                             amex_card_without_cvc.GetRawInfo(
                                 CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR))),
              /*is_obfuscated=*/true, amex_card_without_cvc.guid())));
}

// Tests that RetrieveAll correctly fetches and formats data from
// EntityDataManager (Autofill AI).
TEST_F(AutofillDataProviderTest, RetrieveAll_AutofillAiEntityData) {
  EntityInstance vehicle = test::GetVehicleEntityInstance({.use_count = 1});
  entity_data_manager().AddOrUpdateEntityInstance(vehicle);
  WaitForDatabase();

  // Asking for Vehicle Plate Number should return combined result and
  // individual attributes.
  std::vector<MemorySearchResult> results =
      RetrieveAllHelper(retriever(), MemoryDataType::kVehiclePlateNumber);
  EXPECT_THAT(
      results,
      ElementsAre(IsMemorySearchResult(
          u"123456", u"License plate",
          ElementsAre(
              IsMetadata(MemoryDataType::kVehicleMake, u"BMW"),
              IsMetadata(MemoryDataType::kVehicleModel, u"Series 2"),
              IsMetadata(MemoryDataType::kVehicleYear, u"2025"),
              IsMetadata(MemoryDataType::kVehicleOwner, u"Knecht Ruprecht"),
              IsMetadata(MemoryDataType::kVehiclePlateState, u"California"),
              IsMetadata(MemoryDataType::kVehicleVin, u"12312345")),
          /*is_obfuscated=*/false, vehicle.guid().value())));
}

// Tests that RetrieveAll correctly formats Passport entity data.
TEST_F(AutofillDataProviderTest, RetrieveAll_PassportData) {
  EntityInstance passport =
      test::GetPassportEntityInstance({.number = u"XYZ123", .use_count = 1});
  entity_data_manager().AddOrUpdateEntityInstance(passport);
  WaitForDatabase();

  std::vector<MemorySearchResult> results =
      RetrieveAllHelper(retriever(), MemoryDataType::kPassportNumber);
  ASSERT_FALSE(results.empty());

  auto it = std::find_if(results.begin(), results.end(),
                         [](const MemorySearchResult& r) {
                           return r.type == MemoryDataType::kPassportNumber;
                         });
  ASSERT_NE(it, results.end());

  std::u16string expected_obfuscated_value =
      GetObfuscatedValue(u"XYZ123", /*visible_suffix_length=*/4);
  EXPECT_EQ(it->value, expected_obfuscated_value);
  EXPECT_TRUE(it->is_obfuscated);
  ASSERT_TRUE(std::holds_alternative<std::string>(it->identifier));
  EXPECT_EQ(std::get<std::string>(it->identifier), passport.guid().value());
  ASSERT_FALSE(it->metadata_list.empty());
  EXPECT_THAT(it->metadata_list,
              testing::Not(Contains(IsMetadata(MemoryDataType::kPassportNumber,
                                               expected_obfuscated_value))));
}

// Tests that RetrieveAll correctly fetches data for a specific attribute.
TEST_F(AutofillDataProviderTest, RetrieveAll_AutofillAiAttributeData) {
  EntityInstance vehicle = test::GetVehicleEntityInstance({.use_count = 1});
  entity_data_manager().AddOrUpdateEntityInstance(vehicle);
  WaitForDatabase();

  EXPECT_THAT(
      RetrieveAllHelper(retriever(), MemoryDataType::kVehiclePlateNumber),
      UnorderedElementsAre(IsMemorySearchResult(
          u"123456", u"License plate",
          ElementsAre(
              IsMetadata(MemoryDataType::kVehicleMake, u"BMW"),
              IsMetadata(MemoryDataType::kVehicleModel, u"Series 2"),
              IsMetadata(MemoryDataType::kVehicleYear, u"2025"),
              IsMetadata(MemoryDataType::kVehicleOwner, u"Knecht Ruprecht"),
              IsMetadata(MemoryDataType::kVehiclePlateState, u"California"),
              IsMetadata(MemoryDataType::kVehicleVin, u"12312345")),
          /*is_obfuscated=*/false, vehicle.guid().value())));
}

// Tests that RetrieveAll skips EntityInstance entries with
// RecordType::kPersonalContext while returning kLocal and kServerWallet
// entries.
TEST_F(AutofillDataProviderTest,
       RetrieveAll_FiltersOutPersonalContextEntities) {
  EntityInstance local_vehicle = test::GetVehicleEntityInstanceWithRandomGuid(
      {.plate = u"LOCAL1",
       .number = u"VIN1",
       .record_type = EntityInstance::RecordType::kLocal,
       .use_count = 1});
  EntityInstance wallet_vehicle = test::GetVehicleEntityInstanceWithRandomGuid(
      {.plate = u"WALLET1",
       .number = u"VIN2",
       .record_type = EntityInstance::RecordType::kServerWallet,
       .use_count = 1});
  EntityInstance pcontext_vehicle =
      test::GetVehicleEntityInstanceWithRandomGuid(
          {.plate = u"PCONTEXT1",
           .number = u"VIN3",
           .record_type = EntityInstance::RecordType::kPersonalContext,
           .use_count = 1});

  entity_data_manager().AddOrUpdateEntityInstance(local_vehicle);
  entity_data_manager().AddOrUpdateEntityInstance(wallet_vehicle);
  entity_data_manager().SetPersonalContextEntitiesForTesting(
      {pcontext_vehicle});
  WaitForDatabase();
  ASSERT_EQ(entity_data_manager().GetEntityInstances().size(), 3u);

  EXPECT_THAT(
      RetrieveAllHelper(retriever(), MemoryDataType::kVehiclePlateNumber),
      UnorderedElementsAre(
          Field(&MemorySearchResult::value, Eq(u"LOCAL1")),
          Field(&MemorySearchResult::value, Eq(u"WALLET1"))));
}

// Tests that RetrieveAll omits address suggestions for profiles that only have
// a name but no address data.
TEST_F(AutofillDataProviderTest, RetrieveAll_AddressFull_EmptyProfile) {
  AutofillProfile profile(AddressCountryCode("US"));
  profile.SetRawInfo(NAME_FULL, u"Homer Simpson");
  client().GetPersonalDataManager().address_data_manager().AddProfile(profile);

  EXPECT_THAT(RetrieveAllHelper(retriever(), MemoryDataType::kAddressFull),
              IsEmpty());
}

// Tests that RetrieveAll correctly formats address suggestions for
// partial addresses.
TEST_F(AutofillDataProviderTest, RetrieveAll_AddressFull_PartialAddress) {
  AutofillProfile profile(AddressCountryCode("US"));
  profile.set_guid(test::MakeGuid(1));
  profile.SetRawInfo(NAME_FULL, u"Homer Simpson");
  profile.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"742 Evergreen Terrace");
  profile.SetRawInfo(ADDRESS_HOME_CITY, u"Springfield");
  // Missing State, Zip
  client().GetPersonalDataManager().address_data_manager().AddProfile(profile);

  std::vector<MemorySearchResult> results =
      RetrieveAllHelper(retriever(), MemoryDataType::kAddressFull);

  EXPECT_THAT(
      results,
      UnorderedElementsAre(IsMemorySearchResult(
          u"742 Evergreen Terrace, Springfield, United States", u"Address",
          UnorderedElementsAre(
              IsMetadata(MemoryDataType::kNameFull, u"Homer Simpson"),
              IsMetadata(MemoryDataType::kAddressStreetAddress,
                         u"742 Evergreen Terrace"),
              IsMetadata(MemoryDataType::kAddressCity, u"Springfield"),
              IsMetadata(MemoryDataType::kAddressCountry, u"United States")),
          /*is_obfuscated=*/false, test::MakeGuid(1))));
}

// Tests that `RetrieveAll` yields an entry per requested type of a record, so
// that each entry can be filtered against its own fetch specification.
TEST_F(AutofillDataProviderTest, RetrieveAll_MultipleTypes) {
  AutofillProfile profile = test::GetFullProfile();
  client().GetPersonalDataManager().address_data_manager().AddProfile(profile);

  base::test::TestFuture<std::vector<MemorySearchResult>> future;
  retriever().RetrieveAll(
      {MemoryDataType::kAddressCity, MemoryDataType::kAddressZip},
      future.GetCallback());

  EXPECT_THAT(
      future.Take(),
      UnorderedElementsAre(
          Field(&MemorySearchResult::type, Eq(MemoryDataType::kAddressCity)),
          Field(&MemorySearchResult::type, Eq(MemoryDataType::kAddressZip))));
}

// Tests that `RetrieveAll` yields the records of all requested types if the
// types belong to unrelated records, e.g. of different backends.
TEST_F(AutofillDataProviderTest, RetrieveAll_UnrelatedTypes) {
  client().GetPersonalDataManager().address_data_manager().AddProfile(
      test::GetFullProfile());
  entity_data_manager().AddOrUpdateEntityInstance(
      test::GetPassportEntityInstance({.name = u"Marge Simpson"}));
  WaitForDatabase();

  base::test::TestFuture<std::vector<MemorySearchResult>> future;
  retriever().RetrieveAll(
      {MemoryDataType::kPassportName, MemoryDataType::kNameFull},
      future.GetCallback());

  EXPECT_THAT(
      future.Take(),
      UnorderedElementsAre(
          AllOf(Field(&MemorySearchResult::type,
                      Eq(MemoryDataType::kPassportName)),
                Field(&MemorySearchResult::value, Eq(u"Marge Simpson"))),
          AllOf(Field(&MemorySearchResult::type, Eq(MemoryDataType::kNameFull)),
                Field(&MemorySearchResult::value, Eq(u"John H. Doe")))));
}

// Tests that RetrieveAll populates typed_value on entity instance metadata.
TEST_F(AutofillDataProviderTest,
       RetrieveAll_PopulatesTypedValue_EntityInstance) {
  EntityInstance passport = test::GetPassportEntityInstance(
      {.number = u"XYZ123", .issue_date = u"2010-09-01", .use_count = 1});
  entity_data_manager().AddOrUpdateEntityInstance(passport);
  WaitForDatabase();

  std::vector<MemorySearchResult> results =
      RetrieveAllHelper(retriever(), MemoryDataType::kPassportNumber);
  ASSERT_EQ(results.size(), 1u);
  auto it = std::ranges::find(results[0].metadata_list,
                              MemoryDataType::kPassportIssueDate,
                              &EntryMetadata::type);
  ASSERT_NE(it, results[0].metadata_list.end());
  EXPECT_TRUE(it->typed_value.has_value());
  EXPECT_EQ(it->typed_value->date().year(), 2010);
  EXPECT_EQ(it->typed_value->date().month(), 9);
  EXPECT_EQ(it->typed_value->date().day(), 1);
}

// Tests that RetrieveAll populates typed_value on address profile country
// suggestions.
TEST_F(AutofillDataProviderTest,
       RetrieveAll_PopulatesTypedValue_AddressProfile) {
  AutofillProfile profile = test::GetFullProfile();
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");
  client().GetPersonalDataManager().address_data_manager().AddProfile(profile);

  std::vector<MemorySearchResult> results =
      RetrieveAllHelper(retriever(), MemoryDataType::kAddressCountry);
  ASSERT_EQ(results.size(), 1u);
  EXPECT_TRUE(results[0].typed_value.has_value());
  EXPECT_EQ(results[0].typed_value->country_code(), "US");
}

// Tests that RetrieveAll populates typed_value on credit card expiration dates.
TEST_F(AutofillDataProviderTest, RetrieveAll_PopulatesTypedValue_CreditCard) {
  CreditCard card = test::GetCreditCard();
  card.SetExpirationYear(2028);
  card.SetExpirationMonth(5);
  client().GetPersonalDataManager().payments_data_manager().AddCreditCard(card);

  std::vector<MemorySearchResult> results =
      RetrieveAllHelper(retriever(), MemoryDataType::kCreditCardExpirationDate);
  ASSERT_EQ(results.size(), 1u);
  EXPECT_TRUE(results[0].typed_value.has_value());
  EXPECT_EQ(results[0].typed_value->date().year(), 2028);
  EXPECT_EQ(results[0].typed_value->date().month(), 5);
  EXPECT_EQ(results[0].typed_value->date().day(), 0);
}

// Tests that RetrieveAll correctly fetches and formats loyalty card data.
TEST_F(AutofillDataProviderTest, RetrieveAll_LoyaltyCardData) {
  LoyaltyCard loyalty_card = test::CreateLoyaltyCard();
  loyalty_card.set_merchant_name("Merchant");
  loyalty_card.set_program_name("Program");
  test_api(*client().GetValuablesDataManager()).AddLoyaltyCard(loyalty_card);

  EXPECT_THAT(
      RetrieveAllHelper(retriever(), MemoryDataType::kLoyaltyMembershipId),
      UnorderedElementsAre(IsMemorySearchResult(
          u"1234",
          GetMemoryDataTypeNameForI18n(MemoryDataType::kLoyaltyMembershipId),
          UnorderedElementsAre(
              IsMetadata(MemoryDataType::kLoyaltyMembershipProgram, u"Program"),
              IsMetadata(MemoryDataType::kLoyaltyMembershipProvider,
                         u"Merchant")),
          /*is_obfuscated=*/false, *loyalty_card.id())));

  EXPECT_THAT(
      RetrieveAllHelper(retriever(), MemoryDataType::kLoyaltyMembershipProgram),
      UnorderedElementsAre(IsMemorySearchResult(
          u"Program",
          GetMemoryDataTypeNameForI18n(
              MemoryDataType::kLoyaltyMembershipProgram),
          UnorderedElementsAre(
              IsMetadata(MemoryDataType::kLoyaltyMembershipId, u"1234"),
              IsMetadata(MemoryDataType::kLoyaltyMembershipProvider,
                         u"Merchant")),
          /*is_obfuscated=*/false, *loyalty_card.id())));

  EXPECT_THAT(RetrieveAllHelper(retriever(),
                                MemoryDataType::kLoyaltyMembershipProvider),
              UnorderedElementsAre(IsMemorySearchResult(
                  u"Merchant",
                  GetMemoryDataTypeNameForI18n(
                      MemoryDataType::kLoyaltyMembershipProvider),
                  UnorderedElementsAre(
                      IsMetadata(MemoryDataType::kLoyaltyMembershipId, u"1234"),
                      IsMetadata(MemoryDataType::kLoyaltyMembershipProgram,
                                 u"Program")),
                  /*is_obfuscated=*/false, *loyalty_card.id())));
}

// Tests that `RetrieveAllByContent` returns an entry per matching attribute of
// a record, with the matching attribute as the primary value, ordered by the
// attributes' priority.
TEST_F(AutofillDataProviderTest, RetrieveAllByContent_AddressData) {
  AutofillProfile profile = test::GetFullProfile();
  profile.set_guid(test::MakeGuid(1));
  profile.SetRawInfo(ADDRESS_HOME_ZIP, u"91111");
  profile.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"91111 Elysium Street");
  client().GetPersonalDataManager().address_data_manager().AddProfile(profile);

  // The query is contained in both the zip code and the street address. The
  // zip code comes first since codes precede free text.
  EXPECT_THAT(
      RetrieveAllByContentHelper(retriever(), u"91111"),
      ElementsAre(
          IsMemorySearchResult(
              u"91111", u"Zip",
              Contains(IsMetadata(MemoryDataType::kAddressCity, u"Elysium")),
              /*is_obfuscated=*/false, test::MakeGuid(1)),
          IsMemorySearchResult(u"91111 Elysium Street", u"Street address", _,
                               /*is_obfuscated=*/false, test::MakeGuid(1))));
  // Both the street address and the city contain "Elysium". The street address
  // comes first since its type precedes the city's in `MemoryDataType`.
  EXPECT_THAT(
      RetrieveAllByContentHelper(retriever(), u"Elysium"),
      ElementsAre(
          IsMemorySearchResult(u"91111 Elysium Street", u"Street address", _,
                               /*is_obfuscated=*/false, test::MakeGuid(1)),
          IsMemorySearchResult(u"Elysium", u"City", _,
                               /*is_obfuscated=*/false, test::MakeGuid(1))));
  EXPECT_THAT(RetrieveAllByContentHelper(retriever(), u""), IsEmpty());
}

// Tests that `RetrieveAllByContent` matches local IBANs by their full value,
// but not by their nickname, and never returns server IBANs.
TEST_F(AutofillDataProviderTest, RetrieveAllByContent_IbanData) {
  Iban local_iban = test::GetLocalIban2();
  local_iban.set_nickname(u"My savings");
  client().GetPersonalDataManager().test_payments_data_manager().AddIbanForTest(
      std::make_unique<Iban>(local_iban));
  client().GetPersonalDataManager().test_payments_data_manager().AddIbanForTest(
      std::make_unique<Iban>(test::GetServerIban()));

  EXPECT_THAT(
      RetrieveAllByContentHelper(retriever(), local_iban.value().substr(0, 6)),
      UnorderedElementsAre(IsMemorySearchResult(
          GetObfuscatedIban(local_iban.value()), u"IBAN",
          UnorderedElementsAre(
              IsMetadata(MemoryDataType::kIbanNickname, u"My savings")),
          /*is_obfuscated=*/true, local_iban.guid())));
  // Nicknames are not searchable.
  EXPECT_THAT(RetrieveAllByContentHelper(retriever(), u"savings"), IsEmpty());
  // The server IBAN's prefix does not match.
  EXPECT_THAT(RetrieveAllByContentHelper(retriever(), u"FR76"), IsEmpty());
}

// Tests that `RetrieveAllByContent` matches local credit cards by their full
// number, but not by their nickname, and never returns server cards.
TEST_F(AutofillDataProviderTest, RetrieveAllByContent_CreditCardData) {
  CreditCard local_card = test::GetCreditCard();
  local_card.SetNickname(u"My Credit Card");
  client().GetPersonalDataManager().test_payments_data_manager().AddCreditCard(
      local_card);
  client()
      .GetPersonalDataManager()
      .test_payments_data_manager()
      .AddServerCreditCard(test::GetMaskedServerCard());

  EXPECT_THAT(
      RetrieveAllByContentHelper(retriever(), u"4111 1111"),
      UnorderedElementsAre(IsMemorySearchResult(
          local_card.ObfuscatedNumberWithVisibleLastFourDigits(),
          u"Card number", _, /*is_obfuscated=*/true, local_card.guid())));
  // Nicknames are not searchable.
  EXPECT_THAT(RetrieveAllByContentHelper(retriever(), u"credit card"),
              IsEmpty());
  EXPECT_THAT(RetrieveAllByContentHelper(
                  retriever(), test::GetMaskedServerCard().LastFourDigits()),
              IsEmpty());
}

// Tests that `RetrieveAllByContent` matches Autofill AI entity attributes.
TEST_F(AutofillDataProviderTest, RetrieveAllByContent_AutofillAiEntityData) {
  EntityInstance vehicle = test::GetVehicleEntityInstance({.use_count = 1});
  entity_data_manager().AddOrUpdateEntityInstance(vehicle);
  WaitForDatabase();

  // Both the license plate and the VIN contain the query. The license plate
  // comes first since its type precedes the VIN's in `MemoryDataType`.
  EXPECT_THAT(
      RetrieveAllByContentHelper(retriever(), u"1234"),
      ElementsAre(
          IsMemorySearchResult(
              u"123456", u"License plate",
              Contains(IsMetadata(MemoryDataType::kVehicleMake, u"BMW")),
              /*is_obfuscated=*/false, vehicle.guid().value()),
          IsMemorySearchResult(
              u"12312345", u"VIN (Vehicle Identification Number)", _,
              /*is_obfuscated=*/false, vehicle.guid().value())));
}

// Tests that `RetrieveAllByContent` matches loyalty cards.
TEST_F(AutofillDataProviderTest, RetrieveAllByContent_LoyaltyCardData) {
  LoyaltyCard loyalty_card = test::CreateLoyaltyCard();
  test_api(*client().GetValuablesDataManager()).AddLoyaltyCard(loyalty_card);

  // Both the program and the provider contain the query. The program comes
  // first since its type precedes the provider's in `MemoryDataType`.
  EXPECT_THAT(RetrieveAllByContentHelper(retriever(), u"Bahn"),
              ElementsAre(IsMemorySearchResult(
                              u"BahnBonus",
                              GetMemoryDataTypeNameForI18n(
                                  MemoryDataType::kLoyaltyMembershipProgram),
                              _, /*is_obfuscated=*/false, *loyalty_card.id()),
                          IsMemorySearchResult(
                              u"Deutsche Bahn",
                              GetMemoryDataTypeNameForI18n(
                                  MemoryDataType::kLoyaltyMembershipProvider),
                              _, /*is_obfuscated=*/false, *loyalty_card.id())));
}

}  // namespace

}  // namespace autofill
