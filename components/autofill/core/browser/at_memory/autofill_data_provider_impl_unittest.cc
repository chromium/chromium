// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/autofill_data_provider_impl.h"

#include <vector>

#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/autofill/core/browser/at_memory/at_memory_data_type.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/data_model/autofill_ai/from_accessibility_annotator.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/test_autofill_client.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator_util.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/test_utils/entity_data_test_utils.h"
#include "components/autofill/core/browser/webdata/autofill_ai/entity_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service_test_helper.h"
#include "components/sync/test/test_sync_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

using ::accessibility_annotator::EntryMetadata;
using ::accessibility_annotator::EntryType;
using ::accessibility_annotator::MemorySearchResult;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::UnorderedElementsAre;

namespace {

Matcher<EntryMetadata> IsMetadata(EntryType type, const std::u16string& value) {
  return AllOf(
      Field(&EntryMetadata::type, Eq(type)),
      Field(&EntryMetadata::type_name, Eq(GetEntryTypeNameForI18n(type))),
      Field(&EntryMetadata::value, Eq(value)));
}

Matcher<MemorySearchResult> IsMemorySearchResult(
    const std::u16string& value,
    const std::u16string& type_name,
    Matcher<std::vector<EntryMetadata>> metadata_matcher,
    bool is_obfuscated = false) {
  return AllOf(Field(&MemorySearchResult::value, Eq(value)),
               Field(&MemorySearchResult::type_name, Eq(type_name)),
               Field(&MemorySearchResult::is_obfuscated, Eq(is_obfuscated)),
               Field(&MemorySearchResult::metadata_list, metadata_matcher));
}

std::vector<MemorySearchResult> RetrieveAllHelper(
    AutofillDataProviderImpl& retriever,
    accessibility_annotator::EntryType type) {
  base::test::TestFuture<std::vector<MemorySearchResult>> future;
  retriever.RetrieveAll(type, future.GetCallback());
  return future.Take();
}

class AutofillDataProviderImplTest : public testing::Test {
 public:
  AutofillDataProviderImplTest()
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
        /*strike_database=*/nullptr,
        /*accessibility_annotator_service=*/nullptr,
        /*variation_country_code=*/GeoIpCountryCode("US"));
    entity_data_manager_ = entity_data_manager.get();
    client_.set_entity_data_manager(std::move(entity_data_manager));

    retriever_ = std::make_unique<AutofillDataProviderImpl>(
        &client_.GetPersonalDataManager(), client_.GetEntityDataManager());
  }

  void WaitForDatabase() { webdata_helper_.WaitUntilIdle(); }

  AutofillDataProviderImpl& retriever() { return *retriever_; }
  TestAutofillClient& client() { return client_; }
  EntityDataManager& entity_data_manager() { return *entity_data_manager_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  AutofillWebDataServiceTestHelper webdata_helper_;
  syncer::TestSyncService sync_service_;
  TestAutofillClient client_;
  raw_ptr<EntityDataManager> entity_data_manager_;
  std::unique_ptr<AutofillDataProviderImpl> retriever_;
};

// Tests that RetrieveAll returns an empty list when no data is available
TEST_F(AutofillDataProviderImplTest, RetrieveAll_Empty) {
  EXPECT_THAT(
      RetrieveAllHelper(retriever(),
                        accessibility_annotator::EntryType::kAddressCity),
      IsEmpty());
}

