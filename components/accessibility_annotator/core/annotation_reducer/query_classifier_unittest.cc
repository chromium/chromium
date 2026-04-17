// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/query_classifier.h"

#include <string>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "base/test/test_future.h"
#include "components/accessibility_annotator/core/annotation_reducer/entry_type.h"
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

  // Punctuation matches.
  EXPECT_TRUE(internal::ContainsStandalonePhrase(u"hello, world", u"hello"));
  EXPECT_TRUE(internal::ContainsStandalonePhrase(u"hello, world", u"world"));
  EXPECT_TRUE(internal::ContainsStandalonePhrase(u"hello world!", u"world"));
  EXPECT_TRUE(internal::ContainsStandalonePhrase(u"(hello) world", u"hello"));
}

class QueryClassifierTest : public ::testing::Test {
 public:
  QueryClassifierTest() = default;
  ~QueryClassifierTest() override = default;

  void SetUp() override {
    classifier_ = CreateQueryClassifier(/*remote_model_executor=*/nullptr);
  }

  ClassifiedQuery RunClassifier(QueryClassifier classifier,
                                std::u16string_view query,
                                bool full_search = false) {
    base::test::TestFuture<ClassifiedQuery> future;
    classifier.Run(std::u16string(query), full_search, future.GetCallback());
    return future.Get();
  }

  ClassifiedQuery RunClassifier(std::u16string_view query,
                                bool full_search = false) {
    return RunClassifier(classifier_, query, full_search);
  }

 protected:
  QueryClassifier classifier_;
};

// Tests that empty or whitespace queries are classified as unknown.
TEST_F(QueryClassifierTest, EmptyQuery) {
  EXPECT_EQ(RunClassifier(u"").intent, EntryType::kUnknown);
  EXPECT_EQ(RunClassifier(u"  ").intent, EntryType::kUnknown);
}

// Tests that queries containing only stop words are classified as unknown.
TEST_F(QueryClassifierTest, QueryWithOnlyStopWords) {
  EXPECT_EQ(RunClassifier(u"what is my").intent, EntryType::kUnknown);
  EXPECT_EQ(RunClassifier(u"show me the details please").intent,
            EntryType::kUnknown);
}

// Tests that address-related queries are correctly classified.
TEST_F(QueryClassifierTest, AddressIntents) {
  EXPECT_EQ(RunClassifier(u"my zip code").intent, EntryType::kAddressZip);
  EXPECT_EQ(RunClassifier(u"What is the postal code?").intent,
            EntryType::kAddressZip);
  EXPECT_EQ(RunClassifier(u"show me the City").intent, EntryType::kAddressCity);
  EXPECT_EQ(RunClassifier(u"town").intent, EntryType::kAddressCity);
  EXPECT_EQ(RunClassifier(u"state").intent, EntryType::kAddressState);
  EXPECT_EQ(RunClassifier(u"province please").intent, EntryType::kAddressState);
  EXPECT_EQ(RunClassifier(u"country").intent, EntryType::kAddressCountry);
  EXPECT_EQ(RunClassifier(u"street name").intent,
            EntryType::kAddressStreetAddress);
  EXPECT_EQ(RunClassifier(u"What is my address?").intent,
            EntryType::kAddressFull);
  EXPECT_EQ(RunClassifier(u"home address").intent, EntryType::kAddressFull);
  EXPECT_EQ(RunClassifier(u"company name").intent, EntryType::kCompanyName);
  EXPECT_EQ(RunClassifier(u"organization").intent, EntryType::kCompanyName);
}

// Tests that contact-related queries are correctly classified.
TEST_F(QueryClassifierTest, ContactIntents) {
  EXPECT_EQ(RunClassifier(u"my phone number").intent, EntryType::kPhone);
  EXPECT_EQ(RunClassifier(u"mobile").intent, EntryType::kPhone);
  EXPECT_EQ(RunClassifier(u"what is my email").intent, EntryType::kEmail);
  EXPECT_EQ(RunClassifier(u"e-mail address").intent, EntryType::kEmail);
  EXPECT_EQ(RunClassifier(u"name").intent, EntryType::kNameFull);
  EXPECT_EQ(RunClassifier(u"what is my name").intent, EntryType::kNameFull);
}

