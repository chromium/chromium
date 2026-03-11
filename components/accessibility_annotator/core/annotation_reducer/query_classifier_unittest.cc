// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/query_classifier.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "components/accessibility_annotator/core/annotation_reducer/query_intent_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

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
  EXPECT_EQ(classifier_.Run(u""), QueryIntentType::kUnknown);
  EXPECT_EQ(classifier_.Run(u"  "), QueryIntentType::kUnknown);
}

// Tests that queries containing only stop words are classified as unknown.
TEST_F(QueryClassifierTest, QueryWithOnlyStopWords) {
  EXPECT_EQ(classifier_.Run(u"what is my"), QueryIntentType::kUnknown);
  EXPECT_EQ(classifier_.Run(u"show me the details please"),
            QueryIntentType::kUnknown);
}

// Tests that address-related queries are correctly classified.
TEST_F(QueryClassifierTest, AddressIntents) {
  EXPECT_EQ(classifier_.Run(u"my zip code"), QueryIntentType::kAddressZip);
  EXPECT_EQ(classifier_.Run(u"What is the postal code?"),
            QueryIntentType::kAddressZip);
  EXPECT_EQ(classifier_.Run(u"show me the City"),
            QueryIntentType::kAddressCity);
  EXPECT_EQ(classifier_.Run(u"town"), QueryIntentType::kAddressCity);
  EXPECT_EQ(classifier_.Run(u"state"), QueryIntentType::kAddressState);
  EXPECT_EQ(classifier_.Run(u"province please"),
            QueryIntentType::kAddressState);
  EXPECT_EQ(classifier_.Run(u"country"), QueryIntentType::kAddressCountry);
  EXPECT_EQ(classifier_.Run(u"street name"),
            QueryIntentType::kAddressStreetAddress);
  EXPECT_EQ(classifier_.Run(u"What is my address?"),
            QueryIntentType::kAddressFull);
  EXPECT_EQ(classifier_.Run(u"home address"), QueryIntentType::kAddressFull);
  EXPECT_EQ(classifier_.Run(u"company name"), QueryIntentType::kCompanyName);
  EXPECT_EQ(classifier_.Run(u"organization"), QueryIntentType::kCompanyName);
}

// Tests that contact-related queries are correctly classified.
TEST_F(QueryClassifierTest, ContactIntents) {
  EXPECT_EQ(classifier_.Run(u"my phone number"), QueryIntentType::kPhone);
  EXPECT_EQ(classifier_.Run(u"mobile"), QueryIntentType::kPhone);
  EXPECT_EQ(classifier_.Run(u"what is my email"), QueryIntentType::kEmail);
  EXPECT_EQ(classifier_.Run(u"e-mail address"), QueryIntentType::kEmail);
  EXPECT_EQ(classifier_.Run(u"name"), QueryIntentType::kNameFull);
  EXPECT_EQ(classifier_.Run(u"what is my name"), QueryIntentType::kNameFull);
}

// Tests that payment-related queries are correctly classified.
TEST_F(QueryClassifierTest, PaymentIntents) {
  EXPECT_EQ(classifier_.Run(u"IBAN"), QueryIntentType::kIban);
  EXPECT_EQ(classifier_.Run(u"my bank account number"), QueryIntentType::kIban);
}

// Tests that entity-related queries are correctly classified.
TEST_F(QueryClassifierTest, EntityIntents) {
  EXPECT_EQ(classifier_.Run(u"license plate"),
            QueryIntentType::kVehiclePlateNumber);
  EXPECT_EQ(classifier_.Run(u"VIN number"), QueryIntentType::kVehicleVin);
  EXPECT_EQ(classifier_.Run(u"car details"), QueryIntentType::kVehicle);
  EXPECT_EQ(classifier_.Run(u"my vehicle"), QueryIntentType::kVehicle);
  EXPECT_EQ(classifier_.Run(u"passport info"), QueryIntentType::kPassportFull);
  EXPECT_EQ(classifier_.Run(u"my reservation"),
            QueryIntentType::kFlightReservationFull);
  EXPECT_EQ(classifier_.Run(u"national id"),
            QueryIntentType::kNationalIdCardFull);
  EXPECT_EQ(classifier_.Run(u"redress number"),
            QueryIntentType::kRedressNumberNumber);
  EXPECT_EQ(classifier_.Run(u"known traveler number"),
            QueryIntentType::kKnownTravelerNumberFull);
  EXPECT_EQ(classifier_.Run(u"my KTN"),
            QueryIntentType::kKnownTravelerNumberFull);
  EXPECT_EQ(classifier_.Run(u"driver's license"),
            QueryIntentType::kDriversLicenseFull);
  EXPECT_EQ(classifier_.Run(u"driving license"),
            QueryIntentType::kDriversLicenseFull);
}

