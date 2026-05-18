// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/annotation_reducer/one_p_resolver_impl.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/thread_annotations.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/accessibility_annotator/core/annotation_reducer/memory_search_result.h"
#include "components/accessibility_annotator/core/annotation_reducer/one_p_service.pb.h"
#include "components/optimization_guide/core/model_execution/test/mock_remote_model_executor.h"
#include "components/optimization_guide/proto/features/annotation_reducer_one_p_resolver.pb.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/http/http_request_headers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {
namespace {

constexpr char kFakeToken[] = "fake_access_token";

enum class ServerResponseType {
  kSuccess,
  kMalformed,
  kMissingContext,
  kBadProto,
  kError
};

enum class FeatureState { kEnabledWithUrl, kEnabledEmptyUrl, kDisabled };

using ::optimization_guide::OptimizationGuideModelExecutionError;
using ::optimization_guide::OptimizationGuideModelExecutionResult;
using ::optimization_guide::OptimizationGuideModelExecutionResultCallback;
using ::testing::_;
using ::testing::Invoke;
using ModelExecutionError = ::optimization_guide::
    OptimizationGuideModelExecutionError::ModelExecutionError;

class OnePResolverImplTest : public ::testing::Test {
 public:
  explicit OnePResolverImplTest(
      FeatureState feature_state = FeatureState::kEnabledWithUrl) {
    test_server_.RegisterRequestHandler(base::BindRepeating(
        &OnePResolverImplTest::HandleRequest, base::Unretained(this)));
    CHECK(test_server_.Start());

    if (feature_state == FeatureState::kEnabledWithUrl) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          features::kAccessibilityAnnotationReducerOnePResolver,
          {{"one_p_service_url", test_server_.GetURL("/context").spec()}});
    } else if (feature_state == FeatureState::kEnabledEmptyUrl) {
      scoped_feature_list_.InitAndEnableFeature(
          features::kAccessibilityAnnotationReducerOnePResolver);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          features::kAccessibilityAnnotationReducerOnePResolver);
    }
  }

  void SetUp() override {
    url_loader_factory_ =
        base::MakeRefCounted<network::TestSharedURLLoaderFactory>(
            /*network_service=*/nullptr, /*is_trusted=*/true);
    resolver_ = std::make_unique<OnePResolverImpl>(
        url_loader_factory_, identity_test_environment_.identity_manager(),
        &mock_executor_);
  }

 protected:
  void SetServerResponse(ServerResponseType type) {
    base::AutoLock auto_lock(request_lock_);
    response_type_ = type;
  }

  void SignIn() {
    identity_test_environment_.MakePrimaryAccountAvailable(
        "test@example.com", signin::ConsentLevel::kSignin);
  }

  void IssueToken() {
    identity_test_environment_
        .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
            kFakeToken, base::Time::Max());
  }

  void IssueTokenError() {
    identity_test_environment_
        .WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
            GoogleServiceAuthError::FromConnectionError(net::ERR_FAILED));
  }

  void ExpectModelExecution(
      const std::string& expected_query,
      const std::string& expected_context,
      std::optional<
          optimization_guide::proto::AnnotationReducerOnePResolverResponse>
          response) {
    EXPECT_CALL(
        mock_executor_,
        ExecuteModel(
            optimization_guide::ModelBasedCapabilityKey::
                kAnnotationReducerOnePResolver,
            testing::ResultOf(
                [](const google::protobuf::MessageLite& m)
                    -> const optimization_guide::proto::
                        AnnotationReducerOnePResolverRequest& {
                          return static_cast<
                              const optimization_guide::proto::
                                  AnnotationReducerOnePResolverRequest&>(m);
                        },
                testing::AllOf(
                    testing::Property(
                        &optimization_guide::proto::
                            AnnotationReducerOnePResolverRequest::query,
                        expected_query),
                    testing::Property(
                        &optimization_guide::proto::
                            AnnotationReducerOnePResolverRequest::context,
                        expected_context))),
            _, _))
        .WillOnce([response](
                      optimization_guide::ModelBasedCapabilityKey feature,
                      const google::protobuf::MessageLite& request_metadata,
                      const optimization_guide::ModelExecutionOptions& options,
                      OptimizationGuideModelExecutionResultCallback callback) {
          if (response) {
            optimization_guide::proto::Any any;
            any.set_type_url(
                "type.googleapis.com/"
                "optimization_guide.proto."
                "AnnotationReducerOnePResolverResponse");
            any.set_value(response->SerializeAsString());
            std::move(callback).Run(
                OptimizationGuideModelExecutionResult(any, nullptr), nullptr);
          } else {
            std::move(callback).Run(
                OptimizationGuideModelExecutionResult(
                    base::unexpected(
                        OptimizationGuideModelExecutionError::
                            FromModelExecutionError(
                                ModelExecutionError::kGenericFailure)),
                    nullptr),
                nullptr);
          }
        });
  }

  [[nodiscard]] testing::AssertionResult VerifyCapturedRequest(
      const std::u16string& expected_query) {
    base::AutoLock auto_lock(request_lock_);

    // Verify the HTTP method.
    if (last_request_.method != net::test_server::METHOD_POST) {
      return testing::AssertionFailure()
             << "Expected method POST, got " << last_request_.method;
    }

    // Verify required request headers (Auth and Content-Type).
    auto auth_it = last_request_.headers.find("Authorization");
    if (auth_it == last_request_.headers.end() ||
        auth_it->second != std::string("Bearer ") + kFakeToken) {
      return testing::AssertionFailure()
             << "Missing or incorrect Authorization header";
    }

    auto content_type_it = last_request_.headers.find("Content-Type");
    if (content_type_it == last_request_.headers.end() ||
        content_type_it->second != "application/x-protobuf") {
      return testing::AssertionFailure()
             << "Missing or incorrect Content-Type header";
    }

    // Parse and verify the protobuf body payload.
    accessibility_annotator::OnePAnnotationsRequest request_proto;
    if (!request_proto.ParseFromString(last_request_.content)) {
      return testing::AssertionFailure() << "Failed to parse request content "
                                            "as OnePAnnotationsRequest proto";
    }

    std::string expected_query_utf8 = base::UTF16ToUTF8(expected_query);
    if (request_proto.query() != expected_query_utf8) {
      return testing::AssertionFailure()
             << "Expected query '" << expected_query_utf8 << "', got '"
             << request_proto.query() << "'";
    }

    return testing::AssertionSuccess();
  }

  // These dependencies must be declared before `resolver_` so they are
  // guaranteed to outlive it during test teardown.
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO};
  signin::IdentityTestEnvironment identity_test_environment_;

  // Accessed directly by tests.
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
  optimization_guide::MockRemoteModelExecutor mock_executor_;
  std::unique_ptr<OnePResolverImpl> resolver_;

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    // Only intercept requests to the expected endpoint.
    if (request.relative_url != "/context") {
      return nullptr;
    }

    // Safely capture the incoming request and read the expected response type.
    ServerResponseType response_type;
    {
      base::AutoLock auto_lock(request_lock_);
      last_request_ = request;
      response_type = response_type_;
    }

    // Construct the appropriate mocked HTTP response based on the test setup.
    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    if (response_type == ServerResponseType::kSuccess) {
      accessibility_annotator::OnePAnnotationsResponse response_proto;
      response_proto.set_response(
          R"({"context": {"references": [{"subject": "fake context"}]}})");
      response->set_code(net::HTTP_OK);
      response->set_content(response_proto.SerializeAsString());
      response->set_content_type("application/x-protobuf");
    } else if (response_type == ServerResponseType::kMalformed) {
      accessibility_annotator::OnePAnnotationsResponse response_proto;
      response_proto.set_response(R"({"bad_json": })");
      response->set_code(net::HTTP_OK);
      response->set_content(response_proto.SerializeAsString());
      response->set_content_type("application/x-protobuf");
    } else if (response_type == ServerResponseType::kMissingContext) {
      accessibility_annotator::OnePAnnotationsResponse response_proto;
      response_proto.set_response(R"({"other_field": "value"})");
      response->set_code(net::HTTP_OK);
      response->set_content(response_proto.SerializeAsString());
      response->set_content_type("application/x-protobuf");
    } else if (response_type == ServerResponseType::kBadProto) {
      response->set_code(net::HTTP_OK);
      response->set_content("not a valid proto");
      response->set_content_type("application/x-protobuf");
    } else if (response_type == ServerResponseType::kError) {
      response->set_code(net::HTTP_INTERNAL_SERVER_ERROR);
    } else {
      response->set_code(net::HTTP_NOT_FOUND);
    }
    return response;
  }

  base::Lock request_lock_;
  net::test_server::HttpRequest last_request_ GUARDED_BY(request_lock_);
  ServerResponseType response_type_ GUARDED_BY(request_lock_) =
      ServerResponseType::kSuccess;

  net::EmbeddedTestServer test_server_;
};

