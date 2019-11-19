// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_data_importer.h"

#include <stddef.h>

#include <algorithm>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/guid.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager_observer.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/os_crypt/os_crypt_mocker.h"
#include "components/prefs/pref_service.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_database_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;
using testing::_;
using testing::AtLeast;
using testing::Invoke;
using testing::Return;
using testing::SaveArg;
using testing::UnorderedElementsAre;
using testing::WithoutArgs;

namespace autofill {
namespace {

ACTION_P(QuitMessageLoop, loop) {
  loop->Quit();
}

enum UserMode { USER_MODE_NORMAL, USER_MODE_INCOGNITO };

class PersonalDataLoadedObserverMock : public PersonalDataManagerObserver {
 public:
  PersonalDataLoadedObserverMock() {}
  ~PersonalDataLoadedObserverMock() override {}

  MOCK_METHOD0(OnPersonalDataChanged, void());
  MOCK_METHOD0(OnPersonalDataFinishedProfileTasks, void());
};

template <typename T>
bool CompareElements(T* a, T* b) {
  return a->Compare(*b) < 0;
}

template <typename T>
bool ElementsEqual(T* a, T* b) {
  return a->Compare(*b) == 0;
}

// Verifies that two vectors have the same elements (according to T::Compare)
// while ignoring order. This is useful because multiple profiles or credit
// cards that are added to the SQLite DB within the same second will be returned
// in GUID (aka random) order.
template <typename T>
void ExpectSameElements(const std::vector<T*>& expectations,
                        const std::vector<T*>& results) {
  ASSERT_EQ(expectations.size(), results.size());

  std::vector<T*> expectations_copy = expectations;
  std::sort(expectations_copy.begin(), expectations_copy.end(),
            CompareElements<T>);
  std::vector<T*> results_copy = results;
  std::sort(results_copy.begin(), results_copy.end(), CompareElements<T>);

  EXPECT_EQ(std::mismatch(results_copy.begin(), results_copy.end(),
                          expectations_copy.begin(), ElementsEqual<T>)
                .first,
            results_copy.end());
}

}  // anonymous namespace

class FormDataImporterTestBase {
 protected:
  FormDataImporterTestBase() : autofill_table_(nullptr) {}

  void ResetPersonalDataManager(UserMode user_mode) {
    personal_data_manager_.reset(new PersonalDataManager("en"));
    personal_data_manager_->Init(
        scoped_refptr<AutofillWebDataService>(autofill_database_service_),
        /*account_database=*/nullptr,
        /*pref_service=*/prefs_.get(),
        /*identity_manager=*/nullptr,
        /*client_profile_validator=*/nullptr,
        /*history_service=*/nullptr,
        /*is_off_the_record=*/(user_mode == USER_MODE_INCOGNITO));
    personal_data_manager_->AddObserver(&personal_data_observer_);
    personal_data_manager_->OnSyncServiceInitialized(nullptr);

    WaitForOnPersonalDataChanged();
  }

  void EnableWalletCardImport() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableOfferStoreUnmaskedWalletCards);
  }

  // Helper method that will add credit card fields in |form|, according to the
  // specified values. If a value is nullptr, the corresponding field won't get
  // added (empty string will add a field with an empty string as the value).
  void AddFullCreditCardForm(FormData* form,
                             const char* name,
                             const char* number,
                             const char* month,
                             const char* year) {
    FormFieldData field;
    if (name) {
      test::CreateTestFormField("Name on card:", "name_on_card", name, "text",
                                &field);
      form->fields.push_back(field);
    }
    if (number) {
      test::CreateTestFormField("Card Number:", "card_number", number, "text",
                                &field);
      form->fields.push_back(field);
    }
    if (month) {
      test::CreateTestFormField("Exp Month:", "exp_month", month, "text",
                                &field);
      form->fields.push_back(field);
    }
    if (year) {
      test::CreateTestFormField("Exp Year:", "exp_year", year, "text", &field);
      form->fields.push_back(field);
    }
  }

  // Helper methods that simply forward the call to the private member (to avoid
  // having to friend every test that needs to access the private
  // PersonalDataManager::ImportAddressProfile or ImportCreditCard).
  void ImportAddressProfiles(bool extraction_successful,
                             const FormStructure& form) {
    if (!extraction_successful) {
      EXPECT_FALSE(form_data_importer_->ImportAddressProfiles(form));
      return;
    }

    base::RunLoop run_loop;
    EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
        .WillOnce(QuitMessageLoop(&run_loop));
    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged()).Times(1);
    EXPECT_TRUE(form_data_importer_->ImportAddressProfiles(form));
    run_loop.Run();
  }

  bool ImportCreditCard(const FormStructure& form,
                        bool should_return_local_card,
                        std::unique_ptr<CreditCard>* imported_credit_card) {
    return form_data_importer_->ImportCreditCard(form, should_return_local_card,
                                                 imported_credit_card);
  }

  void SubmitFormAndExpectImportedCardWithData(const FormData& form,
                                               const char* exp_name,
                                               const char* exp_cc_num,
                                               const char* exp_cc_month,
                                               const char* exp_cc_year) {
    FormStructure form_structure(form);
    form_structure.DetermineHeuristicTypes();
    std::unique_ptr<CreditCard> imported_credit_card;
    EXPECT_TRUE(ImportCreditCard(form_structure, false, &imported_credit_card));
    ASSERT_TRUE(imported_credit_card);
    personal_data_manager_->OnAcceptedLocalCreditCardSave(
        *imported_credit_card);

    WaitForOnPersonalDataChanged();
    CreditCard expected(base::GenerateGUID(), test::kEmptyOrigin);
    test::SetCreditCardInfo(&expected, exp_name, exp_cc_num, exp_cc_month,
                            exp_cc_year, "");
    const std::vector<CreditCard*>& results =
        personal_data_manager_->GetCreditCards();
    ASSERT_EQ(1U, results.size());
    EXPECT_EQ(0, expected.Compare(*results[0]));
  }

  void WaitForOnPersonalDataChanged() {
    base::RunLoop run_loop;
    EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
        .WillOnce(QuitMessageLoop(&run_loop));
    EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
        .Times(testing::AnyNumber());
    run_loop.Run();
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::MainThreadType::UI};
  std::unique_ptr<PrefService> prefs_;
  scoped_refptr<AutofillWebDataService> autofill_database_service_;
  scoped_refptr<WebDatabaseService> web_database_;
  AutofillTable* autofill_table_;  // weak ref
  PersonalDataLoadedObserverMock personal_data_observer_;
  std::unique_ptr<TestAutofillClient> autofill_client_;
  std::unique_ptr<PersonalDataManager> personal_data_manager_;
  std::unique_ptr<FormDataImporter> form_data_importer_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

class FormDataImporterTest : public FormDataImporterTestBase,
                             public testing::Test {
  void SetUp() override {
    OSCryptMocker::SetUp();
    prefs_ = test::PrefServiceForTesting();
    base::FilePath path(WebDatabase::kInMemoryPath);
    web_database_ =
        new WebDatabaseService(path, base::ThreadTaskRunnerHandle::Get(),
                               base::ThreadTaskRunnerHandle::Get());

    // Hacky: hold onto a pointer but pass ownership.
    autofill_table_ = new AutofillTable;
    web_database_->AddTable(std::unique_ptr<WebDatabaseTable>(autofill_table_));
    web_database_->LoadDatabase();
    autofill_database_service_ = new AutofillWebDataService(
        web_database_, base::ThreadTaskRunnerHandle::Get(),
        base::ThreadTaskRunnerHandle::Get(),
        WebDataServiceBase::ProfileErrorCallback());
    autofill_database_service_->Init();

    autofill_client_ = std::make_unique<TestAutofillClient>();

    test::DisableSystemServices(prefs_.get());
    ResetPersonalDataManager(USER_MODE_NORMAL);

    form_data_importer_.reset(
        new FormDataImporter(autofill_client_.get(),
                             /*payments::PaymentsClient=*/nullptr,
                             personal_data_manager_.get(), "en"));

    // Reset the deduping pref to its default value.
    personal_data_manager_->pref_service_->SetInteger(
        prefs::kAutofillLastVersionDeduped, 0);
  }

  void TearDown() override {
    // Order of destruction is important as AutofillManager relies on
    // PersonalDataManager to be around when it gets destroyed.
    test::ReenableSystemServices();
    OSCryptMocker::TearDown();
  }
};

// ImportAddressProfiles tests.

TEST_F(FormDataImporterTest, ImportAddressProfiles) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  ImportAddressProfiles(/*extraction_success=*/true, form_structure);

  AutofillProfile expected(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&expected, "George", nullptr, "Washington",
                       "theprez@gmail.com", nullptr, "21 Laussat St", nullptr,
                       "San Francisco", "California", "94102", nullptr,
                       nullptr);
  const std::vector<AutofillProfile*>& results =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));
}

TEST_F(FormDataImporterTest, ImportAddressProfiles_BadEmail) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "bogus", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  ImportAddressProfiles(/*extraction_success=*/false, form_structure);

  ASSERT_EQ(0U, personal_data_manager_->GetProfiles().size());
}

// Tests that a 'confirm email' field does not block profile import.
TEST_F(FormDataImporterTest, ImportAddressProfiles_TwoEmails) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name:", "name", "George Washington", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "example@example.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Confirm email:", "confirm_email",
                            "example@example.com", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  ImportAddressProfiles(/*extraction_success=*/true, form_structure);

  ASSERT_EQ(1U, personal_data_manager_->GetProfiles().size());
}

