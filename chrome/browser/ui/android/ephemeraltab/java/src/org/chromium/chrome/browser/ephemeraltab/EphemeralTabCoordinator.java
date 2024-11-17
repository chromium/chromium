// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ephemeraltab;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.SysUtils;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.chrome.browser.content.ContentUtils;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconUtils;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.IntentRequestTracker;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.url.GURL;

/**
 * Central class for ephemeral tab, responsible for spinning off other classes necessary to display
 * short-lived WebContents on bottom sheet UI.
 */
public class EphemeralTabCoordinator implements View.OnLayoutChangeListener {
    private final Context mContext;
    private final ActivityWindowAndroid mWindow;
    private final View mLayoutView;
    private final Supplier<Tab> mTabProvider;
    private final Supplier<TabCreator> mTabCreator;
    private final BottomSheetController mBottomSheetController;
    private final EphemeralTabMediator mMediator;
    private final boolean mCanPromoteToNewTab;

    private WebContents mWebContents;
    private ContentView mContentView;
    private EphemeralTabSheetContent mSheetContent;
    private EmptyBottomSheetObserver mSheetObserver;

    private GURL mUrl;
    private GURL mFullPageUrl;
    private int mCurrentMaxViewHeight;
    private boolean mPeeked;
    private boolean mFullyOpened;

    /**
     * Constructor.
     *
     * @param context The associated {@link Context}.
     * @param window The associated {@link WindowAndroid}.
     * @param layoutView The {@link View} to listen layout change on.
     * @param tabProvider Provider of the current activity tab.
     * @param tabCreator Supplier for {@link TabCreator} handling a new tab creation.
     * @param bottomSheetController {@link BottomSheetController} as the container of the tab.
     * @param canPromoteToNewTab Whether the tab can be promoted to a normal tab.
     */
    public EphemeralTabCoordinator(
            Context context,
            ActivityWindowAndroid window,
            View layoutView,
            Supplier<Tab> tabProvider,
            Supplier<TabCreator> tabCreator,
            BottomSheetController bottomSheetController,
            boolean canPromoteToNewTab) {
        mContext = context;
        mWindow = window;
        mLayoutView = layoutView;
        mTabProvider = tabProvider;
        mTabCreator = tabCreator;
        mBottomSheetController = bottomSheetController;
        mCanPromoteToNewTab = canPromoteToNewTab;

        float topControlsHeight =
                mContext.getResources().getDimensionPixelSize(R.dimen.toolbar_height_no_shadow)
                        / mWindow.getDisplay().getDipScale();
        mMediator =
                new EphemeralTabMediator(
                        mBottomSheetController,
                        new FaviconLoader(mContext),
                        (int) topControlsHeight);
    }

    /**
     * Checks if this feature (a.k.a. "Preview page/image") is supported.
     * @return {@code true} if the feature is enabled.
     */
    public static boolean isSupported() {
        return !SysUtils.isLowEndDevice();
    }

    /** Checks if the preview tab is in open (peek) state. */
    public boolean isOpened() {
        return mPeeked || mFullyOpened;
    }

    /**
     * Entry point for ephemeral tab flow. This will create an ephemeral tab and show it in the
     * bottom sheet.
     *
     * @param url The URL to be shown.
     * @param title The title to be shown.
     * @param profile Profile associated with the ephemeral tab.
     */
    public void requestOpenSheet(GURL url, String title, Profile profile) {
        requestOpenSheetWithFullPageUrl(url, null, title, profile);
    }

    /** Add observer to be notified of ephemeral tab events. */
    public void addObserver(EphemeralTabObserver ephemeralTabObserver) {
        mMediator.addObserver(ephemeralTabObserver);
    }

    /** Remove observer. */
    public void removeObserver(EphemeralTabObserver ephemeralTabObserver) {
        mMediator.removeObserver(ephemeralTabObserver);
    }

