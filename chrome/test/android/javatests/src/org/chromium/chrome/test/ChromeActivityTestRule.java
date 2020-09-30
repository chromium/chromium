// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Browser;
import android.support.test.InstrumentationRegistry;
import android.support.test.internal.runner.listener.InstrumentationResultPrinter;
import android.support.test.rule.ActivityTestRule;
import android.text.TextUtils;
import android.view.Menu;

import org.hamcrest.Matchers;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.CommandLine;
import org.chromium.base.Log;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.omnibox.UrlBar;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.ui.appmenu.AppMenuCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuTestSupport;
import org.chromium.chrome.test.util.ApplicationTestUtils;
import org.chromium.chrome.test.util.ChromeTabUtils;
import org.chromium.chrome.test.util.MenuUtils;
import org.chromium.chrome.test.util.NewTabPageTestUtils;
import org.chromium.chrome.test.util.WaitForFocusHelper;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.infobars.InfoBar;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.JavaScriptUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.net.test.EmbeddedTestServerRule;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.PageTransition;

import java.util.Calendar;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/**
 * Custom  {@link ActivityTestRule} for test using  {@link ChromeActivity}.
 *
 * @param <T> The {@link Activity} class under test.
 */
public class ChromeActivityTestRule<T extends ChromeActivity> extends ActivityTestRule<T> {
    private static final String TAG = "ChromeATR";

    // The number of ms to wait for the rendering activity to be started.
    private static final int ACTIVITY_START_TIMEOUT_MS = 1000;

    private static final long OMNIBOX_FIND_SUGGESTION_TIMEOUT_MS = 10 * 1000;

    private Thread.UncaughtExceptionHandler mDefaultUncaughtExceptionHandler;
    private Class<T> mChromeActivityClass;
    private T mSetActivity;
    private String mCurrentTestName;

    @Rule
    private EmbeddedTestServerRule mTestServerRule = new EmbeddedTestServerRule();

    protected ChromeActivityTestRule(Class<T> activityClass) {
        this(activityClass, false);
    }

    protected ChromeActivityTestRule(Class<T> activityClass, boolean initialTouchMode) {
        super(activityClass, initialTouchMode, false);
        mChromeActivityClass = activityClass;
    }

    @Override
    public Statement apply(final Statement base, Description description) {
        mCurrentTestName = description.getMethodName();
        Statement chromeActivityStatement = new Statement() {
            @Override
            public void evaluate() throws Throwable {
                mDefaultUncaughtExceptionHandler = Thread.getDefaultUncaughtExceptionHandler();
                Thread.setDefaultUncaughtExceptionHandler(new ChromeUncaughtExceptionHandler());
                ApplicationTestUtils.setUp(InstrumentationRegistry.getTargetContext());

                // Preload Calendar so that it does not trigger ReadFromDisk Strict mode violations
                // if called on the UI Thread. See https://crbug.com/705477 and
                // https://crbug.com/577185
                Calendar.getInstance();

                // Disable offline indicator UI to prevent it from popping up to obstruct other UI
                // views that may make tests flaky.
                Features.getInstance().disable(ChromeFeatureList.OFFLINE_INDICATOR);
                // Tests are run on bots that are offline by default. This might cause offline UI
                // to show and cause flakiness or failures in tests. Using this switch will prevent
                // that.
                // TODO(crbug.com/1093085): Remove this once we disable the offline indicator for
                // specific tests.
                CommandLine.getInstance().appendSwitch(
                        ContentSwitches.FORCE_ONLINE_CONNECTION_STATE_FOR_INDICATOR);

                try {
                    base.evaluate();
                } finally {
                    Thread.setDefaultUncaughtExceptionHandler(mDefaultUncaughtExceptionHandler);
                }
            }
        };
        Statement testServerStatement = mTestServerRule.apply(chromeActivityStatement, description);
        return super.apply(testServerStatement, description);
    }

    /**
     * Return the timeout limit for Chrome activty start in tests
     */
    public static int getActivityStartTimeoutMs() {
        return ACTIVITY_START_TIMEOUT_MS;
    }

    // TODO(yolandyan): remove this once startActivityCompletely is refactored out of
    // ChromeActivityTestRule
    @Override
    public T getActivity() {
        if (mSetActivity != null) {
            return mSetActivity;
        }
        return super.getActivity();
    }

