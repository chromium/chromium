// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/origin_gating_registration.h"

#include "base/test/task_environment.h"
#include "components/origin_gating/core/checker_id.h"
#include "components/origin_gating/core/origin_gating_checker.h"
#include "components/origin_gating/core/origin_gating_configuration.h"
#include "components/origin_gating/core/origin_gating_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace origin_gating {
namespace {

class MockDelegate : public OriginGatingChecker::Delegate {
 public:
  MockDelegate() = default;
  ~MockDelegate() override = default;

  MOCK_METHOD(void,
              DoesOriginRequireUserConfirmation,
              (GatingDecisionContext * context,
               GateableEvent event,
               const GURL& source,
               const GURL& destination,
               DoesOriginRequireUserConfirmationCallback callback),
              (const, override));
  MOCK_METHOD(void,
              EvaluateEnterprisePolicy,
              (const GURL& destination,
               EvaluateEnterprisePolicyCallback callback),
              (const, override));
  MOCK_METHOD(void,
              OnNoVerdict,
              (GatingDecisionContext * context,
               GateableEvent event,
               const GURL& source,
               const GURL& destination,
               bool requires_user_confirmation,
               base::OnceCallback<void(NoVerdictResult)> callback),
              (override));

  base::WeakPtr<MockDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<MockDelegate> weak_ptr_factory_{this};
};

class OriginGatingRegistrationTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<OriginGatingService> service_ =
      OriginGatingService::CreateForTesting();
};

TEST_F(OriginGatingRegistrationTest, UnregistersOnDestruction) {
  testing::NiceMock<MockDelegate> delegate;
  CheckerId id;
  {
    std::unique_ptr<OriginGatingRegistration> registration =
        service_->CreateAndRegisterChecker(
            delegate.GetWeakPtr(),
            OriginGatingConfiguration({}, /*use_site_keyed_cache=*/false));
    id = registration->id();
    EXPECT_FALSE(id.is_null());
    EXPECT_TRUE(service_->GetChecker(id));
  }
  EXPECT_FALSE(service_->GetChecker(id));
}

TEST_F(OriginGatingRegistrationTest, ResetUnregistersChecker) {
  testing::NiceMock<MockDelegate> delegate;
  std::unique_ptr<OriginGatingRegistration> registration =
      service_->CreateAndRegisterChecker(
          delegate.GetWeakPtr(),
          OriginGatingConfiguration({}, /*use_site_keyed_cache=*/false));
  CheckerId id = registration->id();
  EXPECT_FALSE(id.is_null());
  EXPECT_TRUE(service_->GetChecker(id));

  registration.reset();
  EXPECT_FALSE(service_->GetChecker(id));
}

TEST_F(OriginGatingRegistrationTest, MoveUniquePtrTransfersOwnership) {
  testing::NiceMock<MockDelegate> delegate;
  CheckerId id;
  {
    std::unique_ptr<OriginGatingRegistration> reg1 =
        service_->CreateAndRegisterChecker(
            delegate.GetWeakPtr(),
            OriginGatingConfiguration({}, /*use_site_keyed_cache=*/false));
    id = reg1->id();
    EXPECT_FALSE(id.is_null());
    EXPECT_TRUE(service_->GetChecker(id));

    std::unique_ptr<OriginGatingRegistration> reg2 = std::move(reg1);
    EXPECT_FALSE(reg1);
    ASSERT_TRUE(reg2);
    EXPECT_EQ(reg2->id(), id);
    EXPECT_TRUE(service_->GetChecker(id));
  }
  EXPECT_FALSE(service_->GetChecker(id));
}

TEST_F(OriginGatingRegistrationTest, ServiceGetter) {
  testing::NiceMock<MockDelegate> delegate;
  std::unique_ptr<OriginGatingRegistration> registration =
      service_->CreateAndRegisterChecker(
          delegate.GetWeakPtr(),
          OriginGatingConfiguration({}, /*use_site_keyed_cache=*/false));
  EXPECT_EQ(&registration->service(), service_.get());
}

}  // namespace
}  // namespace origin_gating