// Tests two email fields containing different values blocks profile import.
TEST_F(FormDataImporterTest, ImportAddressProfiles_TwoDifferentEmails) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name:", "name", "George Washington", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "example@example.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email2", "example2@example.com", "text",
                            &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  ImportAddressProfiles(/*extraction_success=*/false, form_structure);

  ASSERT_EQ(0U, personal_data_manager_->GetProfiles().size());
}

// Tests that not enough filled fields will result in not importing an address.
TEST_F(FormDataImporterTest, ImportAddressProfiles_NotEnoughFilledFields) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card number:", "card_number",
                            "4111 1111 1111 1111", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  ImportAddressProfiles(/*extraction_success=*/false, form_structure);

  ASSERT_EQ(0U, personal_data_manager_->GetProfiles().size());
  ASSERT_EQ(0U, personal_data_manager_->GetCreditCards().size());
}

TEST_F(FormDataImporterTest, ImportAddressProfiles_MinimumAddressUSA) {
  // United States addresses must specifiy one address line, a city, state and
  // zip code.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name:", "name", "Barack Obama", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address", "1600 Pennsylvania Avenue",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "Washington", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "DC", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "20500", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Country:", "country", "USA", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  ImportAddressProfiles(/*extraction_success=*/true, form_structure);

  ASSERT_EQ(1U, personal_data_manager_->GetProfiles().size());
}

TEST_F(FormDataImporterTest, ImportAddressProfiles_MinimumAddressGB) {
  // British addresses do not require a state/province as the county is usually
  // not requested on forms.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name:", "name", "David Cameron", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address", "10 Downing Street", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "London", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Postcode:", "postcode", "SW1A 2AA", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Country:", "country", "United Kingdom", "text",
                            &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  ImportAddressProfiles(/*extraction_success=*/true, form_structure);

  ASSERT_EQ(1U, personal_data_manager_->GetProfiles().size());
}

TEST_F(FormDataImporterTest, ImportAddressProfiles_MinimumAddressGI) {
  // Gibraltar has the most minimal set of requirements for a valid address.
  // There are no cities or provinces and no postal/zip code system.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name:", "name", "Sir Adrian Johns", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address", "The Convent, Main Street",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Country:", "country", "Gibraltar", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  ImportAddressProfiles(/*extraction_success=*/true, form_structure);

  ASSERT_EQ(1U, personal_data_manager_->GetProfiles().size());
}

TEST_F(FormDataImporterTest,
       ImportAddressProfiles_PhoneNumberSplitAcrossMultipleFields) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Phone #:", "home_phone_area_code", "650", "text",
                            &field);
  field.max_length = 3;
  form.fields.push_back(field);
  test::CreateTestFormField("Phone #:", "home_phone_prefix", "555", "text",
                            &field);
  field.max_length = 3;
  form.fields.push_back(field);
  test::CreateTestFormField("Phone #:", "home_phone_suffix", "0000", "text",
                            &field);
  field.max_length = 4;
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  ImportAddressProfiles(/*extraction_success=*/true, form_structure);

  AutofillProfile expected(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&expected, "George", nullptr, "Washington", nullptr,
                       nullptr, "21 Laussat St", nullptr, "San Francisco",
                       "California", "94102", nullptr, "(650) 555-0000");
  const std::vector<AutofillProfile*>& results =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));
}

TEST_F(FormDataImporterTest, ImportAddressProfiles_MultilineAddress) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "street_address",
                            "21 Laussat St\n"
                            "Apt. #42",
                            "textarea", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  ImportAddressProfiles(/*extraction_success=*/true, form_structure);

  AutofillProfile expected(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&expected, "George", nullptr, "Washington",
                       "theprez@gmail.com", nullptr, "21 Laussat St",
                       "Apt. #42", "San Francisco", "California", "94102",
                       nullptr, nullptr);
  const std::vector<AutofillProfile*>& results =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));
}

TEST_F(FormDataImporterTest,
       ImportAddressProfiles_TwoValidProfilesDifferentForms) {
  FormData form1;
  form1.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes();
  ImportAddressProfiles(/*extraction_success=*/true, form_structure1);

  AutofillProfile expected(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&expected, "George", nullptr, "Washington",
                       "theprez@gmail.com", nullptr, "21 Laussat St", nullptr,
                       "San Francisco", "California", "94102", nullptr,
                       nullptr);
  const std::vector<AutofillProfile*>& results1 =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results1.size());
  EXPECT_EQ(0, expected.Compare(*results1[0]));

  // Now create a completely different profile.
  FormData form2;
  form2.url = GURL("https://wwww.foo.com");

  test::CreateTestFormField("First name:", "first_name", "John", "text",
                            &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Adams", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "second@gmail.com", "text",
                            &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "22 Laussat St", "text",
                            &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes();
  ImportAddressProfiles(/*extraction_success=*/true, form_structure2);

  AutofillProfile expected2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&expected2, "John", nullptr, "Adams", "second@gmail.com",
                       nullptr, "22 Laussat St", nullptr, "San Francisco",
                       "California", "94102", nullptr, nullptr);
  std::vector<AutofillProfile*> profiles;
  profiles.push_back(&expected);
  profiles.push_back(&expected2);
  ExpectSameElements(profiles, personal_data_manager_->GetProfiles());
}

TEST_F(FormDataImporterTest, ImportAddressProfiles_TwoValidProfilesSameForm) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);

  // Different address.
  test::CreateTestFormField("First name:", "first_name", "John", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Adams", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "second@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "22 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  ImportAddressProfiles(/*extraction_success=*/true, form_structure);

  AutofillProfile expected(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&expected, "George", nullptr, "Washington",
                       "theprez@gmail.com", nullptr, "21 Laussat St", nullptr,
                       "San Francisco", "California", "94102", nullptr,
                       nullptr);
  AutofillProfile expected2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&expected2, "John", nullptr, "Adams", "second@gmail.com",
                       nullptr, "22 Laussat St", nullptr, "San Francisco",
                       "California", "94102", nullptr, nullptr);

  const std::vector<AutofillProfile*>& results =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(2U, results.size());

  std::vector<AutofillProfile*> profiles;
  profiles.push_back(&expected);
  profiles.push_back(&expected2);
  ExpectSameElements(profiles, personal_data_manager_->GetProfiles());
}

TEST_F(FormDataImporterTest,
       ImportAddressProfiles_OneValidProfileSameForm_PartsHidden) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);

  // There is an empty but hidden form section (this has been observed on sites
  // where users can choose which form section they choose by unhiding it).
  test::CreateTestFormField("First name:", "first_name", "", "text", &field);
  field.is_focusable = false;
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "", "text", &field);
  field.is_focusable = false;
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "", "text", &field);
  field.is_focusable = false;
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "", "text", &field);
  field.is_focusable = false;
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "", "text", &field);
  field.is_focusable = false;
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "", "text", &field);
  field.is_focusable = false;
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "", "text", &field);
  field.is_focusable = false;
  form.fields.push_back(field);

  // Still able to do the import.
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  ImportAddressProfiles(/*extraction_success=*/true, form_structure);

  AutofillProfile expected(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&expected, "George", nullptr, "Washington",
                       "theprez@gmail.com", nullptr, "21 Laussat St", nullptr,
                       "San Francisco", "California", "94102", nullptr,
                       nullptr);

  const std::vector<AutofillProfile*>& results =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results.size());

  std::vector<AutofillProfile*> profiles;
  profiles.push_back(&expected);
  ExpectSameElements(profiles, personal_data_manager_->GetProfiles());
}

// A maximum of two address profiles are imported per form.
TEST_F(FormDataImporterTest, ImportAddressProfiles_ThreeValidProfilesSameForm) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);

  // Different address within the same form.
  test::CreateTestFormField("First name:", "first_name", "John", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Adams", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "second@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "22 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);

  // Yet another different address.
  test::CreateTestFormField("First name:", "first_name", "David", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Cameron", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address", "10 Downing Street", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "London", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Postcode:", "postcode", "SW1A 2AA", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Country:", "country", "United Kingdom", "text",
                            &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  ImportAddressProfiles(/*extraction_success=*/true, form_structure);

  // Only two are saved.
  AutofillProfile expected(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&expected, "George", nullptr, "Washington",
                       "theprez@gmail.com", nullptr, "21 Laussat St", nullptr,
                       "San Francisco", "California", "94102", nullptr,
                       nullptr);
  AutofillProfile expected2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&expected2, "John", nullptr, "Adams", "second@gmail.com",
                       nullptr, "22 Laussat St", nullptr, "San Francisco",
                       "California", "94102", nullptr, nullptr);

  const std::vector<AutofillProfile*>& results =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(2U, results.size());

  std::vector<AutofillProfile*> profiles;
  profiles.push_back(&expected);
  profiles.push_back(&expected2);
  ExpectSameElements(profiles, personal_data_manager_->GetProfiles());
}

