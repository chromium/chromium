// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>

#include "base/test/mock_callback.h"
#include "components/offline_pages/core/prefetch/generate_page_bundle_request.h"
#include "components/offline_pages/core/prefetch/get_operation_request.h"
#include "components/offline_pages/core/prefetch/prefetch_proto_utils.h"
#include "components/offline_pages/core/prefetch/prefetch_request_test_base.h"
#include "components/offline_pages/core/prefetch/prefetch_types.h"
#include "components/offline_pages/core/prefetch/proto/offline_pages.pb.h"
#include "components/offline_pages/core/prefetch/proto/operation.pb.h"
#include "net/http/http_status_code.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "url/gurl.h"

using testing::_;
using testing::DoAll;
using testing::Eq;
using testing::SaveArg;

namespace offline_pages {

namespace {
const version_info::Channel kTestChannel = version_info::Channel::UNKNOWN;
const char kTestOperationName[] = "operation/test123";
const char kTestURL[] = "http://example.com";
const char kTestURL2[] = "http://example.com/2";
const char kTestURL3[] = "http://example.com/3";
const char kTestURL4[] = "http://example.com/4";
const char kTestUserAgent[] = "Test User Agent";
const char kTestGCMID[] = "Test GCM ID";
const int kTestMaxBundleSize = 100000;
const char kTestBodyName[] = "body_name";
const int64_t kTestBodyLength = 12345678LL;
const char kErrorMessage[] = "Invalid parameter";
}  // namespace

// Builds the request for GeneratePageBundleRequest / GetOperationRequest.
class RequestBuilder {
 public:
  RequestBuilder() {}
  virtual ~RequestBuilder() {}

  virtual void CreateRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefetchRequestFinishedCallback callback) = 0;
};

class GeneratePageBundleRequestBuilder : public RequestBuilder {
 public:
  void CreateRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefetchRequestFinishedCallback callback) override {
    std::vector<std::string> pages = {kTestURL, kTestURL2};
    fetcher_ = std::make_unique<GeneratePageBundleRequest>(
        kTestUserAgent, kTestGCMID, kTestMaxBundleSize, pages, kTestChannel,
        /*testing_header_value=*/"", url_loader_factory, std::move(callback));
  }

 private:
  std::unique_ptr<GeneratePageBundleRequest> fetcher_;
};

class GetOperationRequestBuilder : public RequestBuilder {
 public:
  void CreateRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefetchRequestFinishedCallback callback) override {
    fetcher_ = std::make_unique<GetOperationRequest>(
        kTestOperationName, kTestChannel, url_loader_factory,
        std::move(callback));
  }

 private:
  std::unique_ptr<GetOperationRequest> fetcher_;
};

// Builds the response that returns a pending/done operation.
class OperationBuilder {
 public:
  virtual ~OperationBuilder() {}

  // Builds the opereation with an Any value and returns it in binary serialized
  // format. Notes that Any value could be set in either 'metadata' or
  // 'result.response' field depending on the state of the operation, pending or
  // done.
  virtual std::string BuildFromAny(const std::string& any_type_url,
                                   const std::string& any_value) = 0;

  // Builds the operation with an error value and returns it in binary
  // serialized format. Notes that the error is only respected for done
  // operation.
  virtual std::string BuildFromError(int error_code,
                                     const std::string& error_message) = 0;

 protected:
  // Helper function to build the operation based on |is_done| that controls
  // where Any value goes.
  std::string BuildOperation(bool is_done,
                             int error_code,
                             const std::string& error_message,
                             const std::string& any_type_url,
                             const std::string& any_value) {
    proto::Operation operation;
    operation.set_name(kTestOperationName);
    operation.set_done(is_done);
    if (error_code != proto::OK) {
      operation.mutable_error()->set_code(error_code);
      operation.mutable_error()->set_message(error_message);
    }
    if (!any_type_url.empty()) {
      proto::Any* any =
          is_done ? operation.mutable_response() : operation.mutable_metadata();
      any->set_type_url(any_type_url);
      any->set_value(any_value);
    }
    std::string data;
    EXPECT_TRUE(operation.SerializeToString(&data));
    return data;
  }
};

class DoneOperationBuilder : public OperationBuilder {
 public:
  ~DoneOperationBuilder() override {}

  std::string BuildFromAny(const std::string& any_type_url,
                           const std::string& any_value) override {
    return BuildOperation(true, 0, std::string(), any_type_url, any_value);
  }

