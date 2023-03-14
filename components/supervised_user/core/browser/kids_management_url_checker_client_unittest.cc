// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/kids_management_url_checker_client.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/supervised_user/core/browser/kids_chrome_management_client.h"
#include "components/supervised_user/core/browser/proto/kidschromemanagement_messages.pb.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace {

using kids_chrome_management::ClassifyUrlResponse;

ClassifyUrlResponse::DisplayClassification ConvertClassification(
    safe_search_api::ClientClassification classification) {
  switch (classification) {
    case safe_search_api::ClientClassification::kAllowed:
      return ClassifyUrlResponse::ALLOWED;
    case safe_search_api::ClientClassification::kRestricted:
      return ClassifyUrlResponse::RESTRICTED;
    case safe_search_api::ClientClassification::kUnknown:
      return ClassifyUrlResponse::UNKNOWN_DISPLAY_CLASSIFICATION;
  }
}

// Build fake response proto with a response according to |classification|.
std::unique_ptr<ClassifyUrlResponse> BuildResponseProto(
    safe_search_api::ClientClassification classification) {
  auto response_proto = std::make_unique<ClassifyUrlResponse>();

  response_proto->set_display_classification(
      ConvertClassification(classification));
  return response_proto;
}

class KidsChromeManagementClientForTesting : public KidsChromeManagementClient {
 public:
  using KidsChromeManagementClient::KidsChromeManagementClient;

  void ClassifyURL(
      std::unique_ptr<kids_chrome_management::ClassifyUrlRequest> request_proto,
      KidsChromeManagementClient::KidsChromeManagementCallback callback)
      override {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback),
                                  std::move(response_proto_), error_code_));
  }

  void SetupResponse(std::unique_ptr<ClassifyUrlResponse> response_proto,
                     KidsChromeManagementClient::ErrorCode error_code) {
    response_proto_ = std::move(response_proto);
    error_code_ = error_code;
  }

 private:
  std::unique_ptr<ClassifyUrlResponse> response_proto_;
  KidsChromeManagementClient::ErrorCode error_code_;
};

}  // namespace

class KidsManagementURLCheckerClientTest : public testing::Test {
 public:
  KidsManagementURLCheckerClientTest() = default;

  KidsManagementURLCheckerClientTest(
      const KidsManagementURLCheckerClientTest&) = delete;
  KidsManagementURLCheckerClientTest& operator=(
      const KidsManagementURLCheckerClientTest&) = delete;

  void SetUp() override {
    test_kids_chrome_management_client_ =
        std::make_unique<KidsChromeManagementClientForTesting>(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_),
            identity_test_env_.identity_manager());
    url_classifier_ = std::make_unique<KidsManagementURLCheckerClient>(
        test_kids_chrome_management_client_.get(), "us");
  }

 protected:
  void SetupClientResponse(std::unique_ptr<ClassifyUrlResponse> response_proto,
                           KidsChromeManagementClient::ErrorCode error_code) {
    test_kids_chrome_management_client_->SetupResponse(
        std::move(response_proto), error_code);
  }

  // Asynchronously checks the URL and waits until finished.
  void CheckURL(const GURL& url) {
    StartCheckURL(url);
    task_environment_.RunUntilIdle();
  }

  // Starts a URL check, but doesn't wait for ClassifyURL() to finish.
  void CheckURLWithoutResponse(const GURL& url) { StartCheckURL(url); }

  MOCK_METHOD2(OnCheckDone,
               void(const GURL& url,
                    safe_search_api::ClientClassification classification));

  void DestroyURLClassifier() { url_classifier_.reset(); }

  base::test::TaskEnvironment task_environment_;

 private:
  void StartCheckURL(const GURL& url) {
    url_classifier_->CheckURL(
        url, base::BindOnce(&KidsManagementURLCheckerClientTest::OnCheckDone,
                            base::Unretained(this)));
  }

  network::TestURLLoaderFactory test_url_loader_factory_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::unique_ptr<KidsChromeManagementClientForTesting>
      test_kids_chrome_management_client_;
  std::unique_ptr<KidsManagementURLCheckerClient> url_classifier_;
};

TEST_F(KidsManagementURLCheckerClientTest, Simple) {
  {
    GURL url("http://randomurl1.com");

    safe_search_api::ClientClassification classification =
        safe_search_api::ClientClassification::kAllowed;

    EXPECT_CALL(*this, OnCheckDone(url, classification));

    SetupClientResponse(BuildResponseProto(classification),
                        KidsChromeManagementClient::ErrorCode::kSuccess);

    CheckURL(url);
  }
  {
    GURL url("http://randomurl2.com");

    safe_search_api::ClientClassification classification =
        safe_search_api::ClientClassification::kRestricted;

    EXPECT_CALL(*this, OnCheckDone(url, classification));

    SetupClientResponse(BuildResponseProto(classification),
                        KidsChromeManagementClient::ErrorCode::kSuccess);
    CheckURL(url);
  }
}

TEST_F(KidsManagementURLCheckerClientTest, AccessTokenError) {
  GURL url("http://randomurl3.com");

  safe_search_api::ClientClassification classification =
      safe_search_api::ClientClassification::kUnknown;

  SetupClientResponse(BuildResponseProto(classification),
                      KidsChromeManagementClient::ErrorCode::kTokenError);

  EXPECT_CALL(*this, OnCheckDone(url, classification));
  CheckURL(url);
}

TEST_F(KidsManagementURLCheckerClientTest, NetworkErrors) {
  {
    GURL url("http://randomurl4.com");

    safe_search_api::ClientClassification classification =
        safe_search_api::ClientClassification::kUnknown;

    SetupClientResponse(BuildResponseProto(classification),
                        KidsChromeManagementClient::ErrorCode::kNetworkError);

    EXPECT_CALL(*this, OnCheckDone(url, classification));

    CheckURL(url);
  }

  {
    GURL url("http://randomurl5.com");

    safe_search_api::ClientClassification classification =
        safe_search_api::ClientClassification::kUnknown;

    SetupClientResponse(BuildResponseProto(classification),
                        KidsChromeManagementClient::ErrorCode::kHttpError);

    EXPECT_CALL(*this, OnCheckDone(url, classification));

    CheckURL(url);
  }
}

TEST_F(KidsManagementURLCheckerClientTest, ServiceError) {
  GURL url("http://randomurl6.com");

  safe_search_api::ClientClassification classification =
      safe_search_api::ClientClassification::kUnknown;

  SetupClientResponse(BuildResponseProto(classification),
                      KidsChromeManagementClient::ErrorCode::kServiceError);

  EXPECT_CALL(*this, OnCheckDone(url, classification));
  CheckURL(url);
}

TEST_F(KidsManagementURLCheckerClientTest, DestroyClientBeforeCallback) {
  GURL url("http://randomurl7.com");

  EXPECT_CALL(*this, OnCheckDone(_, _)).Times(0);
  CheckURLWithoutResponse(url);

  DestroyURLClassifier();

  // Now run the callback.
  task_environment_.RunUntilIdle();
}
