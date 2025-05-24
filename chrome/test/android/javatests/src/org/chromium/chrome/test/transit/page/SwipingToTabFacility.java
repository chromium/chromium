// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import org.chromium.base.test.transit.Facility;
import org.chromium.base.test.transit.Transition;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.test.transit.layouts.LayoutTypeVisibleCondition;

/** Represents the state while swiping the toolbar, moving between two tabs. */
public class SwipingToTabFacility extends Facility<PageStation> {
    private final Transition.Trigger mFinishDragTrigger;

    public SwipingToTabFacility(Transition.Trigger finishDragTrigger) {
        mFinishDragTrigger = finishDragTrigger;
    }

    @Override
    public void declareExtraElements() {
        declareEnterCondition(
                new LayoutTypeVisibleCondition(
                        mHostStation.getActivityElement(), LayoutType.TOOLBAR_SWIPE));
    }

    /** Finish the swipe to land at a {@link PageStation}. */
    public <T extends PageStation> T finishSwipe(PageStation.Builder<T> destinationBuilder) {
        T destination = destinationBuilder.initFrom(mHostStation).withIsSelectingTabs(1).build();
        return mHostStation.travelToSync(destination, mFinishDragTrigger);
    }
}
