// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payments_profile_comparator.h"

#include <memory>
#include <vector>

#include "base/guid.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/payments/core/payment_options_provider.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

using autofill::AutofillProfile;

namespace payments {

constexpr uint32_t kRequestPayerName = 1 << 0;
constexpr uint32_t kRequestPayerEmail = 1 << 1;
constexpr uint32_t kRequestPayerPhone = 1 << 2;
constexpr uint32_t kRequestShipping = 1 << 3;

class MockPaymentOptionsProvider : public PaymentOptionsProvider {
 public:
  MockPaymentOptionsProvider(uint32_t options) : options_(options) {}

  ~MockPaymentOptionsProvider() override {}
  bool request_payer_name() const override {
    return options_ & kRequestPayerName;
  }
  bool request_payer_email() const override {
    return options_ & kRequestPayerEmail;
  }
  bool request_payer_phone() const override {
    return options_ & kRequestPayerPhone;
  }
  bool request_shipping() const override { return options_ & kRequestShipping; }
  PaymentShippingType shipping_type() const override {
    return PaymentShippingType::SHIPPING;
  }

 private:
  uint32_t options_;
};

AutofillProfile CreateProfileWithContactInfo(const char* name,
                                             const char* email,
                                             const char* phone) {
  AutofillProfile profile(base::GenerateGUID(), "http://www.example.com/");
  autofill::test::SetProfileInfo(&profile, name, "", "", email, "", "", "", "",
                                 "", "", "", phone);
  return profile;
}

AutofillProfile CreateProfileWithCompleteAddress(const char* name,
                                                 const char* phone) {
  AutofillProfile profile(base::GenerateGUID(), "http://www.example.com/");
  autofill::test::SetProfileInfo(&profile, name, "", "", "", "", "123 Fake St.",
                                 "", "Fakesville", "MN", "54000", "US", phone);
  return profile;
}

AutofillProfile CreateProfileWithPartialAddress(const char* name,
                                                const char* phone) {
  AutofillProfile profile(base::GenerateGUID(), "http://www.example.com/");
  autofill::test::SetProfileInfo(&profile, name, "", "", "", "", "123 Fake St.",
                                 "", "", "", "54000", "", phone);
  return profile;
}

TEST(PaymentRequestProfileUtilTest, FilterProfilesForContact) {
  // These profiles are subset/equal, so only the first complete one is
  // included.
  AutofillProfile exclude_1 =
      CreateProfileWithContactInfo("Homer", "", "6515553226");

  AutofillProfile exclude_2 =
      CreateProfileWithContactInfo("Homer", "homer@simpson.net", "");

  AutofillProfile include_1 =
      CreateProfileWithContactInfo("Homer", "homer@simpson.net", "6515553226");

  AutofillProfile exclude_3 =
      CreateProfileWithContactInfo("Homer", "homer@simpson.net", "6515553226");

  // This profile is different, so it should also be included. Since it is
  // less complete than |include_1|, it will appear after.
  AutofillProfile include_2 =
      CreateProfileWithContactInfo("Marge", "marge@simpson.net", "");

  // This profile is different, so it should also be included. Since it is
  // equally complete with |include_1|, it will appear before |include_2|, but
  // after |include_1| since order is preserved amongst profiles of equal
  // completeness.
  AutofillProfile include_3 = CreateProfileWithContactInfo(
      "Bart", "eatmyshorts@simpson.net", "6515553226");

  std::vector<AutofillProfile*> profiles = {&exclude_1, &exclude_2, &include_1,
                                            &exclude_3, &include_2, &include_3};

  MockPaymentOptionsProvider provider(kRequestPayerName | kRequestPayerEmail |
                                      kRequestPayerPhone);
  PaymentsProfileComparator comp("en-US", provider);

  std::vector<AutofillProfile*> filtered =
      comp.FilterProfilesForContact(profiles);

  ASSERT_EQ(3u, filtered.size());
  EXPECT_EQ(&include_1, filtered[0]);
  EXPECT_EQ(&include_3, filtered[1]);
  EXPECT_EQ(&include_2, filtered[2]);

  // Repeat the filter using a provider set to only request phone numbers.
  // Under these rules, since all profiles have the same (or no) phone number,
  // we should only see the first profile with a phone number, |exclude_1|.
  MockPaymentOptionsProvider phone_only_provider(kRequestPayerPhone);
  PaymentsProfileComparator phone_only_comp("en-US", phone_only_provider);
  std::vector<AutofillProfile*> filtered_phones =
      phone_only_comp.FilterProfilesForContact(profiles);
  ASSERT_EQ(1u, filtered_phones.size());
  EXPECT_EQ(&exclude_1, filtered_phones[0]);
}

TEST(PaymentRequestProfileUtilTest, IsContactEqualOrSuperset) {
  MockPaymentOptionsProvider provider(kRequestPayerName | kRequestPayerEmail |
                                      kRequestPayerPhone);
  PaymentsProfileComparator comp("en-US", provider);

  AutofillProfile p1 =
      CreateProfileWithContactInfo("Homer", "homer@simpson.net", "6515553226");

  // Candidate subset profile is equal.
  AutofillProfile p2 =
      CreateProfileWithContactInfo("Homer", "homer@simpson.net", "6515553226");
  EXPECT_TRUE(comp.IsContactEqualOrSuperset(p1, p2));
  EXPECT_TRUE(comp.IsContactEqualOrSuperset(p2, p1));

  // Candidate subset profile has non-matching fields.
  AutofillProfile p3 = CreateProfileWithContactInfo(
      "Homer", "homer@springfieldnuclear.gov", "6515553226");
  EXPECT_FALSE(comp.IsContactEqualOrSuperset(p1, p3));
  EXPECT_FALSE(comp.IsContactEqualOrSuperset(p3, p1));

  // Candidate subset profile is equal, except for missing fields.
  AutofillProfile p4 =
      CreateProfileWithContactInfo("Homer", "homer@simpson.net", "");
  EXPECT_TRUE(comp.IsContactEqualOrSuperset(p1, p4));
  EXPECT_FALSE(comp.IsContactEqualOrSuperset(p4, p1));

  // One field is common, but each has a field which the other is missing.
  AutofillProfile p5 =
      CreateProfileWithContactInfo("Homer", "homer@simpson.net", "");
  AutofillProfile p6 = CreateProfileWithContactInfo("Homer", "", "6515553226");
  EXPECT_FALSE(comp.IsContactEqualOrSuperset(p5, p6));
  EXPECT_FALSE(comp.IsContactEqualOrSuperset(p6, p5));
}

TEST(PaymentRequestProfileUtilTest, IsContactEqualOrSuperset_WithFieldIgnored) {
  // Discrepancies in email should be ignored throughout this test.
  MockPaymentOptionsProvider provider(kRequestPayerName | kRequestPayerPhone);
  PaymentsProfileComparator comp("en-US", provider);

  AutofillProfile p1 =
      CreateProfileWithContactInfo("Homer", "homer@simpson.net", "6515553226");

  // Candidate subset profile is equal.
  AutofillProfile p2 =
      CreateProfileWithContactInfo("Homer", "homer@simpson.net", "6515553226");
  EXPECT_TRUE(comp.IsContactEqualOrSuperset(p1, p2));
  EXPECT_TRUE(comp.IsContactEqualOrSuperset(p2, p1));

  // Email fields don't match, but profiles are still equal.
  AutofillProfile p3 = CreateProfileWithContactInfo(
      "Homer", "homer@springfieldnuclear.gov", "6515553226");
  EXPECT_TRUE(comp.IsContactEqualOrSuperset(p1, p3));
  EXPECT_TRUE(comp.IsContactEqualOrSuperset(p3, p1));

  // Profile without an email is mutual subset of profile with an email.
  AutofillProfile p4 = CreateProfileWithContactInfo("Homer", "", "6515553226");
  EXPECT_TRUE(comp.IsContactEqualOrSuperset(p1, p4));
  EXPECT_TRUE(comp.IsContactEqualOrSuperset(p4, p1));
}

TEST(PaymentRequestProfileUtilTest, GetContactCompletenessScore) {
  MockPaymentOptionsProvider provider(kRequestPayerName | kRequestPayerPhone);
  PaymentsProfileComparator comp("en-US", provider);

  // Two completeness points: One each for name and phone number, but not email
  // as it was not requested.
  AutofillProfile p1 =
      CreateProfileWithContactInfo("Homer", "homer@simpson.net", "6515553226");
  EXPECT_EQ(2, comp.GetContactCompletenessScore(&p1));

  // One completeness point for name, no points for phone number (missing) or
  // email (not requested).
  AutofillProfile p2 =
      CreateProfileWithContactInfo("Homer", "homer@simpson.net", "");
  EXPECT_EQ(1, comp.GetContactCompletenessScore(&p2));

  // No completeness points, as the only field present was not requested.
  AutofillProfile p3 =
      CreateProfileWithContactInfo("", "homer@simpson.net", "");
  EXPECT_EQ(0, comp.GetContactCompletenessScore(&p3));

  // Null profile returns 0.
  EXPECT_EQ(0, comp.GetContactCompletenessScore(nullptr));
}

TEST(PaymentRequestProfileUtilTest, IsContactInfoComplete) {
  MockPaymentOptionsProvider provider(kRequestPayerName | kRequestPayerEmail);
  PaymentsProfileComparator comp("en-US", provider);

  // If name and email are present, return true regardless of the (ignored)
  // phone value.
  AutofillProfile p1 =
      CreateProfileWithContactInfo("Homer", "homer@simpson.net", "6515553226");
  AutofillProfile p2 =
      CreateProfileWithContactInfo("Homer", "homer@simpson.net", "");

  EXPECT_TRUE(comp.IsContactInfoComplete(&p1));
  EXPECT_TRUE(comp.IsContactInfoComplete(&p2));

  // If name is not present, return false regardless of the (ignored)
  // phone value.
  AutofillProfile p3 =
      CreateProfileWithContactInfo("", "homer@simpson.net", "6515553226");
  AutofillProfile p4 =
      CreateProfileWithContactInfo("", "homer@simpson.net", "");

  EXPECT_FALSE(comp.IsContactInfoComplete(&p3));
  EXPECT_FALSE(comp.IsContactInfoComplete(&p4));

  // If no fields are requested, any profile (even empty or null) is complete.
  MockPaymentOptionsProvider empty_provider(0);
  PaymentsProfileComparator empty_comp("en-US", empty_provider);

  AutofillProfile p5 = CreateProfileWithContactInfo("", "", "");

  EXPECT_TRUE(empty_comp.IsContactInfoComplete(&p1));
  EXPECT_TRUE(empty_comp.IsContactInfoComplete(&p5));
  EXPECT_TRUE(empty_comp.IsContactInfoComplete(nullptr));
}

TEST(PaymentRequestProfileUtilTest, FilterProfilesForShipping) {
  MockPaymentOptionsProvider provider(kRequestShipping);
  PaymentsProfileComparator comp("en-US", provider);

  AutofillProfile address_only = CreateProfileWithCompleteAddress("", "");

  AutofillProfile no_name = CreateProfileWithCompleteAddress("", "6515553226");
  AutofillProfile no_phone = CreateProfileWithCompleteAddress("Homer", "");

  AutofillProfile empty = CreateProfileWithContactInfo("", "", "");

  AutofillProfile complete1 =
      CreateProfileWithCompleteAddress("Homer", "6515553226");

  AutofillProfile partial_address =
      CreateProfileWithPartialAddress("Homer", "6515553226");
  AutofillProfile no_address =
      CreateProfileWithContactInfo("Homer", "", "6515553226");

  AutofillProfile complete2 =
      CreateProfileWithCompleteAddress("Bart", "6515553226");

  AutofillProfile partial_no_phone =
      CreateProfileWithPartialAddress("", "6515553226");
  AutofillProfile partial_no_name =
      CreateProfileWithPartialAddress("Homer", "");

  std::vector<AutofillProfile*> profiles = {
      &address_only,     &no_name,         &no_phone,   &empty,
      &complete1,        &partial_address, &no_address, &complete2,
      &partial_no_phone, &partial_no_name};

  std::vector<AutofillProfile*> filtered =
      comp.FilterProfilesForShipping(profiles);

  // Current logic does not remove profiles, only reorder them.
  ASSERT_EQ(10u, filtered.size());

  // First, the complete profiles should be hoisted to the top, keeping their
  // relative order.
  EXPECT_EQ(&complete1, filtered[0]);
  EXPECT_EQ(&complete2, filtered[1]);

  // Next are profiles with a complete address but missing one other field.
  EXPECT_EQ(&no_name, filtered[2]);
  EXPECT_EQ(&no_phone, filtered[3]);

  // A profile with only a complete address should still appear before profiles
  // with partial/empty addresses.
  EXPECT_EQ(&address_only, filtered[4]);

  // Profiles with partial/no address then are sorted by whether or not they
  // have names and/or phone numbers.
  EXPECT_EQ(&partial_address, filtered[5]);
  EXPECT_EQ(&no_address, filtered[6]);

  EXPECT_EQ(&partial_no_phone, filtered[7]);
  EXPECT_EQ(&partial_no_name, filtered[8]);

  EXPECT_EQ(&empty, filtered[9]);
}

TEST(PaymentRequestProfileUtilTest, GetShippingCompletenessScore) {
  MockPaymentOptionsProvider provider(kRequestShipping);
  PaymentsProfileComparator comp("en-US", provider);

  // 12 points for a complete profile: 10 for address, 1 each for name/phone.
  AutofillProfile p1 = CreateProfileWithCompleteAddress("Homer", "6515553226");
  EXPECT_EQ(12, comp.GetShippingCompletenessScore(&p1));

  // 11 points if name or phone is missing.
  AutofillProfile p2 = CreateProfileWithCompleteAddress("", "6515553226");
  AutofillProfile p3 = CreateProfileWithCompleteAddress("Homer", "");
  EXPECT_EQ(11, comp.GetShippingCompletenessScore(&p2));
  EXPECT_EQ(11, comp.GetShippingCompletenessScore(&p3));

  // 10 points for complete address only.
  AutofillProfile p4 = CreateProfileWithCompleteAddress("", "");
  EXPECT_EQ(10, comp.GetShippingCompletenessScore(&p4));

  // 2 points for name and phone without address.
  AutofillProfile p5 = CreateProfileWithContactInfo("Homer", "", "6515553226");
  EXPECT_EQ(2, comp.GetShippingCompletenessScore(&p5));

  // 1 point for name or phone alone.
  AutofillProfile p6 = CreateProfileWithContactInfo("Homer", "", "");
  AutofillProfile p7 = CreateProfileWithContactInfo("", "", "6515553226");
  EXPECT_EQ(1, comp.GetShippingCompletenessScore(&p6));
  EXPECT_EQ(1, comp.GetShippingCompletenessScore(&p7));

  // No points for empty profile, or profile with only a partial address.
  AutofillProfile p8 = CreateProfileWithContactInfo("", "", "");
  AutofillProfile p9 = CreateProfileWithPartialAddress("", "");
  EXPECT_EQ(0, comp.GetShippingCompletenessScore(&p8));
  EXPECT_EQ(0, comp.GetShippingCompletenessScore(&p9));
}

TEST(PaymentRequestProfileUtilTest, IsShippingComplete) {
  MockPaymentOptionsProvider provider(kRequestShipping);
  PaymentsProfileComparator comp("en-US", provider);

  // True if name, phone, and address are all populated.
  AutofillProfile p1 = CreateProfileWithCompleteAddress("Homer", "6515553226");
  EXPECT_TRUE(comp.IsShippingComplete(&p1));

  // False if address is partially populated.
  AutofillProfile p2 = CreateProfileWithPartialAddress("Homer", "6515553226");
  EXPECT_FALSE(comp.IsShippingComplete(&p2));

  // False if name isn't populated.
  AutofillProfile p3 = CreateProfileWithCompleteAddress("", "6515553226");
  EXPECT_FALSE(comp.IsShippingComplete(&p3));

  // False if phone isn't populated.
  AutofillProfile p4 = CreateProfileWithCompleteAddress("Homer", "");
  EXPECT_FALSE(comp.IsShippingComplete(&p4));

  // False if only contact info (no address fields) is populated.
  AutofillProfile p5 =
      CreateProfileWithContactInfo("Homer", "homer@simpson.net", "6515553226");
  EXPECT_FALSE(comp.IsShippingComplete(&p5));

  MockPaymentOptionsProvider provider_no_shipping(0);
  PaymentsProfileComparator comp_no_shipping("en-US", provider_no_shipping);
  // nullptr is handled correctly: false if shipping requested, true if not.
  EXPECT_FALSE(comp.IsShippingComplete(nullptr));
  EXPECT_TRUE(comp_no_shipping.IsShippingComplete(nullptr));
}

TEST(PaymentRequestProfileUtilTest, GetStringForMissingContactFields) {
  MockPaymentOptionsProvider provider(kRequestPayerName | kRequestPayerPhone |
                                      kRequestPayerEmail | kRequestShipping);
  PaymentsProfileComparator comp("en-US", provider);

  // No error message for complete profile.
  AutofillProfile p1 =
      CreateProfileWithContactInfo("Homer", "homer@simpson.net", "6515553226");
  EXPECT_TRUE(comp.GetStringForMissingContactFields(p1).empty());

  MockPaymentOptionsProvider provider_no_email(
      kRequestPayerName | kRequestPayerPhone | kRequestShipping);
  PaymentsProfileComparator comp_no_email("en-US", provider_no_email);

  // No error message if missing field wasn't required.
  AutofillProfile p2 = CreateProfileWithContactInfo("Homer", "", "6515553226");
  EXPECT_TRUE(comp_no_email.GetStringForMissingContactFields(p2).empty());

  // Error message for email address if email address is missing and required.
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PAYMENTS_EMAIL_REQUIRED),
            comp.GetStringForMissingContactFields(p2));

  // Error message for phone number if phone is missing and required.
  AutofillProfile p3 =
      CreateProfileWithContactInfo("Homer", "homer@simpson.net", "");
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PAYMENTS_PHONE_NUMBER_REQUIRED),
            comp.GetStringForMissingContactFields(p3));

  // Error message for name if name is missing and required.
  AutofillProfile p4 =
      CreateProfileWithContactInfo("", "homer@simpson.net", "6515553226");
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PAYMENTS_NAME_REQUIRED),
            comp.GetStringForMissingContactFields(p4));

  // Generic error message if multiple fields missing.
  AutofillProfile p5 =
      CreateProfileWithContactInfo("", "homer@simpson.net", "");
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PAYMENTS_MORE_INFORMATION_REQUIRED),
            comp.GetStringForMissingContactFields(p5));
}

