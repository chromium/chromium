// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/accessibility_annotator_enablement_service_impl.h"

#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

class AccessibilityAnnotatorEnablementServiceImplTest : public testing::Test {
 public:
  AccessibilityAnnotatorEnablementServiceImplTest()
      : service_(nullptr, identity_test_env_.identity_manager()) {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kAccessibilityAnnotator,
                              features::kAccessibilityAnnotatorFirstRun,
                              features::kAccessibilityAnnotatorDatabaseStorage},
        /*disabled_features=*/{});

    SignIn("test@gmail.com");
  }
  ~AccessibilityAnnotatorEnablementServiceImplTest() override = default;

 protected:
  void SignIn(const std::string& email, bool is_underaged = false) {
    AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
        email, signin::ConsentLevel::kSignin);
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_use_model_execution_features(!is_underaged);
    identity_test_env_.UpdateAccountInfoForAccount(account_info);
  }

  base::test::TaskEnvironment task_environment_;
  signin::IdentityTestEnvironment identity_test_env_;
  base::test::ScopedFeatureList scoped_feature_list_;
  AccessibilityAnnotatorEnablementServiceImpl service_;
};

TEST_F(AccessibilityAnnotatorEnablementServiceImplTest,
       DisabledWhenFeaturesAreOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kAccessibilityAnnotator,
                             features::kAccessibilityAnnotatorFirstRun,
                             features::kAccessibilityAnnotatorDatabaseStorage});

  EXPECT_EQ(service_.GetEnablementState(),
            RemoteAnnotatorEnablementState::kDisabledNotEligible);
}

TEST_F(AccessibilityAnnotatorEnablementServiceImplTest,
       DisabledWhenMainFeatureIsOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitWithFeatures(
      /*enabled_features=*/{features::kAccessibilityAnnotatorFirstRun,
                            features::kAccessibilityAnnotatorDatabaseStorage},
      /*disabled_features=*/{features::kAccessibilityAnnotator});

  EXPECT_EQ(service_.GetEnablementState(),
            RemoteAnnotatorEnablementState::kDisabledNotEligible);
}

TEST_F(AccessibilityAnnotatorEnablementServiceImplTest,
       EnabledWhenAllFeaturesAreOn) {
  EXPECT_EQ(service_.GetEnablementState(),
            RemoteAnnotatorEnablementState::kEnabled);
}

#if !BUILDFLAG(IS_CHROMEOS)  // Signing out does not work on ChromeOS.
TEST_F(AccessibilityAnnotatorEnablementServiceImplTest, DisabledWhenSignedOut) {
  identity_test_env_.ClearPrimaryAccount();
  EXPECT_EQ(service_.GetEnablementState(),
            RemoteAnnotatorEnablementState::kDisabledNotEligible);
}
#endif  // !BUILDFLAG(IS_CHROMEOS)

TEST_F(AccessibilityAnnotatorEnablementServiceImplTest, DisabledWhenUnderaged) {
  SignIn("under@gmail.com", /*is_underaged=*/true);

  EXPECT_EQ(service_.GetEnablementState(),
            RemoteAnnotatorEnablementState::kDisabledNotEligible);
}

}  // namespace accessibility_annotator