TEST_F(FormDataImporterTest, ImportAddressProfiles_SameProfileWithConflict) {
  FormData form1;
  form1.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Address:", "address", "1600 Pennsylvania Avenue",
                            "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Address Line 2:", "address2", "Suite A", "text",
                            &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Phone:", "phone", "6505556666", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes();
  ImportAddressProfiles(/*extraction_success=*/true, form_structure1);

  AutofillProfile expected(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&expected, "George", nullptr, "Washington",
                       "theprez@gmail.com", nullptr, "1600 Pennsylvania Avenue",
                       "Suite A", "San Francisco", "California", "94102",
                       nullptr, "(650) 555-6666");
  const std::vector<AutofillProfile*>& results1 =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results1.size());
  EXPECT_EQ(0, expected.Compare(*results1[0]));

  // Now create an updated profile.
  FormData form2;
  form2.url = GURL("https://wwww.foo.com");

  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Address:", "address", "1600 Pennsylvania Avenue",
                            "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Address Line 2:", "address2", "Suite A", "text",
                            &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form2.fields.push_back(field);
  // Country gets added.
  test::CreateTestFormField("Country:", "country", "USA", "text", &field);
  form2.fields.push_back(field);
  // Same phone number with different formatting doesn't create a new profile.
  test::CreateTestFormField("Phone:", "phone", "650-555-6666", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes();
  ImportAddressProfiles(/*extraction_success=*/true, form_structure2);

  const std::vector<AutofillProfile*>& results2 =
      personal_data_manager_->GetProfiles();

  // Full name, phone formatting and country are updated.
  expected.SetRawInfo(NAME_FULL, base::ASCIIToUTF16("George Washington"));
  expected.SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                      base::ASCIIToUTF16("+1 650-555-6666"));
  expected.SetRawInfo(ADDRESS_HOME_COUNTRY, base::ASCIIToUTF16("US"));
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected.Compare(*results2[0]));
}

TEST_F(FormDataImporterTest, ImportAddressProfiles_MissingInfoInOld) {
  FormData form1;
  form1.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Address Line 1:", "address", "190 High Street",
                            "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "Philadelphia", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "Pennsylvania", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zipcode", "19106", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes();
  ImportAddressProfiles(/*extraction_success=*/true, form_structure1);

  AutofillProfile expected(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&expected, "George", nullptr, "Washington", nullptr,
                       nullptr, "190 High Street", nullptr, "Philadelphia",
                       "Pennsylvania", "19106", nullptr, nullptr);
  const std::vector<AutofillProfile*>& results1 =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results1.size());
  EXPECT_EQ(0, expected.Compare(*results1[0]));

  // Submit a form with new data for the first profile.
  FormData form2;
  form2.url = GURL("https://wwww.foo.com");

  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Address Line 1:", "address", "190 High Street",
                            "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "Philadelphia", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "Pennsylvania", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zipcode", "19106", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes();
  ImportAddressProfiles(/*extraction_success=*/true, form_structure2);

  const std::vector<AutofillProfile*>& results2 =
      personal_data_manager_->GetProfiles();

  AutofillProfile expected2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&expected2, "George", nullptr, "Washington",
                       "theprez@gmail.com", nullptr, "190 High Street", nullptr,
                       "Philadelphia", "Pennsylvania", "19106", nullptr,
                       nullptr);
  expected2.SetRawInfo(NAME_FULL, base::ASCIIToUTF16("George Washington"));
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected2.Compare(*results2[0]));
}

TEST_F(FormDataImporterTest, ImportAddressProfiles_MissingInfoInNew) {
  FormData form1;
  form1.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Company:", "company", "Government", "text",
                            &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Address Line 1:", "address", "190 High Street",
                            "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "Philadelphia", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "Pennsylvania", "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zipcode", "19106", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes();
  ImportAddressProfiles(/*extraction_success=*/true, form_structure1);

  AutofillProfile expected(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&expected, "George", nullptr, "Washington",
                       "theprez@gmail.com", "Government", "190 High Street",
                       nullptr, "Philadelphia", "Pennsylvania", "19106",
                       nullptr, nullptr);
  const std::vector<AutofillProfile*>& results1 =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results1.size());
  EXPECT_EQ(0, expected.Compare(*results1[0]));

  // Submit a form with new data for the first profile.
  FormData form2;
  form2.url = GURL("https://wwww.foo.com");

  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form2.fields.push_back(field);
  // Note missing Company field.
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Address Line 1:", "address", "190 High Street",
                            "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "Philadelphia", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "Pennsylvania", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zipcode", "19106", "text", &field);
  form2.fields.push_back(field);

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes();
  ImportAddressProfiles(/*extraction_success=*/true, form_structure2);

  const std::vector<AutofillProfile*>& results2 =
      personal_data_manager_->GetProfiles();

  // The merge operation will populate the full name if it's empty.
  expected.SetRawInfo(NAME_FULL, base::ASCIIToUTF16("George Washington"));
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected.Compare(*results2[0]));
}

TEST_F(FormDataImporterTest, ImportAddressProfiles_InsufficientAddress) {
  FormData form1;
  form1.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Company:", "company", "Government", "text",
                            &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("Address Line 1:", "address", "190 High Street",
                            "text", &field);
  form1.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "Philadelphia", "text", &field);
  form1.fields.push_back(field);

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes();
  ImportAddressProfiles(/*extraction_success=*/false, form_structure1);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  ASSERT_EQ(0U, personal_data_manager_->GetProfiles().size());
  ASSERT_EQ(0U, personal_data_manager_->GetCreditCards().size());
}

// Ensure that if a verified profile already exists, aggregated profiles cannot
// modify it in any way. This also checks the profile merging/matching algorithm
// works: if either the full name OR all the non-empty name pieces match, the
// profile is a match.
TEST_F(FormDataImporterTest,
       ImportAddressProfiles_ExistingVerifiedProfileWithConflict) {
  // Start with a verified profile.
  AutofillProfile profile(base::GenerateGUID(), kSettingsOrigin);
  test::SetProfileInfo(&profile, "Marion", "Mitchell", "Morrison",
                       "johnwayne@me.xyz", "Fox", "123 Zoo St.", "unit 5",
                       "Hollywood", "CA", "91601", "US", "12345678910");
  EXPECT_TRUE(profile.IsVerified());

  base::RunLoop run_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
      .WillOnce(QuitMessageLoop(&run_loop));
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged()).Times(1);
  personal_data_manager_->AddProfile(profile);
  run_loop.Run();

  // Simulate a form submission with conflicting info.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "Marion Mitchell",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Morrison", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "johnwayne@me.xyz", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "123 Zoo St.", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "Hollywood", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "CA", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "91601", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  ImportAddressProfiles(/*extraction_success=*/true, form_structure);

  // Expect that no new profile is saved.
  const std::vector<AutofillProfile*>& results =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, profile.Compare(*results[0]));

  // Try the same thing, but without "Mitchell". The profiles should still match
  // because the non empty name pieces (first and last) match that stored in the
  // profile.
  test::CreateTestFormField("First name:", "first_name", "Marion", "text",
                            &field);
  form.fields[0] = field;

  FormStructure form_structure2(form);
  form_structure2.DetermineHeuristicTypes();

  ImportAddressProfiles(/*extraction_success=*/true, form_structure2);

  // Expect that no new profile is saved.
  const std::vector<AutofillProfile*>& results2 =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, profile.Compare(*results2[0]));
}

// Tests that no profile is inferred if the country is not recognized.
TEST_F(FormDataImporterTest, ImportAddressProfiles_UnrecognizedCountry) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Country:", "country", "Notacountry", "text",
                            &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  ImportAddressProfiles(/*extraction_success=*/false, form_structure);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  ASSERT_EQ(0U, personal_data_manager_->GetProfiles().size());
  ASSERT_EQ(0U, personal_data_manager_->GetCreditCards().size());
}

// Tests that a profile is created for countries with composed names.
TEST_F(FormDataImporterTest,
       ImportAddressProfiles_CompleteComposedCountryName) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Country:", "country", "Myanmar [Burma]", "text",
                            &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  ImportAddressProfiles(/*extraction_success=*/true, form_structure);

  AutofillProfile expected(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&expected, "George", nullptr, "Washington",
                       "theprez@gmail.com", nullptr, "21 Laussat St", nullptr,
                       "San Francisco", "California", "94102", "MM", nullptr);
  const std::vector<AutofillProfile*>& results =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));
}

// TODO(crbug.com/634131): Create profiles if part of a standalone part of a
// composed country name is present.
// Tests that a profile is created if a standalone part of a composed country
// name is present.
TEST_F(FormDataImporterTest,
       ImportAddressProfiles_IncompleteComposedCountryName) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Country:", "country",
                            "Myanmar",  // Missing the [Burma] part
                            "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  ImportAddressProfiles(/*extraction_success=*/false, form_structure);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  ASSERT_EQ(0U, personal_data_manager_->GetProfiles().size());
  ASSERT_EQ(0U, personal_data_manager_->GetCreditCards().size());
}

// ImportCreditCard tests.

// Tests that a valid credit card is extracted.
TEST_F(FormDataImporterTest, ImportCreditCard_Valid) {
  // Add a single valid credit card form.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(ImportCreditCard(form_structure, false, &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SubmittedCardState",
      AutofillMetrics::HAS_CARD_NUMBER_AND_EXPIRATION_DATE, 1);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(*imported_credit_card);

  WaitForOnPersonalDataChanged();

  CreditCard expected(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected, "Biggie Smalls", "4111111111111111", "01",
                          "2999", "");  // Imported cards have no billing info.
  const std::vector<CreditCard*>& results =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));
}

// Tests that an invalid credit card number is not extracted.
TEST_F(FormDataImporterTest, ImportCreditCard_InvalidCardNumber) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Jim Johansen", "1000000000000000", "02",
                        "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(ImportCreditCard(form_structure, false, &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);
  histogram_tester.ExpectUniqueSample("Autofill.SubmittedCardState",
                                      AutofillMetrics::HAS_EXPIRATION_DATE_ONLY,
                                      1);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  ASSERT_EQ(0U, personal_data_manager_->GetCreditCards().size());
}

