// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.layouts;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.layouts.LayoutType;

/** Check that the current LayoutType is the expected one. */
public class LayoutTypeVisibleCondition extends Condition {
    private final Supplier<ChromeTabbedActivity> mActivitySupplier;
    private final @LayoutType int mExpectedLayoutType;

    public LayoutTypeVisibleCondition(
            Supplier<ChromeTabbedActivity> activitySupplier, @LayoutType int expectedLayoutType) {
        super(/* isRunOnUiThread= */ true);
        mActivitySupplier = dependOnSupplier(activitySupplier, "Activity");
        mExpectedLayoutType = expectedLayoutType;
    }

    @Override
    protected ConditionStatus checkWithSuppliers() throws Exception {
        Layout activeLayout = mActivitySupplier.get().getLayoutManager().getActiveLayout();
        if (activeLayout == null) {
            return whetherEquals(
                    mExpectedLayoutType,
                    LayoutType.NONE,
                    LayoutTypeVisibleCondition::getLayoutTypeName);
        }

        @LayoutType int layoutType = activeLayout.getLayoutType();
        boolean isStartingToHide = activeLayout.isStartingToHide();
        boolean isStartingToShow = activeLayout.isStartingToShow();
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
            case LayoutType.TAB_SWITCHER -> "TAB_SWITCHER";
            case LayoutType.TOOLBAR_SWIPE -> "TOOLBAR_SWIPE";
            case LayoutType.SIMPLE_ANIMATION -> "SIMPLE_ANIMATION";
            default -> "UNKNOWN";
        };
    }
}
