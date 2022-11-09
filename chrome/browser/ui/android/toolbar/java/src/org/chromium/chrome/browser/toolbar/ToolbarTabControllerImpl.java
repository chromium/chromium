// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsCoordinator;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.profile_metrics.BrowserProfileType;
import org.chromium.content_public.browser.LoadCommittedDetails;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.ui.base.PageTransition;

/**
 * Implementation of {@link ToolbarTabController}.
 */
public class ToolbarTabControllerImpl implements ToolbarTabController {
    private final Supplier<Tab> mTabSupplier;
    private final Supplier<Boolean> mOverrideHomePageSupplier;
    private final Supplier<Tracker> mTrackerSupplier;
    private final ObservableSupplier<BottomControlsCoordinator> mBottomControlsCoordinatorSupplier;
    private final Supplier<String> mHomepageUrlSupplier;
    private final Runnable mOnSuccessRunnable;
    private final Callback<Tab> mOnActivityTabCallback = this::onActivityTabChanged;
    private final ObservableSupplier<Tab> mActivityTabSupplier;
    private final ObservableSupplierImpl<Boolean> mBackPressChangedSupplier =
            new ObservableSupplierImpl<>();
    private Tab mOldTab;
    private final Callback<BottomControlsCoordinator> mBottomControlsCoordinatorAvailableCallback;
    @Nullable
    private WebContentsObserver mWebContentsBackPressObserver;
    private final TabObserver mTabBackPressObserver = new EmptyTabObserver() {
        @Override
        public void webContentsWillSwap(Tab tab) {
            if (tab.getWebContents() != null) {
                removeWebContentsBackPressObserver(tab.getWebContents());
            }
            onBackPressedChanged();
        }

        @Override
        public void onWebContentsSwapped(Tab tab, boolean didStartLoad, boolean didFinishLoad) {
            if (tab.getWebContents() != null) {
                addWebContentsBackPressObserver(tab.getWebContents());
            }
            onBackPressedChanged();
        }

        @Override
        public void onDestroyed(Tab tab) {
            if (tab.getWebContents() != null) {
                removeWebContentsBackPressObserver(tab.getWebContents());
            }
            onBackPressedChanged();
        }

        @Override
        public void onContentChanged(Tab tab) {
            if (tab.getWebContents() != null) {
                addWebContentsBackPressObserver(tab.getWebContents());
            }
            onBackPressedChanged();
        }
    };

    /**
     *
     * @param tabSupplier Supplier for the currently active tab.
     * @param overrideHomePageSupplier Supplier that returns true if it overrides the default
     *         homepage behavior.
     * @param trackerSupplier Supplier for the current profile tracker.
     * @param homepageUrlSupplier Supplier for the homepage URL.
     * @param onSuccessRunnable Runnable that is invoked when the active tab is asked to perform the
     *         corresponding ToolbarTabController action; it is not invoked if the tab cannot
     * @param activityTabSupplier Supplier of the current activity tab, which should return null
     *                            in non-browsing mode.
     */
    public ToolbarTabControllerImpl(Supplier<Tab> tabSupplier,
            Supplier<Boolean> overrideHomePageSupplier, Supplier<Tracker> trackerSupplier,
            ObservableSupplier<BottomControlsCoordinator> bottomControlsCoordinatorSupplier,
            Supplier<String> homepageUrlSupplier, Runnable onSuccessRunnable,
            ObservableSupplier<Tab> activityTabSupplier) {
        mTabSupplier = tabSupplier;
        mOverrideHomePageSupplier = overrideHomePageSupplier;
        mTrackerSupplier = trackerSupplier;
        mBottomControlsCoordinatorSupplier = bottomControlsCoordinatorSupplier;
        mHomepageUrlSupplier = homepageUrlSupplier;
        mOnSuccessRunnable = onSuccessRunnable;
        mActivityTabSupplier = activityTabSupplier;
        mBottomControlsCoordinatorAvailableCallback = this::onBottomControlsCoordinatorAvailable;
        if (BackPressManager.isEnabled()) {
            activityTabSupplier.addObserver(mOnActivityTabCallback);
            bottomControlsCoordinatorSupplier.addObserver(
                    mBottomControlsCoordinatorAvailableCallback);
            onBackPressedChanged();
        }
    }