// Tests that payment-related queries are correctly classified.
TEST_F(QueryClassifierTest, PaymentIntents) {
  EXPECT_EQ(RunClassifier(u"IBAN").intent, EntryType::kIban);
  EXPECT_EQ(RunClassifier(u"my bank account number").intent, EntryType::kIban);
}

// Tests that credit card-related queries are correctly classified.
TEST_F(QueryClassifierTest, CreditCardIntents) {
  EXPECT_EQ(RunClassifier(u"credit card").intent, EntryType::kCreditCardFull);
  EXPECT_EQ(RunClassifier(u"credit card number").intent,
            EntryType::kCreditCardNumber);
  EXPECT_EQ(RunClassifier(u"credit card expiration date").intent,
            EntryType::kCreditCardExpirationDate);
  EXPECT_EQ(RunClassifier(u"CVV").intent, EntryType::kCreditCardSecurityCode);
  EXPECT_EQ(RunClassifier(u"name on card").intent,
            EntryType::kCreditCardNameOnCard);
}

// Tests that entity-related queries are correctly classified.
TEST_F(QueryClassifierTest, EntityIntents) {
  EXPECT_EQ(RunClassifier(u"license plate").intent,
            EntryType::kVehiclePlateNumber);
  EXPECT_EQ(RunClassifier(u"VIN number").intent, EntryType::kVehicleVin);
  EXPECT_EQ(RunClassifier(u"car details").intent, EntryType::kVehicle);
  EXPECT_EQ(RunClassifier(u"my vehicle").intent, EntryType::kVehicle);
  EXPECT_EQ(RunClassifier(u"passport info").intent, EntryType::kPassportFull);
  EXPECT_EQ(RunClassifier(u"my reservation").intent,
            EntryType::kFlightReservationFull);
  EXPECT_EQ(RunClassifier(u"national id").intent,
            EntryType::kNationalIdCardFull);
  EXPECT_EQ(RunClassifier(u"redress number").intent,
            EntryType::kRedressNumberNumber);
  EXPECT_EQ(RunClassifier(u"known traveler number").intent,
            EntryType::kKnownTravelerNumberFull);
  EXPECT_EQ(RunClassifier(u"my KTN").intent,
            EntryType::kKnownTravelerNumberFull);
  EXPECT_EQ(RunClassifier(u"driver's license").intent,
            EntryType::kDriversLicenseFull);
  EXPECT_EQ(RunClassifier(u"driving license").intent,
            EntryType::kDriversLicenseFull);
  EXPECT_EQ(RunClassifier(u"my shipment").intent, EntryType::kShipmentFull);
  EXPECT_EQ(RunClassifier(u"where is my package").intent,
            EntryType::kShipmentFull);
}

