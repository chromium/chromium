// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/query_classifier.h"

#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "components/accessibility_annotator/core/annotation_reducer/query_intent_type.h"
#include "components/optimization_guide/optimization_guide_buildflags.h"

#if BUILDFLAG(BUILD_WITH_MODEL_EXECUTION)
#include "components/optimization_guide/core/model_execution/optimization_guide_model_execution_error.h"
#include "components/optimization_guide/core/model_execution/test/mock_remote_model_executor.h"
#include "components/optimization_guide/core/optimization_guide_proto_util.h"
#include "components/optimization_guide/proto/features/annotation_reducer_query_classifier.pb.h"
#endif  // BUILDFLAG(BUILD_WITH_MODEL_EXECUTION)
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

using ::testing::_;
using ::testing::An;
using ::testing::Invoke;

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

  void SetUp() override {
    classifier_ = CreateQueryClassifier(/*remote_model_executor=*/nullptr);
  }

  ClassifiedQuery RunClassifier(QueryClassifier classifier,
                                std::u16string_view query) {
    base::test::TestFuture<ClassifiedQuery> future;
    classifier.Run(std::u16string(query), future.GetCallback());
    return future.Get();
  }

  ClassifiedQuery RunClassifier(std::u16string_view query) {
    return RunClassifier(classifier_, query);
  }

 protected:
  QueryClassifier classifier_;
};

// Tests that empty or whitespace queries are classified as unknown.
TEST_F(QueryClassifierTest, EmptyQuery) {
  EXPECT_EQ(RunClassifier(u"").intent, QueryIntentType::kUnknown);
  EXPECT_EQ(RunClassifier(u"  ").intent, QueryIntentType::kUnknown);
}

// Tests that queries containing only stop words are classified as unknown.
TEST_F(QueryClassifierTest, QueryWithOnlyStopWords) {
  EXPECT_EQ(RunClassifier(u"what is my").intent, QueryIntentType::kUnknown);
  EXPECT_EQ(RunClassifier(u"show me the details please").intent,
            QueryIntentType::kUnknown);
}

// Tests that address-related queries are correctly classified.
TEST_F(QueryClassifierTest, AddressIntents) {
  EXPECT_EQ(RunClassifier(u"my zip code").intent, QueryIntentType::kAddressZip);
  EXPECT_EQ(RunClassifier(u"What is the postal code?").intent,
            QueryIntentType::kAddressZip);
  EXPECT_EQ(RunClassifier(u"show me the City").intent,
            QueryIntentType::kAddressCity);
  EXPECT_EQ(RunClassifier(u"town").intent, QueryIntentType::kAddressCity);
  EXPECT_EQ(RunClassifier(u"state").intent, QueryIntentType::kAddressState);
  EXPECT_EQ(RunClassifier(u"province please").intent,
            QueryIntentType::kAddressState);
  EXPECT_EQ(RunClassifier(u"country").intent, QueryIntentType::kAddressCountry);
  EXPECT_EQ(RunClassifier(u"street name").intent,
            QueryIntentType::kAddressStreetAddress);
  EXPECT_EQ(RunClassifier(u"What is my address?").intent,
            QueryIntentType::kAddressFull);
  EXPECT_EQ(RunClassifier(u"home address").intent,
            QueryIntentType::kAddressFull);
  EXPECT_EQ(RunClassifier(u"company name").intent,
            QueryIntentType::kCompanyName);
  EXPECT_EQ(RunClassifier(u"organization").intent,
            QueryIntentType::kCompanyName);
}

// Tests that contact-related queries are correctly classified.
TEST_F(QueryClassifierTest, ContactIntents) {
  EXPECT_EQ(RunClassifier(u"my phone number").intent, QueryIntentType::kPhone);
  EXPECT_EQ(RunClassifier(u"mobile").intent, QueryIntentType::kPhone);
  EXPECT_EQ(RunClassifier(u"what is my email").intent, QueryIntentType::kEmail);
  EXPECT_EQ(RunClassifier(u"e-mail address").intent, QueryIntentType::kEmail);
  EXPECT_EQ(RunClassifier(u"name").intent, QueryIntentType::kNameFull);
  EXPECT_EQ(RunClassifier(u"what is my name").intent,
            QueryIntentType::kNameFull);
}

