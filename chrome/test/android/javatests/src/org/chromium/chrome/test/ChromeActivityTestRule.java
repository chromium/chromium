// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.platform.app.InstrumentationRegistry;

import org.hamcrest.Matchers;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;
import org.mockito.Mockito;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.CommandLine;
import org.chromium.base.Holder;
import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.ScalableTimeout;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.prefetch.settings.PreloadPagesSettingsBridge;
import org.chromium.chrome.browser.prefetch.settings.PreloadPagesState;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.Tab.LoadUrlResult;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.test.util.ChromeApplicationTestUtils;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.components.browser_ui.widget.highlight.PulseDrawable;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.infobars.InfoBar;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Coordinates;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;

import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Custom {@link BaseActivityTestRule} for test using {@link ChromeActivity}.
 *
 * <p>Relevant adaptations:
 *
 * <ul>
 *   <li>Sets up an {@link EmbeddedTestServer}.
 *   <li>Disables the offline indicator.
 *   <li>Disables IPH (In-Product Help).
 *   <li>Slows down PulseDrawable animations.
 * </ul>
 *
 * @param <T> The {@link Activity} class under test.
 */
public class ChromeActivityTestRule<T extends ChromeActivity> extends BaseActivityTestRule<T> {
    // The number of ms to wait for the rendering activity to be started.
    private static final int ACTIVITY_START_TIMEOUT_MS = 1000;
    private static final String TAG = "TestRule";

    private final EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    protected ChromeActivityTestRule(Class<T> activityClass) {
        super(activityClass);
    }

    @Override
    public Statement apply(final Statement base, Description description) {
        Statement testServerStatement = mTestServerRule.apply(base, description);
        return super.apply(testServerStatement, description);
    }

    @Override
    protected void before() throws Throwable {
        super.before();

        disableOfflineIndicator();
        disableIph();
        slowDownPulseDrawableAnimations();
    }

    private void disableOfflineIndicator() {
        // Tests are run on bots that are offline by default. This might cause
        // offline UI to show and cause flakiness or failures in tests. Using this
        // switch will prevent that.
        // TODO(crbug.com/40134877): Remove this once we disable the offline
        // indicator for specific tests.
        CommandLine.getInstance()
                .appendSwitch(ContentSwitches.FORCE_ONLINE_CONNECTION_STATE_FOR_INDICATOR);
    }

    private void disableIph() {
        // Disable IPH to prevent it from interfering with the tests.
        Log.w(
                TAG,
                "A mock Tracker is set in ChromeActivityTestRule. This will"
                        + " prevent any IPH from showing. See crbug.com/342240475.");
        Tracker tracker = Mockito.mock(Tracker.class);
        when(tracker.shouldTriggerHelpUi(anyString())).thenReturn(false);
        TrackerFactory.setTrackerForTests(tracker);
        ResettersForTesting.register(() -> Mockito.reset(tracker));
    }

    private void slowDownPulseDrawableAnimations() {
        // Reduce PulseDrawable frame rate to keep UI Thread MessageQueue from being busy most of
        // the time, which causes Espresso's ViewInteraction#check() and #perform() to fail with
        // AppNotIdleException since they wait for the UI Thread to be clear.
        PulseDrawable.setFrameRateForTesting(2);
    }

    /**
     * Enables default behavior of IPH again for one test case.
     *
     * <p>Tests can also use {@code TrackerFactory.setTrackerForTests(mMockTracker)} to have more
     * predictable IPH behavior.
     */
    public void reenableIph() {
        TrackerFactory.setTrackerForTests(null);
    }

