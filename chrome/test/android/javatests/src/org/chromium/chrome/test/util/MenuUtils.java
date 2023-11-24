// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import android.app.Instrumentation;

import org.junit.Assert;

import org.chromium.chrome.browser.app.ChromeActivity;

/** Collection of menu utilities. */
public class MenuUtils {
    /** Trigger for a new activity based on a menu being pressed. */
    public static class MenuActivityTrigger implements Runnable {

        private final Instrumentation mInstrumentation;
        private final ChromeActivity mActivity;
        private final int mMenuId;

        public MenuActivityTrigger(
                Instrumentation instrumentation, ChromeActivity activity, int menuId) {
            mInstrumentation = instrumentation;
            mActivity = activity;
            mMenuId = menuId;
        }

        @Override
        public void run() {
            mInstrumentation.runOnMainSync(
                    new Runnable() {
                        @Override
                        public void run() {
                            Assert.assertTrue(
                                    "Could not execute menu item.",
                                    mActivity.onMenuOrKeyboardAction(mMenuId, true));
                        }
                    });
        }
    }

    /**
     * Execute a particular menu item from the custom menu.
     * The item is executed even if it is disabled or not visible.
     */
    public static void invokeCustomMenuActionSync(
            Instrumentation instrumentation, final ChromeActivity activity, final int id) {
        instrumentation.runOnMainSync(
                new Runnable() {
                    @Override
                    public void run() {
                        Assert.assertTrue(
                                "Could not execute menu item.",
                                activity.onMenuOrKeyboardAction(id, true));
                    }
                });
    }
}
