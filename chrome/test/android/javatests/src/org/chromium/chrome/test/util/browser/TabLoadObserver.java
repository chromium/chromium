// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser;

import android.text.TextUtils;

import org.junit.Assert;

import org.chromium.base.task.PostTask;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.test.util.Coordinates;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.ui.base.PageTransition;

import java.util.Locale;

/**
 * Monitors that a Tab starts loading and stops loading a URL.
 */
public class TabLoadObserver extends EmptyTabObserver {
    private static final float FLOAT_EPSILON = 0.001f;

    private final CallbackHelper mTabLoadStartedCallback = new CallbackHelper();
    private final CallbackHelper mTabLoadFinishedCallback = new CallbackHelper();

    private final Tab mTab;
    private final String mExpectedTitle;
    private final Float mExpectedScale;

    public TabLoadObserver(Tab tab) {
        this(tab, null, null);
    }

    public TabLoadObserver(Tab tab, String expectedTitle, Float expectedScale) {
        mTab = tab;
        mTab.addObserver(this);
        mExpectedTitle = expectedTitle;
        mExpectedScale = expectedScale;
    }

    @Override
    public void onPageLoadStarted(Tab tab, String url) {
        mTabLoadStartedCallback.notifyCalled();
    }

    @Override
    public void onPageLoadFinished(Tab tab, String url) {
        mTabLoadFinishedCallback.notifyCalled();
    }

    @Override
    public void onCrash(Tab tab) {
        Assert.fail("Tab crashed; test results will be invalid.  Failing.");
    }

    /**
     * Loads the given URL and waits for it to complete.
     *
     * @param url URL to load and wait for.
     */
    public void fullyLoadUrl(final String url) throws Exception {
        fullyLoadUrl(url, PageTransition.LINK);
    }

    /**
     * Loads the given URL and waits for it to complete.
     *
     * @param url            URL to load and wait for.
     * @param transitionType the transition type to use.
     */
    public void fullyLoadUrl(final String url, final int transitionType) throws Exception {
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                () -> { mTab.loadUrl(new LoadUrlParams(url, transitionType)); });
        assertLoaded();
    }

    /**
     * Asserts the page has loaded.
     */
    public void assertLoaded() throws Exception {
        mTabLoadStartedCallback.waitForCallback(0, 1);
        mTabLoadFinishedCallback.waitForCallback(0, 1);
        final Coordinates coord = Coordinates.createFor(mTab.getWebContents());

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                if (!ChromeTabUtils.isLoadingAndRenderingDone(mTab)) {
                    updateFailureReason("load and rendering never completed");
                    return false;
                }

                String title = mTab.getTitle();
                if (mExpectedTitle != null && !TextUtils.equals(mExpectedTitle, title)) {
                    updateFailureReason(String.format(
                            Locale.ENGLISH,
                            "title did not match -- expected: \"%s\", actual \"%s\"",
                            mExpectedTitle, title));
                    return false;
                }

                if (mExpectedScale != null) {
                    if (mTab.getWebContents() == null) {
                        updateFailureReason("tab has no web contents");
                        return false;
                    }

                    float scale = coord.getPageScaleFactor();
                    if (Math.abs(mExpectedScale - scale) >= FLOAT_EPSILON) {
                        updateFailureReason(String.format(
                                Locale.ENGLISH,
                                "scale did not match with allowed epsilon -- "
                                + "expected: \"%f\", actual \"%f\"", mExpectedScale, scale));
                        return false;
                    }
                }
                updateFailureReason(null);
                return true;
            }
        });
    }
}
