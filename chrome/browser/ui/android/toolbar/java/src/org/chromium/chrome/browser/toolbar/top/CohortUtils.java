// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.feature_engagement.Tracker;

/** Utility class for convenience functions for working with cohort groups. */
public final class CohortUtils {
    @VisibleForTesting
    static final String SYNTHETIC_TRIAL_SUFFIX = "_Synthetic";

    /** Private constructor to avoid instantiation. */
    private CohortUtils() {}

    /**
     * Tags the cohort group with the parameterized feature name if the given feature has been
     * triggered from the feature engagement component before. Note that while the cohort feature
     * can be remotely supplied by the main feature/experiment, it still needs to be locally
     * defined for the lookup to work.
     * @param tracker Feature engagement interface to check triggered state.
     * @param featureName The name of the primary feature that is showing/triggering.
     * @param cohortFeatureParam The name of the param that holds the cohort feature name.
     */
    public static void tagCohortGroupIfTriggered(
            @NonNull Tracker tracker, String featureName, String cohortFeatureParam) {
        if (ChromeFeatureList.isEnabled(featureName)) {
            if (!tracker.hasEverTriggered(featureName, false)) {
                return;
            }

            String cohortFeatureName =
                    ChromeFeatureList.getFieldTrialParamByFeature(featureName, cohortFeatureParam);
            if (TextUtils.isEmpty(cohortFeatureName)) {
                return;
            }

            // Because the cohort group does not start active, and is only queried here, this check
            // check will actually assign membership. This allows easier analysis of metrics for
            // only the cohort of users who were shown the IPH.
            ChromeFeatureList.isEnabled(cohortFeatureName);
        }
    }
}
