// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.NewWindowAppSource;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.BackPressResult;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.base.PageTransition;

import java.util.Collections;
import java.util.function.Supplier;

/** Implementation of {@link ToolbarTabController}. */
@NullMarked
public class ToolbarTabControllerImpl implements ToolbarTabController {
    private final Supplier<@Nullable Tab> mTabSupplier;
    private final Supplier<@Nullable Tracker> mTrackerSupplier;
    private final Supplier<@Nullable BackPressHandler> mBottomControlsBackPressHandlerSupplier;
    private final Supplier<String> mHomepageUrlSupplier;
    private final Runnable mOnSuccessRunnable;
    private final Supplier<@Nullable Tab> mActivityTabSupplier;
    private final TabCreatorManager mTabCreatorManager;
    private final @Nullable MultiInstanceManager mMultiInstanceManager;
    private final Supplier<Boolean> mIsOffTheRecordSupplier;

    /**
     * @param tabSupplier Supplier for the currently active tab.
     * @param trackerSupplier Supplier for the current profile tracker.
     * @param bottomControlsBackPressHandlerSupplier Supplier for the bottom controls back press
     *     handler.
     * @param homepageUrlSupplier Supplier for the homepage URL.
     * @param onSuccessRunnable Runnable that is invoked when the active tab is asked to perform the
     *     corresponding ToolbarTabController action; it is not invoked if the tab cannot
     * @param activityTabSupplier Supplier for the currently active and interactable tab. Both
     *     tabSupplier and activityTabSupplier can return the same tab if tab is active and
     *     interactable. But activityTabSupplier will return null if it is non-interactable, such as
     *     on overview mode.
     * @param tabCreatorManager The {@link TabCreatorManager} used to create new tabs.
     * @param multiInstanceManager The {@link MultiInstanceManager} used to move tabs to new windows
     * @param isOffTheRecordSupplier Supplier for whether the current UI is off-the-record.
     */
    public ToolbarTabControllerImpl(
            Supplier<@Nullable Tab> tabSupplier,
            Supplier<@Nullable Tracker> trackerSupplier,
            Supplier<@Nullable BackPressHandler> bottomControlsBackPressHandlerSupplier,
            Supplier<String> homepageUrlSupplier,
            Runnable onSuccessRunnable,
            Supplier<@Nullable Tab> activityTabSupplier,
            TabCreatorManager tabCreatorManager,
            @Nullable MultiInstanceManager multiInstanceManager,
            Supplier<Boolean> isOffTheRecordSupplier) {
        mTabSupplier = tabSupplier;
        mTrackerSupplier = trackerSupplier;
        mBottomControlsBackPressHandlerSupplier = bottomControlsBackPressHandlerSupplier;
        mHomepageUrlSupplier = homepageUrlSupplier;
        mOnSuccessRunnable = onSuccessRunnable;
        mActivityTabSupplier = activityTabSupplier;
        mTabCreatorManager = tabCreatorManager;
        mMultiInstanceManager = multiInstanceManager;
        mIsOffTheRecordSupplier = isOffTheRecordSupplier;
    }

    @Override
    public boolean back() {
        BackPressHandler bottomControlsBackPressHandler =
                mBottomControlsBackPressHandlerSupplier.get();
        if (bottomControlsBackPressHandler != null
                && bottomControlsBackPressHandler.handleBackPress() == BackPressResult.SUCCESS) {
            return true;
        }
        Tab tab = mActivityTabSupplier.get();
        if (tab != null && tab.canGoBack()) {
            NativePage nativePage = tab.getNativePage();
            if (nativePage != null) {
                nativePage.notifyHidingWithBack();
            }

            tab.goBack();
            mOnSuccessRunnable.run();
            return true;
        }
        return false;
    }

    @Override
    public boolean backInNewTab(boolean foregroundNewTab) {
        Tab tab = mTabSupplier.get();
        if (tab != null && tab.canGoBack()) {
            @Nullable Tab newTab = createTabWithHistory(tab, foregroundNewTab);
            if (newTab == null) return false;
            newTab.goBack();
            // Don't run mOnSuccessRunnable since nothing happened in the current tab.
            return true;
        }
        return false;
    }