// Tests that payment-related queries are correctly classified.
TEST_F(QueryClassifierTest, PaymentIntents) {
  EXPECT_EQ(RunClassifier(u"IBAN").intent, QueryIntentType::kIban);
  EXPECT_EQ(RunClassifier(u"my bank account number").intent,
            QueryIntentType::kIban);
}

// Tests that credit card-related queries are correctly classified.
TEST_F(QueryClassifierTest, CreditCardIntents) {
  EXPECT_EQ(RunClassifier(u"credit card").intent,
            QueryIntentType::kCreditCardFull);
  EXPECT_EQ(RunClassifier(u"credit card number").intent,
            QueryIntentType::kCreditCardNumber);
  EXPECT_EQ(RunClassifier(u"credit card expiration date").intent,
            QueryIntentType::kCreditCardExpirationDate);
  EXPECT_EQ(RunClassifier(u"CVV").intent,
            QueryIntentType::kCreditCardSecurityCode);
  EXPECT_EQ(RunClassifier(u"name on card").intent,
            QueryIntentType::kCreditCardNameOnCard);
}

// Tests that entity-related queries are correctly classified.
TEST_F(QueryClassifierTest, EntityIntents) {
  EXPECT_EQ(RunClassifier(u"license plate").intent,
            QueryIntentType::kVehiclePlateNumber);
  EXPECT_EQ(RunClassifier(u"VIN number").intent, QueryIntentType::kVehicleVin);
  EXPECT_EQ(RunClassifier(u"car details").intent, QueryIntentType::kVehicle);
  EXPECT_EQ(RunClassifier(u"my vehicle").intent, QueryIntentType::kVehicle);
  EXPECT_EQ(RunClassifier(u"passport info").intent,
            QueryIntentType::kPassportFull);
  EXPECT_EQ(RunClassifier(u"my reservation").intent,
            QueryIntentType::kFlightReservationFull);
  EXPECT_EQ(RunClassifier(u"national id").intent,
            QueryIntentType::kNationalIdCardFull);
  EXPECT_EQ(RunClassifier(u"redress number").intent,
            QueryIntentType::kRedressNumberNumber);
  EXPECT_EQ(RunClassifier(u"known traveler number").intent,
            QueryIntentType::kKnownTravelerNumberFull);
  EXPECT_EQ(RunClassifier(u"my KTN").intent,
            QueryIntentType::kKnownTravelerNumberFull);
  EXPECT_EQ(RunClassifier(u"driver's license").intent,
            QueryIntentType::kDriversLicenseFull);
  EXPECT_EQ(RunClassifier(u"driving license").intent,
            QueryIntentType::kDriversLicenseFull);
  EXPECT_EQ(RunClassifier(u"my shipment").intent,
            QueryIntentType::kShipmentFull);
  EXPECT_EQ(RunClassifier(u"where is my package").intent,
            QueryIntentType::kShipmentFull);
}