// Verifies that an empty result is returned when IdentityManager is null.
TEST_F(OnePResolverImplTest, NullIdentityManagerReturnsEmpty) {
  auto resolver_without_identity = std::make_unique<OnePResolverImpl>(
      url_loader_factory_, nullptr, &mock_executor_);

  base::test::TestFuture<std::vector<MemorySearchResult>> future;
  resolver_without_identity->Query(u"any query", future.GetCallback());

  // Await and verify empty results.
  std::vector<MemorySearchResult> results = future.Take();
  EXPECT_TRUE(results.empty());
}

// Verifies that Query returns an empty result set if the RemoteModelExecutor is
// null.
TEST_F(OnePResolverImplTest, NullRemoteModelExecutorReturnsEmpty) {
  // Initialize resolver with a null RemoteModelExecutor.
  OnePResolverImpl resolver_no_executor(
      url_loader_factory_, identity_test_environment_.identity_manager(),
      nullptr);
  SignIn();

  base::test::TestFuture<std::vector<MemorySearchResult>> future;
  resolver_no_executor.Query(u"any query", future.GetCallback());
  IssueToken();

  // Await and verify empty results.
  std::vector<MemorySearchResult> results = future.Take();
  EXPECT_TRUE(results.empty());
}

// Verifies that querying without a signed-in user returns an empty result set.
TEST_F(OnePResolverImplTest, NotSignedInReturnsEmpty) {
  base::test::TestFuture<std::vector<MemorySearchResult>> future;
  resolver_->Query(u"any query", future.GetCallback());

  // Await and verify empty results.
  std::vector<MemorySearchResult> results = future.Take();
  EXPECT_TRUE(results.empty());
}

