// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/origin_gating/core/origin_gating_service.h"

#include <memory>
#include <utility>

#include "base/functional/callback.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "components/origin_gating/core/checker_id.h"
#include "components/origin_gating/core/origin_gating_checker.h"
#include "components/origin_gating/core/origin_gating_configuration.h"
#include "components/origin_gating/core/origin_gating_registration.h"
#include "components/origin_gating/core/types.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

using ::testing::_;
using ::testing::NiceMock;

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

class OriginGatingServiceTest : public ::testing::Test {
 protected:
  base::test::TaskEnvironment task_environment_;
  std::unique_ptr<OriginGatingService> service_ =
      OriginGatingService::CreateForTesting();
};

TEST_F(OriginGatingServiceTest, CreateAndRegisterChecker) {
  NiceMock<MockDelegate> delegate;
  std::unique_ptr<OriginGatingRegistration> reg1 =
      service_->CreateAndRegisterChecker(
          delegate.GetWeakPtr(),
          OriginGatingConfiguration({}, /*use_site_keyed_cache=*/false));
  CheckerId id1 = reg1->id();
  EXPECT_FALSE(id1.is_null());

  std::unique_ptr<OriginGatingRegistration> reg2 =
      service_->CreateAndRegisterChecker(
          delegate.GetWeakPtr(),
          OriginGatingConfiguration({}, /*use_site_keyed_cache=*/false));
  CheckerId id2 = reg2->id();
  EXPECT_FALSE(id2.is_null());
  EXPECT_NE(id1, id2);

  OriginGatingChecker* checker1 = service_->GetChecker(id1);
  ASSERT_TRUE(checker1);

  OriginGatingChecker* checker2 = service_->GetChecker(id2);
  ASSERT_TRUE(checker2);
  EXPECT_NE(checker1, checker2);
}

TEST_F(OriginGatingServiceTest, GetChecker_InvalidOrNullId) {
  EXPECT_FALSE(service_->GetChecker(CheckerId()));
  EXPECT_FALSE(service_->GetChecker(CheckerId::FromUnsafeValue(999)));
}

TEST_F(OriginGatingServiceTest, Shutdown) {
  NiceMock<MockDelegate> delegate;
  std::unique_ptr<OriginGatingRegistration> reg1 =
      service_->CreateAndRegisterChecker(
          delegate.GetWeakPtr(),
          OriginGatingConfiguration({}, /*use_site_keyed_cache=*/false));
  std::unique_ptr<OriginGatingRegistration> reg2 =
      service_->CreateAndRegisterChecker(
          delegate.GetWeakPtr(),
          OriginGatingConfiguration({}, /*use_site_keyed_cache=*/false));
  CheckerId id1 = reg1->id();
  CheckerId id2 = reg2->id();

  ASSERT_TRUE(service_->GetChecker(id1));
  ASSERT_TRUE(service_->GetChecker(id2));

  service_->Shutdown();

  EXPECT_FALSE(service_->GetChecker(id1));
  EXPECT_FALSE(service_->GetChecker(id2));
}

TEST_F(OriginGatingServiceTest, RegistrationUnregistersOnDestruction) {
  NiceMock<MockDelegate> delegate;
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

}  // namespace
}  // namespace origin_gating
