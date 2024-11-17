// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.test.util.browser;

import android.text.TextUtils;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;

import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * A utility class to get notified of title change in a tab. Subclasses can
 * override doesTitleMatch() to customize title matching.
 */
public class TabTitleObserver extends EmptyTabObserver {
    private final String mExpectedTitle;
    private final CallbackHelper mCallback;

    /**
     * A constructor.
     *
     * @param tab The tab to be observed.
     * @param expectedTitle The expected title to wait for.
     */
    public TabTitleObserver(final Tab tab, final String expectedTitle) {
        mExpectedTitle = expectedTitle;
        mCallback = new CallbackHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    if (!notifyCallbackIfTitleMatches(tab)) {
                        tab.addObserver(TabTitleObserver.this);
                    }
                });
    }

    /**
     * Wait for title update up to the given number of seconds.
     *
     * @param seconds The number of seconds to wait.
     */
    public void waitForTitleUpdate(int seconds) throws TimeoutException {
        mCallback.waitForCallback(0, 1, seconds, TimeUnit.SECONDS);
    }

    private boolean notifyCallbackIfTitleMatches(Tab tab) {
        if (doesTitleMatch(mExpectedTitle, tab.getTitle())) {
            mCallback.notifyCalled();
            return true;
        }
        return false;
    }

    @Override
    public void onTitleUpdated(Tab tab) {
        notifyCallbackIfTitleMatches(tab);
    }

    /** @return Whether the title matches the expected condition. */
    protected boolean doesTitleMatch(String expectedTitle, String actualTitle) {
        return TextUtils.equals(expectedTitle, actualTitle);
    }
}