TEST(PaymentRequestProfileUtilTest, GetTitleForMissingContactFields) {
  MockPaymentOptionsProvider provider(kRequestPayerName | kRequestPayerPhone |
                                      kRequestPayerEmail | kRequestShipping);
  PaymentsProfileComparator comp("en-US", provider);

  // Error message for email address if email address is missing and required.
  AutofillProfile p1 = CreateProfileWithContactInfo("Homer", "", "6515553226");
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PAYMENTS_ADD_EMAIL),
            comp.GetTitleForMissingContactFields(p1));

  // Error message for phone number if phone is missing and required.
  AutofillProfile p2 =
      CreateProfileWithContactInfo("Homer", "homer@simpson.net", "");
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PAYMENTS_ADD_PHONE_NUMBER),
            comp.GetTitleForMissingContactFields(p2));

  // Error message for name if name is missing and required.
  AutofillProfile p3 =
      CreateProfileWithContactInfo("", "homer@simpson.net", "6515553226");
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PAYMENTS_ADD_NAME),
            comp.GetTitleForMissingContactFields(p3));

  // Generic error message if multiple fields missing.
  AutofillProfile p4 =
      CreateProfileWithContactInfo("", "homer@simpson.net", "");
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PAYMENTS_ADD_MORE_INFORMATION),
            comp.GetTitleForMissingContactFields(p4));
}