// Verifies that a failure to fetch an access token returns an empty result set.
TEST_F(OnePResolverImplTest, TokenFetchFailureReturnsEmpty) {
  SignIn();

  base::test::TestFuture<std::vector<MemorySearchResult>> future;
  resolver_->Query(u"any query", future.GetCallback());

  // Simulate an error during access token retrieval.
  IssueTokenError();

  // Await and verify empty results.
  std::vector<MemorySearchResult> results = future.Take();
  EXPECT_TRUE(results.empty());
}

// A fixture subclass for tests where the feature is completely disabled.
class OnePResolverImplDisabledTest : public OnePResolverImplTest {
 public:
  OnePResolverImplDisabledTest()
      : OnePResolverImplTest(FeatureState::kDisabled) {}
};

// Verifies that Query early-outs when the 1P resolver feature is disabled.
TEST_F(OnePResolverImplDisabledTest, FeatureDisabledReturnsEmpty) {
  SignIn();

  base::test::TestFuture<std::vector<MemorySearchResult>> future;
  resolver_->Query(u"any query", future.GetCallback());

  // Await and verify empty results.
  std::vector<MemorySearchResult> results = future.Take();
  EXPECT_TRUE(results.empty());
}

// A fixture subclass for tests where the feature is enabled but URL is empty.
class OnePResolverImplEmptyUrlTest : public OnePResolverImplTest {
 public:
  OnePResolverImplEmptyUrlTest()
      : OnePResolverImplTest(FeatureState::kEnabledEmptyUrl) {}
};

// Verifies that Query early-outs if the one_p_service_url parameter is empty.
TEST_F(OnePResolverImplEmptyUrlTest, EmptyServiceUrlReturnsEmpty) {
  SignIn();

  base::test::TestFuture<std::vector<MemorySearchResult>> future;
  resolver_->Query(u"any query", future.GetCallback());

  // Await and verify empty results.
  std::vector<MemorySearchResult> results = future.Take();
  EXPECT_TRUE(results.empty());
}

// Verifies that a concurrent request cancels the previous one, yielding empty
// results for the first.
TEST_F(OnePResolverImplTest, ConcurrentQueriesCancelPrevious) {
  SetServerResponse(ServerResponseType::kError);

  SignIn();

  base::test::TestFuture<std::vector<MemorySearchResult>> future1;
  base::test::TestFuture<std::vector<MemorySearchResult>> future2;

  resolver_->Query(u"first query", future1.GetCallback());

  // This will cancel the first request implicitly and fulfill its future.
  resolver_->Query(u"second query", future2.GetCallback());

  // Wait for the first query to return empty results.
  std::vector<MemorySearchResult> results1 = future1.Take();
  EXPECT_TRUE(results1.empty());

  // Respond to the token request and let the network request fail for the
  // second query so we don't have to mock the remote model executor.
  IssueToken();

  // Wait for the second query to return empty results.
  std::vector<MemorySearchResult> results2 = future2.Take();
  EXPECT_TRUE(results2.empty());
}

