// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.util;

import static org.hamcrest.Matchers.is;

import android.app.Instrumentation;
import android.text.TextUtils;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.hamcrest.Matchers;
import org.junit.Assert;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TriState;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.test.transit.CallbackCondition;
import org.chromium.base.test.transit.Condition;
import org.chromium.base.test.transit.Triggers;
import org.chromium.base.test.transit.TripBuilder;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.ntp.IncognitoNewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabWebContentsObserver;
import org.chromium.chrome.browser.tabmodel.TabClosingSource;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.url_constants.UrlOverrideUtils;
import org.chromium.chrome.test.transit.page.PageHitTestDataCondition;
import org.chromium.chrome.test.transit.page.PageInteractableOrHiddenCondition;
import org.chromium.chrome.test.transit.page.PageLoadedCondition;
import org.chromium.chrome.test.transit.page.PageTitleCondition;
import org.chromium.chrome.test.transit.page.PageUrlCondition;
import org.chromium.chrome.test.transit.tabmodel.TabClosingCondition;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.RenderWidgetHostView;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/** A utility class that contains methods generic to all Tabs tests. */
public class ChromeTabUtils {
    private static final String TAG = "ChromeTabUtils";
    public static final int TITLE_UPDATE_TIMEOUT_SECONDS = 3;

    /**
     * The required page load percentage for the page to be considered ready assuming the
     * TextureView is also ready.
     */
    private static final float CONSIDERED_READY_LOAD_PERCENTAGE = 1;

    /**
     * Fulfilled when the Tab calls onLoadStopped() for the expected URL (or any URL if null) after
     * monitoring starts, so that the state from before a load trigger isn't mistaken for the new
     * load. Unlike PageLoadCallbackCondition, this also covers same-document and failed loads.
     */
    private static class TabLoadStoppedCondition extends CallbackCondition implements TabObserver {
        private final Tab mTab;
        private final @Nullable String mExpectedUrl;

        TabLoadStoppedCondition(Tab tab, @Nullable String expectedUrl) {
            super(
                    expectedUrl == null ? "onLoadStopped" : "onLoadStopped for " + expectedUrl,
                    /* numCallsExpected= */ 1);
            mTab = tab;
            mExpectedUrl = expectedUrl;
        }

        @Override
        public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
            if (mExpectedUrl == null || TextUtils.equals(tab.getUrl().getSpec(), mExpectedUrl)) {
                notifyCalled();
            }
        }

        @Override
        public void onStartMonitoring() {
            super.onStartMonitoring();
            ThreadUtils.runOnUiThreadBlocking(() -> mTab.addObserver(this));
        }

