// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.layouts;

import static org.junit.Assert.assertEquals;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;

/** Tests for the {@link FilterLayoutStateObserver}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class FilterLayoutStateObserverTest {
    private final CallbackHelper mStartedShowingCallbackHelper = new CallbackHelper();
    private final CallbackHelper mStartedHidingCallbackHelper = new CallbackHelper();
    private final CallbackHelper mFinishedShowingCallbackHelper = new CallbackHelper();
    private final CallbackHelper mFinishedHidingCallbackHelper = new CallbackHelper();
    private final CallbackHelper mSelectionHintedCallbackHelper = new CallbackHelper();

    private LayoutStateObserver mBaseObserver;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mBaseObserver = new LayoutStateObserver() {
            @Override
            public void onStartedShowing(int layoutType, boolean showToolbar) {
                mStartedShowingCallbackHelper.notifyCalled();
            }

            @Override
            public void onFinishedShowing(int layoutType) {
                mFinishedShowingCallbackHelper.notifyCalled();
            }

            @Override
            public void onStartedHiding(
                    int layoutType, boolean showToolbar, boolean delayAnimation) {
                mStartedHidingCallbackHelper.notifyCalled();
            }

            @Override
            public void onFinishedHiding(int layoutType) {
                mFinishedHidingCallbackHelper.notifyCalled();
            }

            @Override
            public void onTabSelectionHinted(int tabId) {
                mSelectionHintedCallbackHelper.notifyCalled();
            }
        };
    }

    @Test
    public void testSingleLayoutType() {
        FilterLayoutStateObserver observer =
                new FilterLayoutStateObserver(LayoutType.BROWSING, mBaseObserver);

        int initialCount = mStartedShowingCallbackHelper.getCallCount();
        assertEquals("Event should not have triggered.", initialCount,
                mStartedShowingCallbackHelper.getCallCount());
        observer.onStartedShowing(LayoutType.TAB_SWITCHER, false);
        assertEquals("Event should not have triggered with the specified layout.", initialCount,
                mStartedShowingCallbackHelper.getCallCount());
        observer.onStartedShowing(LayoutType.BROWSING, false);
        assertEquals("Event should have triggered with the specified layout.", initialCount + 1,
                mStartedShowingCallbackHelper.getCallCount());

        initialCount = mFinishedShowingCallbackHelper.getCallCount();
        assertEquals("Event should not have triggered.", initialCount,
                mFinishedShowingCallbackHelper.getCallCount());
        observer.onFinishedShowing(LayoutType.SIMPLE_ANIMATION);
        assertEquals("Event should not have triggered with the specified layout.", initialCount,
                mFinishedShowingCallbackHelper.getCallCount());
        observer.onFinishedShowing(LayoutType.BROWSING);
        assertEquals("Event should have triggered with the specified layout.", initialCount + 1,
                mFinishedShowingCallbackHelper.getCallCount());
    }

    @Test
    public void testOnTabSelectionHintedAlwaysTriggers() {
        FilterLayoutStateObserver observer =
                new FilterLayoutStateObserver(LayoutType.TAB_SWITCHER, mBaseObserver);

        int initialCount = mSelectionHintedCallbackHelper.getCallCount();
        assertEquals("Event should not have triggered.", initialCount,
                mSelectionHintedCallbackHelper.getCallCount());

        observer.onTabSelectionHinted(0);
        assertEquals("Event should have triggered.", initialCount + 1,
                mSelectionHintedCallbackHelper.getCallCount());
    }
}
