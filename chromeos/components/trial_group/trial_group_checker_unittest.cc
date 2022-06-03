// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/trial_group/trial_group_checker.h"

#include "content/public/test/browser_task_environment.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kTestURL[] = "/test";

}  // namespace

namespace chromeos {
namespace trial_group {

class TrialGroupCheckerTest : public testing::Test {
 public:
  TrialGroupCheckerTest()
      : task_environment_(content::BrowserTaskEnvironment::IO_MAINLOOP),
        test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &test_url_loader_factory_)) {}

  void Callback(bool is_member) { is_member_ = is_member; }

 protected:
  void SetUp() override {
    test_server_ = std::make_unique<net::test_server::EmbeddedTestServer>();
    ASSERT_TRUE(test_server_->Start());
  }

  std::unique_ptr<net::test_server::EmbeddedTestServer> test_server_;
  content::BrowserTaskEnvironment task_environment_;

  network::TestURLLoaderFactory test_url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;

  bool is_member_ = false;
  base::WeakPtrFactory<TrialGroupCheckerTest> weak_factory_{this};
};

TEST_F(TrialGroupCheckerTest, IsMemberTest) {
  test_url_loader_factory_.AddResponse(test_server_->GetURL(kTestURL).spec(),
                                       "{\"membership_info\": 1}");

  TrialGroupChecker checker(TrialGroupChecker::TESTING_GROUP);
  checker.SetServerUrl(test_server_->GetURL(kTestURL));
  TrialGroupChecker::Status status =
      checker.LookUpMembership(test_shared_loader_factory_,
                               base::BindOnce(&TrialGroupCheckerTest::Callback,
                                              weak_factory_.GetWeakPtr()));
  task_environment_.RunUntilIdle();
  ASSERT_EQ(TrialGroupChecker::OK, status);
  ASSERT_EQ(true, is_member_);
}

TEST_F(TrialGroupCheckerTest, NotMemberTest) {
  test_url_loader_factory_.AddResponse(test_server_->GetURL(kTestURL).spec(),
                                       "{\"membership_info\": 2}");

  is_member_ = true;
  TrialGroupChecker checker(TrialGroupChecker::TESTING_GROUP);
  checker.SetServerUrl(test_server_->GetURL(kTestURL));
  TrialGroupChecker::Status status =
      checker.LookUpMembership(test_shared_loader_factory_,
                               base::BindOnce(&TrialGroupCheckerTest::Callback,
                                              weak_factory_.GetWeakPtr()));
  task_environment_.RunUntilIdle();
  ASSERT_EQ(TrialGroupChecker::OK, status);
  ASSERT_EQ(false, is_member_);
}

TEST_F(TrialGroupCheckerTest, UnknownMemberTest) {
  test_url_loader_factory_.AddResponse(test_server_->GetURL(kTestURL).spec(),
                                       "{\"membership_info\": 0}");

  is_member_ = true;
  TrialGroupChecker checker(TrialGroupChecker::TESTING_GROUP);
  checker.SetServerUrl(test_server_->GetURL(kTestURL));
  TrialGroupChecker::Status status =
      checker.LookUpMembership(test_shared_loader_factory_,
                               base::BindOnce(&TrialGroupCheckerTest::Callback,
                                              weak_factory_.GetWeakPtr()));
  task_environment_.RunUntilIdle();
  ASSERT_EQ(TrialGroupChecker::OK, status);
  ASSERT_EQ(false, is_member_);
}

}  // namespace trial_group
}  // namespace chromeos