// Tests that an invalid credit card expiration is not extracted when the
// expiration date fix flow experiment is disabled.
TEST_F(FormDataImporterTest, ImportCreditCard_InvalidExpiryDate) {
  scoped_feature_list_.InitAndDisableFeature(
      features::kAutofillUpstreamEditableExpirationDate);
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Smalls Biggie", "4111-1111-1111-1111", "0",
                        "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  base::HistogramTester histogram_tester;
  EXPECT_FALSE(ImportCreditCard(form_structure, false, &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);
  histogram_tester.ExpectUniqueSample("Autofill.SubmittedCardState",
                                      AutofillMetrics::HAS_CARD_NUMBER_ONLY, 1);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  ASSERT_EQ(0U, personal_data_manager_->GetCreditCards().size());
}

// Tests that an empty credit card expiration is extracted when editable
// expiration date experiment on.
TEST_F(FormDataImporterTest,
       ImportCreditCard_InvalidExpiryDate_EditableExpirationExpOn) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillUpstreamEditableExpirationDate);
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Smalls Biggie", "4111-1111-1111-1111", "", "");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(ImportCreditCard(form_structure, false, &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  histogram_tester.ExpectUniqueSample("Autofill.SubmittedCardState",
                                      AutofillMetrics::HAS_CARD_NUMBER_ONLY, 1);
}

// Tests that an expired credit card is extracted when editable expiration date
// experiment on.
TEST_F(FormDataImporterTest,
       ImportCreditCard_ExpiredExpiryDate_EditableExpirationExpOn) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillUpstreamEditableExpirationDate);
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Smalls Biggie", "4111-1111-1111-1111", "01",
                        "2000");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(ImportCreditCard(form_structure, false, &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  histogram_tester.ExpectUniqueSample("Autofill.SubmittedCardState",
                                      AutofillMetrics::HAS_CARD_NUMBER_ONLY, 1);
}

// Tests that a valid credit card is extracted when the option text for month
// select can't be parsed but its value can.
TEST_F(FormDataImporterTest, ImportCreditCard_MonthSelectInvalidText) {
  // Add a single valid credit card form with an invalid option value.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Biggie Smalls", "4111-1111-1111-1111",
                        "Feb (2)", "2999");
  // Add option values and contents to the expiration month field.
  ASSERT_EQ(base::ASCIIToUTF16("exp_month"), form.fields[2].name);
  std::vector<base::string16> values;
  values.push_back(base::ASCIIToUTF16("1"));
  values.push_back(base::ASCIIToUTF16("2"));
  values.push_back(base::ASCIIToUTF16("3"));
  std::vector<base::string16> contents;
  contents.push_back(base::ASCIIToUTF16("Jan (1)"));
  contents.push_back(base::ASCIIToUTF16("Feb (2)"));
  contents.push_back(base::ASCIIToUTF16("Mar (3)"));
  form.fields[2].option_values = values;
  form.fields[2].option_contents = contents;

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  base::HistogramTester histogram_tester;
  EXPECT_TRUE(ImportCreditCard(form_structure, false, &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SubmittedCardState",
      AutofillMetrics::HAS_CARD_NUMBER_AND_EXPIRATION_DATE, 1);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(*imported_credit_card);

  WaitForOnPersonalDataChanged();

  // See that the invalid option text was converted to the right value.
  CreditCard expected(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected, "Biggie Smalls", "4111111111111111", "02",
                          "2999", "");  // Imported cards have no billing info.
  const std::vector<CreditCard*>& results =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));
}

TEST_F(FormDataImporterTest, ImportCreditCard_TwoValidCards) {
  // Start with a single valid credit card form.
  FormData form1;
  form1.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form1, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2999");

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(ImportCreditCard(form_structure1, false, &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(*imported_credit_card);

  WaitForOnPersonalDataChanged();

  CreditCard expected(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected, "Biggie Smalls", "4111111111111111", "01",
                          "2999", "");  // Imported cards have no billing info.
  const std::vector<CreditCard*>& results =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));

  // Add a second different valid credit card.
  FormData form2;
  form2.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form2, "", "5500 0000 0000 0004", "02", "2999");

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes();

  std::unique_ptr<CreditCard> imported_credit_card2;
  EXPECT_TRUE(ImportCreditCard(form_structure2, false, &imported_credit_card2));
  ASSERT_TRUE(imported_credit_card2);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(*imported_credit_card2);

  WaitForOnPersonalDataChanged();

  CreditCard expected2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected2, "", "5500000000000004", "02", "2999",
                          "");  // Imported cards have no billing info.
  std::vector<CreditCard*> cards;
  cards.push_back(&expected);
  cards.push_back(&expected2);
  ExpectSameElements(cards, personal_data_manager_->GetCreditCards());
}

// This form has the expiration year as one field with MM/YY.
TEST_F(FormDataImporterTest, ImportCreditCard_Month2DigitYearCombination) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "John MMYY",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4111111111111111",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Date:", "exp_date", "05/45", "text", &field);
  field.autocomplete_attribute = "cc-exp";
  field.max_length = 5;
  form.fields.push_back(field);

  SubmitFormAndExpectImportedCardWithData(form, "John MMYY", "4111111111111111",
                                          "05", "2045");
}

// This form has the expiration year as one field with MM/YYYY.
TEST_F(FormDataImporterTest, ImportCreditCard_Month4DigitYearCombination) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "John MMYYYY",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4111111111111111",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Date:", "exp_date", "05/2045", "text", &field);
  field.autocomplete_attribute = "cc-exp";
  field.max_length = 7;
  form.fields.push_back(field);

  SubmitFormAndExpectImportedCardWithData(form, "John MMYYYY",
                                          "4111111111111111", "05", "2045");
}

// This form has the expiration year as one field with M/YYYY.
TEST_F(FormDataImporterTest, ImportCreditCard_1DigitMonth4DigitYear) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "John MYYYY",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4111111111111111",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Date:", "exp_date", "5/2045", "text", &field);
  field.autocomplete_attribute = "cc-exp";
  form.fields.push_back(field);

  SubmitFormAndExpectImportedCardWithData(form, "John MYYYY",
                                          "4111111111111111", "05", "2045");
}

// This form has the expiration year as a 2-digit field.
TEST_F(FormDataImporterTest, ImportCreditCard_2DigitYear) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "John Smith",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4111111111111111",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "05", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "45", "text", &field);
  field.max_length = 2;
  form.fields.push_back(field);

  SubmitFormAndExpectImportedCardWithData(form, "John Smith",
                                          "4111111111111111", "05", "2045");
}

// Tests that a credit card is not extracted because the
// card matches a masked server card.
TEST_F(FormDataImporterTest,
       ImportCreditCard_DuplicateServerCards_MaskedCard_DontExtract) {
  // Add a masked server card.
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "a123"));
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "1111" /* Visa */, "01", "2999", "");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);
  test::SetServerCreditCards(autofill_table_, server_cards);

  // Make sure everything is set up correctly.
  personal_data_manager_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // Type the same data as the masked card into a form.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "John Dillinger", "4111111111111111", "01",
                        "2999");

  // The card should not be offered to be saved locally because the feature flag
  // is disabled.
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  EXPECT_FALSE(ImportCreditCard(form_structure, false, &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);
}

// Tests that a credit card is not extracted because it matches a full server
// card.
TEST_F(FormDataImporterTest, ImportCreditCard_DuplicateServerCards_FullCard) {
  // Add a full server card.
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "");  // Imported cards have no billing info.
  test::SetServerCreditCards(autofill_table_, server_cards);

  // Make sure everything is set up correctly.
  personal_data_manager_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // Type the same data as the unmasked card into a form.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Clyde Barrow", "378282246310005", "04", "2999");

  // The card should not be offered to be saved locally because it only matches
  // the full server card.
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  EXPECT_FALSE(ImportCreditCard(form_structure, false, &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);
}

TEST_F(FormDataImporterTest, ImportCreditCard_SameCreditCardWithConflict) {
  // Start with a single valid credit card form.
  FormData form1;
  form1.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form1, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2998");

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(ImportCreditCard(form_structure1, false, &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(*imported_credit_card);

  WaitForOnPersonalDataChanged();

  CreditCard expected(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected, "Biggie Smalls", "4111111111111111", "01",
                          "2998", "");  // Imported cards have no billing info.
  const std::vector<CreditCard*>& results =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));

  // Add a second different valid credit card where the year is different but
  // the credit card number matches.
  FormData form2;
  form2.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form2, "Biggie Smalls", "4111 1111 1111 1111", "01",
                        /* different year */ "2999");

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card2;
  EXPECT_TRUE(ImportCreditCard(form_structure2, false, &imported_credit_card2));
  EXPECT_FALSE(imported_credit_card2);

  WaitForOnPersonalDataChanged();

  // Expect that the newer information is saved.  In this case the year is
  // updated to "2999".
  CreditCard expected2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected2, "Biggie Smalls", "4111111111111111", "01",
                          "2999", "");  // Imported cards have no billing info.
  const std::vector<CreditCard*>& results2 =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected2.Compare(*results2[0]));
}

