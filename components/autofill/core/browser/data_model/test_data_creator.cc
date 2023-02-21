// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/data_model/test_data_creator.h"

#include <inttypes.h>

#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"

namespace autofill {

namespace {
// Time delta to create test data.
base::TimeDelta DeletableUseDateDelta(
    const base::TimeDelta& cc_deletion_delta) {
  static base::TimeDelta delta = cc_deletion_delta + base::Days(5);
  return delta;
}
base::TimeDelta DeletableExpiryDateDelta(
    const base::TimeDelta& cc_deletion_delta) {
  static base::TimeDelta delta = cc_deletion_delta + base::Days(45);
  return delta;
}
}  // namespace

TestDataCreator::TestDataCreator(base::TimeDelta cc_deletion_delta,
                                 std::string app_locale)
    : cc_deletion_delta_(cc_deletion_delta), app_locale_(app_locale) {}

void TestDataCreator::MaybeAddTestProfiles(
    const base::RepeatingCallback<void(const AutofillProfile&)>&
        add_profile_callback) {
  if (has_created_test_addresses_ ||
      !base::FeatureList::IsEnabled(
          features::test::kAutofillCreateDataForTest)) {
    return;
  }

  has_created_test_addresses_ = true;

  for (const auto& profile : GetTestProfiles()) {
    add_profile_callback.Run(profile);
  }

  DLOG(WARNING) << this << " added fake autofill profiles.";
}

void TestDataCreator::MaybeAddTestCreditCards(
    const base::RepeatingCallback<void(const CreditCard&)>& add_cc_callback) {
  if (has_created_test_credit_cards_ ||
      !base::FeatureList::IsEnabled(
          features::test::kAutofillCreateDataForTest)) {
    return;
  }

  has_created_test_credit_cards_ = true;

  for (const auto& credit_card : GetTestCreditCards()) {
    add_cc_callback.Run(credit_card);
  }

  DLOG(WARNING) << this << " added fake credit cards.";
}

std::vector<AutofillProfile> TestDataCreator::GetTestProfiles() {
  return {CreateBasicTestAddress(), CreateDisusedTestAddress(),
          CreateDisusedDeletableTestAddress()};
}

std::vector<CreditCard> TestDataCreator::GetTestCreditCards() {
  return {CreateBasicTestCreditCard(), CreateDisusedTestCreditCard(),
          CreateDisusedDeletableTestCreditCard()};
}

AutofillProfile TestDataCreator::CreateBasicTestAddress() {
  const base::Time use_date = AutofillClock::Now() - base::Days(20);
  AutofillProfile profile;
  profile.SetInfo(NAME_FULL, u"John McTester", app_locale_);
  profile.SetInfo(COMPANY_NAME, u"Test Inc.", app_locale_);
  profile.SetInfo(EMAIL_ADDRESS, u"jmctester@fake.chromium.org", app_locale_);
  profile.SetInfo(ADDRESS_HOME_LINE1, u"123 Invented Street", app_locale_);
  profile.SetInfo(ADDRESS_HOME_LINE2, u"Suite A", app_locale_);
  profile.SetInfo(ADDRESS_HOME_CITY, u"Mountain View", app_locale_);
  profile.SetInfo(ADDRESS_HOME_STATE, u"California", app_locale_);
  profile.SetInfo(ADDRESS_HOME_ZIP, u"94043", app_locale_);
  profile.SetInfo(ADDRESS_HOME_COUNTRY, u"US", app_locale_);
  profile.SetInfo(PHONE_HOME_WHOLE_NUMBER, u"844-555-0173", app_locale_);
  profile.set_use_date(use_date);
  return profile;
}

AutofillProfile TestDataCreator::CreateDisusedTestAddress() {
  const base::Time use_date = AutofillClock::Now() - base::Days(185);
  AutofillProfile profile;
  profile.SetInfo(NAME_FULL, u"Polly Disused", app_locale_);
  profile.SetInfo(COMPANY_NAME,
                  base::UTF8ToUTF16(base::StringPrintf(
                      "%" PRIu64 " Inc.",
                      use_date.ToDeltaSinceWindowsEpoch().InMicroseconds())),
                  app_locale_);
  profile.SetInfo(EMAIL_ADDRESS, u"polly.disused@fake.chromium.org",
                  app_locale_);
  profile.SetInfo(ADDRESS_HOME_LINE1, u"456 Disused Lane", app_locale_);
  profile.SetInfo(ADDRESS_HOME_LINE2, u"Apt. B", app_locale_);
  profile.SetInfo(ADDRESS_HOME_CITY, u"Austin", app_locale_);
  profile.SetInfo(ADDRESS_HOME_STATE, u"Texas", app_locale_);
  profile.SetInfo(ADDRESS_HOME_ZIP, u"73301", app_locale_);
  profile.SetInfo(ADDRESS_HOME_COUNTRY, u"US", app_locale_);
  profile.SetInfo(PHONE_HOME_WHOLE_NUMBER, u"844-555-0174", app_locale_);
  profile.set_use_date(use_date);
  return profile;
}

AutofillProfile TestDataCreator::CreateDisusedDeletableTestAddress() {
  const base::Time use_date = AutofillClock::Now() - base::Days(400);
  AutofillProfile profile;
  profile.SetInfo(NAME_FULL, u"Polly Deletable", app_locale_);
  profile.SetInfo(COMPANY_NAME,
                  base::UTF8ToUTF16(base::StringPrintf(
                      "%" PRIu64 " Inc.",
                      use_date.ToDeltaSinceWindowsEpoch().InMicroseconds())),
                  app_locale_);
  profile.SetInfo(EMAIL_ADDRESS, u"polly.deletable@fake.chromium.org",
                  app_locale_);
  profile.SetInfo(ADDRESS_HOME_LINE1, u"459 Deletable Lane", app_locale_);
  profile.SetInfo(ADDRESS_HOME_LINE2, u"Apt. B", app_locale_);
  profile.SetInfo(ADDRESS_HOME_CITY, u"Austin", app_locale_);
  profile.SetInfo(ADDRESS_HOME_STATE, u"Texas", app_locale_);
  profile.SetInfo(ADDRESS_HOME_ZIP, u"73301", app_locale_);
  profile.SetInfo(ADDRESS_HOME_COUNTRY, u"US", app_locale_);
  profile.SetInfo(PHONE_HOME_WHOLE_NUMBER, u"844-555-0274", app_locale_);
  profile.set_use_date(use_date);
  return profile;
}

// Create a card expiring 500 days from now which was last used 10 days ago.
CreditCard TestDataCreator::CreateBasicTestCreditCard() {
  const base::Time now = AutofillClock::Now();
  const base::Time use_date = now - base::Days(10);
  base::Time::Exploded expiry_date;
  (now + base::Days(500)).LocalExplode(&expiry_date);

  CreditCard credit_card;
  credit_card.SetInfo(CREDIT_CARD_NAME_FULL, u"Alice Testerson", app_locale_);
  credit_card.SetInfo(CREDIT_CARD_NUMBER, u"4545454545454545", app_locale_);
  credit_card.SetExpirationMonth(expiry_date.month);
  credit_card.SetExpirationYear(expiry_date.year);
  credit_card.set_use_date(use_date);
  return credit_card;
}

CreditCard TestDataCreator::CreateDisusedTestCreditCard() {
  const base::Time now = AutofillClock::Now();
  const base::Time use_date = now - base::Days(185);
  base::Time::Exploded expiry_date;
  (now - base::Days(200)).LocalExplode(&expiry_date);

  CreditCard credit_card;
  credit_card.SetInfo(CREDIT_CARD_NAME_FULL, u"Bob Disused", app_locale_);
  credit_card.SetInfo(CREDIT_CARD_NUMBER, u"4111111111111111", app_locale_);
  credit_card.SetExpirationMonth(expiry_date.month);
  credit_card.SetExpirationYear(expiry_date.year);
  credit_card.set_use_date(use_date);
  return credit_card;
}

CreditCard TestDataCreator::CreateDisusedDeletableTestCreditCard() {
  const base::Time now = AutofillClock::Now();
  const base::Time use_date = now - DeletableUseDateDelta(cc_deletion_delta_);
  base::Time::Exploded expiry_date;
  (now - DeletableExpiryDateDelta(cc_deletion_delta_))
      .LocalExplode(&expiry_date);

  CreditCard credit_card;
  credit_card.SetInfo(CREDIT_CARD_NAME_FULL, u"Charlie Deletable", app_locale_);
  credit_card.SetInfo(CREDIT_CARD_NUMBER, u"378282246310005", app_locale_);
  credit_card.SetExpirationMonth(expiry_date.month);
  credit_card.SetExpirationYear(expiry_date.year);
  credit_card.set_use_date(use_date);
  return credit_card;
}
}  // namespace autofill
