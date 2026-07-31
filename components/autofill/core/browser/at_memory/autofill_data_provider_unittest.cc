// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/autofill_data_provider.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/autofill/core/browser/at_memory/at_memory_data_type.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/field_filling_util.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_data_type_util.h"
#include "components/autofill/core/browser/integrators/at_memory/memory_search_result.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator_util.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
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
        &client_.GetPersonalDataManager(), client_.GetEntityDataManager());
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
              IsMetadata(MemoryDataType::kAddressCountry, u"United States")),
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
              IsMetadata(MemoryDataType::kAddressCountry, u"United States")),
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
              IsMetadata(MemoryDataType::kAddressCountry, u"United States")),
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
                      IsMetadata(MemoryDataType::kAddressZip, u"91111")),
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
              IsMetadata(MemoryDataType::kAddressCountry, u"United States")),
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
              IsMetadata(MemoryDataType::kAddressCountry, u"United States")),
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
              IsMetadata(MemoryDataType::kAddressCountry, u"United States")),
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
              IsMetadata(MemoryDataType::kAddressCountry, u"United States")),
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

// Tests that RetrieveAll can fetch multiple types at once (e.g. City and Zip).
TEST_F(AutofillDataProviderTest, RetrieveAll_MultipleTypes) {
  AutofillProfile profile = test::GetFullProfile();
  client().GetPersonalDataManager().address_data_manager().AddProfile(profile);

  base::test::TestFuture<std::vector<MemorySearchResult>> future;
  retriever().RetrieveAll(
      {MemoryDataType::kAddressCity, MemoryDataType::kAddressZip},
      future.GetCallback());
  std::vector<MemorySearchResult> results = future.Take();

  EXPECT_THAT(
      results,
      testing::UnorderedElementsAre(
          Field(&MemorySearchResult::type, Eq(MemoryDataType::kAddressCity)),
          Field(&MemorySearchResult::type, Eq(MemoryDataType::kAddressZip))));
}

}  // namespace

}  // namespace autofill
