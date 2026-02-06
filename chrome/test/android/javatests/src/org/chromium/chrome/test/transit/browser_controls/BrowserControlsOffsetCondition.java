// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.browser_controls;

import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;

import java.util.function.Supplier;

/** Condition to check the physical offset of browser controls (shown or hidden). */
public class BrowserControlsOffsetCondition<ActivityT extends ChromeActivity> extends Condition {
    private final Supplier<ActivityT> mActivitySupplier;
    private final boolean mExpectPhysicallyShown;
    private final boolean mHeightSynced;

    BrowserControlsOffsetCondition(
            Supplier<ActivityT> activitySupplier,
            boolean expectPhysicallyShown,
            boolean heightSynced) {
        super(/* isRunOnUiThread= */ true);
        mActivitySupplier = dependOnSupplier(activitySupplier, "Activity");
        mExpectPhysicallyShown = expectPhysicallyShown;
        mHeightSynced = heightSynced;
    }

    public static <ActivityT extends ChromeActivity>
            BrowserControlsOffsetCondition<ActivityT> showAndScrollable(
                    Supplier<ActivityT> activitySupplier) {
        return new BrowserControlsOffsetCondition<>(activitySupplier, true, false);
    }

    public static <ActivityT extends ChromeActivity>
            BrowserControlsOffsetCondition<ActivityT> showAndHeightSynced(
                    Supplier<ActivityT> activitySupplier) {
        return new BrowserControlsOffsetCondition<>(activitySupplier, true, true);
    }

    public static <ActivityT extends ChromeActivity>
            BrowserControlsOffsetCondition<ActivityT> scrolledOff(
                    Supplier<ActivityT> activitySupplier) {
        return new BrowserControlsOffsetCondition<>(activitySupplier, false, false);
    }

    @Override
    protected ConditionStatus checkWithSuppliers() {
        BrowserControlsStateProvider provider = mActivitySupplier.get().getBrowserControlsManager();
        int topControlsHeight = provider.getTopControlsHeight();
        int topControlsMinHeight = provider.getTopControlsMinHeight();
        int contentOffset = provider.getContentOffset();

        if (mHeightSynced) {
            if (topControlsHeight != topControlsMinHeight) {
                return notFulfilled(
                        "Expected controls height and minHeight to be in sync."
                                + " (height: %d) (minHeight: %d)",
                        topControlsHeight, topControlsMinHeight);
            }
        }

        if (mExpectPhysicallyShown) {
            if (contentOffset != topControlsHeight) {
                return notFulfilled(
                        "Expected controls to be fully shown (content offset %d), but got %d"
                                + " (height: %d)",
                        topControlsHeight, contentOffset, topControlsHeight);
            }
        } else {
            if (contentOffset != topControlsMinHeight) {
                return notFulfilled(
                        "Expected controls to be fully hidden (content offset %d), but got %d"
                                + " (min height: %d)",
                        topControlsMinHeight, contentOffset, topControlsMinHeight);
            }
        }

        return fulfilled(
                "Content Offset: %d, Height: %d, MinHeight: %d",
                contentOffset, topControlsHeight, topControlsMinHeight);
    }

    @Override
    public String buildDescription() {
        return "Browser controls physically " + (mExpectPhysicallyShown ? "shown" : "hidden");
    }
}
