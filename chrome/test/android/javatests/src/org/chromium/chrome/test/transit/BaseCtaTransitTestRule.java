// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import android.os.Build;

import com.google.errorprone.annotations.CheckReturnValue;

import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.TripBuilder;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.ChromeApplicationTestUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

/** Base class for integration tests that start {@link ChromeTabbedActivity}. */
@NullMarked
class BaseCtaTransitTestRule {
    protected final ChromeTabbedActivityTestRule mActivityTestRule;

    BaseCtaTransitTestRule() {
        mActivityTestRule = new ChromeTabbedActivityTestRule();
    }

    BaseCtaTransitTestRule(ChromeTabbedActivityTestRule activityTestRule) {
        mActivityTestRule = activityTestRule;
    }

    public ChromeTabbedActivityTestRule getActivityTestRule() {
        return mActivityTestRule;
    }

    public ChromeTabbedActivity getActivity() {
        return mActivityTestRule.getActivity();
    }

    public EmbeddedTestServer getTestServer() {
        return mActivityTestRule.getTestServer();
    }

    // TODO(crbug.com/406324209): Create WebPageStation#getWebContents() and replace these calls.
    public WebContents getWebContents() {
        return mActivityTestRule.getWebContents();
    }

    // TODO(crbug.com/406324209): Use WebPageStation#runJsTo() to replace these calls.
    public String runJavaScriptCodeInCurrentTab(String code) throws TimeoutException {
        return mActivityTestRule.runJavaScriptCodeInCurrentTab(code);
    }

    public int tabsCount(boolean incognito) {
        return mActivityTestRule.tabsCount(incognito);
    }

    public Profile getProfile(boolean incognito) {
        return mActivityTestRule.getProfile(incognito);
    }

    // TODO(crbug.com/406324209): Develop WebPageStation#FrameInfoUpdatedCondition and replace these
    // calls.
    public void assertWaitForPageScaleFactorMatch(float expectedScale) {
        mActivityTestRule.assertWaitForPageScaleFactorMatch(expectedScale);
    }

    // TODO(crbug.com/406324209): Use PageStation#loadWebPageProgrammatically() or
    // #loadPageProgrammatically to replace these calls.
    public Tab.LoadUrlResult loadUrl(GURL url) {
        return mActivityTestRule.loadUrl(url);
    }

    // TODO(crbug.com/406324209): Use PageStation#loadWebPageProgrammatically() or
    // #loadPageProgrammatically to replace these calls.
    public Tab.LoadUrlResult loadUrl(String url) {
        return mActivityTestRule.loadUrl(url);
    }

    // TODO(crbug.com/406324209): Use PageStation#loadWebPageProgrammatically() or
    // #loadPageProgrammatically to replace these calls.
    public Tab.LoadUrlResult loadUrl(String url, long secondsToWait) {
        return mActivityTestRule.loadUrl(url, secondsToWait);
    }

    // TODO(crbug.com/406324209): Use PageStation#loadWebPageProgrammatically() or
    // #loadPageProgrammatically to replace these calls.
    public Tab.LoadUrlResult loadUrlInTab(
            String url, int pageTransition, Tab tab, long secondsToWait) {
        return mActivityTestRule.loadUrlInTab(url, pageTransition, tab, secondsToWait);
    }

    // TODO(crbug.com/406324209): Use PageStation#loadWebPageProgrammatically() or
    // #loadPageProgrammatically to replace these calls.
    public Tab.LoadUrlResult loadUrlInTab(String url, int pageTransition, Tab tab) {
        return mActivityTestRule.loadUrlInTab(url, pageTransition, tab);
    }

    // TODO(crbug.com/406324209): Use PageStation#openFakeLinkToWebPage() or #openFakeLink to
    // replace these calls.
    public Tab loadUrlInNewTab(String url) {
        return mActivityTestRule.loadUrlInNewTab(url);
    }

    // TODO(crbug.com/406324209): Use Public Transit in a case-by-case basis to replace these calls,
    // often with PageStation#openFakeLinkToWebPage().
    public Tab loadUrlInNewTab(final String url, final boolean incognito) {
        return mActivityTestRule.loadUrlInNewTab(url, incognito);
    }

    public Tab loadUrlInNewTab(
            final String url, final boolean incognito, final @TabLaunchType int launchType) {
        return mActivityTestRule.loadUrlInNewTab(url, incognito, launchType);
    }

    // TODO(crbug.com/406324209): Use PageStation#openNewIncognitoTabFast() to replace these calls.
    public Tab newIncognitoTabFromMenu() {
        return mActivityTestRule.newIncognitoTabFromMenu();
    }

    public KeyboardVisibilityDelegate getKeyboardDelegate() {
        return mActivityTestRule.getKeyboardDelegate();
    }

    public AppMenuCoordinator getAppMenuCoordinator() {
        return mActivityTestRule.getAppMenuCoordinator();
    }

    // TODO(crbug.com/406324209): Support tracking pause/resume state in Public Transit.
    public void resumeMainActivityFromLauncher() throws Exception {
        mActivityTestRule.resumeMainActivityFromLauncher();
    }

    // TODO(crbug.com/406324209): Support finishing and restarting activity in Public Transit.
    public void restartMainActivityFromLauncher() throws Exception {
        mActivityTestRule.startMainActivityFromLauncher();
    }

    // TODO(crbug.com/406324209): ChromeTabbedActivityTestRule#startActivityCompletely() already
    // calls this. Double check that callers are using those entry points and remove this.
    public void waitForActivityNativeInitializationComplete() {
        mActivityTestRule.waitForActivityNativeInitializationComplete();
    }

    // TODO(crbug.com/406324209): ChromeTabbedActivityTestRule#startActivityCompletely() already
    // calls this. Double check that callers are using those entry points and remove this.
    public void waitForActivityCompletelyLoaded() {
        mActivityTestRule.waitForActivityCompletelyLoaded();
    }

    // TODO(crbug.com/406324209): Support recreate() in Public Transit.
    public void recreateActivity() {
        mActivityTestRule.recreateActivity();
    }

    public EmbeddedTestServerRule getEmbeddedTestServerRule() {
        return mActivityTestRule.getEmbeddedTestServerRule();
    }

    // TODO(crbug.com/406324209): Use OmniboxFacility#typeText().
    public void typeInOmnibox(String text, boolean oneCharAtATime) throws InterruptedException {
        mActivityTestRule.typeInOmnibox(text, oneCharAtATime);
    }

    /** Pause the Activity going to home screen, then resume it. */
    @CheckReturnValue
    public TripBuilder pauseAndResumeActivityTo(Station<?> currentStation) {
        return currentStation.runTo(
                () -> {
                    ChromeTabbedActivity cta = getActivity();
                    ChromeApplicationTestUtils.fireHomeScreenIntent(cta);
                    try {
                        resumeMainActivityFromLauncher();
                    } catch (Exception e) {
                        throw new RuntimeException(e);
                    }

                    // crbug.com/324106495: Add an extra sleep in Android 12+ because
                    // SnapshotStartingWindow occludes the ChromeActivity and any input is
                    // considered an untrusted input until the SnapshotStartingWindow disappears.
                    // Since it is a system window being drawn on top, we don't have access to any
                    // signals that the SnapshotStartingWindow disappeared that we can wait for.
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                        try {
                            Thread.sleep(200);
                        } catch (InterruptedException e) {
                        }
                    }
                });
    }
}
