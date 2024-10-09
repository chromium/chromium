// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/supervised_user/core/browser/remote_web_approvals_manager.h"

#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/prefs/testing_pref_service.h"
#include "components/safe_search_api/fake_url_checker_client.h"
#include "components/supervised_user/core/browser/permission_request_creator.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#include "components/supervised_user/core/browser/supervised_user_url_filter.h"
#include "components/supervised_user/core/browser/supervised_user_utils.h"
#include "components/supervised_user/test_support/supervised_user_url_filter_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

class AsyncResultHolder {
 public:
  AsyncResultHolder() = default;

  AsyncResultHolder(const AsyncResultHolder&) = delete;
  AsyncResultHolder& operator=(const AsyncResultHolder&) = delete;

  ~AsyncResultHolder() = default;

  bool GetResult() {
    run_loop_.Run();
    return result_;
  }

  void SetResult(bool result) {
    result_ = result;
    run_loop_.Quit();
  }

 private:
  base::RunLoop run_loop_;
  bool result_ = false;
};

// TODO(agawronska): Check if this can be a real mock.
// Mocks PermissionRequestCreator to test the async responses.
class MockPermissionRequestCreator
    : public supervised_user::PermissionRequestCreator {
 public:
  MockPermissionRequestCreator() = default;

  MockPermissionRequestCreator(const MockPermissionRequestCreator&) = delete;
  MockPermissionRequestCreator& operator=(const MockPermissionRequestCreator&) =
      delete;

  ~MockPermissionRequestCreator() override = default;

  void set_enabled(bool enabled) { enabled_ = enabled; }

  const std::vector<GURL>& requested_urls() const { return requested_urls_; }

  void AnswerRequest(size_t index, bool result) {
    ASSERT_LT(index, requested_urls_.size());
    std::move(callbacks_[index]).Run(result);
    callbacks_.erase(callbacks_.begin() + index);
    requested_urls_.erase(requested_urls_.begin() + index);
  }

 private:
  // PermissionRequestCreator:
  bool IsEnabled() const override { return enabled_; }

  void CreateURLAccessRequest(const GURL& url_requested,
                              SuccessCallback callback) override {
    ASSERT_TRUE(enabled_);
    requested_urls_.push_back(url_requested);
    callbacks_.push_back(std::move(callback));
  }

  bool enabled_ = false;
  std::vector<GURL> requested_urls_;
  std::vector<SuccessCallback> callbacks_;
};

}  // namespace

class RemoteWebApprovalsManagerTest : public ::testing::Test {
 protected:
  RemoteWebApprovalsManagerTest() {
    filter_.SetURLCheckerClient(
        std::make_unique<safe_search_api::FakeURLCheckerClient>());
  }

  RemoteWebApprovalsManagerTest(const RemoteWebApprovalsManagerTest&) = delete;
  RemoteWebApprovalsManagerTest& operator=(
      const RemoteWebApprovalsManagerTest&) = delete;

  ~RemoteWebApprovalsManagerTest() override = default;

  supervised_user::RemoteWebApprovalsManager& remote_web_approvals_manager() {
    return remote_web_approvals_manager_;
  }

  void RequestApproval(const GURL& url,
                       AsyncResultHolder* result_holder,
                       supervised_user::FilteringBehaviorReason reason =
                           supervised_user::FilteringBehaviorReason::DEFAULT) {
    supervised_user::UrlFormatter url_formatter(filter_, reason);
    remote_web_approvals_manager_.RequestApproval(
        url, url_formatter,
        base::BindOnce(&AsyncResultHolder::SetResult,
                       base::Unretained(result_holder)));
  }

