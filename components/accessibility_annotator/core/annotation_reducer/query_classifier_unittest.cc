// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/query_classifier.h"

#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "components/accessibility_annotator/core/annotation_reducer/query_intent_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

// Tests for internal::ContainsStandalonePhrase.
TEST(QueryClassifierUtilsTest, ContainsStandalonePhrase) {
  // Empty needle always matches.
  EXPECT_TRUE(internal::ContainsStandalonePhrase(u"hello world", u""));

  // Basic matches.
  EXPECT_TRUE(internal::ContainsStandalonePhrase(u"hello world", u"hello"));
  EXPECT_TRUE(internal::ContainsStandalonePhrase(u"hello world", u"world"));

  // Standalone word in the middle.
  EXPECT_TRUE(internal::ContainsStandalonePhrase(u"a b c", u"b"));

  // Substring non-matches.
  EXPECT_FALSE(internal::ContainsStandalonePhrase(u"hello", u"hell"));
  EXPECT_FALSE(internal::ContainsStandalonePhrase(u"hello", u"ello"));
  EXPECT_FALSE(internal::ContainsStandalonePhrase(u"provincial", u"vin"));

  // Multiple occurrences, only one being standalone.
  EXPECT_TRUE(
      internal::ContainsStandalonePhrase(u"vin provincial vin", u"vin"));
  EXPECT_TRUE(internal::ContainsStandalonePhrase(u"provincial vin", u"vin"));
  EXPECT_TRUE(internal::ContainsStandalonePhrase(u"vin provincial", u"vin"));

  // Case sensitivity (it's a raw check, so it should be sensitive).
  EXPECT_FALSE(internal::ContainsStandalonePhrase(u"Hello world", u"hello"));
  EXPECT_TRUE(internal::ContainsStandalonePhrase(u"Hello world", u"Hello"));
}

class QueryClassifierTest : public ::testing::Test {
 public:
  QueryClassifierTest() = default;
  ~QueryClassifierTest() override = default;

  void SetUp() override { classifier_ = CreateQueryClassifier(); }

 protected:
  QueryClassifier classifier_;
};

// Tests that empty or whitespace queries are classified as unknown.
TEST_F(QueryClassifierTest, EmptyQuery) {
  EXPECT_EQ(classifier_.Run(u"").intent, QueryIntentType::kUnknown);
  EXPECT_EQ(classifier_.Run(u"  ").intent, QueryIntentType::kUnknown);
}

// Tests that queries containing only stop words are classified as unknown.
TEST_F(QueryClassifierTest, QueryWithOnlyStopWords) {
  EXPECT_EQ(classifier_.Run(u"what is my").intent, QueryIntentType::kUnknown);
  EXPECT_EQ(classifier_.Run(u"show me the details please").intent,
            QueryIntentType::kUnknown);
}

// Tests that address-related queries are correctly classified.
TEST_F(QueryClassifierTest, AddressIntents) {
  EXPECT_EQ(classifier_.Run(u"my zip code").intent,
            QueryIntentType::kAddressZip);
  EXPECT_EQ(classifier_.Run(u"What is the postal code?").intent,
            QueryIntentType::kAddressZip);
  EXPECT_EQ(classifier_.Run(u"show me the City").intent,
            QueryIntentType::kAddressCity);
  EXPECT_EQ(classifier_.Run(u"town").intent, QueryIntentType::kAddressCity);
  EXPECT_EQ(classifier_.Run(u"state").intent, QueryIntentType::kAddressState);
  EXPECT_EQ(classifier_.Run(u"province please").intent,
            QueryIntentType::kAddressState);
  EXPECT_EQ(classifier_.Run(u"country").intent,
            QueryIntentType::kAddressCountry);
  EXPECT_EQ(classifier_.Run(u"street name").intent,
            QueryIntentType::kAddressStreetAddress);
  EXPECT_EQ(classifier_.Run(u"What is my address?").intent,
            QueryIntentType::kAddressFull);
  EXPECT_EQ(classifier_.Run(u"home address").intent,
            QueryIntentType::kAddressFull);
  EXPECT_EQ(classifier_.Run(u"company name").intent,
            QueryIntentType::kCompanyName);
  EXPECT_EQ(classifier_.Run(u"organization").intent,
            QueryIntentType::kCompanyName);
}

