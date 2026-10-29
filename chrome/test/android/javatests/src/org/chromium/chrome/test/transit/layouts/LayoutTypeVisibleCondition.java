// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.layouts;

import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutType;

import java.util.function.Supplier;

/** Check that the current LayoutType is the expected one. */
public class LayoutTypeVisibleCondition extends Condition {
    private final @Nullable Supplier<ChromeTabbedActivity> mActivitySupplier;
    private final @Nullable LayoutManager mLayoutManager;
    private final @LayoutType int mExpectedLayoutType;

    public LayoutTypeVisibleCondition(
            Supplier<ChromeTabbedActivity> activitySupplier, @LayoutType int expectedLayoutType) {
        super(/* isRunOnUiThread= */ true);
        mActivitySupplier = dependOnSupplier(activitySupplier, "Activity");
        mLayoutManager = null;
        mExpectedLayoutType = expectedLayoutType;
    }

    /** For callers that hold a {@link LayoutManager} rather than the Activity. */
    public LayoutTypeVisibleCondition(
            LayoutManager layoutManager, @LayoutType int expectedLayoutType) {
        super(/* isRunOnUiThread= */ true);
        mActivitySupplier = null;
        mLayoutManager = layoutManager;
        mExpectedLayoutType = expectedLayoutType;
    }

    @Override
    protected ConditionStatus checkWithSuppliers() throws Exception {
        LayoutManager layoutManager =
                mLayoutManager != null
                        ? mLayoutManager
                        : mActivitySupplier.get().getLayoutManager();
        @LayoutType int layoutType = layoutManager.getActiveLayoutType();
        boolean isStartingToHide = layoutManager.isLayoutStartingToHide(layoutType);
        boolean isStartingToShow = layoutManager.isLayoutStartingToShow(layoutType);
        String expectedDescription = getLayoutTypeName(mExpectedLayoutType);
        String actualDescription = getLayoutTypeName(layoutType);
        if (isStartingToHide) {
            actualDescription += " (starting to hide)";
        }
        if (isStartingToShow) {
            actualDescription += " (starting to show)";
        }
        return whether(
                mExpectedLayoutType == layoutType && !isStartingToHide && !isStartingToShow,
                "Expected: %s; Actual: %s",
                expectedDescription,
                actualDescription);
    }

    @Override
    public String buildDescription() {
        return "LayoutType is " + getLayoutTypeName(mExpectedLayoutType);
    }

    private static String getLayoutTypeName(@LayoutType int layoutType) {
        return switch (layoutType) {
            case LayoutType.NONE -> "NONE";
            case LayoutType.BROWSING -> "BROWSING";
            case LayoutType.HUB -> "HUB";
            case LayoutType.TOOLBAR_SWIPE -> "TOOLBAR_SWIPE";
            case LayoutType.SIMPLE_ANIMATION -> "SIMPLE_ANIMATION";
            default -> "UNKNOWN";
        };
    }
}