// Tests that entity attribute queries are correctly classified.
TEST_F(QueryClassifierTest, EntityAttributeIntents) {
  // Vehicle attributes
  EXPECT_EQ(RunClassifier(u"car make").intent, EntryType::kVehicleMake);
  EXPECT_EQ(RunClassifier(u"vehicle model").intent, EntryType::kVehicleModel);
  EXPECT_EQ(RunClassifier(u"car year").intent, EntryType::kVehicleYear);
  EXPECT_EQ(RunClassifier(u"vehicle owner").intent, EntryType::kVehicleOwner);
  EXPECT_EQ(RunClassifier(u"plate state").intent,
            EntryType::kVehiclePlateState);

  // Passport attributes
  EXPECT_EQ(RunClassifier(u"passport number").intent,
            EntryType::kPassportNumber);
  EXPECT_EQ(RunClassifier(u"passport expiration").intent,
            EntryType::kPassportExpirationDate);
  EXPECT_EQ(RunClassifier(u"passport issue").intent,
            EntryType::kPassportIssueDate);
  EXPECT_EQ(RunClassifier(u"passport country").intent,
            EntryType::kPassportCountry);
  EXPECT_EQ(RunClassifier(u"passport name").intent, EntryType::kPassportName);

  // Flight Reservation attributes
  EXPECT_EQ(RunClassifier(u"flight number").intent,
            EntryType::kFlightReservationFlightNumber);
  EXPECT_EQ(RunClassifier(u"ticket number").intent,
            EntryType::kFlightReservationTicketNumber);
  EXPECT_EQ(RunClassifier(u"confirmation code").intent,
            EntryType::kFlightReservationConfirmationCode);
  EXPECT_EQ(RunClassifier(u"passenger name").intent,
            EntryType::kFlightReservationPassengerName);
  EXPECT_EQ(RunClassifier(u"departure airport").intent,
            EntryType::kFlightReservationDepartureAirport);
  EXPECT_EQ(RunClassifier(u"arrival airport").intent,
            EntryType::kFlightReservationArrivalAirport);
  EXPECT_EQ(RunClassifier(u"departure date").intent,
            EntryType::kFlightReservationDepartureDate);
  EXPECT_EQ(RunClassifier(u"arrival date").intent,
            EntryType::kFlightReservationArrivalDate);

  // Shipment attributes
  EXPECT_EQ(RunClassifier(u"tracking number").intent,
            EntryType::kShipmentTrackingNumber);
  EXPECT_EQ(RunClassifier(u"associated order id").intent,
            EntryType::kShipmentAssociatedOrderId);
  EXPECT_EQ(RunClassifier(u"shipping address").intent,
            EntryType::kShipmentDeliveryAddress);
  EXPECT_EQ(RunClassifier(u"carrier name").intent,
            EntryType::kShipmentCarrierName);
  EXPECT_EQ(RunClassifier(u"carrier website").intent,
            EntryType::kShipmentCarrierDomain);
  EXPECT_EQ(RunClassifier(u"estimated delivery date").intent,
            EntryType::kShipmentEstimatedDeliveryDate);

  // National ID attributes
  EXPECT_EQ(RunClassifier(u"national id number").intent,
            EntryType::kNationalIdCardNumber);
  EXPECT_EQ(RunClassifier(u"national id name").intent,
            EntryType::kNationalIdCardName);

  // Redress/KTN attributes
  EXPECT_EQ(RunClassifier(u"redress name").intent,
            EntryType::kRedressNumberName);
  EXPECT_EQ(RunClassifier(u"ktn number").intent,
            EntryType::kKnownTravelerNumberNumber);

  // Drivers License attributes
  EXPECT_EQ(RunClassifier(u"driver's license number").intent,
            EntryType::kDriversLicenseNumber);
  EXPECT_EQ(RunClassifier(u"drivers license state").intent,
            EntryType::kDriversLicenseState);
}

// Tests that order-related queries are correctly classified.
TEST_F(QueryClassifierTest, OrderIntents) {
  EXPECT_EQ(RunClassifier(u"order id").intent, EntryType::kOrderId);
  EXPECT_EQ(RunClassifier(u"order number").intent, EntryType::kOrderId);
  EXPECT_EQ(RunClassifier(u"order date").intent, EntryType::kOrderDate);
  EXPECT_EQ(RunClassifier(u"merchant name").intent,
            EntryType::kOrderMerchantName);
  EXPECT_EQ(RunClassifier(u"store name").intent, EntryType::kOrderMerchantName);
  EXPECT_EQ(RunClassifier(u"order grand total").intent,
            EntryType::kOrderGrandTotal);
  EXPECT_EQ(RunClassifier(u"what is my order").intent, EntryType::kOrderFull);
}