// Tests that RetrieveAll fetches and formats address-related data from
// PersonalDataManager.
TEST_F(AutofillDataProviderImplTest, RetrieveAll_AddressData) {
  AutofillProfile profile = test::GetFullProfile();
  client().GetPersonalDataManager().address_data_manager().AddProfile(profile);

  EXPECT_THAT(
      RetrieveAllHelper(retriever(),
                        accessibility_annotator::EntryType::kAddressCity),
      UnorderedElementsAre(IsMemorySearchResult(
          u"Elysium", u"City",
          UnorderedElementsAre(
              IsMetadata(EntryType::kNameFull, u"John H. Doe"),
              IsMetadata(EntryType::kAddressStreetAddress,
                         u"666 Erebus St.\nApt 8"),
              IsMetadata(EntryType::kAddressState, u"CA"),
              IsMetadata(EntryType::kAddressZip, u"91111"),
              IsMetadata(EntryType::kAddressCountry, u"United States")))));

  EXPECT_THAT(
      RetrieveAllHelper(retriever(),
                        accessibility_annotator::EntryType::kAddressZip),
      UnorderedElementsAre(IsMemorySearchResult(
          u"91111", u"Zip",
          UnorderedElementsAre(
              IsMetadata(EntryType::kNameFull, u"John H. Doe"),
              IsMetadata(EntryType::kAddressStreetAddress,
                         u"666 Erebus St.\nApt 8"),
              IsMetadata(EntryType::kAddressCity, u"Elysium"),
              IsMetadata(EntryType::kAddressState, u"CA"),
              IsMetadata(EntryType::kAddressCountry, u"United States")))));

  EXPECT_THAT(
      RetrieveAllHelper(retriever(),
                        accessibility_annotator::EntryType::kAddressState),
      UnorderedElementsAre(IsMemorySearchResult(
          u"CA", u"State",
          UnorderedElementsAre(
              IsMetadata(EntryType::kNameFull, u"John H. Doe"),
              IsMetadata(EntryType::kAddressStreetAddress,
                         u"666 Erebus St.\nApt 8"),
              IsMetadata(EntryType::kAddressCity, u"Elysium"),
              IsMetadata(EntryType::kAddressZip, u"91111"),
              IsMetadata(EntryType::kAddressCountry, u"United States")))));

  EXPECT_THAT(
      RetrieveAllHelper(retriever(),
                        accessibility_annotator::EntryType::kAddressCountry),
      UnorderedElementsAre(IsMemorySearchResult(
          u"United States", u"Country",
          UnorderedElementsAre(IsMetadata(EntryType::kNameFull, u"John H. Doe"),
                               IsMetadata(EntryType::kAddressStreetAddress,
                                          u"666 Erebus St.\nApt 8"),
                               IsMetadata(EntryType::kAddressCity, u"Elysium"),
                               IsMetadata(EntryType::kAddressState, u"CA"),
                               IsMetadata(EntryType::kAddressZip, u"91111")))));

  EXPECT_THAT(
      RetrieveAllHelper(retriever(),
                        accessibility_annotator::EntryType::kNameFull),
      UnorderedElementsAre(IsMemorySearchResult(
          u"John H. Doe", u"Name",
          UnorderedElementsAre(
              IsMetadata(EntryType::kAddressStreetAddress,
                         u"666 Erebus St.\nApt 8"),
              IsMetadata(EntryType::kAddressCity, u"Elysium"),
              IsMetadata(EntryType::kAddressState, u"CA"),
              IsMetadata(EntryType::kAddressZip, u"91111"),
              IsMetadata(EntryType::kAddressCountry, u"United States")))));

  EXPECT_THAT(
      RetrieveAllHelper(retriever(),
                        accessibility_annotator::EntryType::kEmail),
      UnorderedElementsAre(IsMemorySearchResult(
          u"johndoe@hades.com", u"Email",
          UnorderedElementsAre(
              IsMetadata(EntryType::kNameFull, u"John H. Doe"),
              IsMetadata(EntryType::kAddressStreetAddress,
                         u"666 Erebus St.\nApt 8"),
              IsMetadata(EntryType::kAddressCity, u"Elysium"),
              IsMetadata(EntryType::kAddressState, u"CA"),
              IsMetadata(EntryType::kAddressZip, u"91111"),
              IsMetadata(EntryType::kAddressCountry, u"United States")))));

  EXPECT_THAT(
      RetrieveAllHelper(retriever(),
                        accessibility_annotator::EntryType::kPhone),
      UnorderedElementsAre(IsMemorySearchResult(
          u"16502111111", u"Phone",
          UnorderedElementsAre(
              IsMetadata(EntryType::kNameFull, u"John H. Doe"),
              IsMetadata(EntryType::kAddressStreetAddress,
                         u"666 Erebus St.\nApt 8"),
              IsMetadata(EntryType::kAddressCity, u"Elysium"),
              IsMetadata(EntryType::kAddressState, u"CA"),
              IsMetadata(EntryType::kAddressZip, u"91111"),
              IsMetadata(EntryType::kAddressCountry, u"United States")))));

  // Requesting for address should return only the full address.
  EXPECT_THAT(
      RetrieveAllHelper(retriever(),
                        accessibility_annotator::EntryType::kAddressFull),
      UnorderedElementsAre(IsMemorySearchResult(
          u"Underworld, 666 Erebus St., Apt 8, Elysium, CA 91111, "
          u"United States",
          u"Address",
          UnorderedElementsAre(
              IsMetadata(EntryType::kNameFull, u"John H. Doe"),
              IsMetadata(EntryType::kAddressStreetAddress,
                         u"666 Erebus St.\nApt 8"),
              IsMetadata(EntryType::kAddressCity, u"Elysium"),
              IsMetadata(EntryType::kAddressZip, u"91111"),
              IsMetadata(EntryType::kAddressState, u"CA"),
              IsMetadata(EntryType::kAddressCountry, u"United States")))));
}

