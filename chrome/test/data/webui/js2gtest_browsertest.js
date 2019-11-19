// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

GEN(`
#include "base/metrics/field_trial_params.h"

const base::Feature kTestFeature{"TestFeature",
    base::FEATURE_DISABLED_BY_DEFAULT};
const base::Feature kTestFeatureWithParam{"TestFeatureWithParam",
    base::FEATURE_DISABLED_BY_DEFAULT};
const base::FeatureParam<int> kTestFeatureWithParamCount{
    &kTestFeatureWithParam, "count", 5};
`);

/**
 * @constructor
 * @extends {testing.Test}
 */
function JSToGtestBrowserTest() {}

JSToGtestBrowserTest.prototype = {
  __proto__: testing.Test.prototype,

  browsePreload: 'chrome://dummyurl',

  /** @override */
  testGenPostamble() {
    GEN(`
  EXPECT_TRUE(base::FeatureList::IsEnabled(kTestFeature));
  EXPECT_TRUE(base::FeatureList::IsEnabled(kTestFeatureWithParam));
  EXPECT_EQ(5,
      base::GetFieldTrialParamByFeatureAsInt(kTestFeatureWithParam,
      kTestFeatureWithParamCount.name, 0));`);
  },

  /** @override */
  featureList: {enabled: ['kTestFeature']},

  /** @override */
  featuresWithParameters: [
    {
      featureName: 'kTestFeatureWithParam',
      parameters: [{name: 'count', value: 5}],
    },
  ],
};

TEST_F('JSToGtestBrowserTest', 'TestFeatureEnabling', function() {
  // Just to be generated.
});
