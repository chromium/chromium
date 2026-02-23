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
  EXPECT_EQ(classifier_->Classify(u""), AutofillDataType::kUnknown);
  EXPECT_EQ(classifier_->Classify(u"  "), AutofillDataType::kUnknown);
}

TEST_F(QueryClassifierTest, QueryWithOnlyStopWords) {
  EXPECT_EQ(classifier_->Classify(u"what is my"), AutofillDataType::kUnknown);
  EXPECT_EQ(classifier_->Classify(u"show me the details please"),
            AutofillDataType::kUnknown);
}

TEST_F(QueryClassifierTest, AddressIntents) {
  EXPECT_EQ(classifier_->Classify(u"my zip code"),
            AutofillDataType::kAddressZip);
  EXPECT_EQ(classifier_->Classify(u"What is the postal code?"),
            AutofillDataType::kAddressZip);
  EXPECT_EQ(classifier_->Classify(u"show me the City"),
            AutofillDataType::kAddressCity);
  EXPECT_EQ(classifier_->Classify(u"town"), AutofillDataType::kAddressCity);
  EXPECT_EQ(classifier_->Classify(u"state"), AutofillDataType::kAddressState);
  EXPECT_EQ(classifier_->Classify(u"province please"),
            AutofillDataType::kAddressState);
  EXPECT_EQ(classifier_->Classify(u"country"),
            AutofillDataType::kAddressCountry);
  EXPECT_EQ(classifier_->Classify(u"street name"),
            AutofillDataType::kAddressLine1);
  EXPECT_EQ(classifier_->Classify(u"address line 1"),
            AutofillDataType::kAddressLine1);
  EXPECT_EQ(classifier_->Classify(u"What is my address?"),
            AutofillDataType::kAddress);
  EXPECT_EQ(classifier_->Classify(u"home address"), AutofillDataType::kAddress);
}

TEST_F(QueryClassifierTest, ContactIntents) {
  EXPECT_EQ(classifier_->Classify(u"my phone number"),
            AutofillDataType::kPhone);
  EXPECT_EQ(classifier_->Classify(u"mobile"), AutofillDataType::kPhone);
  EXPECT_EQ(classifier_->Classify(u"what is my email"),
            AutofillDataType::kEmail);
  EXPECT_EQ(classifier_->Classify(u"e-mail address"), AutofillDataType::kEmail);
  EXPECT_EQ(classifier_->Classify(u"name"), AutofillDataType::kName);
  EXPECT_EQ(classifier_->Classify(u"what is my name"), AutofillDataType::kName);
}

TEST_F(QueryClassifierTest, PaymentIntents) {
  EXPECT_EQ(classifier_->Classify(u"IBAN"), AutofillDataType::kIban);
  EXPECT_EQ(classifier_->Classify(u"my bank account number"),
            AutofillDataType::kIban);
}

TEST_F(QueryClassifierTest, EntityIntents) {
  EXPECT_EQ(classifier_->Classify(u"license plate"),
            AutofillDataType::kVehiclePlate);
  EXPECT_EQ(classifier_->Classify(u"VIN number"),
            AutofillDataType::kVehicleVin);
  EXPECT_EQ(classifier_->Classify(u"car details"), AutofillDataType::kVehicle);
  EXPECT_EQ(classifier_->Classify(u"my vehicle"), AutofillDataType::kVehicle);
  EXPECT_EQ(classifier_->Classify(u"passport info"),
            AutofillDataType::kPassport);
  EXPECT_EQ(classifier_->Classify(u"flight number"),
            AutofillDataType::kFlightReservation);
  EXPECT_EQ(classifier_->Classify(u"my reservation"),
            AutofillDataType::kFlightReservation);
  EXPECT_EQ(classifier_->Classify(u"national id"),
            AutofillDataType::kNationalIdCard);
  EXPECT_EQ(classifier_->Classify(u"redress number"),
            AutofillDataType::kRedressNumber);
  EXPECT_EQ(classifier_->Classify(u"known traveler number"),
            AutofillDataType::kKnownTravelerNumber);
  EXPECT_EQ(classifier_->Classify(u"my KTN"),
            AutofillDataType::kKnownTravelerNumber);
  EXPECT_EQ(classifier_->Classify(u"driver's license"),
            AutofillDataType::kDriversLicense);
  EXPECT_EQ(classifier_->Classify(u"driving license"),
            AutofillDataType::kDriversLicense);
}

TEST_F(QueryClassifierTest, MixedWithStopWords) {
  EXPECT_EQ(classifier_->Classify(u"show me my zip code please"),
            AutofillDataType::kAddressZip);
  EXPECT_EQ(classifier_->Classify(u"what is the car's VIN"),
            AutofillDataType::kVehicleVin);
  EXPECT_EQ(classifier_->Classify(u"get my flight details"),
            AutofillDataType::kFlightReservation);
}

TEST_F(QueryClassifierTest, CaseInsensitivity) {
  EXPECT_EQ(classifier_->Classify(u"MY ZIP CODE"),
            AutofillDataType::kAddressZip);
  EXPECT_EQ(classifier_->Classify(u"My Zip Code"),
            AutofillDataType::kAddressZip);
}

TEST_F(QueryClassifierTest, PunctuationHandling) {
  EXPECT_EQ(classifier_->Classify(u"zip, code!"),
            AutofillDataType::kAddressZip);
  EXPECT_EQ(classifier_->Classify(u"city?"), AutofillDataType::kAddressCity);
  EXPECT_EQ(classifier_->Classify(u"my email, please"),
            AutofillDataType::kEmail);
}

TEST_F(QueryClassifierTest, NoKeywordMatch) {
  EXPECT_EQ(classifier_->Classify(u"how is the weather"),
            AutofillDataType::kUnknown);
  EXPECT_EQ(classifier_->Classify(u"set a timer"), AutofillDataType::kUnknown);
}

TEST_F(QueryClassifierTest, SubstringNonMatch) {
  EXPECT_EQ(classifier_->Classify(u"bank account"), AutofillDataType::kIban);
  EXPECT_EQ(classifier_->Classify(u"cartoon"), AutofillDataType::kUnknown);
}

TEST_F(QueryClassifierTest, MultiWordAddressIntents) {
  EXPECT_EQ(classifier_->Classify(u"my postal code"),
            AutofillDataType::kAddressZip);
  EXPECT_EQ(classifier_->Classify(u"what is the home address"),
            AutofillDataType::kAddress);
  EXPECT_EQ(classifier_->Classify(u"work address please"),
            AutofillDataType::kAddress);
  EXPECT_EQ(classifier_->Classify(u"address line 1"),
            AutofillDataType::kAddressLine1);
}

TEST_F(QueryClassifierTest, MultiWordEntityIntents) {
  EXPECT_EQ(classifier_->Classify(u"show my license plate"),
            AutofillDataType::kVehiclePlate);
  EXPECT_EQ(classifier_->Classify(u"plate number"),
            AutofillDataType::kVehiclePlate);
  EXPECT_EQ(classifier_->Classify(u"flight reservation code"),
            AutofillDataType::kFlightReservation);
  EXPECT_EQ(classifier_->Classify(u"what is my national id"),
            AutofillDataType::kNationalIdCard);
  EXPECT_EQ(classifier_->Classify(u"known traveler number"),
            AutofillDataType::kKnownTravelerNumber);
  EXPECT_EQ(classifier_->Classify(u"drivers license"),
            AutofillDataType::kDriversLicense);
}

}  // namespace annotation_reducer
