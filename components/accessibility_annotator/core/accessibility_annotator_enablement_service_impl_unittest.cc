// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/accessibility_annotator/core/accessibility_annotator_enablement_service_impl.h"

#include "base/test/scoped_feature_list.h"
#include "components/accessibility_annotator/core/accessibility_annotator_features.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace accessibility_annotator {

class AccessibilityAnnotatorEnablementServiceImplTest : public testing::Test {
 public:
  AccessibilityAnnotatorEnablementServiceImplTest() = default;
  ~AccessibilityAnnotatorEnablementServiceImplTest() override = default;

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
  AccessibilityAnnotatorEnablementServiceImpl service_;
};

TEST_F(AccessibilityAnnotatorEnablementServiceImplTest,
       DisabledWhenFeaturesAreOff) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{},
      /*disabled_features=*/{features::kAccessibilityAnnotator,
                             features::kAccessibilityAnnotatorFirstRun,
                             features::kAccessibilityAnnotatorDatabaseStorage});

  EXPECT_EQ(service_.GetEnablementState(),
            RemoteAnnotatorEnablementState::kDisabledNotEligible);
}

TEST_F(AccessibilityAnnotatorEnablementServiceImplTest,
       DisabledWhenMainFeatureIsOff) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kAccessibilityAnnotatorFirstRun,
                            features::kAccessibilityAnnotatorDatabaseStorage},
      /*disabled_features=*/{features::kAccessibilityAnnotator});

  EXPECT_EQ(service_.GetEnablementState(),
            RemoteAnnotatorEnablementState::kDisabledNotEligible);
}

TEST_F(AccessibilityAnnotatorEnablementServiceImplTest,
       EnabledWhenAllFeaturesAreOn) {
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{features::kAccessibilityAnnotator,
                            features::kAccessibilityAnnotatorFirstRun,
                            features::kAccessibilityAnnotatorDatabaseStorage},
      /*disabled_features=*/{});

  // Current implementation returns kDisabledPendingInfo when eligible.
  EXPECT_EQ(service_.GetEnablementState(),
            RemoteAnnotatorEnablementState::kDisabledPendingInfo);
}

}  // namespace accessibility_annotator