// Tests that queries mixed with stop words are correctly classified.
TEST_F(QueryClassifierTest, MixedWithStopWords) {
  EXPECT_EQ(RunClassifier(u"show me my zip code please").intent,
            EntryType::kAddressZip);
  EXPECT_EQ(RunClassifier(u"what is the car's VIN").intent,
            EntryType::kVehicleVin);
  EXPECT_EQ(RunClassifier(u"get my flight details").intent,
            EntryType::kFlightReservationFull);
}

// Tests that query classification is case-insensitive.
TEST_F(QueryClassifierTest, CaseInsensitivity) {
  EXPECT_EQ(RunClassifier(u"MY ZIP CODE").intent, EntryType::kAddressZip);
  EXPECT_EQ(RunClassifier(u"My Zip Code").intent, EntryType::kAddressZip);
}

// Tests that queries with punctuation are correctly classified.
TEST_F(QueryClassifierTest, PunctuationHandling) {
  EXPECT_EQ(RunClassifier(u"zip, code!").intent, EntryType::kAddressZip);
  EXPECT_EQ(RunClassifier(u"city?").intent, EntryType::kAddressCity);
  EXPECT_EQ(RunClassifier(u"my email, please").intent, EntryType::kEmail);
}

// Tests that queries with no matching keywords are classified as unknown.
TEST_F(QueryClassifierTest, NoKeywordMatch) {
  EXPECT_EQ(RunClassifier(u"how is the weather").intent, EntryType::kUnknown);
  EXPECT_EQ(RunClassifier(u"set a timer").intent, EntryType::kUnknown);
}

// Tests that substring matches don't incorrectly trigger classification.
TEST_F(QueryClassifierTest, SubstringNonMatch) {
  EXPECT_EQ(RunClassifier(u"bank account").intent, EntryType::kIban);
  EXPECT_EQ(RunClassifier(u"cartoon").intent, EntryType::kUnknown);
}

// Tests that multi-word address queries are correctly classified.
TEST_F(QueryClassifierTest, MultiWordAddressIntents) {
  EXPECT_EQ(RunClassifier(u"my postal code").intent, EntryType::kAddressZip);
  EXPECT_EQ(RunClassifier(u"what is the home address").intent,
            EntryType::kAddressFull);
  EXPECT_EQ(RunClassifier(u"work address please").intent,
            EntryType::kAddressFull);
}

// Tests that multi-word entity queries are correctly classified.
TEST_F(QueryClassifierTest, MultiWordEntityIntents) {
  EXPECT_EQ(RunClassifier(u"show my license plate").intent,
            EntryType::kVehiclePlateNumber);
  EXPECT_EQ(RunClassifier(u"plate number").intent,
            EntryType::kVehiclePlateNumber);
  EXPECT_EQ(RunClassifier(u"flight reservation code").intent,
            EntryType::kFlightReservationFull);
  EXPECT_EQ(RunClassifier(u"what is my national id").intent,
            EntryType::kNationalIdCardFull);
  EXPECT_EQ(RunClassifier(u"known traveler number").intent,
            EntryType::kKnownTravelerNumberFull);
  EXPECT_EQ(RunClassifier(u"drivers license").intent,
            EntryType::kDriversLicenseFull);
}

// Tests that filter words are correctly extracted.
TEST_F(QueryClassifierTest, RequiredWords) {
  {
    ClassifiedQuery classified_query =
        RunClassifier(u"What's my home address in San Diego");
    EXPECT_EQ(classified_query.intent, EntryType::kAddressFull);
    EXPECT_THAT(classified_query.filter_words,
                testing::ElementsAre(u"san", u"diego"));
  }
  {
    ClassifiedQuery classified_query =
        RunClassifier(u"show me my VIN for my Tesla");
    EXPECT_EQ(classified_query.intent, EntryType::kVehicleVin);
    EXPECT_THAT(classified_query.filter_words,
                testing::ElementsAre(u"tesla"));
  }
  {
    ClassifiedQuery classified_query =
        RunClassifier(u"get flight number for LH123");
    EXPECT_EQ(classified_query.intent,
              EntryType::kFlightReservationFlightNumber);
    EXPECT_THAT(classified_query.filter_words,
                testing::ElementsAre(u"lh123"));
  }
}