TEST_F(FormDataImporterTest, ImportCreditCard_ShouldReturnLocalCard) {
  // Start with a single valid credit card form.
  FormData form1;
  form1.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form1, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2998");

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(ImportCreditCard(form_structure1, false, &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(*imported_credit_card);

  WaitForOnPersonalDataChanged();

  CreditCard expected(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected, "Biggie Smalls", "4111111111111111", "01",
                          "2998", "");  // Imported cards have no billing info.
  const std::vector<CreditCard*>& results =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));

  // Add a second different valid credit card where the year is different but
  // the credit card number matches.
  FormData form2;
  form2.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form2, "Biggie Smalls", "4111 1111 1111 1111", "01",
                        /* different year */ "2999");

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card2;
  EXPECT_TRUE(ImportCreditCard(form_structure2,
                               /* should_return_local_card= */ true,
                               &imported_credit_card2));
  // The local card is returned after an update.
  EXPECT_TRUE(imported_credit_card2);

  WaitForOnPersonalDataChanged();

  // Expect that the newer information is saved.  In this case the year is
  // updated to "2999".
  CreditCard expected2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected2, "Biggie Smalls", "4111111111111111", "01",
                          "2999", "");  // Imported cards have no billing info.
  const std::vector<CreditCard*>& results2 =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected2.Compare(*results2[0]));
}

TEST_F(FormDataImporterTest, ImportCreditCard_EmptyCardWithConflict) {
  // Start with a single valid credit card form.
  FormData form1;
  form1.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form1, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2998");

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes();

  std::unique_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(ImportCreditCard(form_structure1, false, &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(*imported_credit_card);

  WaitForOnPersonalDataChanged();

  CreditCard expected(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected, "Biggie Smalls", "4111111111111111", "01",
                          "2998", "");  // Imported cards have no billing info.
  const std::vector<CreditCard*>& results =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));

  // Add a second credit card with no number.
  FormData form2;
  form2.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form2, "Biggie Smalls", /* no number */ nullptr, "01",
                        "2999");

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card2;
  EXPECT_FALSE(
      ImportCreditCard(form_structure2, false, &imported_credit_card2));
  EXPECT_FALSE(imported_credit_card2);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // No change is expected.
  CreditCard expected2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected2, "Biggie Smalls", "4111111111111111", "01",
                          "2998", "");
  const std::vector<CreditCard*>& results2 =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected2.Compare(*results2[0]));
}

TEST_F(FormDataImporterTest, ImportCreditCard_MissingInfoInNew) {
  // Start with a single valid credit card form.
  FormData form1;
  form1.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form1, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2999");

  FormStructure form_structure1(form1);
  form_structure1.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(ImportCreditCard(form_structure1, false, &imported_credit_card));
  ASSERT_TRUE(imported_credit_card);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(*imported_credit_card);

  WaitForOnPersonalDataChanged();

  CreditCard expected(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected, "Biggie Smalls", "4111111111111111", "01",
                          "2999", "");
  const std::vector<CreditCard*>& results =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected.Compare(*results[0]));

  // Add a second different valid credit card where the name is missing but
  // the credit card number matches.
  FormData form2;
  form2.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form2, /* missing name */ nullptr,
                        "4111-1111-1111-1111", "01", "2999");

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card2;
  EXPECT_TRUE(ImportCreditCard(form_structure2, false, &imported_credit_card2));
  EXPECT_FALSE(imported_credit_card2);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // No change is expected.
  CreditCard expected2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected2, "Biggie Smalls", "4111111111111111", "01",
                          "2999", "");
  const std::vector<CreditCard*>& results2 =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected2.Compare(*results2[0]));

  // Add a third credit card where the expiration date is missing.
  FormData form3;
  form3.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form3, "Johnny McEnroe", "5555555555554444",
                        /* no month */ nullptr,
                        /* no year */ nullptr);

  FormStructure form_structure3(form3);
  form_structure3.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card3;
  EXPECT_FALSE(
      ImportCreditCard(form_structure3, false, &imported_credit_card3));
  ASSERT_FALSE(imported_credit_card3);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // No change is expected.
  CreditCard expected3(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected3, "Biggie Smalls", "4111111111111111", "01",
                          "2999", "");
  const std::vector<CreditCard*>& results3 =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results3.size());
  EXPECT_EQ(0, expected3.Compare(*results3[0]));
}

TEST_F(FormDataImporterTest, ImportCreditCard_MissingInfoInOld) {
  // Start with a single valid credit card stored via the preferences.
  // Note the empty name.
  CreditCard saved_credit_card(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&saved_credit_card, "", "4111111111111111" /* Visa */,
                          "01", "2998", "1");
  personal_data_manager_->AddCreditCard(saved_credit_card);

  WaitForOnPersonalDataChanged();

  const std::vector<CreditCard*>& results1 =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results1.size());
  EXPECT_EQ(saved_credit_card, *results1[0]);

  // Add a second different valid credit card where the year is different but
  // the credit card number matches.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        /* different year */ "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(ImportCreditCard(form_structure, false, &imported_credit_card));
  EXPECT_FALSE(imported_credit_card);

  WaitForOnPersonalDataChanged();

  // Expect that the newer information is saved.  In this case the year is
  // added to the existing credit card.
  CreditCard expected2(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected2, "Biggie Smalls", "4111111111111111", "01",
                          "2999", "1");
  const std::vector<CreditCard*>& results2 =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, expected2.Compare(*results2[0]));
}

// We allow the user to store a credit card number with separators via the UI.
// We should not try to re-aggregate the same card with the separators stripped.
TEST_F(FormDataImporterTest, ImportCreditCard_SameCardWithSeparators) {
  // Start with a single valid credit card stored via the preferences.
  // Note the separators in the credit card number.
  CreditCard saved_credit_card(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&saved_credit_card, "Biggie Smalls",
                          "4111 1111 1111 1111" /* Visa */, "01", "2999", "");
  personal_data_manager_->AddCreditCard(saved_credit_card);

  WaitForOnPersonalDataChanged();

  const std::vector<CreditCard*>& results1 =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results1.size());
  EXPECT_EQ(0, saved_credit_card.Compare(*results1[0]));

  // Import the same card info, but with different separators in the number.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(ImportCreditCard(form_structure, false, &imported_credit_card));
  EXPECT_FALSE(imported_credit_card);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Expect that no new card is saved.
  const std::vector<CreditCard*>& results2 =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results2.size());
  EXPECT_EQ(0, saved_credit_card.Compare(*results2[0]));
}

// Ensure that if a verified credit card already exists, aggregated credit cards
// cannot modify it in any way.
TEST_F(FormDataImporterTest,
       ImportCreditCard_ExistingVerifiedCardWithConflict) {
  // Start with a verified credit card.
  CreditCard credit_card(base::GenerateGUID(), kSettingsOrigin);
  test::SetCreditCardInfo(&credit_card, "Biggie Smalls",
                          "4111 1111 1111 1111" /* Visa */, "01", "2998", "");
  EXPECT_TRUE(credit_card.IsVerified());

  // Add the credit card to the database.
  personal_data_manager_->AddCreditCard(credit_card);

  // Make sure everything is set up correctly.
  personal_data_manager_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // Simulate a form submission with conflicting expiration year.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Biggie Smalls", "4111 1111 1111 1111", "01",
                        /* different year */ "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  EXPECT_TRUE(ImportCreditCard(form_structure, false, &imported_credit_card));
  ASSERT_FALSE(imported_credit_card);

  // Since no refresh is expected, reload the data from the database to make
  // sure no changes were written out.
  ResetPersonalDataManager(USER_MODE_NORMAL);

  // Expect that the saved credit card is not modified.
  const std::vector<CreditCard*>& results =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, credit_card.Compare(*results[0]));
}

// Ensures that |imported_credit_card_record_type_| is set and reset correctly.
TEST_F(FormDataImporterTest,
       ImportFormData_SecondImportResetsCreditCardRecordType) {
  // Start with a single valid credit card stored via the preferences.
  CreditCard saved_credit_card(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&saved_credit_card, "Biggie Smalls",
                          "4111 1111 1111 1111" /* Visa */, "01", "2999", "");
  personal_data_manager_->AddCreditCard(saved_credit_card);

  WaitForOnPersonalDataChanged();

  const std::vector<CreditCard*>& results =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, saved_credit_card.Compare(*results[0]));

  // Simulate a form submission with the same card.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Biggie Smalls", "4111 1111 1111 1111", "01",
                        "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  base::Optional<std::string> imported_vpa;
  EXPECT_TRUE(form_data_importer_->ImportFormData(
      form_structure, /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/true, &imported_credit_card, &imported_vpa));
  ASSERT_TRUE(imported_credit_card);
  // |imported_credit_card_record_type_| should be LOCAL_CARD because upload was
  // offered and the card is a local card already on the device.
  ASSERT_TRUE(form_data_importer_->imported_credit_card_record_type_ ==
              FormDataImporter::ImportedCreditCardRecordType::LOCAL_CARD);

  // Second form is filled with a new card so
  // |imported_credit_card_record_type_| should be reset.
  // Simulate a form submission with a new card.
  FormData form2;
  form2.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form2, "Biggie Smalls", "4012888888881881", "01",
                        "2999");

  FormStructure form_structure2(form2);
  form_structure2.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card2;
  EXPECT_TRUE(form_data_importer_->ImportFormData(
      form_structure2, /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/true, &imported_credit_card2,
      &imported_vpa));
  ASSERT_TRUE(imported_credit_card2);
  // |imported_credit_card_record_type_| should be NEW_CARD because the imported
  // card is not already on the device.
  ASSERT_TRUE(form_data_importer_->imported_credit_card_record_type_ ==
              FormDataImporter::ImportedCreditCardRecordType::NEW_CARD);

  // Third form is an address form and set |credit_card_autofill_enabled| to be
  // false so that the ImportCreditCard won't be called.
  // |imported_credit_card_record_type_| should still be reset even if
  // ImportCreditCard is not called. Simulate a form submission with no card.
  FormData form3;
  form3.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form3.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form3.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "bogus@example.com", "text",
                            &field);
  form3.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form3.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form3.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form3.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form3.fields.push_back(field);
  FormStructure form_structure3(form3);
  form_structure3.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card3;
  EXPECT_TRUE(form_data_importer_->ImportFormData(
      form_structure3, /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/false,
      /*should_return_local_card=*/true, &imported_credit_card3,
      &imported_vpa));
  // |imported_credit_card_record_type_| should be NO_CARD because no valid card
  // was imported from the form.
  ASSERT_TRUE(form_data_importer_->imported_credit_card_record_type_ ==
              FormDataImporter::ImportedCreditCardRecordType::NO_CARD);
}

