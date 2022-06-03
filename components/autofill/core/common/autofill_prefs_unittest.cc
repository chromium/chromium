// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_prefs.h"

#include <memory>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "build/build_config.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/testing_pref_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace prefs {

class AutofillPrefsTest : public testing::Test {
 protected:
  AutofillPrefsTest() {}
  ~AutofillPrefsTest() override {}

  void SetUp() override { pref_service_ = CreatePrefServiceAndRegisterPrefs(); }

  // Creates a PrefService and registers Autofill prefs.
  std::unique_ptr<PrefService> CreatePrefServiceAndRegisterPrefs() {
    scoped_refptr<user_prefs::PrefRegistrySyncable> registry(
        new user_prefs::PrefRegistrySyncable());
    RegisterProfilePrefs(registry.get());
    PrefServiceFactory factory;
    factory.set_user_prefs(base::MakeRefCounted<TestingPrefStore>());
    return factory.Create(registry);
  }

  PrefService* pref_service() { return pref_service_.get(); }

 private:
  std::unique_ptr<PrefService> pref_service_;
};

// Tests migrating the value of the deprecated Autofill master pref to the new
// prefs and that this operation takes place only once.
TEST_F(AutofillPrefsTest, MigrateDeprecatedAutofillPrefs) {
  // All prefs should be enabled by default.
  ASSERT_TRUE(pref_service()->GetBoolean(kAutofillEnabledDeprecated));
  ASSERT_TRUE(pref_service()->GetBoolean(kAutofillProfileEnabled));
  ASSERT_TRUE(pref_service()->GetBoolean(kAutofillCreditCardEnabled));

  // Disable the deprecated master pref and make sure the new fine-grained prefs
  // are not affected by that.
  pref_service()->SetBoolean(kAutofillEnabledDeprecated, false);
  EXPECT_FALSE(pref_service()->GetBoolean(kAutofillEnabledDeprecated));
  EXPECT_TRUE(pref_service()->GetBoolean(kAutofillProfileEnabled));
  EXPECT_TRUE(pref_service()->GetBoolean(kAutofillCreditCardEnabled));

  // Migrate the deprecated master pref's value to the new fine-grained prefs.
  // Their values should become the same as the deprecated master pref's value.
  MigrateDeprecatedAutofillPrefs(pref_service());
  EXPECT_FALSE(pref_service()->GetBoolean(kAutofillProfileEnabled));
  EXPECT_FALSE(pref_service()->GetBoolean(kAutofillCreditCardEnabled));

  // Enable the deprecated master pref and make sure the new fine-grained prefs
  // are not affected by that.
  pref_service()->SetBoolean(kAutofillEnabledDeprecated, true);
  EXPECT_TRUE(pref_service()->GetBoolean(kAutofillEnabledDeprecated));
  EXPECT_FALSE(pref_service()->GetBoolean(kAutofillProfileEnabled));
  EXPECT_FALSE(pref_service()->GetBoolean(kAutofillCreditCardEnabled));

  // Migrate the deprecated master pref's value to the new fine-grained prefs.
  // Their values should not be affected when migration happens a second time.
  MigrateDeprecatedAutofillPrefs(pref_service());
  EXPECT_FALSE(pref_service()->GetBoolean(kAutofillProfileEnabled));
  EXPECT_FALSE(pref_service()->GetBoolean(kAutofillCreditCardEnabled));
}