// Tests that contact-related queries are correctly classified.
TEST_F(QueryClassifierTest, ContactIntents) {
  EXPECT_EQ(classifier_.Run(u"my phone number").intent,
            QueryIntentType::kPhone);
  EXPECT_EQ(classifier_.Run(u"mobile").intent, QueryIntentType::kPhone);
  EXPECT_EQ(classifier_.Run(u"what is my email").intent,
            QueryIntentType::kEmail);
  EXPECT_EQ(classifier_.Run(u"e-mail address").intent, QueryIntentType::kEmail);
  EXPECT_EQ(classifier_.Run(u"name").intent, QueryIntentType::kNameFull);
  EXPECT_EQ(classifier_.Run(u"what is my name").intent,
            QueryIntentType::kNameFull);
}

// Tests that payment-related queries are correctly classified.
TEST_F(QueryClassifierTest, PaymentIntents) {
  EXPECT_EQ(classifier_.Run(u"IBAN").intent, QueryIntentType::kIban);
  EXPECT_EQ(classifier_.Run(u"my bank account number").intent,
            QueryIntentType::kIban);
}

// Tests that entity-related queries are correctly classified.
TEST_F(QueryClassifierTest, EntityIntents) {
  EXPECT_EQ(classifier_.Run(u"license plate").intent,
            QueryIntentType::kVehiclePlateNumber);
  EXPECT_EQ(classifier_.Run(u"VIN number").intent,
            QueryIntentType::kVehicleVin);
  EXPECT_EQ(classifier_.Run(u"car details").intent, QueryIntentType::kVehicle);
  EXPECT_EQ(classifier_.Run(u"my vehicle").intent, QueryIntentType::kVehicle);
  EXPECT_EQ(classifier_.Run(u"passport info").intent,
            QueryIntentType::kPassportFull);
  EXPECT_EQ(classifier_.Run(u"my reservation").intent,
            QueryIntentType::kFlightReservationFull);
  EXPECT_EQ(classifier_.Run(u"national id").intent,
            QueryIntentType::kNationalIdCardFull);
  EXPECT_EQ(classifier_.Run(u"redress number").intent,
            QueryIntentType::kRedressNumberNumber);
  EXPECT_EQ(classifier_.Run(u"known traveler number").intent,
            QueryIntentType::kKnownTravelerNumberFull);
  EXPECT_EQ(classifier_.Run(u"my KTN").intent,
            QueryIntentType::kKnownTravelerNumberFull);
  EXPECT_EQ(classifier_.Run(u"driver's license").intent,
            QueryIntentType::kDriversLicenseFull);
  EXPECT_EQ(classifier_.Run(u"driving license").intent,
            QueryIntentType::kDriversLicenseFull);
}