    @Override
    public boolean backInNewWindow() {
        Tab tab = mTabSupplier.get();
        if (tab != null && tab.canGoBack()) {
            @Nullable Tab newTab = createTabWithHistory(tab, /* foregroundNewTab= */ false);
            if (newTab == null) return false;
            newTab.goBack();
            if (mMultiInstanceManager == null) return false;
            // Move tab to a new window.
            mMultiInstanceManager.moveTabsToNewWindow(
                    Collections.singletonList(newTab),
                    /* finalizeCallback= */ null,
                    NewWindowAppSource.KEYBOARD_SHORTCUT);
            // Don't run mOnSuccessRunnable since nothing happened in the current tab.
            return true;
        }
        return false;
    }

    @Override
    public boolean forward() {
        Tab tab = mTabSupplier.get();
        if (tab != null && tab.canGoForward()) {
            tab.goForward();
            mOnSuccessRunnable.run();
            return true;
        }
        return false;
    }

    @Override
    public boolean forwardInNewTab(boolean foregroundNewTab) {
        Tab tab = mTabSupplier.get();
        if (tab != null && tab.canGoForward()) {
            @Nullable Tab newTab = createTabWithHistory(tab, foregroundNewTab);
            if (newTab == null) return false;
            newTab.goForward();
            // Don't run mOnSuccessRunnable since nothing happened in the current tab.
            return true;
        }
        return false;
    }

    @Override
    public boolean forwardInNewWindow() {
        Tab tab = mTabSupplier.get();
        if (tab != null && tab.canGoForward()) {
            @Nullable Tab newTab = createTabWithHistory(tab, /* foregroundNewTab= */ false);
            if (newTab == null) return false;
            newTab.goForward();
            if (mMultiInstanceManager == null) return false;
            // Move tab to a new window.
            mMultiInstanceManager.moveTabsToNewWindow(
                    Collections.singletonList(newTab),
                    /* finalizeCallback= */ null,
                    NewWindowAppSource.KEYBOARD_SHORTCUT);
            // Don't run mOnSuccessRunnable since nothing happened in the current tab.
            return true;
        }
        return false;
    }

    private @Nullable Tab createTabWithHistory(Tab tab, boolean foregroundNewTab) {
        return mTabCreatorManager
                .getTabCreator(tab.isOffTheRecord())
                .createTabWithHistory(
                        tab,
                        foregroundNewTab
                                ? TabLaunchType.FROM_HISTORY_NAVIGATION_FOREGROUND
                                : TabLaunchType.FROM_HISTORY_NAVIGATION_BACKGROUND);
    }

    @Override
    public void stopOrReloadCurrentTab(boolean ignoreCache) {
        Tab currentTab = mTabSupplier.get();
        if (currentTab == null) return;

        if (currentTab.isLoading()) {
            currentTab.stopLoading();
            RecordUserAction.record("MobileToolbarStop");
        } else {
            if (ignoreCache) {
                currentTab.reloadIgnoringCache();
            } else {
                currentTab.reload();
            }
            RecordUserAction.record("MobileToolbarReload");
        }
        mOnSuccessRunnable.run();
    }

    @Override
    public void openHomepage() {
        RecordUserAction.record("Home");
        String homePageUrl = mHomepageUrlSupplier.get();
        recordHomeButtonMetrics(homePageUrl);

        recordHomeButtonUseForIph();

        Tab currentTab = mTabSupplier.get();
        if (currentTab != null) {
            currentTab.loadUrl(new LoadUrlParams(homePageUrl, PageTransition.HOME_PAGE));
        } else {
            // Fallback: If there's no active tab (e.g. from tab switcher), open in a new tab
            // instead.
            mTabCreatorManager
                    .getTabCreator(mIsOffTheRecordSupplier.get())
                    .createNewTab(
                            new LoadUrlParams(homePageUrl, PageTransition.HOME_PAGE),
                            TabLaunchType.FROM_CHROME_UI,
                            null);
        }
    }

