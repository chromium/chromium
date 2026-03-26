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
using ::accessibility_annotator::QueryIntentType;
using ::testing::_;
using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::UnorderedElementsAre;

namespace {

Matcher<EntryMetadata> IsMetadata(QueryIntentType type,
                                  const std::u16string& value) {
  return AllOf(
      Field(&EntryMetadata::type, Eq(type)),
      Field(&EntryMetadata::type_name, Eq(GetEntryTypeNameForI18n(type))),
      Field(&EntryMetadata::value, Eq(value)));
}

Matcher<MemorySearchResult> IsMemorySearchResult(
    const std::u16string& value,
    const std::u16string& type_name,
    Matcher<std::vector<EntryMetadata>> metadata_matcher) {
  return AllOf(Field(&MemorySearchResult::value, Eq(value)),
               Field(&MemorySearchResult::type_name, Eq(type_name)),
               Field(&MemorySearchResult::metadata_list, metadata_matcher));
}

std::vector<MemorySearchResult> RetrieveAllHelper(
    AutofillDataProviderImpl& retriever,
    accessibility_annotator::QueryIntentType type) {
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
                        accessibility_annotator::QueryIntentType::kAddressCity),
      IsEmpty());
}

// Tests that RetrieveAll fetches and formats address-related data from
// PersonalDataManager.
TEST_F(AutofillDataProviderImplTest, RetrieveAll_AddressData) {
  AutofillProfile profile = test::GetFullProfile();
  client().GetPersonalDataManager().address_data_manager().AddProfile(profile);

  EXPECT_THAT(
      RetrieveAllHelper(retriever(),
                        accessibility_annotator::QueryIntentType::kAddressCity),
      UnorderedElementsAre(IsMemorySearchResult(
          u"Elysium", u"City",
          UnorderedElementsAre(
              IsMetadata(QueryIntentType::kNameFull, u"John H. Doe"),
              IsMetadata(QueryIntentType::kAddressState, u"CA"),
              IsMetadata(QueryIntentType::kAddressZip, u"91111"),
              IsMetadata(QueryIntentType::kAddressCountry,
                         u"United States")))));

  EXPECT_THAT(
      RetrieveAllHelper(retriever(),
                        accessibility_annotator::QueryIntentType::kAddressZip),
      UnorderedElementsAre(IsMemorySearchResult(
          u"91111", u"Zip",
          UnorderedElementsAre(
              IsMetadata(QueryIntentType::kNameFull, u"John H. Doe"),
              IsMetadata(QueryIntentType::kAddressCity, u"Elysium"),
              IsMetadata(QueryIntentType::kAddressState, u"CA"),
              IsMetadata(QueryIntentType::kAddressCountry,
                         u"United States")))));

  EXPECT_THAT(
      RetrieveAllHelper(
          retriever(), accessibility_annotator::QueryIntentType::kAddressState),
      UnorderedElementsAre(IsMemorySearchResult(
          u"CA", u"State",
          UnorderedElementsAre(
              IsMetadata(QueryIntentType::kNameFull, u"John H. Doe"),
              IsMetadata(QueryIntentType::kAddressCity, u"Elysium"),
              IsMetadata(QueryIntentType::kAddressZip, u"91111"),
              IsMetadata(QueryIntentType::kAddressCountry,
                         u"United States")))));

  EXPECT_THAT(RetrieveAllHelper(
                  retriever(),
                  accessibility_annotator::QueryIntentType::kAddressCountry),
              UnorderedElementsAre(IsMemorySearchResult(
                  u"United States", u"Country",
                  UnorderedElementsAre(
                      IsMetadata(QueryIntentType::kNameFull, u"John H. Doe"),
                      IsMetadata(QueryIntentType::kAddressCity, u"Elysium"),
                      IsMetadata(QueryIntentType::kAddressState, u"CA"),
                      IsMetadata(QueryIntentType::kAddressZip, u"91111")))));

  EXPECT_THAT(
      RetrieveAllHelper(retriever(),
                        accessibility_annotator::QueryIntentType::kNameFull),
      UnorderedElementsAre(IsMemorySearchResult(
          u"John H. Doe", u"Name",
          UnorderedElementsAre(
              IsMetadata(QueryIntentType::kAddressCity, u"Elysium"),
              IsMetadata(QueryIntentType::kAddressState, u"CA"),
              IsMetadata(QueryIntentType::kAddressZip, u"91111"),
              IsMetadata(QueryIntentType::kAddressCountry,
                         u"United States")))));

  EXPECT_THAT(
      RetrieveAllHelper(retriever(),
                        accessibility_annotator::QueryIntentType::kEmail),
      UnorderedElementsAre(IsMemorySearchResult(
          u"johndoe@hades.com", u"Email",
          UnorderedElementsAre(
              IsMetadata(QueryIntentType::kNameFull, u"John H. Doe"),
              IsMetadata(QueryIntentType::kAddressCity, u"Elysium"),
              IsMetadata(QueryIntentType::kAddressState, u"CA"),
              IsMetadata(QueryIntentType::kAddressZip, u"91111"),
              IsMetadata(QueryIntentType::kAddressCountry,
                         u"United States")))));

  EXPECT_THAT(
      RetrieveAllHelper(retriever(),
                        accessibility_annotator::QueryIntentType::kPhone),
      UnorderedElementsAre(IsMemorySearchResult(
          u"16502111111", u"Phone",
          UnorderedElementsAre(
              IsMetadata(QueryIntentType::kNameFull, u"John H. Doe"),
              IsMetadata(QueryIntentType::kAddressCity, u"Elysium"),
              IsMetadata(QueryIntentType::kAddressState, u"CA"),
              IsMetadata(QueryIntentType::kAddressZip, u"91111"),
              IsMetadata(QueryIntentType::kAddressCountry,
                         u"United States")))));

  // Requesting for address should return only the full address.
  EXPECT_THAT(
      RetrieveAllHelper(retriever(),
                        accessibility_annotator::QueryIntentType::kAddressFull),
      UnorderedElementsAre(IsMemorySearchResult(
          u"Underworld, 666 Erebus St., Apt 8, Elysium, CA 91111, "
          u"United States",
          u"Address",
          UnorderedElementsAre(
              IsMetadata(QueryIntentType::kNameFull, u"John H. Doe"),
              IsMetadata(QueryIntentType::kAddressCity, u"Elysium"),
              IsMetadata(QueryIntentType::kAddressZip, u"91111"),
              IsMetadata(QueryIntentType::kAddressState, u"CA"),
              IsMetadata(QueryIntentType::kAddressCountry,
                         u"United States")))));
}