    /**
     * Alternative entry point for ephemeral tab flow. This will create an ephemeral tab and show it
     * in the bottom sheet. When the tab is opened in a fullPage, an alternative URL is opened.
     *
     * @param url The URL to be shown in the bottomsheet.
     * @param fullPageUrl The URL that will be opened when the bottomsheet is transformed to a full
     *     page.
     * @param title The title to be shown.
     * @param profile Profile associated with the ephemeral tab.
     */
    public void requestOpenSheetWithFullPageUrl(
            GURL url, GURL fullPageUrl, String title, Profile profile) {
        mUrl = url;
        mFullPageUrl = fullPageUrl;
        if (mWebContents == null) {
            assert mSheetContent == null;
            createWebContents(profile);
            mSheetObserver =
                    new EmptyBottomSheetObserver() {
                        @Override
                        public void onSheetContentChanged(BottomSheetContent newContent) {
                            if (newContent != mSheetContent) {
                                mPeeked = false;
                                destroyWebContents();
                            }
                        }

                        @Override
                        public void onSheetStateChanged(int newState, int reason) {
                            if (mSheetContent == null) return;
                            switch (newState) {
                                case SheetState.PEEK:
                                    if (!mPeeked) {
                                        mPeeked = true;
                                    }
                                    break;
                                case SheetState.FULL:
                                    if (!mFullyOpened) {
                                        mFullyOpened = true;
                                    }
                                    break;
                            }
                        }

                        @Override
                        public void onSheetOffsetChanged(float heightFraction, float offsetPx) {
                            if (mSheetContent != null && mCanPromoteToNewTab) {
                                mSheetContent.showOpenInNewTabButton(heightFraction);
                            }
                        }
                    };
            mBottomSheetController.addObserver(mSheetObserver);
            IntentRequestTracker intentRequestTracker = mWindow.getIntentRequestTracker();
            assert intentRequestTracker != null
                    : "ActivityWindowAndroid must have a IntentRequestTracker.";
            mSheetContent =
                    new EphemeralTabSheetContent(
                            mContext,
                            this::openInNewTab,
                            this::onToolbarClick,
                            this::close,
                            getMaxViewHeight(),
                            intentRequestTracker,
                            (toolbarView) -> mMediator.onToolbarCreated(toolbarView));
            mMediator.init(mWebContents, mContentView, mSheetContent, profile);
            mLayoutView.addOnLayoutChangeListener(this);
        }

        mPeeked = false;
        mFullyOpened = false;
        mMediator.requestShowContent(url, title);

        Tracker tracker = TrackerFactory.getTrackerForProfile(profile);
        if (tracker.isInitialized()) tracker.notifyEvent(EventConstants.EPHEMERAL_TAB_USED);
    }

    private void createWebContents(Profile profile) {
        assert mWebContents == null;

        // Creates an initially hidden WebContents which gets shown when the panel is opened.
        mWebContents = WebContentsFactory.createWebContents(profile, true, false);

        mContentView = ContentView.createContentView(mContext, mWebContents);

        mWebContents.setDelegates(
                VersionInfo.getProductVersion(),
                ViewAndroidDelegate.createBasicDelegate(mContentView),
                mContentView,
                mWindow,
                WebContents.createDefaultInternalsHolder());
        ContentUtils.setUserAgentOverride(mWebContents, /* overrideInNewTabs= */ false);
    }

    private void destroyWebContents() {
        mSheetContent = null; // Will be destroyed by BottomSheet controller.

        mPeeked = false;
        mFullyOpened = false;

        if (mWebContents != null) {
            mWebContents.destroy();
            mWebContents = null;
            mContentView = null;
        }

        mMediator.destroyContent();

        mLayoutView.removeOnLayoutChangeListener(this);
        if (mSheetObserver != null) mBottomSheetController.removeObserver(mSheetObserver);
    }

