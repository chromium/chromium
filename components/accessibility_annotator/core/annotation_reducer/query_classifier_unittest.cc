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

  void SetUp() override { classifier_ = std::make_unique<QueryClassifier>(); }

 protected:
  std::unique_ptr<QueryClassifier> classifier_;
};

TEST_F(QueryClassifierTest, EmptyQuery) {
  EXPECT_EQ(classifier_->Classify(u""), QueryIntentType::kUnknown);
  EXPECT_EQ(classifier_->Classify(u"  "), QueryIntentType::kUnknown);
}

TEST_F(QueryClassifierTest, QueryWithOnlyStopWords) {
  EXPECT_EQ(classifier_->Classify(u"what is my"), QueryIntentType::kUnknown);
  EXPECT_EQ(classifier_->Classify(u"show me the details please"),
            QueryIntentType::kUnknown);
}

TEST_F(QueryClassifierTest, AddressIntents) {
  EXPECT_EQ(classifier_->Classify(u"my zip code"),
            QueryIntentType::kAddressZip);
  EXPECT_EQ(classifier_->Classify(u"What is the postal code?"),
            QueryIntentType::kAddressZip);
  EXPECT_EQ(classifier_->Classify(u"show me the City"),
            QueryIntentType::kAddressCity);
  EXPECT_EQ(classifier_->Classify(u"town"), QueryIntentType::kAddressCity);
  EXPECT_EQ(classifier_->Classify(u"state"), QueryIntentType::kAddressState);
  EXPECT_EQ(classifier_->Classify(u"province please"),
            QueryIntentType::kAddressState);
  EXPECT_EQ(classifier_->Classify(u"country"),
            QueryIntentType::kAddressCountry);
  EXPECT_EQ(classifier_->Classify(u"street name"),
            QueryIntentType::kAddressStreetAddress);
  EXPECT_EQ(classifier_->Classify(u"What is my address?"),
            QueryIntentType::kAddressFull);
  EXPECT_EQ(classifier_->Classify(u"home address"),
            QueryIntentType::kAddressFull);
  EXPECT_EQ(classifier_->Classify(u"company name"),
            QueryIntentType::kCompanyName);
  EXPECT_EQ(classifier_->Classify(u"organization"),
            QueryIntentType::kCompanyName);
}

TEST_F(QueryClassifierTest, ContactIntents) {
  EXPECT_EQ(classifier_->Classify(u"my phone number"), QueryIntentType::kPhone);
  EXPECT_EQ(classifier_->Classify(u"mobile"), QueryIntentType::kPhone);
  EXPECT_EQ(classifier_->Classify(u"what is my email"),
            QueryIntentType::kEmail);
  EXPECT_EQ(classifier_->Classify(u"e-mail address"), QueryIntentType::kEmail);
  EXPECT_EQ(classifier_->Classify(u"name"), QueryIntentType::kNameFull);
  EXPECT_EQ(classifier_->Classify(u"what is my name"),
            QueryIntentType::kNameFull);
}

TEST_F(QueryClassifierTest, PaymentIntents) {
  EXPECT_EQ(classifier_->Classify(u"IBAN"), QueryIntentType::kIban);
  EXPECT_EQ(classifier_->Classify(u"my bank account number"),
            QueryIntentType::kIban);
}

