// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util.browser;

import org.hamcrest.Matchers;
import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.Coordinates;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;

/** Monitors that a Tab starts loading and stops loading a URL. */
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
        ThreadUtils.runOnUiThreadBlocking(() -> mTab.addObserver(this));
        mExpectedTitle = expectedTitle;
        mExpectedScale = expectedScale;
    }

    @Override
    public void onPageLoadStarted(Tab tab, GURL url) {
        mTabLoadStartedCallback.notifyCalled();
    }

    @Override
    public void onPageLoadFinished(Tab tab, GURL url) {
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
        PostTask.runOrPostTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    mTab.loadUrl(new LoadUrlParams(url, transitionType));
                });
        assertLoaded();
    }

    /** Asserts the page has loaded. */
    public void assertLoaded() throws Exception {
        mTabLoadStartedCallback.waitForCallback(0, 1);
        mTabLoadFinishedCallback.waitForCallback(0, 1);
        final Coordinates coord = Coordinates.createFor(mTab.getWebContents());

        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "load and rendering never completed",
                            ChromeTabUtils.isLoadingAndRenderingDone(mTab),
                            Matchers.is(true));

                    if (mExpectedTitle != null) {
                        Criteria.checkThat(
                                "title did not match",
                                mTab.getTitle(),
                                Matchers.is(mExpectedTitle));
                    }

                    if (mExpectedScale != null) {
                        Criteria.checkThat(
                                "tab has no web contents",
                                mTab.getWebContents(),
                                Matchers.notNullValue());
                        float scale = coord.getPageScaleFactor();
                        Criteria.checkThat(
                                (double) mExpectedScale,
                                Matchers.is(Matchers.closeTo(scale, FLOAT_EPSILON)));
                    }
                });
    }
}
