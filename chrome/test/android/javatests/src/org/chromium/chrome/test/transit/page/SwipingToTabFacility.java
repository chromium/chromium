// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit.page;

import org.chromium.base.test.transit.Facility;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.test.transit.layouts.LayoutTypeVisibleCondition;

/** Represents the state while swiping the toolbar, moving between two tabs. */
public class SwipingToTabFacility extends Facility<CtaPageStation> {
    private final Runnable mFinishDragTrigger;

    public SwipingToTabFacility(Runnable finishDragTrigger) {
        mFinishDragTrigger = finishDragTrigger;
    }

    @Override
    public void declareExtraElements() {
        declareEnterCondition(
                new LayoutTypeVisibleCondition(
                        mHostStation.getActivityElement(), LayoutType.TOOLBAR_SWIPE));
    }

    /** Finish the swipe to land at a {@link CtaPageStation}. */
    public <T extends CtaPageStation> T finishSwipe(BasePageStation.Builder<T> destinationBuilder) {
        return runTo(mFinishDragTrigger)
                .arriveAt(
                        destinationBuilder
                                .initFrom(mHostStation)
                                .initSelectingExistingTab()
                                .build());
    }
}