// Tests that entity attribute queries are correctly classified.
TEST_F(QueryClassifierTest, EntityAttributeIntents) {
  // Vehicle attributes
  EXPECT_EQ(RunClassifier(u"car make").intent, QueryIntentType::kVehicleMake);
  EXPECT_EQ(RunClassifier(u"vehicle model").intent,
            QueryIntentType::kVehicleModel);
  EXPECT_EQ(RunClassifier(u"car year").intent, QueryIntentType::kVehicleYear);
  EXPECT_EQ(RunClassifier(u"vehicle owner").intent,
            QueryIntentType::kVehicleOwner);
  EXPECT_EQ(RunClassifier(u"plate state").intent,
            QueryIntentType::kVehiclePlateState);

  // Passport attributes
  EXPECT_EQ(RunClassifier(u"passport number").intent,
            QueryIntentType::kPassportNumber);
  EXPECT_EQ(RunClassifier(u"passport expiration").intent,
            QueryIntentType::kPassportExpirationDate);
  EXPECT_EQ(RunClassifier(u"passport issue").intent,
            QueryIntentType::kPassportIssueDate);
  EXPECT_EQ(RunClassifier(u"passport country").intent,
            QueryIntentType::kPassportCountry);
  EXPECT_EQ(RunClassifier(u"passport name").intent,
            QueryIntentType::kPassportName);

  // Flight Reservation attributes
  EXPECT_EQ(RunClassifier(u"flight number").intent,
            QueryIntentType::kFlightReservationFlightNumber);
  EXPECT_EQ(RunClassifier(u"ticket number").intent,
            QueryIntentType::kFlightReservationTicketNumber);
  EXPECT_EQ(RunClassifier(u"confirmation code").intent,
            QueryIntentType::kFlightReservationConfirmationCode);
  EXPECT_EQ(RunClassifier(u"passenger name").intent,
            QueryIntentType::kFlightReservationPassengerName);
  EXPECT_EQ(RunClassifier(u"departure airport").intent,
            QueryIntentType::kFlightReservationDepartureAirport);
  EXPECT_EQ(RunClassifier(u"arrival airport").intent,
            QueryIntentType::kFlightReservationArrivalAirport);
  EXPECT_EQ(RunClassifier(u"departure date").intent,
            QueryIntentType::kFlightReservationDepartureDate);
  EXPECT_EQ(RunClassifier(u"arrival date").intent,
            QueryIntentType::kFlightReservationArrivalDate);

  // Shipment attributes
  EXPECT_EQ(RunClassifier(u"tracking number").intent,
            QueryIntentType::kShipmentTrackingNumber);
  EXPECT_EQ(RunClassifier(u"associated order id").intent,
            QueryIntentType::kShipmentAssociatedOrderId);
  EXPECT_EQ(RunClassifier(u"shipping address").intent,
            QueryIntentType::kShipmentDeliveryAddress);
  EXPECT_EQ(RunClassifier(u"carrier name").intent,
            QueryIntentType::kShipmentCarrierName);
  EXPECT_EQ(RunClassifier(u"carrier website").intent,
            QueryIntentType::kShipmentCarrierDomain);
  EXPECT_EQ(RunClassifier(u"estimated delivery date").intent,
            QueryIntentType::kShipmentEstimatedDeliveryDate);

  // National ID attributes
  EXPECT_EQ(RunClassifier(u"national id number").intent,
            QueryIntentType::kNationalIdCardNumber);
  EXPECT_EQ(RunClassifier(u"national id name").intent,
            QueryIntentType::kNationalIdCardName);

  // Redress/KTN attributes
  EXPECT_EQ(RunClassifier(u"redress name").intent,
            QueryIntentType::kRedressNumberName);
  EXPECT_EQ(RunClassifier(u"ktn number").intent,
            QueryIntentType::kKnownTravelerNumberNumber);

  // Drivers License attributes
  EXPECT_EQ(RunClassifier(u"driver's license number").intent,
            QueryIntentType::kDriversLicenseNumber);
  EXPECT_EQ(RunClassifier(u"drivers license state").intent,
            QueryIntentType::kDriversLicenseState);
}

// Tests that order-related queries are correctly classified.
TEST_F(QueryClassifierTest, OrderIntents) {
  EXPECT_EQ(RunClassifier(u"order id").intent, QueryIntentType::kOrderId);
  EXPECT_EQ(RunClassifier(u"order number").intent, QueryIntentType::kOrderId);
  EXPECT_EQ(RunClassifier(u"order date").intent, QueryIntentType::kOrderDate);
  EXPECT_EQ(RunClassifier(u"merchant name").intent,
            QueryIntentType::kOrderMerchantName);
  EXPECT_EQ(RunClassifier(u"store name").intent,
            QueryIntentType::kOrderMerchantName);
  EXPECT_EQ(RunClassifier(u"order grand total").intent,
            QueryIntentType::kOrderGrandTotal);
  EXPECT_EQ(RunClassifier(u"what is my order").intent,
            QueryIntentType::kOrderFull);
}

// Tests that queries mixed with stop words are correctly classified.
TEST_F(QueryClassifierTest, MixedWithStopWords) {
  EXPECT_EQ(RunClassifier(u"show me my zip code please").intent,
            QueryIntentType::kAddressZip);
  EXPECT_EQ(RunClassifier(u"what is the car's VIN").intent,
            QueryIntentType::kVehicleVin);
  EXPECT_EQ(RunClassifier(u"get my flight details").intent,
            QueryIntentType::kFlightReservationFull);
}