// Verifies that an HTTP error from the 1P service yields an empty result set.
TEST_F(OnePResolverImplTest, HttpErrorFromOnePService) {
  SetServerResponse(ServerResponseType::kError);
  SignIn();

  // Trigger query against an endpoint that returns an HTTP error.
  base::test::TestFuture<std::vector<MemorySearchResult>> future;
  resolver_->Query(u"any query", future.GetCallback());

  // Simulate successful token fetch to proceed to network request.
  IssueToken();

  // Await and verify empty results.
  std::vector<MemorySearchResult> results = future.Take();
  EXPECT_TRUE(results.empty());
  EXPECT_TRUE(VerifyCapturedRequest(u"any query"));
}

// Verifies that a malformed JSON response from the 1P service yields an empty
// result set.
TEST_F(OnePResolverImplTest, MalformedOnePServiceResponse) {
  SetServerResponse(ServerResponseType::kMalformed);
  SignIn();

  // Trigger query against an endpoint that returns malformed JSON.
  base::test::TestFuture<std::vector<MemorySearchResult>> future;
  resolver_->Query(u"any query", future.GetCallback());

  // Simulate successful token fetch to proceed to network request.
  IssueToken();

  // Await and verify empty results.
  std::vector<MemorySearchResult> results = future.Take();
  EXPECT_TRUE(results.empty());
  EXPECT_TRUE(VerifyCapturedRequest(u"any query"));
}

// Verifies that a 1P response missing the 'context' field yields an empty
// result set.
TEST_F(OnePResolverImplTest, MissingContextInOnePServiceResponse) {
  SetServerResponse(ServerResponseType::kMissingContext);
  SignIn();

  // Trigger query against an endpoint that returns JSON without the 'context'
  // field.
  base::test::TestFuture<std::vector<MemorySearchResult>> future;
  resolver_->Query(u"any query", future.GetCallback());

  // Simulate successful token fetch to proceed to network request.
  IssueToken();

  // Await and verify empty results.
  std::vector<MemorySearchResult> results = future.Take();
  EXPECT_TRUE(results.empty());
  EXPECT_TRUE(VerifyCapturedRequest(u"any query"));
}

// Verifies that an unparsable protobuf response from the 1P service yields
// an empty result set.
TEST_F(OnePResolverImplTest, BadProtoFromOnePService) {
  SetServerResponse(ServerResponseType::kBadProto);
  SignIn();

  // Trigger query against an endpoint that returns a non-proto payload.
  base::test::TestFuture<std::vector<MemorySearchResult>> future;
  resolver_->Query(u"any query", future.GetCallback());

  // Simulate successful token fetch to proceed to network request.
  IssueToken();

  // Await and verify empty results.
  std::vector<MemorySearchResult> results = future.Take();
  EXPECT_TRUE(results.empty());
  EXPECT_TRUE(VerifyCapturedRequest(u"any query"));
}

// Verifies that a model execution error from the RemoteModelExecutor yields an
// empty result set.
TEST_F(OnePResolverImplTest, ModelExecutionError) {
  // Mock the model executor to return a generic failure error.
  ExpectModelExecution("any query",
                       "{\"references\":[{\"subject\":\"fake context\"}]}",
                       std::nullopt);

  SignIn();

  // Trigger query and issue token to proceed to model execution.
  base::test::TestFuture<std::vector<MemorySearchResult>> future;
  resolver_->Query(u"any query", future.GetCallback());
  IssueToken();

  // Await and verify empty results.
  std::vector<MemorySearchResult> results = future.Take();
  EXPECT_TRUE(results.empty());
}

