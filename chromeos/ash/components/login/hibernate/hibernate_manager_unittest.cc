// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/login/hibernate/hibernate_manager.h"

#include "base/bind.h"
#include "chromeos/ash/components/dbus/hiberman/fake_hiberman_client.h"
#include "chromeos/ash/components/login/auth/public/auth_callbacks.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class HibernateManagerTest : public testing::Test {
 public:
  HibernateManagerTest() {
    HibermanClient::InitializeFake();
    hiberman_client_ = FakeHibermanClient::Get();
    user_context_ = std::make_unique<UserContext>();
    user_context_->SetAccountId(
        AccountId::FromUserEmail("fake_email@gmail.com"));
  }

  HibernateManagerTest(const HibernateManagerTest&) = delete;
  HibernateManagerTest& operator=(const HibernateManagerTest&) = delete;

  ~HibernateManagerTest() override { HibermanClient::Shutdown(); }

  void ResumeCallback(std::unique_ptr<UserContext> user_context,
                      bool resume_call_success) {
    if (resume_call_success) {
      successful_callbacks_++;
    } else {
      failed_callbacks_++;
    }
  }

  void ResumeAuthOpCallback(std::unique_ptr<UserContext> user_context,
                            absl::optional<AuthenticationError> error) {
    if (error == absl::nullopt) {
      successful_callbacks_++;
    } else {
      failed_callbacks_++;
    }
  }

 protected:
  FakeHibermanClient* hiberman_client_;
  HibernateManager hibernate_manager_;
  std::unique_ptr<UserContext> user_context_;
  int successful_callbacks_ = 0;
  int failed_callbacks_ = 0;

  base::WeakPtrFactory<HibernateManagerTest> weak_factory_{this};
};

// Test that the callback gets called.
TEST_F(HibernateManagerTest, BasicResumeCall) {
  hibernate_manager_.PrepareHibernateAndMaybeResume(
      std::move(user_context_),
      base::BindOnce(&HibernateManagerTest::ResumeCallback,
                     weak_factory_.GetWeakPtr()));

  EXPECT_EQ(successful_callbacks_, 1);
  EXPECT_EQ(failed_callbacks_, 0);
}

// Test that ResumeFromHiberateAsAuthOp calls its callback.
TEST_F(HibernateManagerTest, BasicAuthOpResumeCall) {
  hibernate_manager_.PrepareHibernateAndMaybeResumeAuthOp(
      std::move(user_context_),
      base::BindOnce(&HibernateManagerTest::ResumeAuthOpCallback,
                     weak_factory_.GetWeakPtr()));

  EXPECT_EQ(successful_callbacks_, 1);
  EXPECT_EQ(failed_callbacks_, 0);
}

}  // namespace ash