// Tests that query classification is case-insensitive.
TEST_F(QueryClassifierTest, CaseInsensitivity) {
  EXPECT_EQ(RunClassifier(u"MY ZIP CODE").intent, QueryIntentType::kAddressZip);
  EXPECT_EQ(RunClassifier(u"My Zip Code").intent, QueryIntentType::kAddressZip);
}

// Tests that queries with punctuation are correctly classified.
TEST_F(QueryClassifierTest, PunctuationHandling) {
  EXPECT_EQ(RunClassifier(u"zip, code!").intent, QueryIntentType::kAddressZip);
  EXPECT_EQ(RunClassifier(u"city?").intent, QueryIntentType::kAddressCity);
  EXPECT_EQ(RunClassifier(u"my email, please").intent, QueryIntentType::kEmail);
}

// Tests that queries with no matching keywords are classified as unknown.
TEST_F(QueryClassifierTest, NoKeywordMatch) {
  EXPECT_EQ(RunClassifier(u"how is the weather").intent,
            QueryIntentType::kUnknown);
  EXPECT_EQ(RunClassifier(u"set a timer").intent, QueryIntentType::kUnknown);
}

// Tests that substring matches don't incorrectly trigger classification.
TEST_F(QueryClassifierTest, SubstringNonMatch) {
  EXPECT_EQ(RunClassifier(u"bank account").intent, QueryIntentType::kIban);
  EXPECT_EQ(RunClassifier(u"cartoon").intent, QueryIntentType::kUnknown);
}

// Tests that multi-word address queries are correctly classified.
TEST_F(QueryClassifierTest, MultiWordAddressIntents) {
  EXPECT_EQ(RunClassifier(u"my postal code").intent,
            QueryIntentType::kAddressZip);
  EXPECT_EQ(RunClassifier(u"what is the home address").intent,
            QueryIntentType::kAddressFull);
  EXPECT_EQ(RunClassifier(u"work address please").intent,
            QueryIntentType::kAddressFull);
}

// Tests that multi-word entity queries are correctly classified.
TEST_F(QueryClassifierTest, MultiWordEntityIntents) {
  EXPECT_EQ(RunClassifier(u"show my license plate").intent,
            QueryIntentType::kVehiclePlateNumber);
  EXPECT_EQ(RunClassifier(u"plate number").intent,
            QueryIntentType::kVehiclePlateNumber);
  EXPECT_EQ(RunClassifier(u"flight reservation code").intent,
            QueryIntentType::kFlightReservationFull);
  EXPECT_EQ(RunClassifier(u"what is my national id").intent,
            QueryIntentType::kNationalIdCardFull);
  EXPECT_EQ(RunClassifier(u"known traveler number").intent,
            QueryIntentType::kKnownTravelerNumberFull);
  EXPECT_EQ(RunClassifier(u"drivers license").intent,
            QueryIntentType::kDriversLicenseFull);
}

// Tests that filter words are correctly extracted.
TEST_F(QueryClassifierTest, RequiredWords) {
  {
    ClassifiedQuery classified_query =
        RunClassifier(u"What's my home address in San Diego");
    EXPECT_EQ(classified_query.intent, QueryIntentType::kAddressFull);
    EXPECT_THAT(classified_query.filter_words,
                testing::ElementsAre(u"san", u"diego"));
  }
  {
    ClassifiedQuery classified_query =
        RunClassifier(u"show me my VIN for my Tesla");
    EXPECT_EQ(classified_query.intent, QueryIntentType::kVehicleVin);
    EXPECT_THAT(classified_query.filter_words,
                testing::ElementsAre(u"tesla"));
  }
  {
    ClassifiedQuery classified_query =
        RunClassifier(u"get flight number for LH123");
    EXPECT_EQ(classified_query.intent,
              QueryIntentType::kFlightReservationFlightNumber);
    EXPECT_THAT(classified_query.filter_words,
                testing::ElementsAre(u"lh123"));
  }
}