  supervised_user::SupervisedUserURLFilter& filter() { return filter_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  TestingPrefServiceSimple pref_service_;
  supervised_user::SupervisedUserURLFilter filter_ =
      supervised_user::SupervisedUserURLFilter(
          pref_service_,
          std::make_unique<supervised_user::FakeURLFilterDelegate>());
  supervised_user::RemoteWebApprovalsManager remote_web_approvals_manager_;
};

TEST_F(RemoteWebApprovalsManagerTest, CreatePermissionRequest) {
  GURL url("http://www.example.com");
  GURL stripped_url("http://example.com");

  // Without any permission request creators, it should be disabled, and any
  // AddURLAccessRequest() calls should fail.
  EXPECT_FALSE(remote_web_approvals_manager().AreApprovalRequestsEnabled());
  {
    AsyncResultHolder result_holder;
    RequestApproval(url, &result_holder);
    EXPECT_FALSE(result_holder.GetResult());
  }

  // TODO(agawronska): Check if that can be eliminated by using a mock.
  // Add a disabled permission request creator. This should not change anything.
  MockPermissionRequestCreator* creator = new MockPermissionRequestCreator;
  remote_web_approvals_manager().AddApprovalRequestCreator(
      base::WrapUnique(creator));

  EXPECT_FALSE(remote_web_approvals_manager().AreApprovalRequestsEnabled());
  {
    AsyncResultHolder result_holder;
    RequestApproval(url, &result_holder);
    EXPECT_FALSE(result_holder.GetResult());
  }

  // Enable the permission request creator. This should enable permission
  // requests and queue them up.
  creator->set_enabled(true);
  EXPECT_TRUE(remote_web_approvals_manager().AreApprovalRequestsEnabled());
  {
    AsyncResultHolder result_holder;
    RequestApproval(url, &result_holder);
    ASSERT_EQ(1u, creator->requested_urls().size());
    EXPECT_EQ(stripped_url.spec(), creator->requested_urls()[0].spec());

    creator->AnswerRequest(0, true);
    EXPECT_TRUE(result_holder.GetResult());
  }

  {
    AsyncResultHolder result_holder;
    RequestApproval(url, &result_holder);
    ASSERT_EQ(1u, creator->requested_urls().size());
    EXPECT_EQ(stripped_url.spec(), creator->requested_urls()[0].spec());

    creator->AnswerRequest(0, false);
    EXPECT_FALSE(result_holder.GetResult());
  }

  // Add a second permission request creator.
  MockPermissionRequestCreator* creator_2 = new MockPermissionRequestCreator;
  creator_2->set_enabled(true);
  remote_web_approvals_manager().AddApprovalRequestCreator(
      base::WrapUnique(creator_2));

  // Add an entry in for the test url in the blocklist in order to get the
  // unstriped url in the approval.
  std::map<std::string, bool> url_map;
  url_map.emplace(url.host(), false);
  filter().SetManualHosts(url_map);
  {
    AsyncResultHolder result_holder;
    RequestApproval(url, &result_holder,
                    supervised_user::FilteringBehaviorReason::MANUAL);
    ASSERT_EQ(1u, creator->requested_urls().size());
    EXPECT_EQ(url.spec(), creator->requested_urls()[0].spec());

    // Make the first creator succeed. This should make the whole thing succeed.
    creator->AnswerRequest(0, true);
    EXPECT_TRUE(result_holder.GetResult());
  }

  {
    AsyncResultHolder result_holder;
    RequestApproval(url, &result_holder,
                    supervised_user::FilteringBehaviorReason::MANUAL);
    ASSERT_EQ(1u, creator->requested_urls().size());
    EXPECT_EQ(url.spec(), creator->requested_urls()[0].spec());

    // Make the first creator fail. This should fall back to the second one.
    creator->AnswerRequest(0, false);
    ASSERT_EQ(1u, creator_2->requested_urls().size());
    EXPECT_EQ(url.spec(), creator_2->requested_urls()[0].spec());

    // Make the second creator succeed, which will make the whole thing succeed.
    creator_2->AnswerRequest(0, true);
    EXPECT_TRUE(result_holder.GetResult());
  }
}
