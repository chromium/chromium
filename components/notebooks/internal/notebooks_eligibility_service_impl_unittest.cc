// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/notebooks/internal/notebooks_eligibility_service_impl.h"

#include <memory>
#include <optional>

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "components/notebooks/public/features.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace notebooks {
namespace {

class TestObserver : public NotebooksEligibilityService::Observer {
 public:
  void OnNotebooksEligibilityChanged(bool eligible) override {
    last_eligibility_ = eligible;
    notification_count_++;
  }

  std::optional<bool> last_eligibility() const { return last_eligibility_; }
  int notification_count() const { return notification_count_; }

 private:
  std::optional<bool> last_eligibility_;
  int notification_count_ = 0;
};

class NotebooksEligibilityServiceImplTest : public testing::Test {
 public:
  NotebooksEligibilityServiceImplTest() {
    scoped_feature_list_.InitAndEnableFeature(features::kNotebooks);
  }
  ~NotebooksEligibilityServiceImplTest() override = default;

  signin::IdentityTestEnvironment* identity_test_env() {
    return &identity_test_env_;
  }

  void SignIn() {
    identity_test_env_.MakePrimaryAccountAvailable(
        "test@gmail.com", signin::ConsentLevel::kSignin);
  }

 private:
  base::test::TaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  signin::IdentityTestEnvironment identity_test_env_;
};

TEST_F(NotebooksEligibilityServiceImplTest,
       EligibleWhenSignedInAndFeatureEnabled) {
  SignIn();

  NotebooksEligibilityServiceImpl service(
      /*is_profile_eligible=*/true, identity_test_env()->identity_manager());
  EXPECT_TRUE(service.IsEligible());
  EXPECT_FALSE(service.IsEligibilityLoading());
}

TEST_F(NotebooksEligibilityServiceImplTest, IneligibleWhenSignedOut) {
  NotebooksEligibilityServiceImpl service(
      /*is_profile_eligible=*/true, identity_test_env()->identity_manager());
  EXPECT_FALSE(service.IsEligible());
}

TEST_F(NotebooksEligibilityServiceImplTest, IneligibleWhenFeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kNotebooks);
  SignIn();

  NotebooksEligibilityServiceImpl service(
      /*is_profile_eligible=*/true, identity_test_env()->identity_manager());
  EXPECT_FALSE(service.IsEligible());
}

TEST_F(NotebooksEligibilityServiceImplTest, IneligibleWhenProfileIneligible) {
  SignIn();

  NotebooksEligibilityServiceImpl service(
      /*is_profile_eligible=*/false, identity_test_env()->identity_manager());
  EXPECT_FALSE(service.IsEligible());
}

TEST_F(NotebooksEligibilityServiceImplTest,
       NotifiesObserversOnSignInStateChange) {
  NotebooksEligibilityServiceImpl service(
      /*is_profile_eligible=*/true, identity_test_env()->identity_manager());
  TestObserver observer;
  service.AddObserver(&observer);

  EXPECT_FALSE(service.IsEligible());

  // Sign in -> eligibility becomes true.
  SignIn();
  EXPECT_TRUE(service.IsEligible());
  EXPECT_EQ(observer.notification_count(), 1);
  EXPECT_EQ(observer.last_eligibility(), true);

#if !BUILDFLAG(IS_CHROMEOS)
  // Sign out -> eligibility becomes false (ClearPrimaryAccount is unsupported
  // on ChromeOS).
  identity_test_env()->ClearPrimaryAccount();
  EXPECT_FALSE(service.IsEligible());
  EXPECT_EQ(observer.notification_count(), 2);
  EXPECT_EQ(observer.last_eligibility(), false);
#endif

  service.RemoveObserver(&observer);
}

}  // namespace
}  // namespace notebooks
