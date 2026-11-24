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
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider.ControlsPosition;

import java.util.function.Supplier;

/** Condition to check the position of the browser controls, top or bottom. */
@NullMarked
public class BrowserControlsPositionCondition<ActivityT extends ChromeActivity> extends Condition {
    private final Supplier<@Nullable ActivityT> mActivitySupplier;
    private final @ControlsPosition int mExpectedPosition;

    private BrowserControlsPositionCondition(
            Supplier<@Nullable ActivityT> activitySupplier,
            @ControlsPosition int expectedPosition) {
        super(/* isRunOnUiThread= */ true);
        mActivitySupplier = dependOnSupplier(activitySupplier, "Activity");
        mExpectedPosition = expectedPosition;
    }

    /** The controls are at |position|. */
    public static <ActivityT extends ChromeActivity> BrowserControlsPositionCondition<ActivityT> at(
            Supplier<@Nullable ActivityT> activitySupplier, @ControlsPosition int position) {
        return new BrowserControlsPositionCondition<>(activitySupplier, position);
    }

    @Override
    protected ConditionStatus checkWithSuppliers() {
        @ControlsPosition
        int position =
                assumeNonNull(mActivitySupplier.get())
                        .getBrowserControlsManager()
                        .getControlsPosition();
        return whetherEquals(
                mExpectedPosition, position, BrowserControlsPositionCondition::positionToString);
    }

    @Override
    public String buildDescription() {
        return "Browser controls at " + positionToString(mExpectedPosition);
    }

    static String positionToString(@ControlsPosition int position) {
        return position == ControlsPosition.BOTTOM ? "BOTTOM" : "TOP";
    }
}