// Tests that CreateKeywordQueryClassifier performs simple matches.
TEST_F(QueryClassifierTest, SimpleMatch) {
  QueryClassifier classifier = internal::CreateKeywordQueryClassifier();
  EXPECT_EQ(RunClassifier(classifier, u"zip code").intent,
            QueryIntentType::kAddressZip);
}

#if BUILDFLAG(BUILD_WITH_MODEL_EXECUTION)
// Tests that CreateGeminiClassifier currently returns unknown when executor is
// null.
TEST_F(QueryClassifierTest, NoOp) {
  QueryClassifier classifier = internal::CreateGeminiClassifier(nullptr);
  EXPECT_EQ(RunClassifier(classifier, u"something complicated").intent,
            QueryIntentType::kUnknown);
}

class GeminiClassifierTest : public QueryClassifierTest {
 public:
  GeminiClassifierTest() = default;
  ~GeminiClassifierTest() override = default;

  void SetUp() override {
    mock_executor_ =
        std::make_unique<optimization_guide::MockRemoteModelExecutor>();
    classifier_ = internal::CreateGeminiClassifier(mock_executor_.get());
  }

  void SetMockExecutionResponse(std::string_view classification) {
    optimization_guide::proto::AnnotationReducerQueryClassifierResponse
        response;
    response.set_classification(std::string(classification));

    optimization_guide::OptimizationGuideModelExecutionResult result(
        base::ok(optimization_guide::AnyWrapProto(response)),
        /*execution_info=*/nullptr);

    EXPECT_CALL(*mock_executor_,
                ExecuteModel(optimization_guide::ModelBasedCapabilityKey::
                                 kAnnotationReducerQueryClassifier,
                             _, _, _))
        .WillOnce([result = std::move(result)](
                      optimization_guide::ModelBasedCapabilityKey feature,
                      const google::protobuf::MessageLite& request_metadata,
                      const optimization_guide::ModelExecutionOptions& options,
                      optimization_guide::
                          OptimizationGuideModelExecutionResultCallback
                              callback) mutable {
          std::move(callback).Run(std::move(result), nullptr);
        });
  }

 protected:
  std::unique_ptr<optimization_guide::MockRemoteModelExecutor> mock_executor_;
};

// Tests that GeminiClassifier correctly handles a successful classification
// with filter words.
TEST_F(GeminiClassifierTest, SuccessWithFilterWords) {
  SetMockExecutionResponse(
      R"({"intent": "kAddressFull", "filter_words": ["san", "diego"]})");

  ClassifiedQuery result =
      RunClassifier(classifier_, u"What's my address in San Diego");
  EXPECT_EQ(result.intent, QueryIntentType::kAddressFull);
  EXPECT_THAT(result.filter_words, testing::ElementsAre(u"san", u"diego"));
}

// Tests that GeminiClassifier correctly handles unknown intent.
TEST_F(GeminiClassifierTest, UnknownIntent) {
  SetMockExecutionResponse(R"({"intent": "kUnknown", "filter_words": []})");

  EXPECT_EQ(RunClassifier(classifier_, u"something random").intent,
            QueryIntentType::kUnknown);
}

// Tests that GeminiClassifier handles invalid JSON response.
TEST_F(GeminiClassifierTest, InvalidJson) {
  SetMockExecutionResponse("invalid json");

  EXPECT_EQ(RunClassifier(classifier_, u"some query").intent,
            QueryIntentType::kUnknown);
}

// Tests that GeminiClassifier handles response wrapped in Markdown.
TEST_F(GeminiClassifierTest, MarkdownResponse) {
  SetMockExecutionResponse(
      "```json\n"
      R"({"intent": "kNameFull", "filter_words": []})"
      "\n```");

  EXPECT_EQ(RunClassifier(classifier_, u"What's my name").intent,
            QueryIntentType::kNameFull);
}

// Tests that GeminiClassifier handles missing fields in JSON.
TEST_F(GeminiClassifierTest, MissingFields) {
  SetMockExecutionResponse(R"({"filter_words": ["missing", "intent"]})");

  EXPECT_EQ(RunClassifier(classifier_, u"some query").intent,
            QueryIntentType::kUnknown);
}
#endif  // BUILDFLAG(BUILD_WITH_MODEL_EXECUTION)

}  // namespace accessibility_annotator