// Tests that entity attribute queries are correctly classified.
TEST_F(QueryClassifierTest, EntityAttributeIntents) {
  // Vehicle attributes
  EXPECT_EQ(classifier_.Run(u"car make").intent, QueryIntentType::kVehicleMake);
  EXPECT_EQ(classifier_.Run(u"vehicle model").intent,
            QueryIntentType::kVehicleModel);
  EXPECT_EQ(classifier_.Run(u"car year").intent, QueryIntentType::kVehicleYear);
  EXPECT_EQ(classifier_.Run(u"vehicle owner").intent,
            QueryIntentType::kVehicleOwner);
  EXPECT_EQ(classifier_.Run(u"plate state").intent,
            QueryIntentType::kVehiclePlateState);

  // Passport attributes
  EXPECT_EQ(classifier_.Run(u"passport number").intent,
            QueryIntentType::kPassportNumber);
  EXPECT_EQ(classifier_.Run(u"passport expiration").intent,
            QueryIntentType::kPassportExpirationDate);
  EXPECT_EQ(classifier_.Run(u"passport issue").intent,
            QueryIntentType::kPassportIssueDate);
  EXPECT_EQ(classifier_.Run(u"passport country").intent,
            QueryIntentType::kPassportCountry);
  EXPECT_EQ(classifier_.Run(u"passport name").intent,
            QueryIntentType::kPassportName);

  // Flight Reservation attributes
  EXPECT_EQ(classifier_.Run(u"flight number").intent,
            QueryIntentType::kFlightReservationFlightNumber);
  EXPECT_EQ(classifier_.Run(u"ticket number").intent,
            QueryIntentType::kFlightReservationTicketNumber);
  EXPECT_EQ(classifier_.Run(u"confirmation code").intent,
            QueryIntentType::kFlightReservationConfirmationCode);
  EXPECT_EQ(classifier_.Run(u"passenger name").intent,
            QueryIntentType::kFlightReservationPassengerName);
  EXPECT_EQ(classifier_.Run(u"departure airport").intent,
            QueryIntentType::kFlightReservationDepartureAirport);
  EXPECT_EQ(classifier_.Run(u"arrival airport").intent,
            QueryIntentType::kFlightReservationArrivalAirport);
  EXPECT_EQ(classifier_.Run(u"departure date").intent,
            QueryIntentType::kFlightReservationDepartureDate);

  // National ID attributes
  EXPECT_EQ(classifier_.Run(u"national id number").intent,
            QueryIntentType::kNationalIdCardNumber);
  EXPECT_EQ(classifier_.Run(u"national id name").intent,
            QueryIntentType::kNationalIdCardName);

  // Redress/KTN attributes
  EXPECT_EQ(classifier_.Run(u"redress name").intent,
            QueryIntentType::kRedressNumberName);
  EXPECT_EQ(classifier_.Run(u"ktn number").intent,
            QueryIntentType::kKnownTravelerNumberNumber);

  // Drivers License attributes
  EXPECT_EQ(classifier_.Run(u"driver's license number").intent,
            QueryIntentType::kDriversLicenseNumber);
  EXPECT_EQ(classifier_.Run(u"drivers license state").intent,
            QueryIntentType::kDriversLicenseState);
}

// Tests that order-related queries are correctly classified.
TEST_F(QueryClassifierTest, OrderIntents) {
  EXPECT_EQ(classifier_.Run(u"order id").intent, QueryIntentType::kOrderId);
  EXPECT_EQ(classifier_.Run(u"order number").intent, QueryIntentType::kOrderId);
  EXPECT_EQ(classifier_.Run(u"order date").intent, QueryIntentType::kOrderDate);
  EXPECT_EQ(classifier_.Run(u"merchant name").intent,
            QueryIntentType::kOrderMerchantName);
  EXPECT_EQ(classifier_.Run(u"store name").intent,
            QueryIntentType::kOrderMerchantName);
  EXPECT_EQ(classifier_.Run(u"order grand total").intent,
            QueryIntentType::kOrderGrandTotal);
  EXPECT_EQ(classifier_.Run(u"what is my order").intent,
            QueryIntentType::kOrderFull);
}

// Tests that queries mixed with stop words are correctly classified.
TEST_F(QueryClassifierTest, MixedWithStopWords) {
  EXPECT_EQ(classifier_.Run(u"show me my zip code please").intent,
            QueryIntentType::kAddressZip);
  EXPECT_EQ(classifier_.Run(u"what is the car's VIN").intent,
            QueryIntentType::kVehicleVin);
  EXPECT_EQ(classifier_.Run(u"get my flight details").intent,
            QueryIntentType::kFlightReservationFull);
}

// Tests that query classification is case-insensitive.
TEST_F(QueryClassifierTest, CaseInsensitivity) {
  EXPECT_EQ(classifier_.Run(u"MY ZIP CODE").intent,
            QueryIntentType::kAddressZip);
  EXPECT_EQ(classifier_.Run(u"My Zip Code").intent,
            QueryIntentType::kAddressZip);
}