  std::string BuildFromError(int error_code,
                             const std::string& error_message) override {
    return BuildOperation(true, error_code, error_message, std::string(),
                          std::string());
  }
};

class PendingOperationBuilder : public OperationBuilder {
 public:
  ~PendingOperationBuilder() override {}

  std::string BuildFromAny(const std::string& any_type_url,
                           const std::string& any_value) override {
    return BuildOperation(false, 0, std::string(), any_type_url, any_value);
  }

  std::string BuildFromError(int error_code,
                             const std::string& error_message) override {
    return BuildOperation(false, error_code, error_message, std::string(),
                          std::string());
  }
};

// Combines both RequestBuilder and OperationBuilder in order to feed to
// PrefetchRequestOperationResponseTest.
class PrefetchRequestOperationResponseTestBuilder {
 public:
  PrefetchRequestOperationResponseTestBuilder() {}
  virtual ~PrefetchRequestOperationResponseTestBuilder() {}

  void CreateRequest(
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
      PrefetchRequestFinishedCallback callback) {
    request_builder_->CreateRequest(url_loader_factory, std::move(callback));
  }

  std::string BuildFromAny(const std::string& any_type_url,
                           const std::string& any_value) {
    return operation_builder_->BuildFromAny(any_type_url, any_value);
  }

  std::string BuildFromError(int error_code, const std::string& error_message) {
    return operation_builder_->BuildFromError(error_code, error_message);
  }

  // Return the operation_name that we expect when this has been processed.
  virtual std::string expected_operation_name() {
    return expected_operation_name_;
  }

 protected:
  std::unique_ptr<RequestBuilder> request_builder_;
  std::unique_ptr<OperationBuilder> operation_builder_;
  std::string expected_operation_name_;
};

class GeneratePageBundleRequestDoneOperationBuilder
    : public PrefetchRequestOperationResponseTestBuilder {
 public:
  GeneratePageBundleRequestDoneOperationBuilder() {
    request_builder_ = std::make_unique<GeneratePageBundleRequestBuilder>();
    operation_builder_ = std::make_unique<DoneOperationBuilder>();
  }
};

class GeneratePageBundleRequestPendingOperationBuilder
    : public PrefetchRequestOperationResponseTestBuilder {
 public:
  GeneratePageBundleRequestPendingOperationBuilder() {
    request_builder_ = std::make_unique<GeneratePageBundleRequestBuilder>();
    operation_builder_ = std::make_unique<PendingOperationBuilder>();
  }
};

class GetOperationRequestDoneOperationBuilder
    : public PrefetchRequestOperationResponseTestBuilder {
 public:
  GetOperationRequestDoneOperationBuilder() {
    request_builder_ = std::make_unique<GetOperationRequestBuilder>();
    operation_builder_ = std::make_unique<DoneOperationBuilder>();
    expected_operation_name_ = std::string(kTestOperationName);
  }
};

class GetOperationRequestPendingOperationBuilder
    : public PrefetchRequestOperationResponseTestBuilder {
 public:
  GetOperationRequestPendingOperationBuilder() {
    request_builder_ = std::make_unique<GetOperationRequestBuilder>();
    operation_builder_ = std::make_unique<PendingOperationBuilder>();
    expected_operation_name_ = std::string(kTestOperationName);
  }
};

template <typename T>
class PrefetchRequestOperationResponseTest : public PrefetchRequestTestBase {
 public:
  PrefetchRequestStatus SendWithErrorResponse(
      int error_code,
      const std::string& error_message) {
    return SendWithResponse(builder_.BuildFromError(error_code, error_message));
  }

  PrefetchRequestStatus SendWithAnyResponse(const std::string& any_type_url,
                                            const std::string& any_value) {
    return SendWithResponse(builder_.BuildFromAny(any_type_url, any_value));
  }

  PrefetchRequestStatus SendWithPageBundleResponse(
      const proto::PageBundle& bundle) {
    std::string bundle_data;
    EXPECT_TRUE(bundle.SerializeToString(&bundle_data));
    return SendWithResponse(
        builder_.BuildFromAny(kPageBundleTypeURL, bundle_data));
  }

  const std::string& operation_name() const { return operation_name_; }
  const std::vector<RenderPageInfo>& pages() const { return pages_; }
  const std::string expected_operation_name() {
    return builder_.expected_operation_name();
  }