    @Override
    protected void after() {
        super.after();
        // Activity is finish()'ed in super.after(), and CCT activities sometimes trigger creation
        // of spare tabs in their onDestroy() (https://crrev.com/c/5597549).
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    WarmupManager.getInstance().destroySpareTab();
                });
    }

    /** Return the timeout limit for Chrome activty start in tests */
    public static int getActivityStartTimeoutMs() {
        return ACTIVITY_START_TIMEOUT_MS;
    }

    // This has to be here or getActivity will return a T that extends Activity, not a T that
    // extends ChromeActivity.
    @Override
    @SuppressWarnings("RedundantOverride")
    public T getActivity() {
        return super.getActivity();
    }

    /**
     * @return The {@link AppMenuCoordinator} for the activity.
     */
    public AppMenuCoordinator getAppMenuCoordinator() {
        if (getActivity().getRootUiCoordinatorForTesting() == null) return null;
        return getActivity().getRootUiCoordinatorForTesting().getAppMenuCoordinatorForTesting();
    }

    /**
     * Matches testString against baseString.
     * Returns 0 if there is no match, 1 if an exact match and 2 if a fuzzy match.
     */
    public static int matchUrl(String baseString, String testString) {
        if (baseString.equals(testString)) {
            return 1;
        }
        if (baseString.contains(testString)) {
            return 2;
        }
        return 0;
    }

    /**
     * Waits for the activity to fully finish its native initialization.
     * @param activity The {@link ChromeActivity} to wait for.
     */
    public static void waitForActivityNativeInitializationComplete(ChromeActivity activity) {
        CriteriaHelper.pollUiThread(
                () -> ChromeBrowserInitializer.getInstance().isFullBrowserInitialized(),
                "Native initialization never finished",
                20 * CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        CriteriaHelper.pollUiThread(
                () -> activity.didFinishNativeInitialization(),
                "Native initialization (of Activity) never finished");
    }

    /** Waits for the activity to fully finish its native initialization. */
    public void waitForActivityNativeInitializationComplete() {
        waitForActivityNativeInitializationComplete(getActivity());
    }

    /** Similar to #launchActivity(Intent), but waits for the Activity tab to be initialized. */
    public void startActivityCompletely(Intent intent) {
        launchActivity(intent);
        waitForActivityNativeInitializationComplete();
        waitForActivityCompletelyLoaded();
    }

    /** Wait until the activity is completely loaded, and a tab is shown. */
    public void waitForActivityCompletelyLoaded() {
        waitForActivityCompletelyLoaded(getActivity());
    }

    public static void waitForActivityCompletelyLoaded(ChromeActivity activity) {
        CriteriaHelper.pollUiThread(
                () -> activity.getActivityTab() != null, "Tab never selected/initialized.");
        Tab tab = ThreadUtils.runOnUiThreadBlocking(() -> activity.getActivityTab());

        ChromeTabUtils.waitForTabPageLoaded(tab, (String) null);

        if (tab != null
                && UrlUtilities.isNtpUrl(ChromeTabUtils.getUrlStringOnUiThread(tab))
                && !activity.isInOverviewMode()) {
            NewTabPageTestUtils.waitForNtpLoaded(tab);
        }

        assertTrue(waitForDeferredStartup(activity));

        assertNotNull(tab);
        assertNotNull(tab.getView());
    }

    public boolean waitForDeferredStartup() {
        return waitForDeferredStartup(getActivity());
    }

    public static boolean waitForDeferredStartup(ChromeActivity activity) {
        CriteriaHelper.pollUiThread(
                activity::deferredStartupPostedForTesting,
                20000L,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        return DeferredStartupHandler.waitForDeferredStartupCompleteForTesting(
                ScalableTimeout.scaleTimeout(CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL));
    }

    @Override
    public T launchActivity(Intent startIntent) {
        // Avoid relying on explicit intents, bypassing LaunchIntentDispatcher, created by null
        // startIntent launch behavior.
        assertNotNull(startIntent);
        return super.launchActivity(startIntent);
    }

    /**
     * Enables or disables network predictions, i.e. prerendering, prefetching, DNS preresolution,
     * etc. Network predictions are enabled by default.
     */
    public void setNetworkPredictionEnabled(final boolean enabled) {
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            Profile profile = ProfileManager.getLastUsedRegularProfile();
                            if (enabled) {
                                PreloadPagesSettingsBridge.setState(
                                        profile, PreloadPagesState.STANDARD_PRELOADING);
                            } else {
                                PreloadPagesSettingsBridge.setState(
                                        profile, PreloadPagesState.NO_PRELOADING);
                            }
                        });
    }

    /**
     * Navigates to a URL directly without going through the UrlBar. This bypasses the page
     * preloading mechanism of the UrlBar.
     *
     * @param url The URL to load in the current tab.
     * @param secondsToWait The number of seconds to wait for the page to be loaded.
     * @return {@link LoadUrlResult} from Tab#loadUrl.
     */
    public LoadUrlResult loadUrl(String url, long secondsToWait) throws IllegalArgumentException {
        Tab tab = getActivityTab();
        return loadUrlInTab(
                url, PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR, tab, secondsToWait);
    }

    /**
     * Navigates to a URL directly without going through the UrlBar. This bypasses the page
     * preloading mechanism of the UrlBar.
     *
     * @param url The URL to load in the current tab.
     * @return {@link LoadUrlResult} from Tab#loadUrl.
     */
    public LoadUrlResult loadUrl(String url) throws IllegalArgumentException {
        Tab tab = getActivityTab();
        return loadUrlInTab(url, PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR, tab);
    }

    /** {@link #loadUrl(String) */
    public LoadUrlResult loadUrl(GURL url) throws IllegalArgumentException {
        return loadUrl(url.getSpec());
    }

    public LoadUrlResult loadUrlNoWaiting(String url) throws IllegalArgumentException {
        Tab tab = getActivityTab();
        LoadUrlResult result =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> {
                            return tab.loadUrl(
                                    new LoadUrlParams(
                                            url,
                                            PageTransition.TYPED
                                                    | PageTransition.FROM_ADDRESS_BAR));
                        });
        return result;
    }

    /**
     * @param url The URL of the page to load.
     * @param pageTransition The type of transition. see {@link org.chromium.ui.base.PageTransition}
     *     for valid values.
     * @param tab The tab to load the URL into.
     * @param secondsToWait The number of seconds to wait for the page to be loaded.
     * @return {@link LoadUrlResult} from Tab#loadUrl.
     */
    public LoadUrlResult loadUrlInTab(String url, int pageTransition, Tab tab, long secondsToWait) {
        assertNotNull("Cannot load the URL in a null tab", tab);
        AtomicReference<LoadUrlResult> result = new AtomicReference();

        ChromeTabUtils.waitForTabPageLoaded(
                tab,
                url,
                new Runnable() {
                    @Override
                    public void run() {
                        ThreadUtils.runOnUiThreadBlocking(
                                () -> {
                                    result.set(tab.loadUrl(new LoadUrlParams(url, pageTransition)));
                                });
                    }
                },
                secondsToWait);
        ChromeTabUtils.waitForInteractable(tab);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        return result.get();
    }

    /**
     * @param url The URL of the page to load.
     * @param pageTransition The type of transition. see {@link org.chromium.ui.base.PageTransition}
     *     for valid values.
     * @param tab The tab to load the URL into.
     * @return PAGE_LOAD_FAILED if the URL could not be loaded, otherwise DEFAULT_PAGE_LOAD.
     */
    public LoadUrlResult loadUrlInTab(String url, int pageTransition, Tab tab) {
        return loadUrlInTab(url, pageTransition, tab, CallbackHelper.WAIT_TIMEOUT_SECONDS);
    }

    /**
     * Load a URL in a new tab. The {@link Tab} will pretend to be created from a link.
     * @param url The URL of the page to load.
     */
    public Tab loadUrlInNewTab(String url) {
        return loadUrlInNewTab(url, false);
    }

    /**
     * Load a URL in a new tab. The {@link Tab} will pretend to be created from a link.
     * @param url The URL of the page to load.
     * @param incognito Whether the new tab should be incognito.
     */
    public Tab loadUrlInNewTab(final String url, final boolean incognito) {
        return loadUrlInNewTab(url, incognito, TabLaunchType.FROM_LINK);
    }

    /**
     * Load a URL in a new tab, with the given transition type.
     * @param url The URL of the page to load.
     * @param incognito Whether the new tab should be incognito.
     * @param launchType The type of Tab Launch.
     */
    public Tab loadUrlInNewTab(
            final String url, final boolean incognito, final @TabLaunchType int launchType) {
        Tab tab =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> getActivity().getTabCreator(incognito).launchUrl(url, launchType));
        ChromeTabUtils.waitForTabPageLoaded(tab, url);
        ChromeTabUtils.waitForInteractable(tab);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        return tab;
    }

    /**
     * Prepares a URL intent to start the activity.
     * @param intent the intent to be modified
     * @param url the URL to be used (may be null)
     */
    public Intent prepareUrlIntent(Intent intent, String url) {
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        if (intent.getComponent() == null) {
            intent.setComponent(
                    new ComponentName(
                            ApplicationProvider.getApplicationContext(),
                            ChromeLauncherActivity.class));
        }

        if (url != null) {
            intent.setData(Uri.parse(url));
        }
        return intent;
    }

    public Profile getProfile(boolean incognito) {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    return ProfileProvider.getOrCreateProfile(
                            getActivity().getProfileProviderSupplier().get(), incognito);
                });
    }

    /**
     * @return The number of tabs currently open.
     */
    public int tabsCount(boolean incognito) {
        return ThreadUtils.runOnUiThreadBlocking(
                new Callable<>() {
                    @Override
                    public Integer call() {
                        return getActivity().getTabModelSelector().getModel(incognito).getCount();
                    }
                });
    }

    /** Returns the infobars being displayed by the current tab, or null if they don't exist. */
    public List<InfoBar> getInfoBars() {
        return ThreadUtils.runOnUiThreadBlocking(
                new Callable<>() {
                    @Override
                    public List<InfoBar> call() {
                        Tab currentTab = getActivityTab();
                        assertNotNull(currentTab);
                        assertNotNull(InfoBarContainer.get(currentTab));
                        return InfoBarContainer.get(currentTab).getInfoBarsForTesting();
                    }
                });
    }

    /**
     * Executes the given snippet of JavaScript code within the current tab. Returns the result of
     * its execution in JSON format.
     */
    public String runJavaScriptCodeInCurrentTab(String code) throws TimeoutException {
        WebContents webContents = getWebContents();
        return JavaScriptUtils.executeJavaScriptAndWaitForResult(webContents, code);
    }

    /**
     * Executes the given snippet of JavaScript code within the current tab, acting as if a user
     * gesture has been made. Returns the result of its execution in JSON format.
     */
    public String runJavaScriptCodeWithUserGestureInCurrentTab(String code)
            throws TimeoutException {
        WebContents webContents = getWebContents();
        return JavaScriptUtils.executeJavaScriptWithUserGestureAndWaitForResult(webContents, code);
    }

    /**
     * Waits till the WebContents receives the expected page scale factor
     * from the compositor and asserts that this happens.
     */
    public void assertWaitForPageScaleFactorMatch(float expectedScale) {
        ChromeApplicationTestUtils.assertWaitForPageScaleFactorMatch(getActivity(), expectedScale);
    }

    /**
     * @return {@link InfoBarContainer} of the active tab of the activity. {@code null} if there is
     *     no tab for the activity or infobar is available.
     */
    public InfoBarContainer getInfoBarContainer() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> getActivityTab() != null ? InfoBarContainer.get(getActivityTab()) : null);
    }

    /** Gets the ChromeActivityTestRule's EmbeddedTestServer instance if it has one. */
    public EmbeddedTestServer getTestServer() {
        return mTestServerRule.getServer();
    }

    /** Gets the underlying EmbeddedTestServerRule for getTestServer(). */
    public EmbeddedTestServerRule getEmbeddedTestServerRule() {
        return mTestServerRule;
    }

    /** Returns the active {@link Tab} of the activity. */
    public Tab getActivityTab() {
        return ThreadUtils.runOnUiThreadBlocking(() -> getActivity().getActivityTab());
    }

    /** Returns the {@link WebContents} of the active tab of the activity. */
    public WebContents getWebContents() {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> getActivity().getActivityTab().getWebContents());
    }

    /**
     * @return {@link KeyboardVisibilityDelegate} for the activity.
     */
    public KeyboardVisibilityDelegate getKeyboardDelegate() {
        if (getActivity().getWindowAndroid() == null) {
            return KeyboardVisibilityDelegate.getInstance();
        }
        return getActivity().getWindowAndroid().getKeyboardDelegate();
    }

    /**
     * Waits for an Activity of the given class to be started.
     * @return The Activity.
     */
    @SuppressWarnings("unchecked")
    public static <T extends ChromeActivity> T waitFor(final Class<T> expectedClass) {
        return waitFor(expectedClass, CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL);
    }

    /**
     * Waits for an Activity of the given class to be started.
     *
     * @param expectedClass The class of the Activity being waited on.
     * @param maxTimeToPoll Maximum time in milliseconds to poll.
     * @return The Activity.
     */
    @SuppressWarnings("unchecked")
    public static <T extends ChromeActivity> T waitFor(
            final Class<T> expectedClass, long maxTimeToPoll) {
        final Holder<@Nullable Activity> holder = new Holder<>(null);
        CriteriaHelper.pollUiThread(
                () -> {
                    holder.value = ApplicationStatus.getLastTrackedFocusedActivity();
                    Criteria.checkThat(holder.value, Matchers.notNullValue());
                    Criteria.checkThat(
                            holder.value.getClass(), Matchers.typeCompatibleWith(expectedClass));
                    Criteria.checkThat(
                            ((ChromeActivity) holder.value).getActivityTab(),
                            Matchers.notNullValue());
                },
                maxTimeToPoll,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        return (T) holder.value;
    }

    /**
     * Waits for the first frame so that the page scale factor is set. Skipping this causes
     * flakiness when clicking DOM objects since the page scale factor might not have been set yet,
     * and coordinates can be wrong.
     */
    protected void waitForFirstFrame() {
        final Coordinates coord = Coordinates.createFor(getWebContents());
        CriteriaHelper.pollUiThread(
                coord::frameInfoUpdated, "FrameInfo has not been updated in time.");
    }
}