    private void openInNewTab() {
        if (mCanPromoteToNewTab && mUrl != null) {
            mBottomSheetController.hideContent(
                    mSheetContent, /* animate= */ true, StateChangeReason.PROMOTE_TAB);
            GURL url = mFullPageUrl != null ? mFullPageUrl : mUrl;
            mTabCreator
                    .get()
                    .createNewTab(
                            new LoadUrlParams(url.getSpec(), PageTransition.LINK),
                            TabLaunchType.FROM_LINK,
                            mTabProvider.get());
        }
    }

    private void onToolbarClick() {
        int state = mBottomSheetController.getSheetState();
        if (state == SheetState.PEEK) {
            mBottomSheetController.expandSheet();
        } else if (state == SheetState.FULL) {
            mBottomSheetController.collapseSheet(true);
        }
    }

    /**
     * @return The WebContents that this Ephemeral tab currently holds.
     */
    public WebContents getWebContentsForTesting() {
        return mWebContents;
    }

    /**
     * @return The current url that this Ephemeral tab is displaying.
     */
    public GURL getUrlForTesting() {
        return mUrl;
    }

    /**
     * @return The current full page url that this Ephemeral tab is displaying.
     */
    public GURL getFullPageUrlForTesting() {
        return mFullPageUrl;
    }

    /** Close the ephemeral tab. */
    public void close() {
        mBottomSheetController.hideContent(mSheetContent, /* animate= */ true);
    }

    @Override
    public void onLayoutChange(
            View view,
            int left,
            int top,
            int right,
            int bottom,
            int oldLeft,
            int oldTop,
            int oldRight,
            int oldBottom) {
        if (mSheetContent == null) return;

        // It may not be possible to update the content height when the actual height changes
        // due to the current tab not being ready yet. Try it later again when the tab
        // (hence MaxViewHeight) becomes valid.
        int maxViewHeight = getMaxViewHeight();
        if (maxViewHeight == 0 || mCurrentMaxViewHeight == maxViewHeight) return;
        mSheetContent.updateContentHeight(maxViewHeight);
        mCurrentMaxViewHeight = maxViewHeight;
    }

    /** @return The maximum base view height for sheet content view. */
    private int getMaxViewHeight() {
        Tab tab = mTabProvider.get();
        if (tab == null || tab.getView() == null) return 0;
        return tab.getView().getHeight();
    }

    /**
     * Helper class to generate a favicon for a given URL and resize it to the desired dimensions
     * for displaying it on the image view.
     */
    static class FaviconLoader {
        private final Context mContext;
        private final FaviconHelper mFaviconHelper;
        private final int mFaviconSize;

        /** Constructor. */
        public FaviconLoader(Context context) {
            mContext = context;
            mFaviconHelper = new FaviconHelper();
            int sizeResId = R.dimen.ephemeral_tab_favicon_size;
            mFaviconSize = mContext.getResources().getDimensionPixelSize(sizeResId);
        }

        /**
         * Generates a favicon for a given URL. If no favicon was could be found or generated from
         * the URL, a default favicon will be shown.
         * @param url The URL for which favicon is to be generated.
         * @param callback The callback to be invoked to display the final image.
         * @param profile The profile for which favicon service is used.
         */
        public void loadFavicon(final GURL url, Callback<Drawable> callback, Profile profile) {
            assert profile != null;
            FaviconHelper.FaviconImageCallback imageCallback =
                    (bitmap, iconUrl) -> {
                        Drawable drawable;
                        if (bitmap != null) {
                            drawable =
                                    FaviconUtils.createRoundedBitmapDrawable(
                                            mContext.getResources(), bitmap);
                        } else {
                            drawable =
                                    UiUtils.getTintedDrawable(
                                            mContext,
                                            R.drawable.ic_globe_24dp,
                                            R.color.default_icon_color_tint_list);
                        }

                        callback.onResult(drawable);
                    };

            mFaviconHelper.getLocalFaviconImageForURL(profile, url, mFaviconSize, imageCallback);
        }
    }
}
