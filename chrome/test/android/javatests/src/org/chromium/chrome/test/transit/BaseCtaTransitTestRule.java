// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.transit;

import android.app.Activity;
import android.os.Build;

import androidx.test.runner.lifecycle.Stage;

import com.google.errorprone.annotations.CheckReturnValue;

import org.junit.rules.TestRule;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.transit.Station;
import org.chromium.base.test.transit.TripBuilder;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.page.BasePageStation;
import org.chromium.chrome.test.transit.page.CtaPageStation;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.util.ChromeApplicationTestUtils;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.url.GURL;

import java.util.Collection;
import java.util.Collections;
import java.util.concurrent.TimeoutException;

/** Base class for integration tests that start {@link ChromeTabbedActivity}. */
@NullMarked
public abstract class BaseCtaTransitTestRule implements TestRule {
    protected final ChromeTabbedActivityTestRule mActivityTestRule;

    BaseCtaTransitTestRule() {
        mActivityTestRule = new ChromeTabbedActivityTestRule();
    }

    BaseCtaTransitTestRule(ChromeTabbedActivityTestRule activityTestRule) {
        mActivityTestRule = activityTestRule;
    }

    /** Start the test in a blank page. */
    public abstract WebPageStation startOnBlankPage();

    /** Start the test in an NTP. */
    public abstract RegularNewTabPageStation startOnNtp();

    /** Start the test at |url| arriving at the Station built by |builder|. */
    public abstract <T extends CtaPageStation> T startOnPage(
            String url, BasePageStation.Builder<T> builder);

    /** Start the test in a web page at |url|. */
    public WebPageStation startOnWebPage(String url) {
        return startOnPage(url, WebPageStation.newBuilder());
    }