 private:
  PrefetchRequestStatus SendWithResponse(const std::string& response_data) {
    base::MockCallback<PrefetchRequestFinishedCallback> callback;
    builder_.CreateRequest(shared_url_loader_factory(), callback.Get());

    PrefetchRequestStatus status;
    operation_name_.clear();
    pages_.clear();
    EXPECT_CALL(callback, Run(_, _, _))
        .WillOnce(DoAll(SaveArg<0>(&status), SaveArg<1>(&operation_name_),
                        SaveArg<2>(&pages_)));
    RespondWithData(response_data);
    RunUntilIdle();
    return status;
  }

  T builder_;
  std::string operation_name_;
  std::vector<RenderPageInfo> pages_;
};

// Lists all scenarios we want to test.
typedef testing::Types<GeneratePageBundleRequestDoneOperationBuilder,
                       GeneratePageBundleRequestPendingOperationBuilder,
                       GetOperationRequestDoneOperationBuilder,
                       GetOperationRequestPendingOperationBuilder>
    MyTypes;
TYPED_TEST_SUITE(PrefetchRequestOperationResponseTest, MyTypes);

TYPED_TEST(PrefetchRequestOperationResponseTest, EmptyOperation) {
  EXPECT_EQ(PrefetchRequestStatus::kShouldRetryWithBackoff,
            // No error is set for OK. Thus this will cause the operation
            // being filled with only done flag.
            this->SendWithErrorResponse(proto::OK, ""));
  EXPECT_EQ(this->operation_name(), this->expected_operation_name());
  EXPECT_TRUE(this->pages().empty());
}

TYPED_TEST(PrefetchRequestOperationResponseTest, ErrorValue) {
  EXPECT_EQ(PrefetchRequestStatus::kShouldRetryWithBackoff,
            this->SendWithErrorResponse(proto::UNKNOWN, kErrorMessage));
  EXPECT_EQ(this->operation_name(), this->expected_operation_name());
  EXPECT_TRUE(this->pages().empty());
}

TYPED_TEST(PrefetchRequestOperationResponseTest, InvalidTypeUrl) {
  EXPECT_EQ(PrefetchRequestStatus::kShouldRetryWithBackoff,
            this->SendWithAnyResponse("foo", ""));
  EXPECT_EQ(this->operation_name(), this->expected_operation_name());
  EXPECT_TRUE(this->pages().empty());
}

TYPED_TEST(PrefetchRequestOperationResponseTest, InvalidValue) {
  EXPECT_EQ(PrefetchRequestStatus::kShouldRetryWithBackoff,
            this->SendWithAnyResponse(kPageBundleTypeURL, "foo"));
  EXPECT_EQ(this->operation_name(), this->expected_operation_name());
  EXPECT_TRUE(this->pages().empty());
}

TYPED_TEST(PrefetchRequestOperationResponseTest, EmptyPageBundle) {
  proto::PageBundle bundle;
  EXPECT_EQ(PrefetchRequestStatus::kShouldRetryWithBackoff,
            this->SendWithPageBundleResponse(bundle));
  EXPECT_EQ(this->operation_name(), this->expected_operation_name());
  EXPECT_TRUE(this->pages().empty());
}

TYPED_TEST(PrefetchRequestOperationResponseTest, EmptyArchive) {
  proto::PageBundle bundle;
  bundle.add_archives();
  EXPECT_EQ(PrefetchRequestStatus::kShouldRetryWithBackoff,
            this->SendWithPageBundleResponse(bundle));
  EXPECT_EQ(this->operation_name(), this->expected_operation_name());
  EXPECT_TRUE(this->pages().empty());
}

TYPED_TEST(PrefetchRequestOperationResponseTest, NoPageInfo) {
  proto::PageBundle bundle;
  proto::Archive* archive = bundle.add_archives();
  archive->set_body_name(kTestBodyName);
  archive->set_body_length(kTestBodyLength);
  EXPECT_EQ(PrefetchRequestStatus::kShouldRetryWithBackoff,
            this->SendWithPageBundleResponse(bundle));
  EXPECT_EQ(this->operation_name(), this->expected_operation_name());
  EXPECT_TRUE(this->pages().empty());
}

TYPED_TEST(PrefetchRequestOperationResponseTest, MissingPageInfoUrl) {
  proto::PageBundle bundle;
  proto::Archive* archive = bundle.add_archives();
  proto::PageInfo* page_info = archive->add_page_infos();
  page_info->set_redirect_url(kTestURL);
  EXPECT_EQ(PrefetchRequestStatus::kShouldRetryWithBackoff,
            this->SendWithPageBundleResponse(bundle));
  EXPECT_EQ(this->operation_name(), this->expected_operation_name());
  EXPECT_TRUE(this->pages().empty());
}