        @Override
        public void onStopMonitoring() {
            super.onStopMonitoring();
            ThreadUtils.runOnUiThreadBlocking(() -> mTab.removeObserver(this));
        }
    }

    /**
     * Fulfilled when the Tab calls onPageLoadStarted() for the expected URL (or any URL if null).
     */
    private static class TabLoadStartedCondition extends CallbackCondition implements TabObserver {
        private final Tab mTab;
        private final @Nullable String mExpectedUrl;

        TabLoadStartedCondition(Tab tab, @Nullable String expectedUrl) {
            super(
                    expectedUrl == null
                            ? "onPageLoadStarted"
                            : "onPageLoadStarted for " + expectedUrl,
                    /* numCallsExpected= */ 1);
            mTab = tab;
            mExpectedUrl = expectedUrl;
        }

        @Override
        public void onPageLoadStarted(Tab tab, GURL url) {
            if (mExpectedUrl == null || TextUtils.equals(url.getSpec(), mExpectedUrl)) {
                notifyCalled();
            }
        }

        @Override
        public void onStartMonitoring() {
            super.onStartMonitoring();
            ThreadUtils.runOnUiThreadBlocking(() -> mTab.addObserver(this));
        }

        @Override
        public void onStopMonitoring() {
            super.onStopMonitoring();
            ThreadUtils.runOnUiThreadBlocking(() -> mTab.removeObserver(this));
        }
    }

    public static Tab getActivityTab(ChromeActivity activity) {
        return ThreadUtils.runOnUiThreadBlocking(() -> activity.getActivityTab());
    }

    public static int getIndexOnUiThread(TabModel tabModel) {
        return ThreadUtils.runOnUiThreadBlocking(() -> tabModel.index());
    }

    public static int getTabCountOnUiThread(TabModel tabModel) {
        return ThreadUtils.runOnUiThreadBlocking(() -> tabModel.getCount());
    }

    public static String getTitleOnUiThread(Tab tab) {
        return ThreadUtils.runOnUiThreadBlocking(() -> tab.getTitle());
    }

    public static String getUrlStringOnUiThread(Tab tab) {
        return ThreadUtils.runOnUiThreadBlocking(() -> tab.getUrl().getSpec());
    }

    public static GURL getUrlOnUiThread(Tab tab) {
        return ThreadUtils.runOnUiThreadBlocking(() -> tab.getUrl());
    }

    /**
     * Waits for the given tab to finish loading the given URL, or, if the given URL is null, waits
     * for the current page to load.
     *
     * @param tab The tab to wait for the page loading to be complete.
     * @param url The URL that will be waited to load for. Pass in null if loading the current page
     *     is sufficient.
     * @deprecated See {@link #waitForTabPageLoaded(Tab, String, Runnable, long)}.
     */
    @Deprecated
    public static void waitForTabPageLoaded(final Tab tab, @Nullable final String url) {
        waitForTabPageLoaded(tab, url, null, 10L);
    }

    /**
     * Waits for the given tab to load the given URL, or, if the given URL is null, waits for the
     * triggered load to complete.
     *
     * @param tab The tab to wait for the page loading to be complete.
     * @param url The expected url of the loaded page. Pass in null if loading the current page is
     *     sufficient.
     * @param loadTrigger The trigger action that will result in a page load finished event to be
     *     fired (not run on the UI thread by default).
     * @deprecated See {@link #waitForTabPageLoaded(Tab, String, Runnable, long)}.
     */
    @Deprecated
    public static void waitForTabPageLoaded(
            final Tab tab, @Nullable final String url, @Nullable Runnable loadTrigger) {
        waitForTabPageLoaded(tab, url, loadTrigger, CallbackHelper.WAIT_TIMEOUT_SECONDS);
    }

    /**
     * Waits for the given tab to finish loading its current page.
     *
     * @param tab The tab to wait for the page loading to be complete.
     * @param loadTrigger The trigger action that will result in a page load finished event to be
     *     fired (not run on the UI thread by default).
     * @param secondsToWait The number of seconds to wait for the page to be loaded.
     * @deprecated See {@link #waitForTabPageLoaded(Tab, String, Runnable, long)}.
     */
    @Deprecated
    public static void waitForTabPageLoaded(
            final Tab tab, Runnable loadTrigger, long secondsToWait) {
        waitForTabPageLoaded(tab, null, loadTrigger, secondsToWait);
    }

    /**
     * Waits for the given tab to load the given URL, or, if the given URL is null, waits for the
     * triggered load to complete.
     *
     * @param tab The tab to wait for the page loading to be complete.
     * @param url The expected url of the loaded page. Pass in null if loading the current page is
     *     sufficient.
     * @param loadTrigger The trigger action that will result in a page load finished event to be
     *     fired (not run on the UI thread by default). Pass in null if the load is triggered
     *     externally.
     * @param secondsToWait The number of seconds to wait for the page to be loaded.
     * @deprecated Prefer Station transitions. From a Station, use {@code
     *     loadWebPageProgrammatically()} or {@code runTo(trigger).arriveAt(WebPageStation...)}.
     *     From a test without a Station, use {@code runTo(trigger).hopOnTo(activity,
     *     WebPageStation/CctPageStation...)}. Kept for callers that cannot hop on, such as
     *     ChromeActivityTestRule, newTabFromMenu and webapps.
     */
    @Deprecated
    public static void waitForTabPageLoaded(
            final Tab tab,
            @Nullable final String url,
            @Nullable Runnable loadTrigger,
            long secondsToWait) {
        Assert.assertFalse(ThreadUtils.runningOnUiThread());

        List<Condition> conditions = new ArrayList<>();
        conditions.add(new PageLoadedCondition(() -> tab, TriState.NOT_SET));
        if (url != null) conditions.add(new PageUrlCondition(is(url), () -> tab));
        // With a trigger, also wait for the load it starts to stop, so the state from before the
        // trigger isn't mistaken for it. Without one, the load may already be complete.
        if (loadTrigger != null) conditions.add(new TabLoadStoppedCondition(tab, url));
        TripBuilder trip = loadTrigger == null ? Triggers.noopTo() : Triggers.runTo(loadTrigger);
        trip.withTimeout(TimeUnit.SECONDS.toMillis(secondsToWait))
                .waitFor(conditions.toArray(new Condition[0]));
    }

    /**
     * Waits for the given tab to start loading its current page.
     *
     * @param tab The tab to wait for the page loading to be started.
     * @param expectedUrl The expected url of the started page load. Pass in null if starting any
     *     load is sufficient.
     * @param loadTrigger The trigger action that will result in a page load started event to be
     *     fired (not run on the UI thread by default).
     */
    public static void waitForTabPageLoadStart(
            final Tab tab, @Nullable final String expectedUrl, Runnable loadTrigger) {
        waitForTabPageLoadStart(tab, expectedUrl, loadTrigger, CallbackHelper.WAIT_TIMEOUT_SECONDS);
    }

    /**
     * Waits for the given tab to start loading its current page.
     *
     * @param tab The tab to wait for the page loading to be started.
     * @param expectedUrl The expected url of the started page load. Pass in null if starting any
     *     load is sufficient.
     * @param loadTrigger The trigger action that will result in a page load started event to be
     *     fired (not run on the UI thread by default).
     * @param secondsToWait The number of seconds to wait for the page to be load to be started.
     */
    public static void waitForTabPageLoadStart(
            final Tab tab,
            @Nullable final String expectedUrl,
            Runnable loadTrigger,
            long secondsToWait) {
        Triggers.runTo(loadTrigger)
                .withTimeout(TimeUnit.SECONDS.toMillis(secondsToWait))
                .waitFor(new TabLoadStartedCondition(tab, expectedUrl));
    }

    /**
     * Waits for the tab to become interactable. This occurs after load, once all view animations
     * have completed.
     *
     * @param tab The tab to wait for interactability on.
     */
    public static void waitForInteractable(final Tab tab) {
        Assert.assertFalse(ThreadUtils.runningOnUiThread());

        Triggers.noopTo()
                .withTimeout(10000L)
                .waitFor(
                        new PageInteractableOrHiddenCondition(() -> tab),
                        new PageHitTestDataCondition(() -> tab));
    }

    /** Switch to the given TabIndex in the current tabModel. */
    public static void switchTabInCurrentTabModel(
            final ChromeActivity activity, final int tabIndex) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabModelUtils.setIndex(activity.getCurrentTabModel(), tabIndex);
                });
    }

    /**
     * Creates a new tab by invoking the 'New Tab' menu item.
     *
     * <p>Returns when the tab has been created and has finished navigating.
     */
    public static void newTabFromMenu(
            Instrumentation instrumentation, final ChromeActivity activity) {
        newTabFromMenu(instrumentation, activity, false, true);
    }

    /**
     * Creates a new tab by invoking the 'New Tab' or 'New Incognito Tab' menu item.
     * <p>
     * Returns when the tab has been created and has finished navigating.
     */
    public static void newTabFromMenu(
            Instrumentation instrumentation,
            final ChromeActivity activity,
            boolean incognito,
            boolean waitForNtpLoad) {
        final CallbackHelper createdCallback = new CallbackHelper();
        final CallbackHelper selectedCallback = new CallbackHelper();

        TabModel tabModel = activity.getTabModelSelector().getModel(incognito);
        TabModelObserver observer =
                new TabModelObserver() {
                    @Override
                    public void didAddTab(
                            Tab tab,
                            @TabLaunchType int type,
                            @TabCreationState int creationState,
                            boolean markedForSelection) {
                        createdCallback.notifyCalled();
                    }

                    @Override
                    public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                        selectedCallback.notifyCalled();
                    }
                };
        ThreadUtils.runOnUiThreadBlocking(() -> tabModel.addObserver(observer));

        MenuUtils.invokeCustomMenuActionSync(
                instrumentation,
                activity,
                incognito ? R.id.new_incognito_tab_menu_id : R.id.new_tab_menu_id);

        try {
            createdCallback.waitForCallback(0);
        } catch (TimeoutException ex) {
            throw new AssertionError("Never received tab created event", ex);
        }
        try {
            selectedCallback.waitForCallback(0);
        } catch (TimeoutException ex) {
            throw new AssertionError("Never received tab selected event", ex);
        }
        ThreadUtils.runOnUiThreadBlocking(() -> tabModel.removeObserver(observer));

        Tab tab = getActivityTab(activity);
        waitForTabPageLoaded(tab, (String) null);
        if (waitForNtpLoad) waitForNtpLoaded(tab);
        instrumentation.waitForIdleSync();
        Log.d(TAG, "newTabFromMenu <<");
    }

    /**
     * Creates a new tab in the specified model then waits for it to load.
     *
     * <p>Returns when the tab has been created and finishes loading.
     *
     * @return Newly created Tab object.
     * @deprecated From a page Station, use {@code openWebPageInNewTab(url)}, which like this method
     *     appends the new tab at the end of the model. Use {@code openFakeLinkToWebPage(url)} to
     *     open it next to the current tab instead, or {@code
     *     Journeys.createRegularTabsWithWebPages()} / {@code
     *     Journeys.createIncognitoTabsWithWebPages()} to open one or more tabs with web pages.
     */
    @Deprecated
    public static Tab fullyLoadUrlInNewTab(
            Instrumentation instrumentation,
            final ChromeTabbedActivity activity,
            final String url,
            final boolean incognito) {
        newTabFromMenu(instrumentation, activity, incognito, false);

        final Tab tab = getActivityTab(activity);
        waitForTabPageLoaded(
                tab,
                url,
                new Runnable() {
                    @Override
                    public void run() {
                        loadUrlOnUiThread(tab, url);
                    }
                });
        waitForInteractable(tab);
        return tab;
    }

    /**
     * Starts loading {@code url} in {@code tab} without waiting for the load.
     *
     * <p>Use this for loads that aren't expected to finish normally, e.g. error or interstitial
     * pages, auth prompts, or slow pages in a background tab. For loads expected to finish, use
     * {@code CtaPageStation#loadWebPageProgrammatically()} from a page Station, or without a
     * Station hop on with {@code Triggers.runTo(trigger).hopOnTo(activity, WebPageStation...)}
     * ({@code CctPageStation} for Custom Tabs), which also wait for the page.
     */
    public static void loadUrlOnUiThread(final Tab tab, final String url) {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    tab.loadUrl(new LoadUrlParams(url));
                });
    }

    /** Fetch the number of tabs open in the current model. */
    public static int getNumOpenTabs(final ChromeActivity activity) {
        return ThreadUtils.runOnUiThreadBlocking(
                new Callable<>() {
                    @Override
                    public Integer call() {
                        return activity.getCurrentTabModel().getCount();
                    }
                });
    }

    /**
     * Closes the current tab through TabModelSelector.
     *
     * <p>Returns after the tab has been closed.
     */
    public static void closeCurrentTab(
            final Instrumentation instrumentation, final ChromeActivity activity) {
        closeCurrentTab(instrumentation, activity, TabClosingSource.UNKNOWN);
    }

    /**
     * Closes the current tab through TabModelSelector with an explicit closing source.
     *
     * <p>Returns after the tab has been closed.
     */
    public static void closeCurrentTab(
            final Instrumentation instrumentation,
            final ChromeActivity activity,
            @TabClosingSource int source) {
        TabModel model = ThreadUtils.runOnUiThreadBlocking(activity::getCurrentTabModel);
        Triggers.runOnUiThreadTo(
                        () -> {
                            Tab tab = TabModelUtils.getCurrentTab(model);
                            if (tab == null) return;
                            model.getTabRemover()
                                    .closeTabs(
                                            TabClosureParams.closeTab(tab)
                                                    .allowUndo(false)
                                                    .tabClosingSource(source)
                                                    .build(),
                                            /* allowDialog= */ false);
                        })
                .waitFor(new TabClosingCondition(() -> model));
        instrumentation.waitForIdleSync();
    }

    /** Close all tabs and waits for all tabs pending closure to be observed. */
    public static void closeAllTabs(
            Instrumentation instrumentation,
            MonotonicObservableSupplier<TabModelSelector> tabModelSelectorSupplier) {
        final CallbackHelper closeCallback = new CallbackHelper();
        final TabModelObserver observer =
                new TabModelObserver() {
                    @Override
                    public void onTabClosePending(
                            List<Tab> tabs,
                            boolean isAllTabs,
                            @TabClosingSource int closingSource) {
                        closeCallback.notifyCalled();
                    }
                };
        instrumentation.runOnMainSync(
                new Runnable() {
                    @Override
                    public void run() {
                        TabModelSelector selector = tabModelSelectorSupplier.get();
                        for (TabModel tabModel : selector.getModels()) {
                            tabModel.addObserver(observer);
                        }
                    }
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    TabClosureParams params =
                            TabClosureParams.closeAllTabs()
                                    .uponExit(false)
                                    .tabClosingSource(TabClosingSource.GRID_TAB_SWITCHER)
                                    .build();
                    TabModelSelector selector = tabModelSelectorSupplier.get();
                    selector.getModel(false)
                            .getTabRemover()
                            .closeTabs(params, /* allowDialog= */ false);
                    selector.getModel(true)
                            .getTabRemover()
                            .closeTabs(params, /* allowDialog= */ false);
                });

        try {
            closeCallback.waitForCallback(0);
        } catch (TimeoutException e) {
            throw new AssertionError("All tabs pending closure event was never received", e);
        }
        instrumentation.runOnMainSync(
                new Runnable() {
                    @Override
                    public void run() {
                        TabModelSelector selector = tabModelSelectorSupplier.get();
                        for (TabModel tabModel : selector.getModels()) {
                            tabModel.removeObserver(observer);
                        }
                    }
                });
        instrumentation.waitForIdleSync();
    }

    /**
     * @deprecated Transitory method, use {@link #closeAllTabs(Instrumentation,
     *     MonotonicObservableSupplier <TabModelSelector>)} instead. TODO(crbug.com/40191386): Remove this
     *     after the usages are migrated.
     */
    public static void closeAllTabs(
            Instrumentation instrumentation, final ChromeTabbedActivity activity) {
        closeAllTabs(instrumentation, activity.getTabModelSelectorSupplier());
    }

    /**
     * @param windowAndroid the WindowAndroid used to acquire the TabModelSelector.
     * @return The TabModelSelector used with the supplied WindowAndroid.
     */
    public static @NonNull TabModelSelector getTabModelSelector(WindowAndroid windowAndroid) {
        Assert.assertTrue(ThreadUtils.runningOnUiThread());
        Assert.assertNotNull(windowAndroid);

        final MonotonicObservableSupplier<TabModelSelector> supplier =
                TabModelSelectorSupplier.from(windowAndroid);
        Assert.assertNotNull(supplier);

        final TabModelSelector selector = supplier.get();
        Assert.assertNotNull(selector);
        return selector;
    }

    /**
     * Groups together two tabs.
     *
     * @param tab1 First tab to group.
     * @param tab2 Second tab to group.\
     */
    public static void mergeTabsToGroup(Tab tab1, Tab tab2) {
        Assert.assertTrue(ThreadUtils.runningOnUiThread());

        // Verify that the two tabs do not belong with different models.
        Assert.assertEquals(tab1.isIncognito(), tab2.isIncognito());
        final TabModelSelector selector = getTabModelSelector(tab1.getWindowAndroid());
        final TabModel tabModel = selector.getModel(tab1.isIncognito());

        tabModel.mergeTabsToGroup(tab1.getId(), tab2.getId());
        Assert.assertEquals(tab1.getTabGroupId(), tab2.getTabGroupId());
    }

    /**
     * Issues a fake notification about the renderer being killed.
     *
     * @param tab {@link Tab} instance where the target renderer resides.
     */
    public static void simulateRendererKilledForTesting(Tab tab) {
        TabWebContentsObserver observer = TabWebContentsObserver.get(tab);
        if (observer != null) {
            observer.simulateRendererKilledForTesting();
        }
    }

    public static void waitForTitle(Tab tab, String newTitle) {
        Triggers.noopTo()
                .withTimeout(TimeUnit.SECONDS.toMillis(TITLE_UPDATE_TIMEOUT_SECONDS))
                .waitFor(new PageTitleCondition(newTitle, () -> tab));
    }

    /**
     * @return Whether or not the loading and rendering of the page is done.
     */
    public static boolean isLoadingAndRenderingDone(Tab tab) {
        return isRendererReady(tab) && tab.getProgress() >= CONSIDERED_READY_LOAD_PERCENTAGE;
    }

    /**
     * @return Whether or not the tab has something valid to render.
     */
    public static boolean isRendererReady(Tab tab) {
        if (tab.getNativePage() != null) return true;
        WebContents webContents = tab.getWebContents();
        if (webContents == null) return false;

        RenderWidgetHostView rwhv = webContents.getRenderWidgetHostView();
        return rwhv != null && rwhv.isReady();
    }

    /**
     * Waits for the NTP owned by the passed in tab to be fully loaded.
     *
     * @param tab The tab to be monitored for NTP loading.
     * @deprecated Use {@code NewTabPageTestUtils#waitForNtpLoaded(Tab)}, which hops on Public
     *     Transit at the NTP Station, or start at the NTP with an entry point such as {@code
     *     startOnNtp()}.
     */
    // TODO(crbug.com/428022848): Remove once legacy tests that start on the NTP or open NTPs via
    // the *FromMenu() helpers use a Public Transit entry point or hop on.
    @Deprecated
    public static void waitForNtpLoaded(final Tab tab) {
        CriteriaHelper.pollUiThread(
                () -> {
                    if (!tab.isIncognito()) {
                        // If the WebUI NTP override is enabled, check that the renderer is ready.
                        // Otherwise, check that the NTP is loaded in the native page.
                        if (UrlOverrideUtils.isWebUiNtpOverrideEnabled()) {
                            Criteria.checkThat(
                                    ChromeTabUtils.isRendererReady(tab), Matchers.is(true));
                        } else {
                            Criteria.checkThat(
                                    tab.getNativePage(), Matchers.instanceOf(NewTabPage.class));
                            Criteria.checkThat(
                                    ((NewTabPage) tab.getNativePage()).isLoadedForTests(),
                                    Matchers.is(true));
                        }
                    } else {
                        Criteria.checkThat(
                                tab.getNativePage(),
                                Matchers.instanceOf(IncognitoNewTabPage.class));
                        Criteria.checkThat(
                                ((IncognitoNewTabPage) tab.getNativePage()).isLoadedForTests(),
                                Matchers.is(true));
                    }
                });
    }
}