// Ensures that |imported_credit_card_record_type_| is set correctly.
TEST_F(FormDataImporterTest,
       ImportFormData_ImportCreditCardRecordType_NewCard) {
  // Simulate a form submission with a new credit card.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Biggie Smalls", "4111 1111 1111 1111", "01",
                        "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  base::Optional<std::string> imported_vpa;
  EXPECT_TRUE(form_data_importer_->ImportFormData(
      form_structure, /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/true, &imported_credit_card, &imported_vpa));
  ASSERT_TRUE(imported_credit_card);
  // |imported_credit_card_record_type_| should be NEW_CARD because the imported
  // card is not already on the device.
  ASSERT_TRUE(form_data_importer_->imported_credit_card_record_type_ ==
              FormDataImporter::ImportedCreditCardRecordType::NEW_CARD);
}

// Ensures that |imported_credit_card_record_type_| is set correctly.
TEST_F(FormDataImporterTest,
       ImportFormData_ImportCreditCardRecordType_LocalCard) {
  // Start with a single valid credit card stored via the preferences.
  CreditCard saved_credit_card(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&saved_credit_card, "Biggie Smalls",
                          "4111 1111 1111 1111" /* Visa */, "01", "2999", "");
  personal_data_manager_->AddCreditCard(saved_credit_card);

  WaitForOnPersonalDataChanged();

  const std::vector<CreditCard*>& results =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, saved_credit_card.Compare(*results[0]));

  // Simulate a form submission with the same card.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Biggie Smalls", "4111 1111 1111 1111", "01",
                        "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  base::Optional<std::string> imported_vpa;
  EXPECT_TRUE(form_data_importer_->ImportFormData(
      form_structure, /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/true, &imported_credit_card, &imported_vpa));
  ASSERT_TRUE(imported_credit_card);
  // |imported_credit_card_record_type_| should be LOCAL_CARD because upload was
  // offered and the card is a local card already on the device.
  ASSERT_TRUE(form_data_importer_->imported_credit_card_record_type_ ==
              FormDataImporter::ImportedCreditCardRecordType::LOCAL_CARD);
}

// Ensures that |imported_credit_card_record_type_| is set correctly.
TEST_F(FormDataImporterTest,
       ImportFormData_ImportCreditCardRecordType_MaskedServerCard) {
  // Add a masked server card.
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "a123"));
  test::SetCreditCardInfo(&server_cards.back(), "Biggie Smalls",
                          "1111" /* Visa */, "01", "2999", "");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);
  test::SetServerCreditCards(autofill_table_, server_cards);

  // Make sure everything is set up correctly.
  personal_data_manager_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // Simulate a form submission with the same masked server card.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Biggie Smalls", "4111 1111 1111 1111", "01",
                        "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  base::Optional<std::string> imported_vpa;
  EXPECT_FALSE(form_data_importer_->ImportFormData(
      form_structure, /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/true, &imported_credit_card, &imported_vpa));
  ASSERT_FALSE(imported_credit_card);
  // |imported_credit_card_record_type_| should be SERVER_CARD.
  ASSERT_TRUE(form_data_importer_->imported_credit_card_record_type_ ==
              FormDataImporter::ImportedCreditCardRecordType::SERVER_CARD);
}

// Ensures that |imported_credit_card_record_type_| is set correctly.
TEST_F(FormDataImporterTest,
       ImportFormData_ImportCreditCardRecordType_FullServerCard) {
  // Add a full server card.
  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Biggie Smalls",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");
  test::SetServerCreditCards(autofill_table_, server_cards);

  // Make sure everything is set up correctly.
  personal_data_manager_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // Simulate a form submission with the same full server card.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Biggie Smalls", "378282246310005", "04",
                        "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  base::Optional<std::string> imported_vpa;
  EXPECT_FALSE(form_data_importer_->ImportFormData(
      form_structure, /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/true, &imported_credit_card, &imported_vpa));
  ASSERT_FALSE(imported_credit_card);
  // |imported_credit_card_record_type_| should be SERVER_CARD.
  ASSERT_TRUE(form_data_importer_->imported_credit_card_record_type_ ==
              FormDataImporter::ImportedCreditCardRecordType::SERVER_CARD);
}

// Ensures that |imported_credit_card_record_type_| is set correctly.
TEST_F(FormDataImporterTest,
       ImportFormData_ImportCreditCardRecordType_NoCard_InvalidCardNumber) {
  // Simulate a form submission using a credit card with an invalid card number.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Biggie Smalls", "4111 1111 1111 1112", "01",
                        "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  base::Optional<std::string> imported_vpa;
  EXPECT_FALSE(form_data_importer_->ImportFormData(
      form_structure, /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/true, &imported_credit_card, &imported_vpa));
  ASSERT_FALSE(imported_credit_card);
  // |imported_credit_card_record_type_| should be NO_CARD because no valid card
  // was successfully imported from the form.
  ASSERT_TRUE(form_data_importer_->imported_credit_card_record_type_ ==
              FormDataImporter::ImportedCreditCardRecordType::NO_CARD);
}

// Ensures that |imported_credit_card_record_type_| is set correctly.
TEST_F(
    FormDataImporterTest,
    ImportFormData_ImportCreditCardRecordType_NoCard_ExpiredCard_EditableExpDateOff) {
  scoped_feature_list_.InitAndDisableFeature(
      features::kAutofillUpstreamEditableExpirationDate);
  // Simulate a form submission with an expired credit card.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Biggie Smalls", "4111 1111 1111 1111", "01",
                        "1999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  base::Optional<std::string> imported_vpa;
  EXPECT_FALSE(form_data_importer_->ImportFormData(
      form_structure, /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/true, &imported_credit_card, &imported_vpa));
  ASSERT_FALSE(imported_credit_card);
  // |imported_credit_card_record_type_| should be NO_CARD because no valid card
  // was successfully imported from the form.
  ASSERT_TRUE(form_data_importer_->imported_credit_card_record_type_ ==
              FormDataImporter::ImportedCreditCardRecordType::NO_CARD);
}

// Ensures that |imported_credit_card_record_type_| is set correctly.
TEST_F(
    FormDataImporterTest,
    ImportFormData_ImportCreditCardRecordType_NewCard_ExpiredCard_WithExpDateFixFlow) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillUpstreamEditableExpirationDate);
  // Simulate a form submission with an expired credit card.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Biggie Smalls", "4111 1111 1111 1111", "01",
                        "1999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  base::Optional<std::string> imported_vpa;
  EXPECT_TRUE(form_data_importer_->ImportFormData(
      form_structure, /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/true, &imported_credit_card, &imported_vpa));
  ASSERT_TRUE(imported_credit_card);
  // |imported_credit_card_record_type_| should be NEW_CARD because card was
  // successfully imported from the form via the expiration date fix flow.
  ASSERT_TRUE(form_data_importer_->imported_credit_card_record_type_ ==
              FormDataImporter::ImportedCreditCardRecordType::NEW_CARD);
}

// Ensures that |imported_credit_card_record_type_| is set correctly.
TEST_F(FormDataImporterTest,
       ImportFormData_ImportCreditCardRecordType_NoCard_NoCardOnForm) {
  // Simulate a form submission with no credit card on form.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "bogus@example.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  base::Optional<std::string> imported_vpa;
  EXPECT_TRUE(form_data_importer_->ImportFormData(
      form_structure, /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/true, &imported_credit_card, &imported_vpa));
  ASSERT_FALSE(imported_credit_card);
  // |imported_credit_card_record_type_| should be NO_CARD because the form
  // doesn't have credit card section.
  ASSERT_TRUE(form_data_importer_->imported_credit_card_record_type_ ==
              FormDataImporter::ImportedCreditCardRecordType::NO_CARD);
}

// ImportFormData tests (both addresses and credit cards).