TYPED_TEST(PrefetchRequestOperationResponseTest, SinglePage) {
  proto::PageBundle bundle;
  proto::Archive* archive = bundle.add_archives();
  archive->set_body_name(kTestBodyName);
  archive->set_body_length(kTestBodyLength);
  proto::PageInfo* page_info = archive->add_page_infos();
  page_info->set_url(kTestURL);
  page_info->set_redirect_url(kTestURL2);
  page_info->mutable_status()->set_code(proto::OK);
  page_info->set_transformation(proto::NO_TRANSFORMATION);
  int64_t ms_since_epoch = base::Time::Now().ToJavaTime();
  page_info->mutable_render_time()->set_seconds(ms_since_epoch / 1000);
  page_info->mutable_render_time()->set_nanos((ms_since_epoch % 1000) *
                                              1000000);
  EXPECT_EQ(PrefetchRequestStatus::kSuccess,
            this->SendWithPageBundleResponse(bundle));
  EXPECT_EQ(kTestOperationName, this->operation_name());
  ASSERT_EQ(1u, this->pages().size());
  EXPECT_EQ(kTestURL, this->pages().back().url);
  EXPECT_EQ(kTestURL2, this->pages().back().redirect_url);
  EXPECT_EQ(RenderStatus::RENDERED, this->pages().back().status);
  EXPECT_EQ(kTestBodyName, this->pages().back().body_name);
  EXPECT_EQ(kTestBodyLength, this->pages().back().body_length);
  EXPECT_EQ(ms_since_epoch, this->pages().back().render_time.ToJavaTime());
}

TYPED_TEST(PrefetchRequestOperationResponseTest, MultiplePages) {
  proto::PageBundle bundle;

  // Adds a page that is still being rendered.
  proto::Archive* archive = bundle.add_archives();
  proto::PageInfo* page_info = archive->add_page_infos();
  page_info->set_url(kTestURL);
  page_info->mutable_status()->set_code(proto::NOT_FOUND);

  // Adds a page that failed to render due to bundle size limits.
  archive = bundle.add_archives();
  page_info = archive->add_page_infos();
  page_info->set_url(kTestURL2);
  page_info->mutable_status()->set_code(proto::FAILED_PRECONDITION);

  // Adds a page that failed to render for any other reason.
  archive = bundle.add_archives();
  page_info = archive->add_page_infos();
  page_info->set_url(kTestURL3);
  page_info->mutable_status()->set_code(proto::UNKNOWN);

  // Adds a page that was rendered successfully.
  archive = bundle.add_archives();
  archive->set_body_name(kTestBodyName);
  archive->set_body_length(kTestBodyLength);
  page_info = archive->add_page_infos();
  page_info->set_url(kTestURL4);
  page_info->mutable_status()->set_code(proto::OK);
  page_info->set_transformation(proto::NO_TRANSFORMATION);
  int64_t ms_since_epoch = base::Time::Now().ToJavaTime();
  page_info->mutable_render_time()->set_seconds(ms_since_epoch / 1000);
  page_info->mutable_render_time()->set_nanos((ms_since_epoch % 1000) *
                                              1000000);

  EXPECT_EQ(PrefetchRequestStatus::kSuccess,
            this->SendWithPageBundleResponse(bundle));
  EXPECT_EQ(kTestOperationName, this->operation_name());
  ASSERT_EQ(4u, this->pages().size());
  EXPECT_EQ(kTestURL, this->pages().at(0).url);
  EXPECT_EQ(RenderStatus::PENDING, this->pages().at(0).status);
  EXPECT_EQ(kTestURL2, this->pages().at(1).url);
  EXPECT_EQ(RenderStatus::EXCEEDED_LIMIT, this->pages().at(1).status);
  EXPECT_EQ(kTestURL3, this->pages().at(2).url);
  EXPECT_EQ(RenderStatus::FAILED, this->pages().at(2).status);
  EXPECT_EQ(kTestURL4, this->pages().at(3).url);
  EXPECT_EQ(RenderStatus::RENDERED, this->pages().at(3).status);
  EXPECT_EQ(kTestBodyName, this->pages().at(3).body_name);
  EXPECT_EQ(kTestBodyLength, this->pages().at(3).body_length);
  EXPECT_EQ(ms_since_epoch, this->pages().at(3).render_time.ToJavaTime());
}

}  // namespace offline_pages
