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

class OnePResolverImplTest : public ::testing::Test {
 public:
  explicit OnePResolverImplTest(
      FeatureState feature_state = FeatureState::kEnabledWithUrl) {
    test_server_.RegisterRequestHandler(base::BindRepeating(
        &OnePResolverImplTest::HandleRequest, base::Unretained(this)));
    CHECK(test_server_.Start());

    if (feature_state == FeatureState::kEnabledWithUrl) {
      scoped_feature_list_.InitAndEnableFeatureWithParameters(
          kAccessibilityAnnotationReducerOnePResolver,
          {{"one_p_service_url", test_server_.GetURL("/context").spec()}});
    } else if (feature_state == FeatureState::kEnabledEmptyUrl) {
      scoped_feature_list_.InitAndEnableFeature(
          kAccessibilityAnnotationReducerOnePResolver);
    } else {
      scoped_feature_list_.InitAndDisableFeature(
          kAccessibilityAnnotationReducerOnePResolver);
    }
  }

  void SetUp() override {
    url_loader_factory_ =
        base::MakeRefCounted<network::TestSharedURLLoaderFactory>(
            /*network_service=*/nullptr, /*is_trusted=*/true);
    resolver_ = std::make_unique<OnePResolverImpl>(
        url_loader_factory_, identity_test_environment_.identity_manager());
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
            GoogleServiceAuthError(GoogleServiceAuthError::CONNECTION_FAILED));
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
    OnePAnnotationsRequest request_proto;
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
      OnePAnnotationsResponse response_proto;
      response_proto.set_response("{\"context\": \"fake context\"}");
      response->set_code(net::HTTP_OK);
      response->set_content(response_proto.SerializeAsString());
      response->set_content_type("application/x-protobuf");
    } else if (response_type == ServerResponseType::kMalformed) {
      OnePAnnotationsResponse response_proto;
      response_proto.set_response("{\"bad_json\": }");
      response->set_code(net::HTTP_OK);
      response->set_content(response_proto.SerializeAsString());
      response->set_content_type("application/x-protobuf");
    } else if (response_type == ServerResponseType::kMissingContext) {
      OnePAnnotationsResponse response_proto;
      response_proto.set_response("{\"other_field\": \"value\"}");
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

// Verifies that instantiating with a null IdentityManager degrades gracefully.
TEST_F(OnePResolverImplTest, NullIdentityManagerReturnsEmpty) {
  auto resolver_without_identity =
      std::make_unique<OnePResolverImpl>(url_loader_factory_, nullptr);

  base::test::TestFuture<std::vector<MemorySearchResult>> future;
  resolver_without_identity->Query(u"any query", future.GetCallback());

  std::vector<MemorySearchResult> results = future.Take();
  EXPECT_TRUE(results.empty());
}

// Verifies that querying without a signed-in user returns an empty result set.
TEST_F(OnePResolverImplTest, NotSignedInReturnsEmpty) {
  base::test::TestFuture<std::vector<MemorySearchResult>> future;
  resolver_->Query(u"any query", future.GetCallback());

  std::vector<MemorySearchResult> results = future.Take();
  EXPECT_TRUE(results.empty());
}

// Verifies that a failure to fetch an access token returns an empty result set.
TEST_F(OnePResolverImplTest, TokenFetchFailureReturnsEmpty) {
  SignIn();

  base::test::TestFuture<std::vector<MemorySearchResult>> future;
  resolver_->Query(u"any query", future.GetCallback());
  IssueTokenError();

  std::vector<MemorySearchResult> results = future.Take();
  EXPECT_TRUE(results.empty());
}

// A fixture subclass for tests where the feature is completely disabled.
class OnePResolverImplDisabledTest : public OnePResolverImplTest {
 public:
  OnePResolverImplDisabledTest()
      : OnePResolverImplTest(FeatureState::kDisabled) {}
};

// Verifies that Query successfully executes its asynchronous callback,
// but returns an empty result set when the OneP resolver feature is disabled.
TEST_F(OnePResolverImplDisabledTest, FeatureDisabledReturnsEmpty) {
  SignIn();

  base::test::TestFuture<std::vector<MemorySearchResult>> future;
  resolver_->Query(u"any query", future.GetCallback());

  std::vector<MemorySearchResult> results = future.Take();
  EXPECT_TRUE(results.empty());
}

// A fixture subclass for tests where the feature is enabled but URL is empty.
class OnePResolverImplEmptyUrlTest : public OnePResolverImplTest {
 public:
  OnePResolverImplEmptyUrlTest()
      : OnePResolverImplTest(FeatureState::kEnabledEmptyUrl) {}
};

// Verifies that if no URL is provided in the feature parameters, the resolver
// immediately returns an empty result without requesting a token.
TEST_F(OnePResolverImplEmptyUrlTest, EmptyServiceUrlReturnsEmpty) {
  SignIn();

  base::test::TestFuture<std::vector<MemorySearchResult>> future;
  resolver_->Query(u"any query", future.GetCallback());

  std::vector<MemorySearchResult> results = future.Take();
  EXPECT_TRUE(results.empty());
}

// Verifies that a concurrent request will cancel the previous one and execute
// the previous callback explicitly with an empty result set.
TEST_F(OnePResolverImplTest, ConcurrentQueriesCancelPrevious) {
  SignIn();

  base::test::TestFuture<std::vector<MemorySearchResult>> future1;
  base::test::TestFuture<std::vector<MemorySearchResult>> future2;

  resolver_->Query(u"first query", future1.GetCallback());

  // This will cancel the first request implicitly and fulfill its future.
  resolver_->Query(u"second query", future2.GetCallback());

  std::vector<MemorySearchResult> results1 = future1.Take();
  EXPECT_TRUE(results1.empty());

  // Respond to the token request and let the network request succeed for the
  // second query.
  IssueToken();

  std::vector<MemorySearchResult> results2 = future2.Take();
  EXPECT_TRUE(results2.empty());
}

// TODO(b:487416734): Update to verify memory search results once the mapping is
// implemented. Verifies that a successful response from the OneP service is
// handled correctly.
TEST_F(OnePResolverImplTest, ValidOnePServiceResponse) {
  SignIn();

  base::test::TestFuture<std::vector<MemorySearchResult>> future;
  resolver_->Query(u"any query", future.GetCallback());
  IssueToken();

  std::vector<MemorySearchResult> results = future.Take();
  EXPECT_TRUE(results.empty());
  EXPECT_TRUE(VerifyCapturedRequest(u"any query"));
}

// Verifies that a malformed response from the OneP service gracefully returns
// an empty vector.
TEST_F(OnePResolverImplTest, MalformedOnePServiceResponse) {
  SetServerResponse(ServerResponseType::kMalformed);
  SignIn();

  base::test::TestFuture<std::vector<MemorySearchResult>> future;
  resolver_->Query(u"any query", future.GetCallback());
  IssueToken();

  std::vector<MemorySearchResult> results = future.Take();
  EXPECT_TRUE(results.empty());
  EXPECT_TRUE(VerifyCapturedRequest(u"any query"));
}

// Verifies that a response missing the context field returns an empty vector.
TEST_F(OnePResolverImplTest, MissingContextInOnePServiceResponse) {
  SetServerResponse(ServerResponseType::kMissingContext);
  SignIn();

  base::test::TestFuture<std::vector<MemorySearchResult>> future;
  resolver_->Query(u"any query", future.GetCallback());
  IssueToken();

  std::vector<MemorySearchResult> results = future.Take();
  EXPECT_TRUE(results.empty());
  EXPECT_TRUE(VerifyCapturedRequest(u"any query"));
}

// Verifies that an HTTP error from the OneP service returns an empty vector.
TEST_F(OnePResolverImplTest, HttpErrorFromOnePService) {
  SetServerResponse(ServerResponseType::kError);
  SignIn();

  base::test::TestFuture<std::vector<MemorySearchResult>> future;
  resolver_->Query(u"any query", future.GetCallback());
  IssueToken();

  std::vector<MemorySearchResult> results = future.Take();
  EXPECT_TRUE(results.empty());
  EXPECT_TRUE(VerifyCapturedRequest(u"any query"));
}

// Verifies that a response that fails to parse as a proto returns an empty
// vector.
TEST_F(OnePResolverImplTest, BadProtoFromOnePService) {
  SetServerResponse(ServerResponseType::kBadProto);
  SignIn();

  base::test::TestFuture<std::vector<MemorySearchResult>> future;
  resolver_->Query(u"any query", future.GetCallback());
  IssueToken();

  std::vector<MemorySearchResult> results = future.Take();
  EXPECT_TRUE(results.empty());
  EXPECT_TRUE(VerifyCapturedRequest(u"any query"));
}

}  // namespace

}  // namespace accessibility_annotator
