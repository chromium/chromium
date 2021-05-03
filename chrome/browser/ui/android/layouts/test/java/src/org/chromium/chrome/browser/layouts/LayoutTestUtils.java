// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.layouts;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;

import java.util.concurrent.TimeoutException;

/** Utilities to aid in testing that involves switching between layouts. */
public class LayoutTestUtils {
    /**
     * Wait for a specified layout to be shown. If the layout is already showing, this returns
     * immediately.
     * @param layoutManager The {@link LayoutManager} showing the layout.
     * @param type The type of layout to wait for.
     * @throws TimeoutException
     */
    public static void waitForLayout(LayoutManager layoutManager, @LayoutType int type)
            throws TimeoutException {
        if (layoutManager.isLayoutVisible(type)) return;

        CallbackHelper finishedShowingCallbackHelper = new CallbackHelper();
        LayoutStateObserver observer = new LayoutStateObserver() {
            @Override
            public void onFinishedShowing(int layoutType) {
                finishedShowingCallbackHelper.notifyCalled();
            }
        };
        layoutManager.addObserver(observer);

        finishedShowingCallbackHelper.waitForFirst();
        layoutManager.removeObserver(observer);
    }
}