// Tests that queries with punctuation are correctly classified.
TEST_F(QueryClassifierTest, PunctuationHandling) {
  EXPECT_EQ(classifier_.Run(u"zip, code!").intent,
            QueryIntentType::kAddressZip);
  EXPECT_EQ(classifier_.Run(u"city?").intent, QueryIntentType::kAddressCity);
  EXPECT_EQ(classifier_.Run(u"my email, please").intent,
            QueryIntentType::kEmail);
}

// Tests that queries with no matching keywords are classified as unknown.
TEST_F(QueryClassifierTest, NoKeywordMatch) {
  EXPECT_EQ(classifier_.Run(u"how is the weather").intent,
            QueryIntentType::kUnknown);
  EXPECT_EQ(classifier_.Run(u"set a timer").intent, QueryIntentType::kUnknown);
}

// Tests that substring matches don't incorrectly trigger classification.
TEST_F(QueryClassifierTest, SubstringNonMatch) {
  EXPECT_EQ(classifier_.Run(u"bank account").intent, QueryIntentType::kIban);
  EXPECT_EQ(classifier_.Run(u"cartoon").intent, QueryIntentType::kUnknown);
}

// Tests that multi-word address queries are correctly classified.
TEST_F(QueryClassifierTest, MultiWordAddressIntents) {
  EXPECT_EQ(classifier_.Run(u"my postal code").intent,
            QueryIntentType::kAddressZip);
  EXPECT_EQ(classifier_.Run(u"what is the home address").intent,
            QueryIntentType::kAddressFull);
  EXPECT_EQ(classifier_.Run(u"work address please").intent,
            QueryIntentType::kAddressFull);
}

// Tests that multi-word entity queries are correctly classified.
TEST_F(QueryClassifierTest, MultiWordEntityIntents) {
  EXPECT_EQ(classifier_.Run(u"show my license plate").intent,
            QueryIntentType::kVehiclePlateNumber);
  EXPECT_EQ(classifier_.Run(u"plate number").intent,
            QueryIntentType::kVehiclePlateNumber);
  EXPECT_EQ(classifier_.Run(u"flight reservation code").intent,
            QueryIntentType::kFlightReservationFull);
  EXPECT_EQ(classifier_.Run(u"what is my national id").intent,
            QueryIntentType::kNationalIdCardFull);
  EXPECT_EQ(classifier_.Run(u"known traveler number").intent,
            QueryIntentType::kKnownTravelerNumberFull);
  EXPECT_EQ(classifier_.Run(u"drivers license").intent,
            QueryIntentType::kDriversLicenseFull);
}

// Tests that filter words are correctly extracted.
TEST_F(QueryClassifierTest, RequiredWords) {
  {
    ClassifiedQuery classified_query =
        classifier_.Run(u"What's my home address in San Diego");
    EXPECT_EQ(classified_query.intent, QueryIntentType::kAddressFull);
    EXPECT_THAT(classified_query.filter_words,
                testing::ElementsAre(u"san", u"diego"));
  }
  {
    ClassifiedQuery classified_query =
        classifier_.Run(u"show me my VIN for my Tesla");
    EXPECT_EQ(classified_query.intent, QueryIntentType::kVehicleVin);
    EXPECT_THAT(classified_query.filter_words,
                testing::ElementsAre(u"tesla"));
  }
  {
    ClassifiedQuery classified_query =
        classifier_.Run(u"get flight number for LH123");
    EXPECT_EQ(classified_query.intent,
              QueryIntentType::kFlightReservationFlightNumber);
    EXPECT_THAT(classified_query.filter_words,
                testing::ElementsAre(u"lh123"));
  }
}

// Tests that CreateKeywordQueryClassifier performs simple matches.
TEST_F(QueryClassifierTest, SimpleMatch) {
  QueryClassifier classifier = internal::CreateKeywordQueryClassifier();
  EXPECT_EQ(classifier.Run(u"zip code").intent, QueryIntentType::kAddressZip);
}

// Tests that CreateGeminiClassifier currently returns unknown.
TEST_F(QueryClassifierTest, NoOp) {
  QueryClassifier classifier = internal::CreateGeminiClassifier();
  EXPECT_EQ(classifier.Run(u"something complicated").intent,
            QueryIntentType::kUnknown);
}

}  // namespace accessibility_annotator