// Verifies that a successful response from the 1P service is successfully
// delegated to the remote model executor and parsed correctly into memory
// search results.
TEST_F(OnePResolverImplTest, ValidOnePServiceResponse) {
  optimization_guide::proto::AnnotationReducerOnePResolverResponse response;
  auto* answer = response.add_answers();
  answer->set_type(optimization_guide::proto::ReducedAnswer::
                       ANSWER_TYPE_FLIGHT_RESERVATION_CONFIRMATION_CODE);
  answer->set_value("NEXJ8P");
  answer->set_confidence_score(0.95f);

  auto* source = answer->add_sources();
  source->set_type(
      optimization_guide::proto::ReducedAnswer::Source::SOURCE_TYPE_GMAIL);
  source->set_deeplink_url("https://mail.google.com/mail/u/0/#inbox/1");

  auto* meta = answer->add_metadata_list();
  meta->set_type(optimization_guide::proto::ReducedAnswer::
                     ANSWER_TYPE_FLIGHT_RESERVATION_ARRIVAL_AIRPORT);
  meta->set_value("ASE");

  ExpectModelExecution("any query",
                       "{\"references\":[{\"subject\":\"fake context\"}]}",
                       response);

  SignIn();

  // Trigger query and issue token to proceed to model execution.
  base::test::TestFuture<std::vector<MemorySearchResult>> future;
  resolver_->Query(u"any query", future.GetCallback());
  IssueToken();

  // Await and verify the fully parsed results.
  std::vector<MemorySearchResult> results = future.Take();
  ASSERT_EQ(results.size(), 1u);
  EXPECT_EQ(results[0].type, EntryType::kFlightReservationConfirmationCode);
  EXPECT_EQ(results[0].value, u"NEXJ8P");
  EXPECT_EQ(results[0].type_name, u"");
  EXPECT_DOUBLE_EQ(results[0].confidence_score, 0.95f);
  ASSERT_EQ(results[0].sources.size(), 1u);
  EXPECT_EQ(results[0].sources[0].type, MemoryEntrySourceType::kGmail);
  EXPECT_EQ(results[0].sources[0].deeplink_url,
            "https://mail.google.com/mail/u/0/#inbox/1");
  ASSERT_EQ(results[0].metadata_list.size(), 1u);
  EXPECT_EQ(results[0].metadata_list[0].type,
            EntryType::kFlightReservationArrivalAirport);
  EXPECT_EQ(results[0].metadata_list[0].type_name, u"");
  EXPECT_EQ(results[0].metadata_list[0].value, u"ASE");
}

// Verifies that an invalid unmapped answer type is rejected.
TEST_F(OnePResolverImplTest, ModelExecutionInvalidAnswerTypeFails) {
  optimization_guide::proto::AnnotationReducerOnePResolverResponse response;
  auto* answer = response.add_answers();
  answer->set_type(
      static_cast<optimization_guide::proto::ReducedAnswer::AnswerType>(9999));
  answer->set_value("John Doe");

  ExpectModelExecution("any query",
                       "{\"references\":[{\"subject\":\"fake context\"}]}",
                       response);

  SignIn();
  base::test::TestFuture<std::vector<MemorySearchResult>> future;
  resolver_->Query(u"any query", future.GetCallback());
  IssueToken();

  std::vector<MemorySearchResult> results = future.Take();
  EXPECT_TRUE(results.empty());
}

// Verifies that the parser correctly handles a model response containing
// multiple valid results.
TEST_F(OnePResolverImplTest, ModelExecutionParsesMultipleResults) {
  optimization_guide::proto::AnnotationReducerOnePResolverResponse response;

  auto* answer1 = response.add_answers();
  answer1->set_type(optimization_guide::proto::ReducedAnswer::
                        ANSWER_TYPE_FLIGHT_RESERVATION_CONFIRMATION_CODE);
  answer1->set_value("NEXJ8P");
  answer1->set_confidence_score(0.95f);

  auto* answer2 = response.add_answers();
  answer2->set_type(
      optimization_guide::proto::ReducedAnswer::ANSWER_TYPE_UNKNOWN);
  answer2->set_type_name("Custom Passport");
  answer2->set_value("ABC1234");
  answer2->set_confidence_score(0.85f);

  ExpectModelExecution("any query",
                       "{\"references\":[{\"subject\":\"fake context\"}]}",
                       response);

  SignIn();

  // Trigger query and issue token to proceed to model execution.
  base::test::TestFuture<std::vector<MemorySearchResult>> future;
  resolver_->Query(u"any query", future.GetCallback());
  IssueToken();

  // Await and verify that both results were fully parsed.
  std::vector<MemorySearchResult> results = future.Take();
  ASSERT_EQ(results.size(), 2u);
  EXPECT_EQ(results[0].type, EntryType::kFlightReservationConfirmationCode);
  EXPECT_EQ(results[0].value, u"NEXJ8P");
  EXPECT_EQ(results[0].type_name, u"");
  EXPECT_EQ(results[1].type, EntryType::kUnknown);
  EXPECT_EQ(results[1].value, u"ABC1234");
  EXPECT_EQ(results[1].type_name, u"Custom Passport");
}

}  // namespace
}  // namespace accessibility_annotator