// Tests that setting and getting the AutofillSyncTransportOptIn works as
// expected.
// On mobile, no dedicated opt-in is required for WalletSyncTransport - the
// user is always considered opted-in and thus this test doesn't make sense.
#if !defined(OS_ANDROID) && !defined(OS_IOS)
TEST_F(AutofillPrefsTest, WalletSyncTransportPref_GetAndSet) {
  const CoreAccountId account1("account1");
  const CoreAccountId account2("account2");

  // There should be no opt-in recorded at first.
  ASSERT_FALSE(IsUserOptedInWalletSyncTransport(pref_service(), account1));
  ASSERT_FALSE(IsUserOptedInWalletSyncTransport(pref_service(), account2));
  // There should be no entry for the accounts in the dictionary.
  EXPECT_TRUE(pref_service()
                  ->GetDictionary(prefs::kAutofillSyncTransportOptIn)
                  ->DictEmpty());

  // Set the opt-in for the first account.
  SetUserOptedInWalletSyncTransport(pref_service(), account1, true);
  EXPECT_TRUE(IsUserOptedInWalletSyncTransport(pref_service(), account1));
  EXPECT_FALSE(IsUserOptedInWalletSyncTransport(pref_service(), account2));
  // There should only be one entry in the dictionary.
  EXPECT_EQ(1U, pref_service()
                    ->GetDictionary(prefs::kAutofillSyncTransportOptIn)
                    ->DictSize());

  // Unset the opt-in for the first account.
  SetUserOptedInWalletSyncTransport(pref_service(), account1, false);
  EXPECT_FALSE(IsUserOptedInWalletSyncTransport(pref_service(), account1));
  EXPECT_FALSE(IsUserOptedInWalletSyncTransport(pref_service(), account2));
  // There should be no entry for the accounts in the dictionary.
  EXPECT_TRUE(pref_service()
                  ->GetDictionary(prefs::kAutofillSyncTransportOptIn)
                  ->DictEmpty());

  // Set the opt-in for the second account.
  SetUserOptedInWalletSyncTransport(pref_service(), account2, true);
  EXPECT_FALSE(IsUserOptedInWalletSyncTransport(pref_service(), account1));
  EXPECT_TRUE(IsUserOptedInWalletSyncTransport(pref_service(), account2));
  // There should only be one entry in the dictionary.
  EXPECT_EQ(1U, pref_service()
                    ->GetDictionary(prefs::kAutofillSyncTransportOptIn)
                    ->DictSize());

  // Set the opt-in for the first account too.
  SetUserOptedInWalletSyncTransport(pref_service(), account1, true);
  EXPECT_TRUE(IsUserOptedInWalletSyncTransport(pref_service(), account1));
  EXPECT_TRUE(IsUserOptedInWalletSyncTransport(pref_service(), account1));
  // There should be tow entries in the dictionary.
  EXPECT_EQ(2U, pref_service()
                    ->GetDictionary(prefs::kAutofillSyncTransportOptIn)
                    ->DictSize());
}
#endif  // !defined(OS_ANDROID) && !defined(OS_IOS)

// Tests that AutofillSyncTransportOptIn is not stored using the plain text
// account id.
TEST_F(AutofillPrefsTest, WalletSyncTransportPref_UsesHashAccountId) {
  const CoreAccountId account1("account1");

  // There should be no opt-in recorded at first.
  EXPECT_TRUE(pref_service()
                  ->GetDictionary(prefs::kAutofillSyncTransportOptIn)
                  ->DictEmpty());

  // Set the opt-in for the first account.
  SetUserOptedInWalletSyncTransport(pref_service(), account1, true);
  EXPECT_FALSE(pref_service()
                   ->GetDictionary(prefs::kAutofillSyncTransportOptIn)
                   ->DictEmpty());

  // Make sure that the dictionary keys don't contain the account id.
  auto* dictionary =
      pref_service()->GetDictionary(prefs::kAutofillSyncTransportOptIn);
  EXPECT_EQ(nullptr, dictionary->FindKeyOfType(account1.ToString(),
                                               base::Value::Type::INTEGER));
}

// Tests that clearing the AutofillSyncTransportOptIn works as expected.
TEST_F(AutofillPrefsTest, WalletSyncTransportPref_Clear) {
  const CoreAccountId account1("account1");
  const CoreAccountId account2("account2");

  // There should be no opt-in recorded at first.
  EXPECT_TRUE(pref_service()
                  ->GetDictionary(prefs::kAutofillSyncTransportOptIn)
                  ->DictEmpty());

  // Set the opt-in for the first account.
  SetUserOptedInWalletSyncTransport(pref_service(), account1, true);
  EXPECT_FALSE(pref_service()
                   ->GetDictionary(prefs::kAutofillSyncTransportOptIn)
                   ->DictEmpty());

  // Set the opt-in for the second account.
  SetUserOptedInWalletSyncTransport(pref_service(), account2, true);
  EXPECT_FALSE(pref_service()
                   ->GetDictionary(prefs::kAutofillSyncTransportOptIn)
                   ->DictEmpty());

  // Clear all opt-ins. The dictionary should be empty.
  ClearSyncTransportOptIns(pref_service());
  EXPECT_TRUE(pref_service()
                  ->GetDictionary(prefs::kAutofillSyncTransportOptIn)
                  ->DictEmpty());
}

// Tests that the account id hash that we generate can be written and read from
// JSON properly.
TEST_F(AutofillPrefsTest, WalletSyncTransportPref_CanBeSetAndReadFromJSON) {
  const CoreAccountId account1("account1");

  // Set the opt-in for the first account.
  SetUserOptedInWalletSyncTransport(pref_service(), account1, true);
  EXPECT_FALSE(pref_service()
                   ->GetDictionary(prefs::kAutofillSyncTransportOptIn)
                   ->DictEmpty());

  const base::Value* dictionary =
      pref_service()->GetDictionary(prefs::kAutofillSyncTransportOptIn);
  ASSERT_TRUE(dictionary);

  std::string output_js;
  ASSERT_TRUE(base::JSONWriter::Write(*dictionary, &output_js));
  EXPECT_EQ(*dictionary, *base::JSONReader::Read(output_js));
}

}  // namespace prefs
}  // namespace autofill