    @Override
    public boolean back() {
        BottomControlsCoordinator controlsCoordinator = mBottomControlsCoordinatorSupplier.get();
        if (controlsCoordinator != null && controlsCoordinator.onBackPressed()) {
            return true;
        }
        Tab tab = mTabSupplier.get();
        if (BackPressManager.isEnabled()) {
            tab = mActivityTabSupplier.get();
        }
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
    public void stopOrReloadCurrentTab() {
        Tab currentTab = mTabSupplier.get();
        if (currentTab == null) return;

        if (currentTab.isLoading()) {
            currentTab.stopLoading();
            RecordUserAction.record("MobileToolbarStop");
        } else {
            currentTab.reload();
            RecordUserAction.record("MobileToolbarReload");
        }
        mOnSuccessRunnable.run();
    }

    @Override
    public void openHomepage() {
        RecordUserAction.record("Home");
        recordHomeButtonUserPerProfileType();
        if (mOverrideHomePageSupplier.get()) {
            // While some other element is handling the routing of this click event, something
            // still needs to notify the event. This approach allows consolidation of events for
            // the home button.
            Tracker tracker = mTrackerSupplier.get();
            if (tracker != null) tracker.notifyEvent(EventConstants.HOMEPAGE_BUTTON_CLICKED);
            return;
        }
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

        recordHomeButtonUseForIPH(homePageUrl);
        currentTab.loadUrl(new LoadUrlParams(homePageUrl, PageTransition.HOME_PAGE));
    }

    @Override
    public void handleBackPress() {
        boolean ret = back();
        onBackPressedChanged();
        assert ret;
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressChangedSupplier;
    }

    @Override
    public void destroy() {
        if (BackPressManager.isEnabled()) {
            mActivityTabSupplier.removeObserver(mOnActivityTabCallback);
            mBottomControlsCoordinatorSupplier.removeObserver(
                    mBottomControlsCoordinatorAvailableCallback);
            if (mWebContentsBackPressObserver != null) mWebContentsBackPressObserver.destroy();
        }
    }

    private void onActivityTabChanged(@Nullable Tab tab) {
        if (mOldTab != null) {
            mOldTab.removeObserver(mTabBackPressObserver);
            if (mOldTab.getWebContents() != null) {
                removeWebContentsBackPressObserver(mOldTab.getWebContents());
            }
        }
        if (tab != null) {
            if (tab.getWebContents() != null) {
                addWebContentsBackPressObserver(tab.getWebContents());
            }
            tab.addObserver(mTabBackPressObserver);
            mOldTab = tab;
        }
        onBackPressedChanged();
    }

    private void addWebContentsBackPressObserver(@NonNull WebContents webContents) {
        if (mWebContentsBackPressObserver != null) mWebContentsBackPressObserver.destroy();
        mWebContentsBackPressObserver = new WebContentsObserver(webContents) {
            @Override
            public void navigationEntryCommitted(LoadCommittedDetails details) {
                onBackPressedChanged();
            }

            @Override
            public void navigationEntriesDeleted() {
                onBackPressedChanged();
            }

            @Override
            public void navigationEntriesChanged() {
                onBackPressedChanged();
            }

            @Override
            public void frameReceivedUserActivation() {
                onBackPressedChanged();
            }
        };
    }

    private void removeWebContentsBackPressObserver(@NonNull WebContents webContents) {
        webContents.removeObserver(mWebContentsBackPressObserver);
        if (mWebContentsBackPressObserver != null) mWebContentsBackPressObserver.destroy();
    }

    private void onBottomControlsCoordinatorAvailable(
            BottomControlsCoordinator bottomControlsCoordinator) {
        onBackPressedChanged();
        bottomControlsCoordinator.getHandleBackPressChangedSupplier().addObserver(
                (v) -> this.onBackPressedChanged());
    }

    private void onBackPressedChanged() {
        if (mBottomControlsCoordinatorSupplier.get() != null) {
            BottomControlsCoordinator coordinator = mBottomControlsCoordinatorSupplier.get();
            if (Boolean.TRUE.equals(coordinator.getHandleBackPressChangedSupplier().get())) {
                mBackPressChangedSupplier.set(true);
                return;
            }
        }
        Tab tab = mActivityTabSupplier.get();
        mBackPressChangedSupplier.set(tab != null && tab.canGoBack());
    }

    /** Record that homepage button was used for IPH reasons */
    private void recordHomeButtonUseForIPH(String homepageUrl) {
        Tab tab = mTabSupplier.get();
        Tracker tracker = mTrackerSupplier.get();
        if (tab == null || tracker == null) return;

        tracker.notifyEvent(EventConstants.HOMEPAGE_BUTTON_CLICKED);
    }

    private void recordHomeButtonUserPerProfileType() {
        Tab tab = mTabSupplier.get();
        if (tab == null) return;
        Profile profile = Profile.fromWebContents(tab.getWebContents());
        if (profile == null) return;

        @BrowserProfileType
        int type = Profile.getBrowserProfileTypeFromProfile(profile);
        RecordHistogram.recordEnumeratedHistogram(
                "Android.HomeButton.PerProfileType", type, BrowserProfileType.MAX_VALUE + 1);
    }
}