TEST(PaymentRequestProfileUtilTest, GetStringForMissingShippingFields) {
  MockPaymentOptionsProvider provider(kRequestPayerName | kRequestPayerPhone |
                                      kRequestPayerEmail | kRequestShipping);
  PaymentsProfileComparator comp("en-US", provider);

  // No error message for complete profile.
  AutofillProfile p1 = CreateProfileWithCompleteAddress("Homer", "6515553226");
  EXPECT_TRUE(comp.GetStringForMissingShippingFields(p1).empty());

  // Error message for shipping if shipping requested and not present.
  AutofillProfile p2 =
      CreateProfileWithContactInfo("Homer", "homer@simpson.net", "6515553226");
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PAYMENTS_INVALID_ADDRESS),
            comp.GetStringForMissingShippingFields(p2));

  // Error message for shipping if shipping requested and only partially
  // complete.
  AutofillProfile p3 = CreateProfileWithPartialAddress("Homer", "6515553226");
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PAYMENTS_INVALID_ADDRESS),
            comp.GetStringForMissingShippingFields(p3));

  // Error message for name if name requested and missing.
  AutofillProfile p4 = CreateProfileWithCompleteAddress("", "6515553226");
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PAYMENTS_NAME_REQUIRED),
            comp.GetStringForMissingShippingFields(p4));

  // Error message for phone if phone requested and missing.
  AutofillProfile p5 = CreateProfileWithCompleteAddress("Homer", "");
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PAYMENTS_PHONE_NUMBER_REQUIRED),
            comp.GetStringForMissingShippingFields(p5));

  // Generic error message if multiple fields missing.
  AutofillProfile p6 = CreateProfileWithContactInfo("", "", "");
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_PAYMENTS_MORE_INFORMATION_REQUIRED),
            comp.GetStringForMissingShippingFields(p6));

  MockPaymentOptionsProvider provider_no_shipping(
      kRequestPayerName | kRequestPayerPhone | kRequestPayerEmail);
  PaymentsProfileComparator comp_no_shipping("en-US", provider_no_shipping);

  // No error message if everything is missing but shipping wasn't requested.
  EXPECT_TRUE(comp_no_shipping.GetStringForMissingShippingFields(p6).empty());
}

}  // namespace payments