    /**
     * Start the test in a web page served by the test server.
     *
     * @param relativeUrl the relative URL of the page to serve and load
     * @return the active entry {@link WebPageStation}
     */
    public WebPageStation startOnTestServerUrl(String relativeUrl) {
        assert relativeUrl.startsWith("/") : "|relativeUrl| must be relative";
        String fullUrl = mActivityTestRule.getTestServer().getURL(relativeUrl);
        return startOnWebPage(fullUrl);
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

    public Tab getActivityTab() {
        return mActivityTestRule.getActivityTab();
    }

    // TODO(crbug.com/406324209): Create WebPageStation#getWebContents() and replace these calls.
    public WebContents getWebContents() {
        return mActivityTestRule.getWebContents();
    }

    // TODO(crbug.com/406324209): Use WebPageStation#runJsTo() to replace these calls.
    public String runJavaScriptCodeInCurrentTab(String code) throws TimeoutException {
        return mActivityTestRule.runJavaScriptCodeInCurrentTab(code);
    }

    // TODO(crbug.com/406324209): Use WebPageStation#runJsTo() to replace these calls.
    public String runJavaScriptCodeWithUserGestureInCurrentTab(String code)
            throws TimeoutException {
        return mActivityTestRule.runJavaScriptCodeWithUserGestureInCurrentTab(code);
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

    // TODO(crbug.com/406324209): Use CtaPageStation#loadWebPageProgrammatically() or
    // #loadPageProgrammatically to replace these calls.
    public Tab.LoadUrlResult loadUrl(GURL url) {
        return mActivityTestRule.loadUrl(url);
    }

    // TODO(crbug.com/406324209): Use CtaPageStation#loadWebPageProgrammatically() or
    // #loadPageProgrammatically to replace these calls.
    public Tab.LoadUrlResult loadUrl(String url) {
        return mActivityTestRule.loadUrl(url);
    }

    // TODO(crbug.com/406324209): Use CtaPageStation#loadWebPageProgrammatically() or
    // #loadPageProgrammatically to replace these calls.
    public Tab.LoadUrlResult loadUrl(String url, long secondsToWait) {
        return mActivityTestRule.loadUrl(url, secondsToWait);
    }

    // TODO(crbug.com/406324209): Use CtaPageStation#loadWebPageProgrammatically() or
    // #loadPageProgrammatically to replace these calls.
    public Tab.LoadUrlResult loadUrlInTab(
            String url, int pageTransition, Tab tab, long secondsToWait) {
        return mActivityTestRule.loadUrlInTab(url, pageTransition, tab, secondsToWait);
    }

    // TODO(crbug.com/406324209): Use CtaPageStation#loadWebPageProgrammatically() or
    // #loadPageProgrammatically to replace these calls.
    public Tab.LoadUrlResult loadUrlInTab(String url, int pageTransition, Tab tab) {
        return mActivityTestRule.loadUrlInTab(url, pageTransition, tab);
    }

    // TODO(crbug.com/406324209): Use CtaPageStation#openFakeLinkToWebPage() or #openFakeLink to
    // replace these calls.
    public Tab loadUrlInNewTab(String url) {
        return mActivityTestRule.loadUrlInNewTab(url);
    }

    // TODO(crbug.com/406324209): Use Public Transit in a case-by-case basis to replace these calls,
    // often with CtaPageStation#openFakeLinkToWebPage().
    public Tab loadUrlInNewTab(final String url, final boolean incognito) {
        return mActivityTestRule.loadUrlInNewTab(url, incognito);
    }

    public Tab loadUrlInNewTab(
            final String url, final boolean incognito, final @TabLaunchType int launchType) {
        return mActivityTestRule.loadUrlInNewTab(url, incognito, launchType);
    }

    // TODO(crbug.com/406324209): Use CtaPageStation#openNewIncognitoTabFast() to replace these
    // calls.
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

    /**
     * @see ChromeTabbedActivityTestRule#launchMainActivityFromLauncher()
     */
    public void launchMainActivityFromLauncher() throws Exception {
        mActivityTestRule.launchMainActivityFromLauncher();
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

    // TODO(crbug.com/406324209): Support finishActivity() in Public Transit.
    public void finishActivity() {
        mActivityTestRule.finishActivity();
    }

    // TODO(crbug.com/406324209): Support recreateActivity() in Public Transit.
    public void recreateActivity() {
        mActivityTestRule.recreateActivity();
    }

    public EmbeddedTestServerRule getEmbeddedTestServerRule() {
        return mActivityTestRule.getEmbeddedTestServerRule();
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

    /** Enables IPH again for one test case. */
    public void reenableIph() {
        mActivityTestRule.reenableIph();
    }

    protected static void finishActivityWithCleanup(Activity activity) {
        finishActivitiesWithCleanup(Collections.singletonList(activity));
    }

    protected static void finishActivitiesWithCleanup(Collection<Activity> activities) {
        if (activities.isEmpty()) return;

        // Initiate finishAndRemoveTask() on all activities in a single UI-thread pass
        // (ChromeTabbedActivities first, then non-ChromeTabbedActivities) before waiting for
        // Stage.DESTROYED so that when closing all windows, closing a top activity does not resume
        // an underlying ChromeTabbedActivity during teardown.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    for (Activity activity : activities) {
                        if (activity instanceof ChromeTabbedActivity cta) {
                            MultiInstanceManager mim = cta.getMultiInstanceMangerForTesting();
                            if (mim != null
                                    && mim.getCurrentInstanceId()
                                            != MultiInstanceManager.INVALID_WINDOW_ID) {
                                // On Android 12+ (API 31+), MultiInstanceManagerApi31 closes all
                                // tabs, deletes instance/tab state files, and calls
                                // finishAndRemoveTask().
                                mim.closeWindows(
                                        Collections.singletonList(cta.getWindowId()),
                                        MultiInstanceManager.CloseWindowAppSource.OTHER);
                            } else {
                                // On Android <= 11 (API <= 30) or if the Activity never reached
                                // onPreCreate(), MultiInstanceManagerImpl#closeWindows is a no-op,
                                // so close all tabs and overwrite persisted tab state with the
                                // empty model before finishing the Activity (crbug.com/580067196).
                                TabModelOrchestrator orchestrator =
                                        cta.getTabModelOrchestratorSupplier().get();
                                if (orchestrator != null && orchestrator.areStoresInitialized()) {
                                    TabModelSelector selector = orchestrator.getTabModelSelector();
                                    if (selector != null) {
                                        selector.commitAllTabClosures();
                                        TabClosureParams params =
                                                TabClosureParams.closeAllTabs()
                                                        .uponExit(true)
                                                        .hideTabGroups(true)
                                                        .saveToTabRestoreService(false)
                                                        .build();
                                        selector.getModel(true)
                                                .getTabRemover()
                                                .closeTabs(params, /* allowDialog= */ false);
                                        selector.getModel(false)
                                                .getTabRemover()
                                                .closeTabs(params, /* allowDialog= */ false);
                                    }
                                    orchestrator.saveState();
                                }
                            }
                            if (!cta.isFinishing()) {
                                cta.finishAndRemoveTask();
                            }
                        }
                    }
                    for (Activity activity : activities) {
                        if (!(activity instanceof ChromeTabbedActivity)
                                && !activity.isFinishing()) {
                            activity.finishAndRemoveTask();
                        }
                    }
                });

        for (Activity activity : activities) {
            ApplicationTestUtils.waitForActivityState(activity, Stage.DESTROYED);
        }
    }
}