// Tests that RetrieveAll correctly fetches and formats IBAN data.
TEST_F(AutofillDataProviderImplTest, RetrieveAll_IbanData) {
  Iban iban = test::GetLocalIban();
  iban.set_nickname(u"My IBAN");
  client().GetPersonalDataManager().test_payments_data_manager().AddIbanForTest(
      std::make_unique<Iban>(iban));

  std::vector<MemorySearchResult> results =
      RetrieveAllHelper(retriever(), accessibility_annotator::EntryType::kIban);
  EXPECT_THAT(results, UnorderedElementsAre(IsMemorySearchResult(
                           GetObfuscatedIban(iban.value()), u"IBAN",
                           UnorderedElementsAre(IsMetadata(
                               EntryType::kIbanNickname, u"My IBAN")),
                           /*is_obfuscated=*/true)));
  ASSERT_TRUE(std::holds_alternative<std::string>(results[0].identifier));
  EXPECT_EQ(std::get<std::string>(results[0].identifier), iban.guid());
}

// Tests that RetrieveAll correctly fetches and formats credit card data.
TEST_F(AutofillDataProviderImplTest, RetrieveAll_CreditCardData) {
  CreditCard credit_card = test::WithCvc(test::GetCreditCard(), u"123");
  credit_card.SetExpirationYear(2030);
  credit_card.SetExpirationMonth(10);
  credit_card.SetNickname(u"My Credit Card");
  client().GetPersonalDataManager().test_payments_data_manager().AddCreditCard(
      credit_card);

  std::vector<MemorySearchResult> number_results = RetrieveAllHelper(
      retriever(), accessibility_annotator::EntryType::kCreditCardNumber);
  EXPECT_THAT(
      number_results,
      UnorderedElementsAre(IsMemorySearchResult(
          credit_card.ObfuscatedNumberWithVisibleLastFourDigits(),
          GetEntryTypeNameForI18n(EntryType::kCreditCardNumber),
          UnorderedElementsAre(
              IsMetadata(EntryType::kCreditCardNameOnCard,
                         credit_card.GetRawInfo(CREDIT_CARD_NAME_FULL)),
              IsMetadata(
                  EntryType::kCreditCardExpirationDate,
                  credit_card.GetRawInfo(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR)),
              IsMetadata(EntryType::kCreditCardNickname, u"My Credit Card"),
              IsMetadata(EntryType::kCreditCardSecurityCode,
                         std::u16string(3, kMidlineEllipsisPlainDot))))));
  ASSERT_TRUE(
      std::holds_alternative<std::string>(number_results[0].identifier));
  EXPECT_EQ(std::get<std::string>(number_results[0].identifier),
            credit_card.guid());

  std::vector<MemorySearchResult> cvc_results = RetrieveAllHelper(
      retriever(),
      accessibility_annotator::EntryType::kCreditCardSecurityCode);
  EXPECT_THAT(
      cvc_results,
      UnorderedElementsAre(IsMemorySearchResult(
          std::u16string(3, kMidlineEllipsisPlainDot),
          GetEntryTypeNameForI18n(EntryType::kCreditCardSecurityCode),
          UnorderedElementsAre(
              IsMetadata(EntryType::kCreditCardNameOnCard,
                         credit_card.GetRawInfo(CREDIT_CARD_NAME_FULL)),
              IsMetadata(
                  EntryType::kCreditCardExpirationDate,
                  credit_card.GetRawInfo(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR)),
              IsMetadata(EntryType::kCreditCardNickname, u"My Credit Card"),
              IsMetadata(
                  EntryType::kCreditCardNumber,
                  credit_card.ObfuscatedNumberWithVisibleLastFourDigits())))));

  std::vector<MemorySearchResult> name_results = RetrieveAllHelper(
      retriever(),
      accessibility_annotator::EntryType::kCreditCardNameOnCard);
  EXPECT_THAT(
      name_results,
      UnorderedElementsAre(IsMemorySearchResult(
          credit_card.GetRawInfo(CREDIT_CARD_NAME_FULL),
          GetEntryTypeNameForI18n(EntryType::kCreditCardNameOnCard),
          UnorderedElementsAre(
              IsMetadata(
                  EntryType::kCreditCardExpirationDate,
                  credit_card.GetRawInfo(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR)),
              IsMetadata(EntryType::kCreditCardNickname, u"My Credit Card"),
              IsMetadata(
                  EntryType::kCreditCardNumber,
                  credit_card.ObfuscatedNumberWithVisibleLastFourDigits()),
              IsMetadata(EntryType::kCreditCardSecurityCode,
                         std::u16string(3, kMidlineEllipsisPlainDot))))));

  std::vector<MemorySearchResult> exp_results = RetrieveAllHelper(
      retriever(),
      accessibility_annotator::EntryType::kCreditCardExpirationDate);
  EXPECT_THAT(
      exp_results,
      UnorderedElementsAre(IsMemorySearchResult(
          credit_card.GetRawInfo(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR),
          GetEntryTypeNameForI18n(EntryType::kCreditCardExpirationDate),
          UnorderedElementsAre(
              IsMetadata(EntryType::kCreditCardNameOnCard,
                         credit_card.GetRawInfo(CREDIT_CARD_NAME_FULL)),
              IsMetadata(EntryType::kCreditCardNickname, u"My Credit Card"),
              IsMetadata(
                  EntryType::kCreditCardNumber,
                  credit_card.ObfuscatedNumberWithVisibleLastFourDigits()),
              IsMetadata(EntryType::kCreditCardSecurityCode,
                         std::u16string(3, kMidlineEllipsisPlainDot))))));
}

