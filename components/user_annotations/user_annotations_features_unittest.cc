// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/user_annotations/user_annotations_features.h"

#include "base/test/scoped_feature_list.h"
#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_features.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace user_annotations {

namespace {

using ::testing::UnorderedElementsAre;

TEST(UserAnnotationsFeaturesTest, GetAllowedHostsForFormsAnnotations) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      autofill_prediction_improvements::kAutofillPredictionImprovements,
      {{"allowed_hosts_for_form_submissions", "example.com,otherhost.com"}});
  EXPECT_THAT(GetAllowedHostsForFormsAnnotations(),
              UnorderedElementsAre("example.com", "otherhost.com"));
}

}  // namespace
}  // namespace user_annotations