TEST_F(QueryClassifierTest, EntityIntents) {
  EXPECT_EQ(classifier_->Classify(u"license plate"),
            QueryIntentType::kVehiclePlateNumber);
  EXPECT_EQ(classifier_->Classify(u"VIN number"), QueryIntentType::kVehicleVin);
  EXPECT_EQ(classifier_->Classify(u"car details"), QueryIntentType::kVehicle);
  EXPECT_EQ(classifier_->Classify(u"my vehicle"), QueryIntentType::kVehicle);
  EXPECT_EQ(classifier_->Classify(u"passport info"),
            QueryIntentType::kPassportFull);
  EXPECT_EQ(classifier_->Classify(u"my reservation"),
            QueryIntentType::kFlightReservationFull);
  EXPECT_EQ(classifier_->Classify(u"national id"),
            QueryIntentType::kNationalIdCardFull);
  EXPECT_EQ(classifier_->Classify(u"redress number"),
            QueryIntentType::kRedressNumberNumber);
  EXPECT_EQ(classifier_->Classify(u"known traveler number"),
            QueryIntentType::kKnownTravelerNumberFull);
  EXPECT_EQ(classifier_->Classify(u"my KTN"),
            QueryIntentType::kKnownTravelerNumberFull);
  EXPECT_EQ(classifier_->Classify(u"driver's license"),
            QueryIntentType::kDriversLicenseFull);
  EXPECT_EQ(classifier_->Classify(u"driving license"),
            QueryIntentType::kDriversLicenseFull);
}

TEST_F(QueryClassifierTest, EntityAttributeIntents) {
  // Vehicle attributes
  EXPECT_EQ(classifier_->Classify(u"car make"), QueryIntentType::kVehicleMake);
  EXPECT_EQ(classifier_->Classify(u"vehicle model"),
            QueryIntentType::kVehicleModel);
  EXPECT_EQ(classifier_->Classify(u"car year"), QueryIntentType::kVehicleYear);
  EXPECT_EQ(classifier_->Classify(u"vehicle owner"),
            QueryIntentType::kVehicleOwner);
  EXPECT_EQ(classifier_->Classify(u"plate state"),
            QueryIntentType::kVehiclePlateState);

  // Passport attributes
  EXPECT_EQ(classifier_->Classify(u"passport number"),
            QueryIntentType::kPassportNumber);
  EXPECT_EQ(classifier_->Classify(u"passport expiration"),
            QueryIntentType::kPassportExpirationDate);
  EXPECT_EQ(classifier_->Classify(u"passport issue"),
            QueryIntentType::kPassportIssueDate);
  EXPECT_EQ(classifier_->Classify(u"passport country"),
            QueryIntentType::kPassportCountry);
  EXPECT_EQ(classifier_->Classify(u"passport name"),
            QueryIntentType::kPassportName);

  // Flight Reservation attributes
  EXPECT_EQ(classifier_->Classify(u"flight number"),
            QueryIntentType::kFlightReservationFlightNumber);
  EXPECT_EQ(classifier_->Classify(u"ticket number"),
            QueryIntentType::kFlightReservationTicketNumber);
  EXPECT_EQ(classifier_->Classify(u"confirmation code"),
            QueryIntentType::kFlightReservationConfirmationCode);
  EXPECT_EQ(classifier_->Classify(u"passenger name"),
            QueryIntentType::kFlightReservationPassengerName);
  EXPECT_EQ(classifier_->Classify(u"departure airport"),
            QueryIntentType::kFlightReservationDepartureAirport);
  EXPECT_EQ(classifier_->Classify(u"arrival airport"),
            QueryIntentType::kFlightReservationArrivalAirport);
  EXPECT_EQ(classifier_->Classify(u"departure date"),
            QueryIntentType::kFlightReservationDepartureDate);

  // National ID attributes
  EXPECT_EQ(classifier_->Classify(u"national id number"),
            QueryIntentType::kNationalIdCardNumber);
  EXPECT_EQ(classifier_->Classify(u"national id name"),
            QueryIntentType::kNationalIdCardName);

  // Redress/KTN attributes
  EXPECT_EQ(classifier_->Classify(u"redress name"),
            QueryIntentType::kRedressNumberName);
  EXPECT_EQ(classifier_->Classify(u"ktn number"),
            QueryIntentType::kKnownTravelerNumberNumber);

  // Drivers License attributes
  EXPECT_EQ(classifier_->Classify(u"driver's license number"),
            QueryIntentType::kDriversLicenseNumber);
  EXPECT_EQ(classifier_->Classify(u"drivers license state"),
            QueryIntentType::kDriversLicenseState);
}