// Tests that RetrieveAll correctly fetches and formats data from
// EntityDataManager (Autofill AI).
TEST_F(AutofillDataProviderImplTest, RetrieveAll_AutofillAiEntityData) {
  EntityInstance vehicle = test::GetVehicleEntityInstance({.use_count = 1});
  entity_data_manager().AddOrUpdateEntityInstance(vehicle);
  WaitForDatabase();

  // Asking for Vehicle should return combined result and individual attributes.
  std::vector<MemorySearchResult> results = RetrieveAllHelper(
      retriever(), accessibility_annotator::EntryType::kVehicle);
  EXPECT_THAT(
      results,
      ElementsAre(IsMemorySearchResult(
          u"123456", u"Vehicle",
          ElementsAre(IsMetadata(EntryType::kVehicleMake, u"BMW"),
                      IsMetadata(EntryType::kVehicleModel, u"Series 2"),
                      IsMetadata(EntryType::kVehicleYear, u"2025"),
                      IsMetadata(EntryType::kVehicleOwner, u"Knecht Ruprecht"),
                      IsMetadata(EntryType::kVehiclePlateNumber, u"123456"),
                      IsMetadata(EntryType::kVehiclePlateState, u"California"),
                      IsMetadata(EntryType::kVehicleVin, u"12312345")))));
}

// Tests that RetrieveAll correctly formats Passport entity data.
TEST_F(AutofillDataProviderImplTest, RetrieveAll_PassportData) {
  EntityInstance passport =
      test::GetPassportEntityInstance({.number = u"XYZ123", .use_count = 1});
  entity_data_manager().AddOrUpdateEntityInstance(passport);
  WaitForDatabase();

  std::vector<MemorySearchResult> results = RetrieveAllHelper(
      retriever(), accessibility_annotator::EntryType::kPassportFull);
  ASSERT_FALSE(results.empty());

  auto it = std::find_if(
      results.begin(), results.end(), [](const MemorySearchResult& r) {
        return r.type == accessibility_annotator::EntryType::kPassportFull;
      });
  ASSERT_NE(it, results.end());

  EXPECT_EQ(it->value, u"XYZ123");
  ASSERT_FALSE(it->metadata_list.empty());
  EXPECT_THAT(
      it->metadata_list,
      Contains(IsMetadata(accessibility_annotator::EntryType::kPassportNumber,
                          u"XYZ123")));
}

