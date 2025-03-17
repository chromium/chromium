// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.layouts;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.ConditionStatus;
import org.chromium.chrome.browser.ChromeTabbedActivity;
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
        // TODO(crbug.com/394600771): Also check that the layout is not in transition, like
        // {@link HubLayoutNotInTransition} does.
        @LayoutType
        int layoutType = mActivitySupplier.get().getLayoutManager().getActiveLayoutType();
        return whetherEquals(
                mExpectedLayoutType, layoutType, LayoutTypeVisibleCondition::getLayoutTypeName);
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