    /** Retrieves the application Menu */
    public Menu getMenu() throws ExecutionException {
        return TestThreadUtils.runOnUiThreadBlocking(
                () -> AppMenuTestSupport.getMenu(getAppMenuCoordinator()));
    }

    /**
     * @return The {@link AppMenuCoordinator} for the activity.
     */
    public AppMenuCoordinator getAppMenuCoordinator() {
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
                ()
                        -> ChromeBrowserInitializer.getInstance().isFullBrowserInitialized(),
                "Native initialization never finished",
                20 * CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL,
                CriteriaHelper.DEFAULT_POLLING_INTERVAL);

        CriteriaHelper.pollUiThread(() -> activity.didFinishNativeInitialization(),
                "Native initialization (of Activity) never finished");
    }

    /**
     * Waits for the activity to fully finish its native initialization.
     */
    public void waitForActivityNativeInitializationComplete() {
        waitForActivityNativeInitializationComplete(getActivity());
    }

    /**
     * Invokes {@link Instrumentation#startActivitySync(Intent)} and sets the
     * test case's activity to the result. See the documentation for
     * {@link Instrumentation#startActivitySync(Intent)} on the timing of the
     * return, but generally speaking the activity's "onCreate" has completed
     * and the activity's main looper has become idle.
     *
     * TODO(yolandyan): very similar to ActivityTestRule#launchActivity(Intent),
     * yet small differences remains (e.g. launchActivity() uses FLAG_ACTIVITY_NEW_TASK while
     * startActivityCompletely doesn't), need to refactor and use only launchActivity
     * after the JUnit4 migration
     */
    public void startActivityCompletely(Intent intent) {
        Features.ensureCommandLineIsUpToDate();

        final CallbackHelper activityCallback = new CallbackHelper();
        final AtomicReference<T> activityRef = new AtomicReference<>();
        ActivityStateListener stateListener = new ActivityStateListener() {
            @SuppressWarnings("unchecked")
            @Override
            public void onActivityStateChange(Activity activity, int newState) {
                if (newState == ActivityState.RESUMED) {
                    if (!mChromeActivityClass.isAssignableFrom(activity.getClass())) return;

                    activityRef.set((T) activity);
                    activityCallback.notifyCalled();
                    ApplicationStatus.unregisterActivityStateListener(this);
                }
            }
        };
        ApplicationStatus.registerStateListenerForAllActivities(stateListener);

        try {
            InstrumentationRegistry.getInstrumentation().startActivitySync(intent);
            activityCallback.waitForCallback("Activity did not start as expected", 0);
            T activity = activityRef.get();
            Assert.assertNotNull("Activity reference is null.", activity);
            setActivity(activity);
            Log.d(TAG, "startActivityCompletely <<");
        } catch (TimeoutException e) {
            throw new RuntimeException(e);
        } finally {
            ApplicationStatus.unregisterActivityStateListener(stateListener);
        }
    }

    /**
     * Enables or disables network predictions, i.e. prerendering, prefetching, DNS preresolution,
     * etc. Network predictions are enabled by default.
     */
    public void setNetworkPredictionEnabled(final boolean enabled) {
        InstrumentationRegistry.getInstrumentation().runOnMainSync(new Runnable() {
            @Override
            public void run() {
                PrivacyPreferencesManager.getInstance().setNetworkPredictionEnabled(enabled);
            }
        });
    }

    /**
     * Navigates to a URL directly without going through the UrlBar. This bypasses the page
     * preloading mechanism of the UrlBar.
     * @param url            The URL to load in the current tab.
     * @param secondsToWait  The number of seconds to wait for the page to be loaded.
     * @return FULL_PRERENDERED_PAGE_LOAD or PARTIAL_PRERENDERED_PAGE_LOAD if the page has been
     *         prerendered. DEFAULT_PAGE_LOAD if it had not.
     */
    public int loadUrl(String url, long secondsToWait) throws IllegalArgumentException {
        return loadUrlInTab(url, PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR,
                getActivity().getActivityTab(), secondsToWait);
    }

    /**
     * Navigates to a URL directly without going through the UrlBar. This bypasses the page
     * preloading mechanism of the UrlBar.
     * @param url The URL to load in the current tab.
     * @return FULL_PRERENDERED_PAGE_LOAD or PARTIAL_PRERENDERED_PAGE_LOAD if the page has been
     *         prerendered. DEFAULT_PAGE_LOAD if it had not.
     */
    public int loadUrl(String url) throws IllegalArgumentException {
        return loadUrlInTab(url, PageTransition.TYPED | PageTransition.FROM_ADDRESS_BAR,
                getActivity().getActivityTab());
    }

    /**
     * @param url            The URL of the page to load.
     * @param pageTransition The type of transition. see
     *                       {@link org.chromium.ui.base.PageTransition}
     *                       for valid values.
     * @param tab            The tab to load the URL into.
     * @param secondsToWait  The number of seconds to wait for the page to be loaded.
     * @return               FULL_PRERENDERED_PAGE_LOAD or PARTIAL_PRERENDERED_PAGE_LOAD if the
     *                       page has been prerendered. DEFAULT_PAGE_LOAD if it had not.
     */
    public int loadUrlInTab(String url, int pageTransition, Tab tab, long secondsToWait) {
        Assert.assertNotNull("Cannot load the URL in a null tab", tab);
        final AtomicInteger result = new AtomicInteger();

        ChromeTabUtils.waitForTabPageLoaded(tab, url, new Runnable() {
            @Override
            public void run() {
                TestThreadUtils.runOnUiThreadBlocking(
                        () -> { result.set(tab.loadUrl(new LoadUrlParams(url, pageTransition))); });
            }
        }, secondsToWait);
        ChromeTabUtils.waitForInteractable(tab);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        return result.get();
    }

    /**
     * @param url            The URL of the page to load.
     * @param pageTransition The type of transition. see
     *                       {@link org.chromium.ui.base.PageTransition}
     *                       for valid values.
     * @param tab            The tab to load the URL into.
     * @return               FULL_PRERENDERED_PAGE_LOAD or PARTIAL_PRERENDERED_PAGE_LOAD if the
     *                       page has been prerendered. DEFAULT_PAGE_LOAD if it had not.
     */
    public int loadUrlInTab(String url, int pageTransition, Tab tab) {
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
        Tab tab = null;
        try {
            tab = TestThreadUtils.runOnUiThreadBlocking(new Callable<Tab>() {
                @Override
                public Tab call() {
                    return getActivity().getTabCreator(incognito).launchUrl(url, launchType);
                }
            });
        } catch (ExecutionException e) {
            Assert.fail("Failed to create new tab");
        }
        ChromeTabUtils.waitForTabPageLoaded(tab, url);
        ChromeTabUtils.waitForInteractable(tab);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        return tab;
    }

    /**
     * Simulates starting Main Activity from launcher, blocks until it is started.
     */
    public void startMainActivityFromLauncher() {
        startMainActivityWithURL(null);
    }

    /**
     * Starts the Main activity on the specified URL. Passing a null URL ensures the default page is
     * loaded, which is the NTP with a new profile .
     */
    public void startMainActivityWithURL(String url) {
        // Only launch Chrome.
        Intent intent =
                new Intent(TextUtils.isEmpty(url) ? Intent.ACTION_MAIN : Intent.ACTION_VIEW);
        intent.addCategory(Intent.CATEGORY_LAUNCHER);
        startMainActivityFromIntent(intent, url);
    }

    /**
     * Starts the Main activity and open a blank page.
     * This is faster and less flakyness-prone than starting on the NTP.
     */
    public void startMainActivityOnBlankPage() {
        startMainActivityWithURL("about:blank");
    }

    /**
     * Starts the Main activity as if it was started from an external application, on the
     * specified URL.
     */
    public void startMainActivityFromExternalApp(String url, String appId) {
        Intent intent = new Intent(Intent.ACTION_VIEW);
        if (appId != null) {
            intent.putExtra(Browser.EXTRA_APPLICATION_ID, appId);
        }
        startMainActivityFromIntent(intent, url);
    }

    /**
     * Starts the Main activity using the passed intent, and using the specified URL.
     * This method waits for DEFERRED_STARTUP to fire as well as a subsequent
     * idle-sync of the main looper thread, and the initial tab must either
     * complete its load or it must crash before this method will return.
     */
    public void startMainActivityFromIntent(Intent intent, String url) {
        prepareUrlIntent(intent, url);

        startActivityCompletely(intent);
        waitForActivityNativeInitializationComplete();

        CriteriaHelper.pollUiThread(
                () -> getActivity().getActivityTab() != null, "Tab never selected/initialized.");
        Tab tab = getActivity().getActivityTab();

        ChromeTabUtils.waitForTabPageLoaded(tab, (String) null);

        if (tab != null && NewTabPage.isNTPUrl(ChromeTabUtils.getUrlStringOnUiThread(tab))
                && !getActivity().isInOverviewMode()) {
            NewTabPageTestUtils.waitForNtpLoaded(tab);
        }

        CriteriaHelper.pollUiThread(
                () -> DeferredStartupHandler.getInstance().isDeferredStartupCompleteForApp(),
                "Deferred startup never completed");

        Assert.assertNotNull(tab);
        Assert.assertNotNull(tab.getView());
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
    }

    /**
     * Prepares a URL intent to start the activity.
     * @param intent the intent to be modified
     * @param url the URL to be used (may be null)
     */
    public Intent prepareUrlIntent(Intent intent, String url) {
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        if (intent.getComponent() == null) {
            intent.setComponent(new ComponentName(
                    InstrumentationRegistry.getTargetContext(), ChromeLauncherActivity.class));
        }

        if (url != null) {
            intent.setData(Uri.parse(url));
        }
        return intent;
    }

    /**
     * Open an incognito tab by invoking the 'new incognito' menu item.
     * Returns when receiving the 'PAGE_LOAD_FINISHED' notification.
     *
     * TODO(yolandyan): split this into the seperate test rule, this only applies to tabbed mode
     */
    public Tab newIncognitoTabFromMenu() {
        final CallbackHelper createdCallback = new CallbackHelper();
        final CallbackHelper selectedCallback = new CallbackHelper();

        TabModel incognitoTabModel = getActivity().getTabModelSelector().getModel(true);
        TabModelObserver observer = new TabModelObserver() {
            @Override
            public void didAddTab(
                    Tab tab, @TabLaunchType int type, @TabCreationState int creationState) {
                createdCallback.notifyCalled();
            }

            @Override
            public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                selectedCallback.notifyCalled();
            }
        };
        incognitoTabModel.addObserver(observer);

        MenuUtils.invokeCustomMenuActionSync(InstrumentationRegistry.getInstrumentation(),
                getActivity(), R.id.new_incognito_tab_menu_id);

        try {
            createdCallback.waitForCallback(0);
        } catch (TimeoutException ex) {
            Assert.fail("Never received tab created event");
        }
        try {
            selectedCallback.waitForCallback(0);
        } catch (TimeoutException ex) {
            Assert.fail("Never received tab selected event");
        }
        incognitoTabModel.removeObserver(observer);

        Tab tab = getActivity().getActivityTab();

        ChromeTabUtils.waitForTabPageLoaded(tab, (String) null);
        NewTabPageTestUtils.waitForNtpLoaded(tab);
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();
        Log.d(TAG, "newIncognitoTabFromMenu <<");
        return tab;
    }

    /**
     * New multiple incognito tabs by invoking the 'new incognito' menu item n times.
     * @param n The number of tabs you want to create.
     *
     * TODO(yolandyan): split this into the seperate test rule, this only applies to tabbed mode
     */
    public void newIncognitoTabsFromMenu(int n) {
        while (n > 0) {
            newIncognitoTabFromMenu();
            --n;
        }
    }

    /**
     * @return The number of tabs currently open.
     */
    public int tabsCount(boolean incognito) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<Integer>() {
            @Override
            public Integer call() {
                return getActivity().getTabModelSelector().getModel(incognito).getCount();
            }
        });
    }

    /**
     * Looks up the Omnibox in the view hierarchy and types the specified
     * text into it, requesting focus and using an inter-character delay of
     * 200ms.
     *
     * @param oneCharAtATime Whether to type text one character at a time or all at once.
     *
     * @throws InterruptedException
     *
     * TODO(yolandyan): split this into the seperate test rule, this only applies to tabbed mode
     */
    public void typeInOmnibox(String text, boolean oneCharAtATime) throws InterruptedException {
        final UrlBar urlBar = getActivity().findViewById(R.id.url_bar);
        Assert.assertNotNull(urlBar);

        WaitForFocusHelper.acquireFocusForView(urlBar);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            if (!oneCharAtATime) {
                urlBar.setText(text);
            }
        });

        if (oneCharAtATime) {
            final Instrumentation instrumentation = InstrumentationRegistry.getInstrumentation();
            for (int i = 0; i < text.length(); ++i) {
                instrumentation.sendStringSync(text.substring(i, i + 1));
                // Let's put some delay between key strokes to simulate a user pressing the keys.
                Thread.sleep(20);
            }
        }
    }

    /**
     * Returns the infobars being displayed by the current tab, or null if they don't exist.
     */
    public List<InfoBar> getInfoBars() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(new Callable<List<InfoBar>>() {
            @Override
            public List<InfoBar> call() {
                Tab currentTab = getActivity().getActivityTab();
                Assert.assertNotNull(currentTab);
                Assert.assertNotNull(InfoBarContainer.get(currentTab));
                return InfoBarContainer.get(currentTab).getInfoBarsForTesting();
            }
        });
    }

    /**
     * Executes the given snippet of JavaScript code within the current tab. Returns the result of
     * its execution in JSON format.
     */
    public String runJavaScriptCodeInCurrentTab(String code) throws TimeoutException {
        return JavaScriptUtils.executeJavaScriptAndWaitForResult(
                getActivity().getCurrentWebContents(), code);
    }

    /**
     * Waits till the WebContents receives the expected page scale factor
     * from the compositor and asserts that this happens.
     */
    public void assertWaitForPageScaleFactorMatch(float expectedScale) {
        ApplicationTestUtils.assertWaitForPageScaleFactorMatch(getActivity(), expectedScale);
    }

    public String getName() {
        return mCurrentTestName;
    }

    public String getTestName() {
        return mCurrentTestName;
    }

    /**
     * @return {@link InfoBarContainer} of the active tab of the activity.
     *     {@code null} if there is no tab for the activity or infobar is available.
     */
    public InfoBarContainer getInfoBarContainer() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> getActivity().getActivityTab() != null
                        ? InfoBarContainer.get(getActivity().getActivityTab())
                        : null);
    }

    /**
     * Gets the ChromeActivityTestRule's EmbeddedTestServer instance if it has one.
     */
    public EmbeddedTestServer getTestServer() {
        return mTestServerRule.getServer();
    }

    /**
     * Gets the underlying EmbeddedTestServerRule for getTestServer().
     */
    public EmbeddedTestServerRule getEmbeddedTestServerRule() {
        return mTestServerRule;
    }

    /**
     * @return {@link WebContents} of the active tab of the activity.
     */
    public WebContents getWebContents() {
        return getActivity().getActivityTab().getWebContents();
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

    public void setActivity(T chromeActivity) {
        mSetActivity = chromeActivity;
    }

    /**
     * Waits for an Activity of the given class to be started.
     * @return The Activity.
     */
    @SuppressWarnings("unchecked")
    public static <T extends ChromeActivity> T waitFor(final Class<T> expectedClass) {
        final Activity[] holder = new Activity[1];
        CriteriaHelper.pollUiThread(() -> {
            holder[0] = ApplicationStatus.getLastTrackedFocusedActivity();
            Criteria.checkThat(holder[0], Matchers.notNullValue());
            Criteria.checkThat(holder[0].getClass(), Matchers.typeCompatibleWith(expectedClass));
            Criteria.checkThat(
                    ((ChromeActivity) holder[0]).getActivityTab(), Matchers.notNullValue());
        });
        return (T) holder[0];
    }

    private class ChromeUncaughtExceptionHandler implements Thread.UncaughtExceptionHandler {
        @Override
        public void uncaughtException(Thread t, Throwable e) {
            String stackTrace = android.util.Log.getStackTraceString(e);
            if (e.getClass().getName().endsWith("StrictModeViolation")) {
                stackTrace += "\nSearch logcat for \"StrictMode policy violation\" for full stack.";
            }
            Bundle resultsBundle = new Bundle();
            resultsBundle.putString(
                    InstrumentationResultPrinter.REPORT_KEY_NAME_CLASS, getClass().getName());
            resultsBundle.putString(
                    InstrumentationResultPrinter.REPORT_KEY_NAME_TEST, mCurrentTestName);
            resultsBundle.putString(InstrumentationResultPrinter.REPORT_KEY_STACK, stackTrace);
            InstrumentationRegistry.getInstrumentation().sendStatus(-1, resultsBundle);
            mDefaultUncaughtExceptionHandler.uncaughtException(t, e);
        }
    }
}