// Test that a form with both address and credit card sections imports the
// address and the credit card.
TEST_F(FormDataImporterTest, ImportFormData_OneAddressOneCreditCard) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  // Address section.
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);

  // Credit card section.
  AddFullCreditCardForm(&form, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  base::Optional<std::string> imported_vpa;
  EXPECT_TRUE(form_data_importer_->ImportFormData(
      form_structure,
      /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_vpa));
  ASSERT_TRUE(imported_credit_card);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(*imported_credit_card);

  WaitForOnPersonalDataChanged();

  // Test that the address has been saved.
  AutofillProfile expected_address(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&expected_address, "George", nullptr, "Washington",
                       "theprez@gmail.com", nullptr, "21 Laussat St", nullptr,
                       "San Francisco", "California", "94102", nullptr,
                       nullptr);
  const std::vector<AutofillProfile*>& results_addr =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results_addr.size());
  EXPECT_EQ(0, expected_address.Compare(*results_addr[0]));

  // Test that the credit card has also been saved.
  CreditCard expected_card(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected_card, "Biggie Smalls", "4111111111111111",
                          "01", "2999", "");
  const std::vector<CreditCard*>& results_cards =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results_cards.size());
  EXPECT_EQ(0, expected_card.Compare(*results_cards[0]));
}

// Test that a form with two address sections and a credit card section does not
// import the address but does import the credit card.
TEST_F(FormDataImporterTest, ImportFormData_TwoAddressesOneCreditCard) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  // Address section 1.
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);

  // Address section 2.
  test::CreateTestFormField("Name:", "name", "Barack Obama", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address", "1600 Pennsylvania Avenue",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "Washington", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "DC", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "20500", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Country:", "country", "USA", "text", &field);
  form.fields.push_back(field);

  // Credit card section.
  AddFullCreditCardForm(&form, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;

  base::RunLoop run_loop;
  EXPECT_CALL(personal_data_observer_, OnPersonalDataFinishedProfileTasks())
      .WillRepeatedly(QuitMessageLoop(&run_loop));
  EXPECT_CALL(personal_data_observer_, OnPersonalDataChanged())
      .Times(testing::AnyNumber());
  base::Optional<std::string> imported_vpa;
  // Still returns true because the credit card import was successful.
  EXPECT_TRUE(form_data_importer_->ImportFormData(
      form_structure, /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_vpa));
  run_loop.Run();

  ASSERT_TRUE(imported_credit_card);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(*imported_credit_card);

  WaitForOnPersonalDataChanged();

  // Test that both addresses have been saved.
  EXPECT_EQ(2U, personal_data_manager_->GetProfiles().size());

  // Test that the credit card has been saved.
  CreditCard expected_card(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected_card, "Biggie Smalls", "4111111111111111",
                          "01", "2999", "");
  const std::vector<CreditCard*>& results =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected_card.Compare(*results[0]));
}

// Test that a form with both address and credit card sections imports only the
// the credit card if addresses are disabled.
TEST_F(FormDataImporterTest, ImportFormData_AddressesDisabledOneCreditCard) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  // Address section.
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);

  // Credit card section.
  AddFullCreditCardForm(&form, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  base::Optional<std::string> imported_vpa;
  EXPECT_TRUE(form_data_importer_->ImportFormData(
      form_structure, /*profile_autofill_enabled=*/false,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_vpa));
  ASSERT_TRUE(imported_credit_card);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(*imported_credit_card);

  WaitForOnPersonalDataChanged();

  // Test that addresses were not saved.
  EXPECT_EQ(0U, personal_data_manager_->GetProfiles().size());

  // Test that the credit card has been saved.
  CreditCard expected_card(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected_card, "Biggie Smalls", "4111111111111111",
                          "01", "2999", "");
  const std::vector<CreditCard*>& results =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected_card.Compare(*results[0]));
}

// Test that a form with both address and credit card sections imports only the
// the address if credit cards are disabled.
TEST_F(FormDataImporterTest, ImportFormData_OneAddressCreditCardDisabled) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  // Address section.
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);

  // Credit card section.
  AddFullCreditCardForm(&form, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  base::Optional<std::string> imported_vpa;
  EXPECT_TRUE(form_data_importer_->ImportFormData(
      form_structure,
      /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/false,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_vpa));
  ASSERT_FALSE(imported_credit_card);

  WaitForOnPersonalDataChanged();

  // Test that the address has been saved.
  AutofillProfile expected_address(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetProfileInfo(&expected_address, "George", nullptr, "Washington",
                       "theprez@gmail.com", nullptr, "21 Laussat St", nullptr,
                       "San Francisco", "California", "94102", nullptr,
                       nullptr);
  const std::vector<AutofillProfile*>& results_addr =
      personal_data_manager_->GetProfiles();
  ASSERT_EQ(1U, results_addr.size());
  EXPECT_EQ(0, expected_address.Compare(*results_addr[0]));

  // Test that the credit card was not saved.
  const std::vector<CreditCard*>& results_cards =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(0U, results_cards.size());
}

// Test that a form with both address and credit card sections imports nothing
// if both addressed and credit cards are disabled.
TEST_F(FormDataImporterTest, ImportFormData_AddressCreditCardDisabled) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  // Address section.
  test::CreateTestFormField("First name:", "first_name", "George", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name:", "last_name", "Washington", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address:", "address1", "21 Laussat St", "text",
                            &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City:", "city", "San Francisco", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State:", "state", "California", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip:", "zip", "94102", "text", &field);
  form.fields.push_back(field);

  // Credit card section.
  AddFullCreditCardForm(&form, "Biggie Smalls", "4111-1111-1111-1111", "01",
                        "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  base::Optional<std::string> imported_vpa;
  EXPECT_FALSE(form_data_importer_->ImportFormData(
      form_structure,
      /*profile_autofill_enabled=*/false,
      /*credit_card_autofill_enabled=*/false,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_vpa));
  ASSERT_FALSE(imported_credit_card);

  // Test that addresses were not saved.
  EXPECT_EQ(0U, personal_data_manager_->GetProfiles().size());

  // Test that the credit card was not saved.
  const std::vector<CreditCard*>& results_cards =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(0U, results_cards.size());
}

TEST_F(FormDataImporterTest, DontDuplicateMaskedServerCard) {
  EnableWalletCardImport();

  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "a123"));
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "1881" /* Visa */, "01", "2999", "");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "");

  test::SetServerCreditCards(autofill_table_, server_cards);

  // Make sure everything is set up correctly.
  personal_data_manager_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(2U, personal_data_manager_->GetCreditCards().size());

  // A valid credit card form. A user re-enters one of their masked cards.
  // We should not offer to save locally.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "John Dillinger",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4012888888881881",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "01", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2999", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  base::Optional<std::string> imported_vpa;
  EXPECT_FALSE(form_data_importer_->ImportFormData(
      form_structure, /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_vpa));
  ASSERT_FALSE(imported_credit_card);
}

// Tests that a credit card form that is hidden after receiving input still
// imports the card.
TEST_F(FormDataImporterTest, ImportFormData_HiddenCreditCardFormAfterEntered) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;

  test::CreateTestFormField("Name on card:", "name_on_card", "Biggie Smalls",
                            "text", &field);
  field.is_focusable = false;
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4111111111111111",
                            "text", &field);
  field.is_focusable = false;
  form.fields.push_back(field);
  test::CreateTestFormField("Email:", "email", "theprez@gmail.com", "text",
                            &field);
  field.is_focusable = false;
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "01", "text", &field);
  field.is_focusable = false;
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2999", "text", &field);
  field.is_focusable = false;
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  base::Optional<std::string> imported_vpa;
  // Still returns true because the credit card import was successful.
  EXPECT_TRUE(form_data_importer_->ImportFormData(
      form_structure, /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_vpa));
  ASSERT_TRUE(imported_credit_card);
  personal_data_manager_->OnAcceptedLocalCreditCardSave(*imported_credit_card);

  WaitForOnPersonalDataChanged();

  // Test that the credit card has been saved.
  CreditCard expected_card(base::GenerateGUID(), test::kEmptyOrigin);
  test::SetCreditCardInfo(&expected_card, "Biggie Smalls", "4111111111111111",
                          "01", "2999", "");
  const std::vector<CreditCard*>& results =
      personal_data_manager_->GetCreditCards();
  ASSERT_EQ(1U, results.size());
  EXPECT_EQ(0, expected_card.Compare(*results[0]));
}

// Ensures that no VPA value is returned when there's a credit card and no VPA.
TEST_F(FormDataImporterTest,
       ImportFormData_DontSetVPAWhenOnlyCreditCardExists) {
  // Simulate a form submission with a new credit card.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  AddFullCreditCardForm(&form, "Biggie Smalls", "4111 1111 1111 1111", "01",
                        "2999");

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  base::Optional<std::string> imported_vpa;
  EXPECT_TRUE(form_data_importer_->ImportFormData(
      form_structure, /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/true, &imported_credit_card, &imported_vpa));
  ASSERT_FALSE(imported_vpa.has_value());
}

TEST_F(FormDataImporterTest, DontDuplicateFullServerCard) {
  EnableWalletCardImport();

  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "a123"));
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "1881" /* Visa */, "01", "2999", "1");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "378282246310005" /* American Express */, "04",
                          "2999", "1");

  test::SetServerCreditCards(autofill_table_, server_cards);

  // Make sure everything is set up correctly.
  personal_data_manager_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(2U, personal_data_manager_->GetCreditCards().size());

  // A user re-types (or fills with) an unmasked card. Don't offer to save
  // here, either. Since it's unmasked, we know for certain that it's the same
  // card.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "Clyde Barrow",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "378282246310005",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "04", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2999", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  base::Optional<std::string> imported_vpa;
  EXPECT_FALSE(form_data_importer_->ImportFormData(
      form_structure,
      /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_vpa));
  EXPECT_FALSE(imported_credit_card);
}

