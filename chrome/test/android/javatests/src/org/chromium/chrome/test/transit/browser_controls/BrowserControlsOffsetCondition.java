// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.browser_controls;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;

import java.util.function.Supplier;

/**
 * Condition to check the physical offset of browser controls (shown or hidden).
 *
 * <p>Checks the controls at their current {@link ControlsPosition}, top or bottom. Use {@link
 * BrowserControlsPositionCondition} to also expect a position.
 */
@NullMarked
public class BrowserControlsOffsetCondition<ActivityT extends ChromeActivity> extends Condition {
    private final Supplier<@Nullable ActivityT> mActivitySupplier;
    private final boolean mExpectPhysicallyShown;
    private final boolean mHeightSynced;

    BrowserControlsOffsetCondition(
            Supplier<@Nullable ActivityT> activitySupplier,
            boolean expectPhysicallyShown,
            boolean heightSynced) {
        super(/* isRunOnUiThread= */ true);
        mActivitySupplier = dependOnSupplier(activitySupplier, "Activity");
        mExpectPhysicallyShown = expectPhysicallyShown;
        mHeightSynced = heightSynced;
    }

    /** The controls are fully shown. */
    public static <ActivityT extends ChromeActivity>
            BrowserControlsOffsetCondition<ActivityT> shown(
                    Supplier<@Nullable ActivityT> activitySupplier) {
        return new BrowserControlsOffsetCondition<>(activitySupplier, true, false);
    }

    /** The controls are fully shown and can't scroll off (min height equals height). */
    public static <ActivityT extends ChromeActivity>
            BrowserControlsOffsetCondition<ActivityT> shownAndHeightSynced(
                    Supplier<@Nullable ActivityT> activitySupplier) {
        return new BrowserControlsOffsetCondition<>(activitySupplier, true, true);
    }

    /** The controls are scrolled off down to their min height. */
    public static <ActivityT extends ChromeActivity>
            BrowserControlsOffsetCondition<ActivityT> scrolledOff(
                    Supplier<@Nullable ActivityT> activitySupplier) {
        return new BrowserControlsOffsetCondition<>(activitySupplier, false, false);
    }

    @Override
    protected ConditionStatus checkWithSuppliers() {
        BrowserControlsStateProvider provider =
                assumeNonNull(mActivitySupplier.get()).getBrowserControlsManager();
        @ControlsPosition int position = provider.getControlsPosition();
        boolean isBottom = position == ControlsPosition.BOTTOM;
        int controlsHeight =
                isBottom ? provider.getBottomControlsHeight() : provider.getTopControlsHeight();
        int controlsMinHeight =
                isBottom
                        ? provider.getBottomControlsMinHeight()
                        : provider.getTopControlsMinHeight();
        int contentOffset =
                isBottom
                        ? BrowserControlsUtils.getBottomContentOffset(provider)
                        : provider.getContentOffset();

        if (mHeightSynced) {
            if (controlsHeight != controlsMinHeight) {
                return notFulfilled(
                        "Expected controls height and minHeight to be in sync."
                                + " (height: %d) (minHeight: %d)",
                        controlsHeight, controlsMinHeight);
            }
        }

        if (mExpectPhysicallyShown) {
            // The offsets are clamped to 0 when the height is 0, so the check below would pass.
            if (controlsHeight <= 0) {
                return notFulfilled("Expected controls height > 0, but got %d", controlsHeight);
            }
            if (contentOffset != controlsHeight) {
                return notFulfilled(
                        "Expected controls to be fully shown (content offset %d), but got %d"
                                + " (height: %d)",
                        controlsHeight, contentOffset, controlsHeight);
            }
        } else {
            if (contentOffset != controlsMinHeight) {
                return notFulfilled(
                        "Expected controls to be fully hidden (content offset %d), but got %d"
                                + " (min height: %d)",
                        controlsMinHeight, contentOffset, controlsMinHeight);
            }
        }

        return fulfilled(
                "Position: %s, Content Offset: %d, Height: %d, MinHeight: %d",
                BrowserControlsPositionCondition.positionToString(position),
                contentOffset,
                controlsHeight,
                controlsMinHeight);
    }

    @Override
    public String buildDescription() {
        return "Browser controls physically "
                + (mExpectPhysicallyShown ? "shown" : "hidden")
                + (mHeightSynced ? ", height synced" : "");
    }
}
