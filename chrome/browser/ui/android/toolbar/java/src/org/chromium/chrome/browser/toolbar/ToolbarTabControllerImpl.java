// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.profile_metrics.BrowserProfileType;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.base.PageTransition;

/** Implementation of {@link ToolbarTabController}. */
@NullMarked
public class ToolbarTabControllerImpl implements ToolbarTabController {
    private final Supplier<Tab> mTabSupplier;
    private final Supplier<Tracker> mTrackerSupplier;
    private final ObservableSupplier<BottomControlsCoordinator> mBottomControlsCoordinatorSupplier;
    private final Supplier<String> mHomepageUrlSupplier;
    private final Runnable mOnSuccessRunnable;
    private final Supplier<Tab> mActivityTabSupplier;
    private final TabCreatorManager mTabCreatorManager;
    private final @Nullable MultiInstanceManager mMultiInstanceManager;

    /**
     * @param tabSupplier Supplier for the currently active tab.
     * @param trackerSupplier Supplier for the current profile tracker.
     * @param homepageUrlSupplier Supplier for the homepage URL.
     * @param onSuccessRunnable Runnable that is invoked when the active tab is asked to perform the
     *     corresponding ToolbarTabController action; it is not invoked if the tab cannot
     * @param activityTabSupplier Supplier for the currently active and interactable tab. Both
     *     tabSupplier and activityTabSupplier can return the same tab if tab is active and
     *     interactable. But activityTabSupplier will return null if it is non-interactable, such as
     *     on overview mode.
     * @param tabCreatorManager The {@link TabCreatorManager} used to create new tabs.
     * @param multiInstanceManager The {@link MultiInstanceManager} used to move tabs to new windows
     */
    public ToolbarTabControllerImpl(
            Supplier<Tab> tabSupplier,
            Supplier<Tracker> trackerSupplier,
            ObservableSupplier<BottomControlsCoordinator> bottomControlsCoordinatorSupplier,
            Supplier<String> homepageUrlSupplier,
            Runnable onSuccessRunnable,
            Supplier<Tab> activityTabSupplier,
            TabCreatorManager tabCreatorManager,
            @Nullable MultiInstanceManager multiInstanceManager) {
        mTabSupplier = tabSupplier;
        mTrackerSupplier = trackerSupplier;
        mBottomControlsCoordinatorSupplier = bottomControlsCoordinatorSupplier;
        mHomepageUrlSupplier = homepageUrlSupplier;
        mOnSuccessRunnable = onSuccessRunnable;
        mActivityTabSupplier = activityTabSupplier;
        mTabCreatorManager = tabCreatorManager;
        mMultiInstanceManager = multiInstanceManager;
    }

    @Override
    public boolean back() {
        BottomControlsCoordinator controlsCoordinator = mBottomControlsCoordinatorSupplier.get();
        if (controlsCoordinator != null && controlsCoordinator.onBackPressed()) {
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
            mMultiInstanceManager.moveTabToNewWindow(newTab);
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
            mMultiInstanceManager.moveTabToNewWindow(newTab);
            // Don't run mOnSuccessRunnable since nothing happened in the current tab.
            return true;
        }
        return false;
    }

    private @Nullable Tab createTabWithHistory(Tab tab, boolean foregroundNewTab) {
        return mTabCreatorManager
                .getTabCreator(tab.isIncognitoBranded())
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
        recordHomeButtonUserPerProfileType();
        Tab currentTab = mTabSupplier.get();
        if (currentTab == null) return;
        String homePageUrl = mHomepageUrlSupplier.get();
        boolean is_chrome_internal =
                homePageUrl.startsWith(ContentUrlConstants.ABOUT_URL_SHORT_PREFIX)
                        || homePageUrl.startsWith(UrlConstants.CHROME_URL_SHORT_PREFIX)
                        || homePageUrl.startsWith(UrlConstants.CHROME_NATIVE_URL_SHORT_PREFIX);
        RecordHistogram.recordBooleanHistogram(
                "Navigation.Home.IsChromeInternal", is_chrome_internal);
        // Log a user action for the !is_chrome_internal case. This value is used as part of a
        // high-level guiding metric, which is being migrated to user actions.
        if (!is_chrome_internal) {
            RecordUserAction.record("Navigation.Home.NotChromeInternal");
        }

        recordHomeButtonUseForIph();
        currentTab.loadUrl(new LoadUrlParams(homePageUrl, PageTransition.HOME_PAGE));
    }

    /**
     * Whether Toolbar Tab Controller can consume a back press.
     *
     * @return True if a back press can be consumed.
     */
    boolean canGoBack() {
        BottomControlsCoordinator controlsCoordinator = mBottomControlsCoordinatorSupplier.get();
        if (controlsCoordinator != null
                && Boolean.TRUE.equals(
                        controlsCoordinator.getHandleBackPressChangedSupplier().get())) {
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

    private void recordHomeButtonUserPerProfileType() {
        Tab tab = mTabSupplier.get();
        if (tab == null) return;
        Profile profile = tab.getProfile();
        @BrowserProfileType int type = Profile.getBrowserProfileTypeFromProfile(profile);
        RecordHistogram.recordEnumeratedHistogram(
                "Android.HomeButton.PerProfileType", type, BrowserProfileType.MAX_VALUE);
    }
}