TEST_F(QueryClassifierTest, OrderIntents) {
  EXPECT_EQ(classifier_->Classify(u"order id"), QueryIntentType::kOrderId);
  EXPECT_EQ(classifier_->Classify(u"order number"), QueryIntentType::kOrderId);
  EXPECT_EQ(classifier_->Classify(u"order date"), QueryIntentType::kOrderDate);
  EXPECT_EQ(classifier_->Classify(u"merchant name"),
            QueryIntentType::kOrderMerchantName);
  EXPECT_EQ(classifier_->Classify(u"store name"),
            QueryIntentType::kOrderMerchantName);
  EXPECT_EQ(classifier_->Classify(u"order grand total"),
            QueryIntentType::kOrderGrandTotal);
  EXPECT_EQ(classifier_->Classify(u"what is my order"),
            QueryIntentType::kOrderFull);
}

TEST_F(QueryClassifierTest, MixedWithStopWords) {
  EXPECT_EQ(classifier_->Classify(u"show me my zip code please"),
            QueryIntentType::kAddressZip);
  EXPECT_EQ(classifier_->Classify(u"what is the car's VIN"),
            QueryIntentType::kVehicleVin);
  EXPECT_EQ(classifier_->Classify(u"get my flight details"),
            QueryIntentType::kFlightReservationFull);
}

TEST_F(QueryClassifierTest, CaseInsensitivity) {
  EXPECT_EQ(classifier_->Classify(u"MY ZIP CODE"),
            QueryIntentType::kAddressZip);
  EXPECT_EQ(classifier_->Classify(u"My Zip Code"),
            QueryIntentType::kAddressZip);
}

TEST_F(QueryClassifierTest, PunctuationHandling) {
  EXPECT_EQ(classifier_->Classify(u"zip, code!"), QueryIntentType::kAddressZip);
  EXPECT_EQ(classifier_->Classify(u"city?"), QueryIntentType::kAddressCity);
  EXPECT_EQ(classifier_->Classify(u"my email, please"),
            QueryIntentType::kEmail);
}

TEST_F(QueryClassifierTest, NoKeywordMatch) {
  EXPECT_EQ(classifier_->Classify(u"how is the weather"),
            QueryIntentType::kUnknown);
  EXPECT_EQ(classifier_->Classify(u"set a timer"), QueryIntentType::kUnknown);
}

TEST_F(QueryClassifierTest, SubstringNonMatch) {
  EXPECT_EQ(classifier_->Classify(u"bank account"), QueryIntentType::kIban);
  EXPECT_EQ(classifier_->Classify(u"cartoon"), QueryIntentType::kUnknown);
}

TEST_F(QueryClassifierTest, MultiWordAddressIntents) {
  EXPECT_EQ(classifier_->Classify(u"my postal code"),
            QueryIntentType::kAddressZip);
  EXPECT_EQ(classifier_->Classify(u"what is the home address"),
            QueryIntentType::kAddressFull);
  EXPECT_EQ(classifier_->Classify(u"work address please"),
            QueryIntentType::kAddressFull);
}

TEST_F(QueryClassifierTest, MultiWordEntityIntents) {
  EXPECT_EQ(classifier_->Classify(u"show my license plate"),
            QueryIntentType::kVehiclePlateNumber);
  EXPECT_EQ(classifier_->Classify(u"plate number"),
            QueryIntentType::kVehiclePlateNumber);
  EXPECT_EQ(classifier_->Classify(u"flight reservation code"),
            QueryIntentType::kFlightReservationFull);
  EXPECT_EQ(classifier_->Classify(u"what is my national id"),
            QueryIntentType::kNationalIdCardFull);
  EXPECT_EQ(classifier_->Classify(u"known traveler number"),
            QueryIntentType::kKnownTravelerNumberFull);
  EXPECT_EQ(classifier_->Classify(u"drivers license"),
            QueryIntentType::kDriversLicenseFull);
}

}  // namespace accessibility_annotator
