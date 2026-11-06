// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.tab_strip;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.test.transit.ConditionStatusWithResult;
import org.chromium.base.test.transit.ConditionWithResult;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome;
import org.chromium.chrome.browser.compositor.overlays.strip.StripLayoutHelperManager;

import java.util.function.Supplier;

/**
 * Fulfilled when the ChromeTabbedActivity has a {@link StripLayoutHelperManager}, i.e. a horizontal
 * tab strip. Only tablets and desktop have one.
 */
@NullMarked
public class StripLayoutHelperManagerCondition
        extends ConditionWithResult<StripLayoutHelperManager> {
    private final Supplier<@Nullable ChromeTabbedActivity> mActivitySupplier;

    public StripLayoutHelperManagerCondition(
            Supplier<@Nullable ChromeTabbedActivity> activitySupplier) {
        super(/* isRunOnUiThread= */ true);
        mActivitySupplier = dependOnSupplier(activitySupplier, "ChromeTabbedActivity");
    }

    @Override
    protected ConditionStatusWithResult<StripLayoutHelperManager> resolveWithSuppliers() {
        LayoutManagerChrome layoutManager =
                assumeNonNull(mActivitySupplier.get()).getLayoutManager();
        if (layoutManager == null) {
            return awaiting("No LayoutManager").withoutResult();
        }
        StripLayoutHelperManager manager = layoutManager.getStripLayoutHelperManager();
        if (manager == null) {
            return notFulfilled("No StripLayoutHelperManager").withoutResult();
        }
        return fulfilled().withResult(manager);
    }

    @Override
    public String buildDescription() {
        return "StripLayoutHelperManager exists";
    }
}
