// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/annotation_reducer/query_classifier.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace annotation_reducer {

class QueryClassifierTest : public ::testing::Test {
 public:
  QueryClassifierTest() = default;
  ~QueryClassifierTest() override = default;

  void SetUp() override {
    classifier_ = std::make_unique<QueryClassifier>();
  }

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
  EXPECT_EQ(classifier_->Classify(u"flight number"),
            QueryIntentType::kFlightReservationFull);
  EXPECT_EQ(classifier_->Classify(u"my reservation"),
            QueryIntentType::kFlightReservationFull);
  EXPECT_EQ(classifier_->Classify(u"national id"),
            QueryIntentType::kNationalIdCardFull);
  EXPECT_EQ(classifier_->Classify(u"redress number"),
            QueryIntentType::kRedressNumberFull);
  EXPECT_EQ(classifier_->Classify(u"known traveler number"),
            QueryIntentType::kKnownTravelerNumberFull);
  EXPECT_EQ(classifier_->Classify(u"my KTN"),
            QueryIntentType::kKnownTravelerNumberFull);
  EXPECT_EQ(classifier_->Classify(u"driver's license"),
            QueryIntentType::kDriversLicenseFull);
  EXPECT_EQ(classifier_->Classify(u"driving license"),
            QueryIntentType::kDriversLicenseFull);
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

}  // namespace annotation_reducer