TEST_F(FormDataImporterTest,
       Metrics_SubmittedServerCardExpirationStatus_FullServerCardMatch) {
  EnableWalletCardImport();

  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "4444333322221111" /* Visa */, "04", "2111", "1");

  test::SetServerCreditCards(autofill_table_, server_cards);

  // Make sure everything is set up correctly.
  personal_data_manager_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // A user fills/enters the card's information on a checkout form.  Ensure that
  // an expiration date match is recorded.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "Clyde Barrow",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4444333322221111",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "04", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2111", "text", &field);
  form.fields.push_back(field);

  base::HistogramTester histogram_tester;
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  base::Optional<std::string> imported_vpa;
  EXPECT_FALSE(form_data_importer_->ImportFormData(
      form_structure,
      /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_vpa));
  EXPECT_FALSE(imported_credit_card);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SubmittedServerCardExpirationStatus",
      AutofillMetrics::FULL_SERVER_CARD_EXPIRATION_DATE_MATCHED, 1);
}

// Ensure that we don't offer to save if we already have same card stored as a
// server card and user submitted an invalid expiration date month.
TEST_F(FormDataImporterTest,
       Metrics_SubmittedServerCardExpirationStatus_EmptyExpirationMonth) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillUpstreamEditableExpirationDate);
  EnableWalletCardImport();

  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "4444333322221111" /* Visa */, "04", "2111", "1");

  test::SetServerCreditCards(autofill_table_, server_cards);

  // Make sure everything is set up correctly.
  personal_data_manager_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // A user fills/enters the card's information on a checkout form with an empty
  // expiration date.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "Clyde Barrow",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4444333322221111",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2111", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  base::Optional<std::string> imported_vpa;
  EXPECT_FALSE(form_data_importer_->ImportFormData(
      form_structure,
      /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_vpa));
  EXPECT_FALSE(imported_credit_card);
}

// Ensure that we don't offer to save if we already have same card stored as a
// server card and user submitted an invalid expiration date year.
TEST_F(FormDataImporterTest,
       Metrics_SubmittedServerCardExpirationStatus_EmptyExpirationYear) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillUpstreamEditableExpirationDate);
  EnableWalletCardImport();

  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "4444333322221111" /* Visa */, "04", "2111", "1");

  test::SetServerCreditCards(autofill_table_, server_cards);

  // Make sure everything is set up correctly.
  personal_data_manager_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // A user fills/enters the card's information on a checkout form with an empty
  // expiration date.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "Clyde Barrow",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4444333322221111",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "08", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  base::Optional<std::string> imported_vpa;
  EXPECT_FALSE(form_data_importer_->ImportFormData(
      form_structure,
      /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_vpa));
  EXPECT_FALSE(imported_credit_card);
}

// Ensure that we still offer to save if we have different cards stored as a
// server card and user submitted an invalid expiration date year.
TEST_F(
    FormDataImporterTest,
    Metrics_SubmittedDifferentServerCardExpirationStatus_EmptyExpirationYear) {
  scoped_feature_list_.InitAndEnableFeature(
      features::kAutofillUpstreamEditableExpirationDate);
  EnableWalletCardImport();

  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "4111111111111111" /* Visa */, "04", "2111", "1");

  test::SetServerCreditCards(autofill_table_, server_cards);

  // Make sure everything is set up correctly.
  personal_data_manager_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // A user fills/enters the card's information on a checkout form with an empty
  // expiration date.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "Clyde Barrow",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4444333322221111",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "08", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  base::Optional<std::string> imported_vpa;
  EXPECT_TRUE(form_data_importer_->ImportFormData(
      form_structure,
      /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_vpa));
  EXPECT_TRUE(imported_credit_card);
}

TEST_F(FormDataImporterTest,
       Metrics_SubmittedServerCardExpirationStatus_FullServerCardMismatch) {
  EnableWalletCardImport();

  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::FULL_SERVER_CARD, "c789"));
  test::SetCreditCardInfo(&server_cards.back(), "Clyde Barrow",
                          "4444333322221111" /* Visa */, "04", "2111", "1");

  test::SetServerCreditCards(autofill_table_, server_cards);

  // Make sure everything is set up correctly.
  personal_data_manager_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // A user fills/enters the card's information on a checkout form but changes
  // the expiration date of the card.  Ensure that an expiration date mismatch
  // is recorded.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "Clyde Barrow",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4444333322221111",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "04", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2345", "text", &field);
  form.fields.push_back(field);

  base::HistogramTester histogram_tester;
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  base::Optional<std::string> imported_vpa;
  EXPECT_FALSE(form_data_importer_->ImportFormData(
      form_structure,
      /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_vpa));
  EXPECT_FALSE(imported_credit_card);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SubmittedServerCardExpirationStatus",
      AutofillMetrics::FULL_SERVER_CARD_EXPIRATION_DATE_DID_NOT_MATCH, 1);
}

TEST_F(FormDataImporterTest,
       Metrics_SubmittedServerCardExpirationStatus_MaskedServerCardMatch) {
  EnableWalletCardImport();

  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "a123"));
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "1111" /* Visa */, "01", "2111", "");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  test::SetServerCreditCards(autofill_table_, server_cards);

  // Make sure everything is set up correctly.
  personal_data_manager_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // A user fills/enters the card's information on a checkout form.  Ensure that
  // an expiration date match is recorded.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "Clyde Barrow",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4444333322221111",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "01", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2111", "text", &field);
  form.fields.push_back(field);

  base::HistogramTester histogram_tester;
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  base::Optional<std::string> imported_vpa;
  EXPECT_FALSE(form_data_importer_->ImportFormData(
      form_structure,
      /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_vpa));
  EXPECT_FALSE(imported_credit_card);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SubmittedServerCardExpirationStatus",
      AutofillMetrics::MASKED_SERVER_CARD_EXPIRATION_DATE_MATCHED, 1);
}

TEST_F(FormDataImporterTest,
       Metrics_SubmittedServerCardExpirationStatus_MaskedServerCardMismatch) {
  EnableWalletCardImport();

  std::vector<CreditCard> server_cards;
  server_cards.push_back(CreditCard(CreditCard::MASKED_SERVER_CARD, "a123"));
  test::SetCreditCardInfo(&server_cards.back(), "John Dillinger",
                          "1111" /* Visa */, "01", "2111", "");
  server_cards.back().SetNetworkForMaskedCard(kVisaCard);

  test::SetServerCreditCards(autofill_table_, server_cards);

  // Make sure everything is set up correctly.
  personal_data_manager_->Refresh();
  WaitForOnPersonalDataChanged();
  EXPECT_EQ(1U, personal_data_manager_->GetCreditCards().size());

  // A user fills/enters the card's information on a checkout form but changes
  // the expiration date of the card.  Ensure that an expiration date mismatch
  // is recorded.
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("Name on card:", "name_on_card", "Clyde Barrow",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number:", "card_number", "4444333322221111",
                            "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Month:", "exp_month", "04", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Exp Year:", "exp_year", "2345", "text", &field);
  form.fields.push_back(field);

  base::HistogramTester histogram_tester;
  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();
  std::unique_ptr<CreditCard> imported_credit_card;
  base::Optional<std::string> imported_vpa;
  EXPECT_FALSE(form_data_importer_->ImportFormData(
      form_structure, /*profile_autofill_enabled=*/true,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_vpa));
  EXPECT_FALSE(imported_credit_card);
  histogram_tester.ExpectUniqueSample(
      "Autofill.SubmittedServerCardExpirationStatus",
      AutofillMetrics::MASKED_SERVER_CARD_EXPIRATION_DATE_DID_NOT_MATCH, 1);
}

TEST_F(FormDataImporterTest, ImportVPA) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("VPA:", "vpa", "user@indianbank", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();

  std::unique_ptr<CreditCard> imported_credit_card;  // Discarded.
  base::Optional<std::string> imported_vpa;

  EXPECT_TRUE(form_data_importer_->ImportFormData(
      form_structure, /*profile_autofill_enabled=*/false,
      /*credit_card_autofill_enabled=*/true,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_vpa));

  ASSERT_TRUE(imported_vpa.has_value());
  EXPECT_EQ(imported_vpa.value(), "user@indianbank");
}

TEST_F(FormDataImporterTest, ImportVPADisabled) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("VPA:", "vpa", "user@indianbank", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();

  std::unique_ptr<CreditCard> imported_credit_card;  // Discarded.
  base::Optional<std::string> imported_vpa;

  EXPECT_FALSE(form_data_importer_->ImportFormData(
      form_structure, /*profile_autofill_enabled=*/false,
      /*credit_card_autofill_enabled=*/false,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_vpa));

  EXPECT_FALSE(imported_vpa.has_value());
}

TEST_F(FormDataImporterTest, ImportVPAIgnoreNonVPA) {
  FormData form;
  form.url = GURL("https://wwww.foo.com");

  FormFieldData field;
  test::CreateTestFormField("VPA:", "vpa", "user@gmail.com", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);
  form_structure.DetermineHeuristicTypes();

  std::unique_ptr<CreditCard> imported_credit_card;  // Discarded.
  base::Optional<std::string> imported_vpa;

  EXPECT_FALSE(form_data_importer_->ImportFormData(
      form_structure, /*profile_autofill_enabled=*/false,
      /*credit_card_autofill_enabled=*/false,
      /*should_return_local_card=*/false, &imported_credit_card,
      &imported_vpa));

  EXPECT_FALSE(imported_vpa.has_value());
}

}  // namespace autofill
