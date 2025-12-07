// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import android.annotation.SuppressLint;

import androidx.core.util.Function;

import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.test.transit.BatchedPublicTransitRule;
import org.chromium.base.test.transit.Station;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

/**
 * Rule for integration tests that reuse the same {@link ChromeTabbedActivity} throughout the batch.
 *
 * @param <HostStationT> The type of the home station.
 */
@NullMarked
public class ReusedCtaTransitTestRule<HostStationT extends Station<? extends ChromeTabbedActivity>>
        extends BaseCtaTransitTestRule implements TestRule {
    @SuppressLint("StaticFieldLeak")
    private static @Nullable ChromeTabbedActivity sActivity;

    private final Function<ChromeTabbedActivityTestRule, HostStationT> mEntryPointFunction;
    private final BatchedPublicTransitRule<HostStationT> mBatchedRule;
    private final RuleChain mChain;

    ReusedCtaTransitTestRule(
            Class<HostStationT> homeStationType,
            Function<ChromeTabbedActivityTestRule, HostStationT> entryPointFunction) {
        super();
        mEntryPointFunction = entryPointFunction;
        mBatchedRule = new BatchedPublicTransitRule<>(homeStationType, true);
        mActivityTestRule.setFinishActivity(false);
        mChain = RuleChain.outerRule(mActivityTestRule).around(mBatchedRule);
    }

    @Override
    public Statement apply(Statement statement, Description description) {
        return mChain.apply(statement, description);
    }

    /** Start the batched test in a specific home station. */
    public HostStationT start() {
        mActivityTestRule.setFinishActivity(false);
        HostStationT station = mBatchedRule.getHomeStation();
        if (station == null) {
            try {
                station = mEntryPointFunction.apply(mActivityTestRule);
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
            sActivity = mActivityTestRule.getActivity();
        } else {
            mActivityTestRule.setActivity(sActivity);
        }
        return station;
    }
}
