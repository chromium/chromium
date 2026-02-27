// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/at_memory/autofill_data_retriever.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "components/autofill/core/browser/at_memory/at_memory_data_type.h"
#include "components/autofill/core/browser/at_memory/memory_search_result.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_manager/test_personal_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
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

using ::testing::AllOf;
using ::testing::Contains;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Matcher;
using ::testing::UnorderedElementsAre;

namespace {

Matcher<MemorySearchResult> IsMemorySearchResult(
    const std::u16string& value,
    const std::u16string& title,
    const std::u16string& description) {
  return AllOf(Field(&MemorySearchResult::value, value),
               Field(&MemorySearchResult::title, title),
               Field(&MemorySearchResult::description, description));
}

class AutofillDataRetrieverTest : public testing::Test {
 public:
  AutofillDataRetrieverTest()
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
        /*accessibility_annotator_data_adapter=*/nullptr,
        /*variation_country_code=*/GeoIpCountryCode("US"));
    entity_data_manager_ = entity_data_manager.get();
    client_.set_entity_data_manager(std::move(entity_data_manager));

    retriever_ = std::make_unique<AutofillDataRetriever>(client_);
  }

  void WaitForDatabase() { webdata_helper_.WaitUntilIdle(); }

  AutofillDataRetriever& retriever() { return *retriever_; }
  TestAutofillClient& client() { return client_; }
  EntityDataManager& entity_data_manager() { return *entity_data_manager_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  AutofillWebDataServiceTestHelper webdata_helper_;
  syncer::TestSyncService sync_service_;
  TestAutofillClient client_;
  raw_ptr<EntityDataManager> entity_data_manager_;
  std::unique_ptr<AutofillDataRetriever> retriever_;
};

// Tests that RetrieveAll returns an empty list when no data is available
TEST_F(AutofillDataRetrieverTest, RetrieveAll_Empty) {
  EXPECT_THAT(retriever().RetrieveAll(ADDRESS_HOME_CITY), IsEmpty());
}

// Tests that RetrieveAll fetches and formats address-related data from
// PersonalDataManager.
TEST_F(AutofillDataRetrieverTest, RetrieveAll_AddressData) {
  AutofillProfile profile = test::GetFullProfile();
  client().GetPersonalDataManager().address_data_manager().AddProfile(profile);

  EXPECT_THAT(retriever().RetrieveAll(ADDRESS_HOME_CITY),
              UnorderedElementsAre(IsMemorySearchResult(
                  u"Elysium", u"Elysium", u"Address: John H. Doe")));

  EXPECT_THAT(retriever().RetrieveAll(ADDRESS_HOME_ZIP),
              UnorderedElementsAre(IsMemorySearchResult(
                  u"91111", u"91111", u"Address: John H. Doe")));

  EXPECT_THAT(retriever().RetrieveAll(ADDRESS_HOME_STATE),
              UnorderedElementsAre(
                  IsMemorySearchResult(u"CA", u"CA", u"Address: John H. Doe")));

  EXPECT_THAT(retriever().RetrieveAll(ADDRESS_HOME_COUNTRY),
              UnorderedElementsAre(
                  IsMemorySearchResult(u"US", u"US", u"Address: John H. Doe")));

  EXPECT_THAT(retriever().RetrieveAll(NAME_FULL),
              UnorderedElementsAre(IsMemorySearchResult(
                  u"John H. Doe", u"John H. Doe", u"Address: John H. Doe")));

  EXPECT_THAT(retriever().RetrieveAll(EMAIL_ADDRESS),
              UnorderedElementsAre(IsMemorySearchResult(
                  u"johndoe@hades.com", u"johndoe@hades.com",
                  u"Address: John H. Doe")));

  EXPECT_THAT(retriever().RetrieveAll(PHONE_HOME_WHOLE_NUMBER),
              UnorderedElementsAre(IsMemorySearchResult(
                  u"16502111111", u"16502111111", u"Address: John H. Doe")));

  // Requesting for address should return both the street address and the
  // constructed full address.
  EXPECT_THAT(retriever().RetrieveAll(ADDRESS_HOME_ADDRESS),
              UnorderedElementsAre(
                  IsMemorySearchResult(u"666 Erebus St.\nApt 8",
                                       u"666 Erebus St.\nApt 8",
                                       u"Address: John H. Doe"),
                  IsMemorySearchResult(
                      u"Underworld, 666 Erebus St., Apt 8, Elysium, CA 91111, "
                      u"United States",
                      u"Underworld, 666 Erebus St., Apt 8, Elysium, CA 91111, "
                      u"United States",
                      u"Address: John H. Doe")));
}

// Tests that RetrieveAll correctly fetches and formats IBAN data.
TEST_F(AutofillDataRetrieverTest, RetrieveAll_IbanData) {
  Iban iban = test::GetLocalIban();
  iban.set_nickname(u"My IBAN");
  client().GetPersonalDataManager().test_payments_data_manager().AddIbanForTest(
      std::make_unique<Iban>(iban));

  std::vector<MemorySearchResult> results = retriever().RetrieveAll(IBAN_VALUE);
  EXPECT_THAT(results,
              UnorderedElementsAre(IsMemorySearchResult(
                  iban.value(), iban.GetIdentifierStringForAutofillDisplay(),
                  u"IBAN: My IBAN")));
}

// Tests that RetrieveAll correctly fetches and formats data from
// EntityDataManager (Autofill AI).
TEST_F(AutofillDataRetrieverTest, RetrieveAll_AutofillAiEntityData) {
  EntityInstance vehicle = test::GetVehicleEntityInstance({.use_count = 1});
  entity_data_manager().AddOrUpdateEntityInstance(vehicle);
  WaitForDatabase();

  // Asking for Vehicle should return combined "Make Model" result
  std::vector<MemorySearchResult> results =
      retriever().RetrieveAll(EntityType(EntityTypeName::kVehicle));
  EXPECT_THAT(
      results,
      AllOf(Contains(IsMemorySearchResult(u"BMW Series 2", u"BMW Series 2",
                                          u"Vehicle")),
            Contains(IsMemorySearchResult(u"BMW", u"BMW", u"Vehicle - Make")),
            Contains(IsMemorySearchResult(u"Series 2", u"Series 2",
                                          u"Vehicle - Model")),
            Contains(IsMemorySearchResult(u"123456", u"123456",
                                          u"Vehicle - License plate"))));

  // VehiclePlate
  EXPECT_THAT(retriever().RetrieveAll(
                  AttributeType(AttributeTypeName::kVehiclePlateNumber)),
              UnorderedElementsAre(IsMemorySearchResult(
                  u"123456", u"123456", u"Vehicle - License plate")));
}

}  // namespace

}  // namespace autofill