// Tests that entity attribute queries are correctly classified.
TEST_F(QueryClassifierTest, EntityAttributeIntents) {
  // Vehicle attributes
  EXPECT_EQ(classifier_.Run(u"car make"), QueryIntentType::kVehicleMake);
  EXPECT_EQ(classifier_.Run(u"vehicle model"), QueryIntentType::kVehicleModel);
  EXPECT_EQ(classifier_.Run(u"car year"), QueryIntentType::kVehicleYear);
  EXPECT_EQ(classifier_.Run(u"vehicle owner"), QueryIntentType::kVehicleOwner);
  EXPECT_EQ(classifier_.Run(u"plate state"),
            QueryIntentType::kVehiclePlateState);

  // Passport attributes
  EXPECT_EQ(classifier_.Run(u"passport number"),
            QueryIntentType::kPassportNumber);
  EXPECT_EQ(classifier_.Run(u"passport expiration"),
            QueryIntentType::kPassportExpirationDate);
  EXPECT_EQ(classifier_.Run(u"passport issue"),
            QueryIntentType::kPassportIssueDate);
  EXPECT_EQ(classifier_.Run(u"passport country"),
            QueryIntentType::kPassportCountry);
  EXPECT_EQ(classifier_.Run(u"passport name"), QueryIntentType::kPassportName);

  // Flight Reservation attributes
  EXPECT_EQ(classifier_.Run(u"flight number"),
            QueryIntentType::kFlightReservationFlightNumber);
  EXPECT_EQ(classifier_.Run(u"ticket number"),
            QueryIntentType::kFlightReservationTicketNumber);
  EXPECT_EQ(classifier_.Run(u"confirmation code"),
            QueryIntentType::kFlightReservationConfirmationCode);
  EXPECT_EQ(classifier_.Run(u"passenger name"),
            QueryIntentType::kFlightReservationPassengerName);
  EXPECT_EQ(classifier_.Run(u"departure airport"),
            QueryIntentType::kFlightReservationDepartureAirport);
  EXPECT_EQ(classifier_.Run(u"arrival airport"),
            QueryIntentType::kFlightReservationArrivalAirport);
  EXPECT_EQ(classifier_.Run(u"departure date"),
            QueryIntentType::kFlightReservationDepartureDate);

  // National ID attributes
  EXPECT_EQ(classifier_.Run(u"national id number"),
            QueryIntentType::kNationalIdCardNumber);
  EXPECT_EQ(classifier_.Run(u"national id name"),
            QueryIntentType::kNationalIdCardName);

  // Redress/KTN attributes
  EXPECT_EQ(classifier_.Run(u"redress name"),
            QueryIntentType::kRedressNumberName);
  EXPECT_EQ(classifier_.Run(u"ktn number"),
            QueryIntentType::kKnownTravelerNumberNumber);

  // Drivers License attributes
  EXPECT_EQ(classifier_.Run(u"driver's license number"),
            QueryIntentType::kDriversLicenseNumber);
  EXPECT_EQ(classifier_.Run(u"drivers license state"),
            QueryIntentType::kDriversLicenseState);
}

// Tests that order-related queries are correctly classified.
TEST_F(QueryClassifierTest, OrderIntents) {
  EXPECT_EQ(classifier_.Run(u"order id"), QueryIntentType::kOrderId);
  EXPECT_EQ(classifier_.Run(u"order number"), QueryIntentType::kOrderId);
  EXPECT_EQ(classifier_.Run(u"order date"), QueryIntentType::kOrderDate);
  EXPECT_EQ(classifier_.Run(u"merchant name"),
            QueryIntentType::kOrderMerchantName);
  EXPECT_EQ(classifier_.Run(u"store name"),
            QueryIntentType::kOrderMerchantName);
  EXPECT_EQ(classifier_.Run(u"order grand total"),
            QueryIntentType::kOrderGrandTotal);
  EXPECT_EQ(classifier_.Run(u"what is my order"), QueryIntentType::kOrderFull);
}