// Tests that CreateKeywordQueryClassifier performs simple matches.
TEST_F(QueryClassifierTest, SimpleMatch) {
  QueryClassifier classifier = internal::CreateKeywordQueryClassifier();
  EXPECT_EQ(RunClassifier(classifier, u"zip code").intent,
            EntryType::kAddressZip);
}

#if BUILDFLAG(BUILD_WITH_MODEL_EXECUTION)
// Tests that CreateGeminiClassifier currently returns unknown when executor is
// null.
TEST_F(QueryClassifierTest, NoOp) {
  QueryClassifier classifier = internal::CreateGeminiClassifier(nullptr);
  EXPECT_EQ(RunClassifier(classifier, u"something complicated").intent,
            EntryType::kUnknown);
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
// with filter words and ensures that the filter words are lower cased.
TEST_F(GeminiClassifierTest, SuccessWithFilterWords) {
  SetMockExecutionResponse(
      R"({"intent": "kAddressFull", "filter_words": ["San", "Diego"]})");

  ClassifiedQuery result =
      RunClassifier(classifier_, u"What's my address in San Diego",
                    /*full_search=*/true);
  EXPECT_EQ(result.intent, EntryType::kAddressFull);
  EXPECT_THAT(result.filter_words, testing::ElementsAre(u"san", u"diego"));
}

// Tests that GeminiClassifier is only called when full_search is true.
TEST_F(GeminiClassifierTest, FullSearchOnly) {
  // If full_search is false, it should return kUnknown without calling Gemini.
  // No EXPECT_CALL for ExecuteModel should be triggered here.
  EXPECT_EQ(
      RunClassifier(classifier_, u"some query", /*full_search=*/false).intent,
      EntryType::kUnknown);

  // If full_search is true, it should call Gemini.
  SetMockExecutionResponse(R"({"intent": "kNameFull", "filter_words": []})");
  EXPECT_EQ(
      RunClassifier(classifier_, u"some query", /*full_search=*/true).intent,
      EntryType::kNameFull);
}

// Tests that GeminiClassifier correctly handles unknown intent.
TEST_F(GeminiClassifierTest, UnknownIntent) {
  SetMockExecutionResponse(R"({"intent": "kUnknown", "filter_words": []})");

  EXPECT_EQ(
      RunClassifier(classifier_, u"something random", /*full_search=*/true)
          .intent,
      EntryType::kUnknown);
}

// Tests that GeminiClassifier handles invalid JSON response.
TEST_F(GeminiClassifierTest, InvalidJson) {
  SetMockExecutionResponse("invalid json");

  EXPECT_EQ(
      RunClassifier(classifier_, u"some query", /*full_search=*/true).intent,
      EntryType::kUnknown);
}

// Tests that GeminiClassifier handles response wrapped in Markdown.
TEST_F(GeminiClassifierTest, MarkdownResponse) {
  SetMockExecutionResponse(
      "```json\n"
      R"({"intent": "kNameFull", "filter_words": []})"
      "\n```");

  EXPECT_EQ(RunClassifier(classifier_, u"What's my name", /*full_search=*/true)
                .intent,
            EntryType::kNameFull);
}

// Tests that GeminiClassifier handles missing fields in JSON.
TEST_F(GeminiClassifierTest, MissingFields) {
  SetMockExecutionResponse(R"({"filter_words": ["missing", "intent"]})");

  EXPECT_EQ(
      RunClassifier(classifier_, u"some query", /*full_search=*/true).intent,
      EntryType::kUnknown);
}
#endif  // BUILDFLAG(BUILD_WITH_MODEL_EXECUTION)

}  // namespace accessibility_annotator