    @Override
    public void openHomepageInNewTab(boolean foregroundNewTab) {
        RecordUserAction.record(
                foregroundNewTab ? "HomeInNewForegroundTab" : "HomeInNewBackgroundTab");
        String homePageUrl = mHomepageUrlSupplier.get();
        recordHomeButtonMetrics(homePageUrl);

        recordHomeButtonUseForIph();
        Tab currentTab = mTabSupplier.get();
        boolean isOffTheRecord =
                currentTab != null ? currentTab.isOffTheRecord() : mIsOffTheRecordSupplier.get();
        mTabCreatorManager
                .getTabCreator(isOffTheRecord)
                .createNewTab(
                        new LoadUrlParams(homePageUrl, PageTransition.HOME_PAGE),
                        foregroundNewTab
                                ? TabLaunchType.FROM_CHROME_UI
                                : TabLaunchType.FROM_LONGPRESS_BACKGROUND,
                        currentTab);
    }

    @Override
    public void openHomepageInNewWindow() {
        RecordUserAction.record("HomeInNewForegroundWindow");
        String homePageUrl = mHomepageUrlSupplier.get();
        recordHomeButtonMetrics(homePageUrl);

        recordHomeButtonUseForIph();
        Tab currentTab = mTabSupplier.get();
        boolean isOffTheRecord =
                currentTab != null ? currentTab.isOffTheRecord() : mIsOffTheRecordSupplier.get();
        Tab newTab =
                mTabCreatorManager
                        .getTabCreator(isOffTheRecord)
                        .createNewTab(
                                new LoadUrlParams(homePageUrl, PageTransition.HOME_PAGE),
                                TabLaunchType.FROM_CHROME_UI,
                                currentTab);

        if (mMultiInstanceManager == null) return;
        // Move tab to a new window.
        mMultiInstanceManager.moveTabsToNewWindow(
                Collections.singletonList(newTab),
                /* finalizeCallback= */ null,
                NewWindowAppSource.KEYBOARD_SHORTCUT);
    }

    private void recordHomeButtonMetrics(String homePageUrl) {
        boolean isChromeInternal =
                homePageUrl.startsWith(ContentUrlConstants.ABOUT_URL_SHORT_PREFIX)
                        || homePageUrl.startsWith(UrlConstants.CHROME_URL_SHORT_PREFIX)
                        || homePageUrl.startsWith(UrlConstants.CHROME_NATIVE_URL_SHORT_PREFIX);
        RecordHistogram.recordBooleanHistogram(
                "Navigation.Home.IsChromeInternal", isChromeInternal);
        // Log a user action for the !is_chrome_internal case. This value is used as part of a
        // high-level guiding metric, which is being migrated to user actions.
        if (!isChromeInternal) {
            RecordUserAction.record("Navigation.Home.NotChromeInternal");
        }
    }

    /**
     * Whether Toolbar Tab Controller can consume a back press.
     *
     * @return True if a back press can be consumed.
     */
    boolean canGoBack() {
        BackPressHandler bottomControlsBackPressHandler =
                mBottomControlsBackPressHandlerSupplier.get();
        if (bottomControlsBackPressHandler != null
                && Boolean.TRUE.equals(
                        bottomControlsBackPressHandler.getHandleBackPressChangedSupplier().get())) {
            return true;
        }
        Tab tab = mActivityTabSupplier.get();
        return tab != null && tab.canGoBack();
    }

    /** Record that homepage button was used for IPH reasons */
    private void recordHomeButtonUseForIph() {
        Tab tab = mTabSupplier.get();
        Tracker tracker = mTrackerSupplier.get();
        if (tab == null || tracker == null) return;

        tracker.notifyEvent(EventConstants.HOMEPAGE_BUTTON_CLICKED);
    }
}
