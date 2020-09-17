// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/well_known_change_password_state.h"

#include "base/task/post_task.h"
#include "base/test/task_environment.h"
#include "components/password_manager/core/browser/well_known_change_password_util.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "services/network/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

namespace password_manager {

// To simulate different arrivals of the response codes, a delay for the
// response is added.
struct ResponseDelayParams {
  int change_password_delay;
  int not_exist_delay;
};

constexpr char kOrigin[] = "https://foo.bar";

class MockWellKnownChangePasswordStateDelegate
    : public WellKnownChangePasswordStateDelegate {
 public:
  MockWellKnownChangePasswordStateDelegate() = default;
  ~MockWellKnownChangePasswordStateDelegate() override = default;

  MOCK_METHOD(void, OnProcessingFinished, (bool), (override));
};

class WellKnownChangePasswordStateTest
    : public testing::Test,
      public testing::WithParamInterface<ResponseDelayParams> {
 public:
  WellKnownChangePasswordStateTest() {
    auto origin = url::Origin::Create(GURL(kOrigin));
    trusted_params_.isolation_info = net::IsolationInfo::CreatePartial(
        net::IsolationInfo::RedirectMode::kUpdateNothing,
        net::NetworkIsolationKey(origin, origin));
    state_.FetchNonExistingResource(
        test_shared_loader_factory_.get(), GURL(kOrigin),
        url::Origin::Create(GURL(kOrigin)), trusted_params_);
  }
  // Mocking and sending the response for the non_existing request with status
  // code |status| after a time delay |delay|.
  void RespondeToNonExistingRequest(net::HttpStatusCode status, int delay);
  // Mocking and setting the response for the change_password request with
  // status code |status| after a time delay |delay|.
  void RespondeToChangePasswordRequest(net::HttpStatusCode status, int delay);

  MockWellKnownChangePasswordStateDelegate* delegate() { return &delegate_; }

  // Wait until all PostTasks are processed.
  void FastForwardPostTasks() {
    task_environment_.FastForwardUntilNoTasksRemain();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  MockWellKnownChangePasswordStateDelegate delegate_;
  WellKnownChangePasswordState state_{&delegate_};
  network::ResourceRequest::TrustedParams trusted_params_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);
};

void WellKnownChangePasswordStateTest::RespondeToNonExistingRequest(
    net::HttpStatusCode status,
    int delay) {
  ASSERT_EQ(test_url_loader_factory_.NumPending(), 1);
  const network::ResourceRequest& request =
      test_url_loader_factory_.GetPendingRequest(0)->request;
  EXPECT_EQ(CreateWellKnownNonExistingResourceURL(GURL(kOrigin)), request.url);
  EXPECT_EQ(network::mojom::CredentialsMode::kOmit, request.credentials_mode);
  EXPECT_EQ(net::LOAD_DISABLE_CACHE, request.load_flags);
  EXPECT_EQ(url::Origin::Create(GURL(kOrigin)), request.request_initiator);
  EXPECT_TRUE(request.trusted_params->EqualsForTesting(trusted_params_));
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          [](net::HttpStatusCode status,
             network::TestURLLoaderFactory* factory) {
            factory->SimulateResponseForPendingRequest(
                CreateWellKnownNonExistingResourceURL(GURL(kOrigin)),
                network::URLLoaderCompletionStatus(net::OK),
                network::CreateURLResponseHead(status), "");
          },
          status, &test_url_loader_factory_),
      base::TimeDelta::FromMilliseconds(delay));
}

void WellKnownChangePasswordStateTest::RespondeToChangePasswordRequest(
    net::HttpStatusCode status,
    int delay) {
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(
          &WellKnownChangePasswordState::SetChangePasswordResponseCode,
          base::Unretained(&state_), status),
      base::TimeDelta::FromMilliseconds(delay));
}

TEST_P(WellKnownChangePasswordStateTest, Support_Ok) {
  ResponseDelayParams params = GetParam();

  EXPECT_CALL(*delegate(), OnProcessingFinished(true));

  RespondeToChangePasswordRequest(net::HTTP_OK, params.change_password_delay);
  RespondeToNonExistingRequest(net::HTTP_NOT_FOUND, params.not_exist_delay);
  FastForwardPostTasks();
}

TEST_P(WellKnownChangePasswordStateTest, Support_PartialContent) {
  ResponseDelayParams params = GetParam();

  EXPECT_CALL(*delegate(), OnProcessingFinished(true));

  RespondeToChangePasswordRequest(net::HTTP_PARTIAL_CONTENT,
                                  params.change_password_delay);
  RespondeToNonExistingRequest(net::HTTP_NOT_FOUND, params.not_exist_delay);
  FastForwardPostTasks();
}

TEST_P(WellKnownChangePasswordStateTest, NoSupport_NotFound) {
  ResponseDelayParams params = GetParam();

  EXPECT_CALL(*delegate(), OnProcessingFinished(false));

  RespondeToChangePasswordRequest(net::HTTP_NOT_FOUND,
                                  params.change_password_delay);
  RespondeToNonExistingRequest(net::HTTP_NOT_FOUND, params.not_exist_delay);
  FastForwardPostTasks();
}

TEST_P(WellKnownChangePasswordStateTest, NoSupport_Ok) {
  ResponseDelayParams params = GetParam();

  EXPECT_CALL(*delegate(), OnProcessingFinished(false));

  RespondeToChangePasswordRequest(net::HTTP_OK, params.change_password_delay);
  RespondeToNonExistingRequest(net::HTTP_OK, params.not_exist_delay);
  FastForwardPostTasks();
}

// Expect no support because the State should not handle redirects.
TEST_P(WellKnownChangePasswordStateTest, NoSupport_Redirect) {
  ResponseDelayParams params = GetParam();

  EXPECT_CALL(*delegate(), OnProcessingFinished(false));

  RespondeToChangePasswordRequest(net::HTTP_PERMANENT_REDIRECT,
                                  params.change_password_delay);
  RespondeToNonExistingRequest(net::HTTP_NOT_FOUND, params.not_exist_delay);
  FastForwardPostTasks();
}

constexpr ResponseDelayParams kDelayParams[] = {{0, 1}, {1, 0}};

INSTANTIATE_TEST_SUITE_P(All,
                         WellKnownChangePasswordStateTest,
                         ::testing::ValuesIn(kDelayParams));

}  // namespace password_manager