// Tests that RetrieveAll correctly fetches data for a specific attribute.
TEST_F(AutofillDataProviderImplTest, RetrieveAll_AutofillAiAttributeData) {
  EntityInstance vehicle = test::GetVehicleEntityInstance({.use_count = 1});
  entity_data_manager().AddOrUpdateEntityInstance(vehicle);
  WaitForDatabase();

  EXPECT_THAT(
      RetrieveAllHelper(
          retriever(), accessibility_annotator::EntryType::kVehiclePlateNumber),
      UnorderedElementsAre(IsMemorySearchResult(
          u"123456", u"License plate",
          ElementsAre(IsMetadata(EntryType::kVehicleMake, u"BMW"),
                      IsMetadata(EntryType::kVehicleModel, u"Series 2"),
                      IsMetadata(EntryType::kVehicleYear, u"2025"),
                      IsMetadata(EntryType::kVehicleOwner, u"Knecht Ruprecht"),
                      IsMetadata(EntryType::kVehiclePlateState, u"California"),
                      IsMetadata(EntryType::kVehicleVin, u"12312345")))));
}

// Tests that RetrieveAll falls back to the first non-empty attribute for
// Vehicle when plate number is missing.
TEST_F(AutofillDataProviderImplTest,
       RetrieveAll_VehicleFallbackToFirstNonEmpty) {
  EntityInstance vehicle = test::GetVehicleEntityInstance(
      {.name = u"", .plate = u"", .make = u"BMW", .year = u"", .use_count = 1});
  entity_data_manager().AddOrUpdateEntityInstance(vehicle);
  WaitForDatabase();

  std::vector<MemorySearchResult> results = RetrieveAllHelper(
      retriever(), accessibility_annotator::EntryType::kVehicle);
  EXPECT_THAT(
      results,
      ElementsAre(IsMemorySearchResult(
          u"BMW", u"Vehicle",
          ElementsAre(IsMetadata(EntryType::kVehicleMake, u"BMW"),
                      IsMetadata(EntryType::kVehicleModel, u"Series 2"),
                      IsMetadata(EntryType::kVehiclePlateState, u"California"),
                      IsMetadata(EntryType::kVehicleVin, u"12312345")))));
}

// Tests that RetrieveAll omits address suggestions for profiles that only have
// a name but no address data.
TEST_F(AutofillDataProviderImplTest, RetrieveAll_AddressFull_EmptyProfile) {
  AutofillProfile profile(AddressCountryCode("US"));
  profile.SetRawInfo(NAME_FULL, u"Homer Simpson");
  client().GetPersonalDataManager().address_data_manager().AddProfile(profile);

  EXPECT_THAT(
      RetrieveAllHelper(retriever(),
                        accessibility_annotator::EntryType::kAddressFull),
      IsEmpty());
}

// Tests that RetrieveAll correctly formats address suggestions for
// partial addresses.
TEST_F(AutofillDataProviderImplTest, RetrieveAll_AddressFull_PartialAddress) {
  AutofillProfile profile(AddressCountryCode("US"));
  profile.SetRawInfo(NAME_FULL, u"Homer Simpson");
  profile.SetRawInfo(ADDRESS_HOME_STREET_ADDRESS, u"742 Evergreen Terrace");
  profile.SetRawInfo(ADDRESS_HOME_CITY, u"Springfield");
  // Missing State, Zip
  client().GetPersonalDataManager().address_data_manager().AddProfile(profile);

  std::vector<MemorySearchResult> results = RetrieveAllHelper(
      retriever(), accessibility_annotator::EntryType::kAddressFull);

  EXPECT_THAT(
      results,
      UnorderedElementsAre(IsMemorySearchResult(
          u"742 Evergreen Terrace, Springfield, United States", u"Address",
          UnorderedElementsAre(
              IsMetadata(EntryType::kNameFull, u"Homer Simpson"),
              IsMetadata(EntryType::kAddressStreetAddress,
                         u"742 Evergreen Terrace"),
              IsMetadata(EntryType::kAddressCity, u"Springfield"),
              IsMetadata(EntryType::kAddressCountry, u"United States")))));
}

}  // namespace

}  // namespace autofill