// Tests that RetrieveAll correctly fetches and formats IBAN data.
TEST_F(AutofillDataProviderImplTest, RetrieveAll_IbanData) {
  Iban iban = test::GetLocalIban();
  iban.set_nickname(u"My IBAN");
  client().GetPersonalDataManager().test_payments_data_manager().AddIbanForTest(
      std::make_unique<Iban>(iban));

  std::vector<MemorySearchResult> results = RetrieveAllHelper(
      retriever(), accessibility_annotator::QueryIntentType::kIban);
  EXPECT_THAT(results, UnorderedElementsAre(IsMemorySearchResult(
                           iban.value(), u"IBAN",
                           UnorderedElementsAre(IsMetadata(
                               QueryIntentType::kIbanNickname, u"My IBAN")))));
}

// Tests that RetrieveAll correctly fetches and formats data from
// EntityDataManager (Autofill AI).
TEST_F(AutofillDataProviderImplTest, RetrieveAll_AutofillAiEntityData) {
  EntityInstance vehicle = test::GetVehicleEntityInstance({.use_count = 1});
  entity_data_manager().AddOrUpdateEntityInstance(vehicle);
  WaitForDatabase();

  // Asking for Vehicle should return combined result and individual attributes.
  std::vector<MemorySearchResult> results = RetrieveAllHelper(
      retriever(), accessibility_annotator::QueryIntentType::kVehicle);
  EXPECT_THAT(
      results,
      ElementsAre(
          IsMemorySearchResult(
              u"BMW Series 2 2025 Knecht Ruprecht 123456 California 12312345",
              u"Vehicle",
              ElementsAre(
                  IsMetadata(QueryIntentType::kVehicleMake, u"BMW"),
                  IsMetadata(QueryIntentType::kVehicleModel, u"Series 2"),
                  IsMetadata(QueryIntentType::kVehicleYear, u"2025"),
                  IsMetadata(QueryIntentType::kVehicleOwner,
                             u"Knecht Ruprecht"),
                  IsMetadata(QueryIntentType::kVehiclePlateNumber, u"123456"),
                  IsMetadata(QueryIntentType::kVehiclePlateState,
                             u"California"),
                  IsMetadata(QueryIntentType::kVehicleVin, u"12312345"))),
          IsMemorySearchResult(
              u"BMW", u"Make",
              ElementsAre(
                  IsMetadata(QueryIntentType::kVehicleModel, u"Series 2"),
                  IsMetadata(QueryIntentType::kVehicleYear, u"2025"),
                  IsMetadata(QueryIntentType::kVehicleOwner,
                             u"Knecht Ruprecht"),
                  IsMetadata(QueryIntentType::kVehiclePlateNumber, u"123456"),
                  IsMetadata(QueryIntentType::kVehiclePlateState,
                             u"California"),
                  IsMetadata(QueryIntentType::kVehicleVin, u"12312345"))),
          IsMemorySearchResult(
              u"Series 2", u"Model",
              ElementsAre(
                  IsMetadata(QueryIntentType::kVehicleMake, u"BMW"),
                  IsMetadata(QueryIntentType::kVehicleYear, u"2025"),
                  IsMetadata(QueryIntentType::kVehicleOwner,
                             u"Knecht Ruprecht"),
                  IsMetadata(QueryIntentType::kVehiclePlateNumber, u"123456"),
                  IsMetadata(QueryIntentType::kVehiclePlateState,
                             u"California"),
                  IsMetadata(QueryIntentType::kVehicleVin, u"12312345"))),
          IsMemorySearchResult(
              u"2025", u"Year",
              ElementsAre(
                  IsMetadata(QueryIntentType::kVehicleMake, u"BMW"),
                  IsMetadata(QueryIntentType::kVehicleModel, u"Series 2"),
                  IsMetadata(QueryIntentType::kVehicleOwner,
                             u"Knecht Ruprecht"),
                  IsMetadata(QueryIntentType::kVehiclePlateNumber, u"123456"),
                  IsMetadata(QueryIntentType::kVehiclePlateState,
                             u"California"),
                  IsMetadata(QueryIntentType::kVehicleVin, u"12312345"))),
          IsMemorySearchResult(
              u"Knecht Ruprecht", u"Owner",
              ElementsAre(
                  IsMetadata(QueryIntentType::kVehicleMake, u"BMW"),
                  IsMetadata(QueryIntentType::kVehicleModel, u"Series 2"),
                  IsMetadata(QueryIntentType::kVehicleYear, u"2025"),
                  IsMetadata(QueryIntentType::kVehiclePlateNumber, u"123456"),
                  IsMetadata(QueryIntentType::kVehiclePlateState,
                             u"California"),
                  IsMetadata(QueryIntentType::kVehicleVin, u"12312345"))),
          IsMemorySearchResult(
              u"123456", u"License plate",
              ElementsAre(
                  IsMetadata(QueryIntentType::kVehicleMake, u"BMW"),
                  IsMetadata(QueryIntentType::kVehicleModel, u"Series 2"),
                  IsMetadata(QueryIntentType::kVehicleYear, u"2025"),
                  IsMetadata(QueryIntentType::kVehicleOwner,
                             u"Knecht Ruprecht"),
                  IsMetadata(QueryIntentType::kVehiclePlateState,
                             u"California"),
                  IsMetadata(QueryIntentType::kVehicleVin, u"12312345"))),
          IsMemorySearchResult(
              u"California", u"Plate state",
              ElementsAre(
                  IsMetadata(QueryIntentType::kVehicleMake, u"BMW"),
                  IsMetadata(QueryIntentType::kVehicleModel, u"Series 2"),
                  IsMetadata(QueryIntentType::kVehicleYear, u"2025"),
                  IsMetadata(QueryIntentType::kVehicleOwner,
                             u"Knecht Ruprecht"),
                  IsMetadata(QueryIntentType::kVehiclePlateNumber, u"123456"),
                  IsMetadata(QueryIntentType::kVehicleVin, u"12312345"))),
          IsMemorySearchResult(
              u"12312345", u"VIN (Vehicle Identification Number)",
              ElementsAre(
                  IsMetadata(QueryIntentType::kVehicleMake, u"BMW"),
                  IsMetadata(QueryIntentType::kVehicleModel, u"Series 2"),
                  IsMetadata(QueryIntentType::kVehicleYear, u"2025"),
                  IsMetadata(QueryIntentType::kVehicleOwner,
                             u"Knecht Ruprecht"),
                  IsMetadata(QueryIntentType::kVehiclePlateNumber, u"123456"),
                  IsMetadata(QueryIntentType::kVehiclePlateState,
                             u"California")))));

  // Asking specifically for Entity Attribute
  EXPECT_THAT(
      RetrieveAllHelper(
          retriever(),
          accessibility_annotator::QueryIntentType::kVehiclePlateNumber),
      UnorderedElementsAre(IsMemorySearchResult(
          u"123456", u"License plate",
          ElementsAre(
              IsMetadata(QueryIntentType::kVehicleMake, u"BMW"),
              IsMetadata(QueryIntentType::kVehicleModel, u"Series 2"),
              IsMetadata(QueryIntentType::kVehicleYear, u"2025"),
              IsMetadata(QueryIntentType::kVehicleOwner, u"Knecht Ruprecht"),
              IsMetadata(QueryIntentType::kVehiclePlateState, u"California"),
              IsMetadata(QueryIntentType::kVehicleVin, u"12312345")))));
}

// Tests that RetrieveAll omits address suggestions for profiles that only have
// a name but no address data.
TEST_F(AutofillDataProviderImplTest, RetrieveAll_AddressFull_EmptyProfile) {
  AutofillProfile profile(AddressCountryCode("US"));
  profile.SetRawInfo(NAME_FULL, u"Homer Simpson");
  client().GetPersonalDataManager().address_data_manager().AddProfile(profile);

  EXPECT_THAT(
      RetrieveAllHelper(retriever(),
                        accessibility_annotator::QueryIntentType::kAddressFull),
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
      retriever(), accessibility_annotator::QueryIntentType::kAddressFull);

  EXPECT_THAT(
      results,
      UnorderedElementsAre(IsMemorySearchResult(
          u"742 Evergreen Terrace, Springfield, United States", u"Address",
          UnorderedElementsAre(
              IsMetadata(QueryIntentType::kNameFull, u"Homer Simpson"),
              IsMetadata(QueryIntentType::kAddressCity, u"Springfield"),
              IsMetadata(QueryIntentType::kAddressCountry,
                         u"United States")))));
}

}  // namespace

}  // namespace autofill
