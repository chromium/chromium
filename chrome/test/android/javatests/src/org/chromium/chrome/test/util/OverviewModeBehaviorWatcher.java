// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import org.hamcrest.Matchers;

import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.compositor.layouts.EmptyOverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior.OverviewModeObserver;

/**
 * Checks and waits for certain overview mode events to happen.  Can be used to block test threads
 * until certain overview mode state criteria are met.
 */
public class OverviewModeBehaviorWatcher {
    private final OverviewModeBehavior mOverviewModeBehavior;
    private final OverviewModeObserver mOverviewModeObserver;
    private boolean mWaitingForShow;
    private boolean mWaitingForHide;

    /**
     * Creates an instance of an {@link OverviewModeBehaviorWatcher}.  Note that at this point
     * it will be registered for overview mode events.
     *
     * @param behavior          The {@link OverviewModeBehavior} to watch.
     * @param waitForShow Whether or not to wait for overview mode to finish showing.
     * @param waitForHide Whether or not to wait for overview mode to finish hiding.
     */
    public OverviewModeBehaviorWatcher(OverviewModeBehavior behavior, boolean waitForShow,
            boolean waitForHide) {
        mOverviewModeBehavior = behavior;
        mOverviewModeObserver = new EmptyOverviewModeObserver() {
            @Override
            public void onOverviewModeFinishedShowing() {
                mWaitingForShow = false;
            }

            @Override
            public void onOverviewModeFinishedHiding() {
                mWaitingForHide = false;
            }
        };

        mOverviewModeBehavior.addOverviewModeObserver(mOverviewModeObserver);

        mWaitingForShow = waitForShow;
        mWaitingForHide = waitForHide;
    }

    /**
     * Blocks until all of the expected events have occurred.  Once the events of this class
     * are met it will always return immediately from {@link #waitForBehavior()}.
     */
    public void waitForBehavior() {
        try {
            CriteriaHelper.pollUiThread(() -> {
                Criteria.checkThat(
                        "OverviewModeObserver#onOverviewModeFinishedShowing() not called.",
                        mWaitingForShow, Matchers.is(false));
                Criteria.checkThat(
                        "OverviewModeObserver#onOverviewModeFinishedHiding() not called.",
                        mWaitingForHide, Matchers.is(false));
            });
        } finally {
            mOverviewModeBehavior.removeOverviewModeObserver(mOverviewModeObserver);
        }
    }
}
