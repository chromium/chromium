// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.browser_controls;

import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.tab.Tab;

import java.util.function.Supplier;

/** Condition to check the constraints state of browser controls. */
public class BrowserControlsStateCondition<ActivityT extends ChromeActivity> extends Condition {
    private final Supplier<ActivityT> mActivitySupplier;
    private final @BrowserControlsState int mExpectedConstraints;

    BrowserControlsStateCondition(
            Supplier<ActivityT> activitySupplier,
            Supplier<Tab> tabSupplier,
            @BrowserControlsState int expectedConstraints) {
        super(/* isRunOnUiThread= */ true);
        mActivitySupplier = dependOnSupplier(activitySupplier, "ChromeActivity");
        // Wait until a tab that's available so BrowserControlsState considers the tab state.
        dependOnSupplier(tabSupplier, "Tab");
        mExpectedConstraints = expectedConstraints;
    }

    public static <ActivityT extends ChromeActivity> BrowserControlsStateCondition<ActivityT> shown(
            Supplier<ActivityT> activitySupplier, Supplier<Tab> tabSupplier) {
        return new BrowserControlsStateCondition<>(
                activitySupplier, tabSupplier, BrowserControlsState.SHOWN);
    }

    public static <ActivityT extends ChromeActivity>
            BrowserControlsStateCondition<ActivityT> hidden(
                    Supplier<ActivityT> activitySupplier, Supplier<Tab> tabSupplier) {
        return new BrowserControlsStateCondition<>(
                activitySupplier, tabSupplier, BrowserControlsState.HIDDEN);
    }

    public static <ActivityT extends ChromeActivity> BrowserControlsStateCondition<ActivityT> both(
            Supplier<ActivityT> activitySupplier, Supplier<Tab> tabSupplier) {
        return new BrowserControlsStateCondition<>(
                activitySupplier, tabSupplier, BrowserControlsState.BOTH);
    }

    @Override
    protected ConditionStatus checkWithSuppliers() {
        int currentConstraints =
                mActivitySupplier
                        .get()
                        .getRootUiCoordinatorForTesting()
                        .getAppBrowserControlsVisibilityDelegate()
                        .get();

        return whetherEquals(mExpectedConstraints, currentConstraints, this::constraintsToString);
    }

    private String constraintsToString(@BrowserControlsState int constraints) {
        return switch (constraints) {
            case BrowserControlsState.SHOWN -> "SHOWN";
            case BrowserControlsState.HIDDEN -> "HIDDEN";
            case BrowserControlsState.BOTH -> "BOTH";
            default -> "UNKNOWN (" + constraints + ")";
        };
    }

    @Override
    public String buildDescription() {
        return "Browser controls constraints is " + constraintsToString(mExpectedConstraints);
    }
}
