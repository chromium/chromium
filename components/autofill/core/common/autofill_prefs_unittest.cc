// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_prefs.h"

#include <memory>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/pref_service_factory.h"
#include "components/prefs/testing_pref_store.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace prefs {

class AutofillPrefsTest : public testing::Test {
 protected:
  AutofillPrefsTest() = default;
  ~AutofillPrefsTest() override = default;

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

// Tests that setting and getting the AutofillSyncTransportOptIn works as
// expected.
// On mobile, no dedicated opt-in is required for WalletSyncTransport - the
// user is always considered opted-in and thus this test doesn't make sense.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
TEST_F(AutofillPrefsTest, WalletSyncTransportPref_GetAndSet) {
  const CoreAccountId account1 = CoreAccountId::FromGaiaId("account1");
  const CoreAccountId account2 = CoreAccountId::FromGaiaId("account2");

  // There should be no opt-in recorded at first.
  ASSERT_FALSE(IsUserOptedInWalletSyncTransport(pref_service(), account1));
  ASSERT_FALSE(IsUserOptedInWalletSyncTransport(pref_service(), account2));
  // There should be no entry for the accounts in the dictionary.
  EXPECT_TRUE(
      pref_service()->GetDict(prefs::kAutofillSyncTransportOptIn).empty());

  // Set the opt-in for the first account.
  SetUserOptedInWalletSyncTransport(pref_service(), account1, true);
  EXPECT_TRUE(IsUserOptedInWalletSyncTransport(pref_service(), account1));
  EXPECT_FALSE(IsUserOptedInWalletSyncTransport(pref_service(), account2));
  // There should only be one entry in the dictionary.
  EXPECT_EQ(1U,
            pref_service()->GetDict(prefs::kAutofillSyncTransportOptIn).size());

  // Unset the opt-in for the first account.
  SetUserOptedInWalletSyncTransport(pref_service(), account1, false);
  EXPECT_FALSE(IsUserOptedInWalletSyncTransport(pref_service(), account1));
  EXPECT_FALSE(IsUserOptedInWalletSyncTransport(pref_service(), account2));
  // There should be no entry for the accounts in the dictionary.
  EXPECT_TRUE(
      pref_service()->GetDict(prefs::kAutofillSyncTransportOptIn).empty());

  // Set the opt-in for the second account.
  SetUserOptedInWalletSyncTransport(pref_service(), account2, true);
  EXPECT_FALSE(IsUserOptedInWalletSyncTransport(pref_service(), account1));
  EXPECT_TRUE(IsUserOptedInWalletSyncTransport(pref_service(), account2));
  // There should only be one entry in the dictionary.
  EXPECT_EQ(1U,
            pref_service()->GetDict(prefs::kAutofillSyncTransportOptIn).size());

  // Set the opt-in for the first account too.
  SetUserOptedInWalletSyncTransport(pref_service(), account1, true);
  EXPECT_TRUE(IsUserOptedInWalletSyncTransport(pref_service(), account1));
  EXPECT_TRUE(IsUserOptedInWalletSyncTransport(pref_service(), account1));
  // There should be tow entries in the dictionary.
  EXPECT_EQ(2U,
            pref_service()->GetDict(prefs::kAutofillSyncTransportOptIn).size());
}
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)

// Tests that AutofillSyncTransportOptIn is not stored using the plain text
// account id.
TEST_F(AutofillPrefsTest, WalletSyncTransportPref_UsesHashAccountId) {
  const CoreAccountId account1 = CoreAccountId::FromGaiaId("account1");

  // There should be no opt-in recorded at first.
  EXPECT_TRUE(
      pref_service()->GetDict(prefs::kAutofillSyncTransportOptIn).empty());

  // Set the opt-in for the first account.
  SetUserOptedInWalletSyncTransport(pref_service(), account1, true);
  EXPECT_FALSE(
      pref_service()->GetDict(prefs::kAutofillSyncTransportOptIn).empty());

  // Make sure that the dictionary keys don't contain the account id.
  const auto& dictionary =
      pref_service()->GetDict(prefs::kAutofillSyncTransportOptIn);
  EXPECT_EQ(std::nullopt, dictionary.FindInt(account1.ToString()));
}

// Tests that clearing the AutofillSyncTransportOptIn works as expected.
TEST_F(AutofillPrefsTest, WalletSyncTransportPref_Clear) {
  const CoreAccountId account1 = CoreAccountId::FromGaiaId("account1");
  const CoreAccountId account2 = CoreAccountId::FromGaiaId("account2");

  // There should be no opt-in recorded at first.
  EXPECT_TRUE(
      pref_service()->GetDict(prefs::kAutofillSyncTransportOptIn).empty());

  // Set the opt-in for the first account.
  SetUserOptedInWalletSyncTransport(pref_service(), account1, true);
  EXPECT_FALSE(
      pref_service()->GetDict(prefs::kAutofillSyncTransportOptIn).empty());

  // Set the opt-in for the second account.
  SetUserOptedInWalletSyncTransport(pref_service(), account2, true);
  EXPECT_FALSE(
      pref_service()->GetDict(prefs::kAutofillSyncTransportOptIn).empty());

  // Clear all opt-ins. The dictionary should be empty.
  ClearSyncTransportOptIns(pref_service());
  EXPECT_TRUE(
      pref_service()->GetDict(prefs::kAutofillSyncTransportOptIn).empty());
}

// Tests that the account id hash that we generate can be written and read from
// JSON properly.
TEST_F(AutofillPrefsTest, WalletSyncTransportPref_CanBeSetAndReadFromJSON) {
  const CoreAccountId account1 = CoreAccountId::FromGaiaId("account1");

  // Set the opt-in for the first account.
  SetUserOptedInWalletSyncTransport(pref_service(), account1, true);
  EXPECT_FALSE(
      pref_service()->GetDict(prefs::kAutofillSyncTransportOptIn).empty());

  const base::Value::Dict& dictionary =
      pref_service()->GetDict(prefs::kAutofillSyncTransportOptIn);

  std::string output_js;
  ASSERT_TRUE(base::JSONWriter::Write(dictionary, &output_js));
  EXPECT_EQ(dictionary, *base::JSONReader::Read(output_js));
}

#if BUILDFLAG(IS_ANDROID)
TEST_F(AutofillPrefsTest, FacilitatedPaymentsPixPref_DefaultValueSetToTrue) {
  EXPECT_TRUE(pref_service()->GetBoolean(prefs::kFacilitatedPaymentsPix));
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace prefs
}  // namespace autofill