// Tests that queries mixed with stop words are correctly classified.
TEST_F(QueryClassifierTest, MixedWithStopWords) {
  EXPECT_EQ(classifier_.Run(u"show me my zip code please"),
            QueryIntentType::kAddressZip);
  EXPECT_EQ(classifier_.Run(u"what is the car's VIN"),
            QueryIntentType::kVehicleVin);
  EXPECT_EQ(classifier_.Run(u"get my flight details"),
            QueryIntentType::kFlightReservationFull);
}

// Tests that query classification is case-insensitive.
TEST_F(QueryClassifierTest, CaseInsensitivity) {
  EXPECT_EQ(classifier_.Run(u"MY ZIP CODE"), QueryIntentType::kAddressZip);
  EXPECT_EQ(classifier_.Run(u"My Zip Code"), QueryIntentType::kAddressZip);
}

// Tests that queries with punctuation are correctly classified.
TEST_F(QueryClassifierTest, PunctuationHandling) {
  EXPECT_EQ(classifier_.Run(u"zip, code!"), QueryIntentType::kAddressZip);
  EXPECT_EQ(classifier_.Run(u"city?"), QueryIntentType::kAddressCity);
  EXPECT_EQ(classifier_.Run(u"my email, please"), QueryIntentType::kEmail);
}

// Tests that queries with no matching keywords are classified as unknown.
TEST_F(QueryClassifierTest, NoKeywordMatch) {
  EXPECT_EQ(classifier_.Run(u"how is the weather"), QueryIntentType::kUnknown);
  EXPECT_EQ(classifier_.Run(u"set a timer"), QueryIntentType::kUnknown);
}

// Tests that substring matches don't incorrectly trigger classification.
TEST_F(QueryClassifierTest, SubstringNonMatch) {
  EXPECT_EQ(classifier_.Run(u"bank account"), QueryIntentType::kIban);
  EXPECT_EQ(classifier_.Run(u"cartoon"), QueryIntentType::kUnknown);
}

// Tests that multi-word address queries are correctly classified.
TEST_F(QueryClassifierTest, MultiWordAddressIntents) {
  EXPECT_EQ(classifier_.Run(u"my postal code"), QueryIntentType::kAddressZip);
  EXPECT_EQ(classifier_.Run(u"what is the home address"),
            QueryIntentType::kAddressFull);
  EXPECT_EQ(classifier_.Run(u"work address please"),
            QueryIntentType::kAddressFull);
}

// Tests that multi-word entity queries are correctly classified.
TEST_F(QueryClassifierTest, MultiWordEntityIntents) {
  EXPECT_EQ(classifier_.Run(u"show my license plate"),
            QueryIntentType::kVehiclePlateNumber);
  EXPECT_EQ(classifier_.Run(u"plate number"),
            QueryIntentType::kVehiclePlateNumber);
  EXPECT_EQ(classifier_.Run(u"flight reservation code"),
            QueryIntentType::kFlightReservationFull);
  EXPECT_EQ(classifier_.Run(u"what is my national id"),
            QueryIntentType::kNationalIdCardFull);
  EXPECT_EQ(classifier_.Run(u"known traveler number"),
            QueryIntentType::kKnownTravelerNumberFull);
  EXPECT_EQ(classifier_.Run(u"drivers license"),
            QueryIntentType::kDriversLicenseFull);
}

// Tests that CreateRegExpQueryClassifier performs simple matches.
TEST_F(QueryClassifierTest, SimpleMatch) {
  QueryClassifier classifier = internal::CreateRegExpQueryClassifier();
  EXPECT_EQ(classifier.Run(u"zip code"), QueryIntentType::kAddressZip);
}

// Tests that CreateGeminiClassifier currently returns unknown.
TEST_F(QueryClassifierTest, NoOp) {
  QueryClassifier classifier = internal::CreateGeminiClassifier();
  EXPECT_EQ(classifier.Run(u"something complicated"),
            QueryIntentType::kUnknown);
}

}  // namespace accessibility_annotator
