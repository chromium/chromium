// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ephemeraltab;

import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.view.ViewGroup;

import androidx.annotation.DrawableRes;

import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.components.security_state.SecurityStateModel;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.common.ResourceRequestBody;
import org.chromium.ui.widget.Toast;
import org.chromium.url.GURL;

/** Mediator class for preview tab, responsible for communicating with other objects. */
public class EphemeralTabMediator {
    /** The delay (four video frames) after which the hide progress will be hidden. */
    private static final long HIDE_PROGRESS_BAR_DELAY_MS = (1000 / 60) * 4;

    private final BottomSheetController mBottomSheetController;
    private final EphemeralTabCoordinator.FaviconLoader mFaviconLoader;
    private final ObserverList<EphemeralTabObserver> mObservers;
    private final int mTopControlsHeightDp;

    private WebContents mWebContents;
    private EphemeralTabSheetContent mSheetContent;
    private WebContentsObserver mWebContentsObserver;
    private WebContentsDelegateAndroid mWebContentsDelegate;
    private Profile mProfile;

    /** Constructor. */
    public EphemeralTabMediator(
            BottomSheetController bottomSheetController,
            EphemeralTabCoordinator.FaviconLoader faviconLoader,
            int topControlsHeightDp) {
        mBottomSheetController = bottomSheetController;
        mFaviconLoader = faviconLoader;
        mTopControlsHeightDp = topControlsHeightDp;
        mObservers = new ObserverList<EphemeralTabObserver>();
    }

    /** Initializes various objects for a new tab. */
    void init(
            WebContents webContents,
            ContentView contentView,
            EphemeralTabSheetContent sheetContent,
            Profile profile) {
        // Ensures that initialization is performed only when a new tab is opened.
        assert mProfile == null && mWebContentsObserver == null && mWebContentsDelegate == null;

        mProfile = profile;
        mWebContents = webContents;
        mSheetContent = sheetContent;
        createWebContentsObserver();
        createWebContentsDelegate();
        mSheetContent.attachWebContents(mWebContents, contentView, mWebContentsDelegate);
    }

    /** Add observer to be notified of ephemeral tab events. */
    void addObserver(EphemeralTabObserver ephemeralTabObserver) {
        mObservers.addObserver(ephemeralTabObserver);
    }

    /** Remove observer. */
    void removeObserver(EphemeralTabObserver ephemeralTabObserver) {
        mObservers.removeObserver(ephemeralTabObserver);
    }

    /** Clear observers. */
    void clearObservers() {
        mObservers.clear();
    }

    /** Notify observers on toolbar creation. */
    public void onToolbarCreated(ViewGroup toolbarView) {
        RewindableIterator<EphemeralTabObserver> observersIterator =
                mObservers.rewindableIterator();
        while (observersIterator.hasNext()) {
            observersIterator.next().onToolbarCreated(toolbarView);
        }
    }

    /** Notify observers on navigation start. */
    public void onNavigationStarted(GURL clickedUrl) {
        RewindableIterator<EphemeralTabObserver> observersIterator =
                mObservers.rewindableIterator();
        while (observersIterator.hasNext()) {
            observersIterator.next().onNavigationStarted(clickedUrl);
        }
    }

    /** Notify observers on title set. */
    public void onTitleSet(EphemeralTabSheetContent sheetContent, String title) {
        RewindableIterator<EphemeralTabObserver> observersIterator =
                mObservers.rewindableIterator();
        while (observersIterator.hasNext()) {
            observersIterator.next().onTitleSet(sheetContent, title);
        }
    }

    /** Loads a new URL into the tab and makes it visible. */
    void requestShowContent(GURL url, String title) {
        loadUrl(url);
        mSheetContent.updateTitle(title);
        mBottomSheetController.requestShowContent(mSheetContent, true);
    }

    private void loadUrl(GURL url) {
        mWebContents.getNavigationController().loadUrl(new LoadUrlParams(url.getSpec()));
    }

    private void createWebContentsObserver() {
        assert mWebContentsObserver == null;
        mWebContentsObserver =
                new WebContentsObserver(mWebContents) {
                    /** Whether the currently loaded page is an error (interstitial) page. */
                    private boolean mIsOnErrorPage;

                    private GURL mCurrentUrl;

                    @Override
                    public void loadProgressChanged(float progress) {
                        if (mSheetContent != null) mSheetContent.setProgress(progress);
                    }

                    @Override
                    public void didStartNavigationInPrimaryMainFrame(NavigationHandle navigation) {
                        if (!navigation.isSameDocument()) {
                            GURL url = navigation.getUrl();
                            if (url.equals(mCurrentUrl)) return;

                            // The link Back to Safety on the interstitial page will go to the
                            // previous page. If there is no previous page, i.e. previous page is
                            // NTP, the preview tab will be closed.
                            if (mIsOnErrorPage && UrlUtilities.isNtpUrl(url)) {
                                mBottomSheetController.hideContent(
                                        mSheetContent, /* animate= */ true);
                                mCurrentUrl = null;
                                return;
                            }

                            onNavigationStarted(url);

                            mCurrentUrl = url;
                            mFaviconLoader.loadFavicon(
                                    url, (drawable) -> onFaviconAvailable(drawable), mProfile);
                        }
                    }

                    @Override
                    public void titleWasSet(String title) {
                        mSheetContent.updateTitle(title);
                        onTitleSet(mSheetContent, title);
                    }

                    @Override
                    public void didFinishNavigationInPrimaryMainFrame(NavigationHandle navigation) {
                        if (navigation.hasCommitted()) {
                            mIsOnErrorPage = navigation.isErrorPage();
                            mSheetContent.updateURL(mWebContents.get().getVisibleUrl());
                        } else if (navigation.isDownload()) {
                            // Not viewable contents such as download. Show a toast and close the
                            // tab.
                            Toast.makeText(
                                            ContextUtils.getApplicationContext(),
                                            R.string.ephemeral_tab_sheet_not_viewable,
                                            Toast.LENGTH_SHORT)
                                    .show();
                            mBottomSheetController.hideContent(mSheetContent, /* animate= */ true);
                        }
                    }
                };
    }

    private void onFaviconAvailable(Drawable drawable) {
        if (mSheetContent != null) mSheetContent.startFaviconAnimation(drawable);
    }

    private void createWebContentsDelegate() {
        assert mWebContentsDelegate == null;
        mWebContentsDelegate =
                new WebContentsDelegateAndroid() {
                    @Override
                    public void visibleSSLStateChanged() {
                        if (mSheetContent == null) return;
                        int securityLevel =
                                SecurityStateModel.getSecurityLevelForWebContents(mWebContents);
                        mSheetContent.setSecurityIcon(getSecurityIconResource(securityLevel));
                        mSheetContent.updateURL(mWebContents.getVisibleUrl());
                    }

                    @Override
                    public void openNewTab(
                            GURL url,
                            String extraHeaders,
                            ResourceRequestBody postData,
                            int disposition,
                            boolean isRendererInitiated) {
                        // We never open a separate tab when navigating in a preview tab.
                        loadUrl(url);
                    }

                    @Override
                    public boolean shouldCreateWebContents(GURL targetUrl) {
                        loadUrl(targetUrl);
                        return false;
                    }

                    @Override
                    public void loadingStateChanged(boolean shouldShowLoadingUI) {
                        boolean isLoading = mWebContents != null && mWebContents.isLoading();
                        if (isLoading) {
                            if (mSheetContent == null) return;
                            mSheetContent.setProgress(0);
                            mSheetContent.setProgressVisible(true);
                        } else {
                            // Hides the Progress Bar after a delay to make sure it is rendered for
                            // at least a few frames, otherwise its completion won't be visually
                            // noticeable.
                            new Handler()
                                    .postDelayed(
                                            () -> {
                                                if (mSheetContent != null) {
                                                    mSheetContent.setProgressVisible(false);
                                                }
                                            },
                                            HIDE_PROGRESS_BAR_DELAY_MS);
                        }
                    }

                    @Override
                    public int getTopControlsHeight() {
                        return mTopControlsHeightDp;
                    }
                };
    }

    private static @DrawableRes int getSecurityIconResource(
            @ConnectionSecurityLevel int securityLevel) {
        switch (securityLevel) {
            case ConnectionSecurityLevel.NONE:
            case ConnectionSecurityLevel.WARNING:
                return R.drawable.omnibox_info;
            case ConnectionSecurityLevel.DANGEROUS:
                return R.drawable.omnibox_not_secure_warning;
            case ConnectionSecurityLevel.SECURE_WITH_POLICY_INSTALLED_CERT:
            case ConnectionSecurityLevel.SECURE:
                return R.drawable.omnibox_https_valid;
            default:
                assert false;
        }
        return 0;
    }

    /** Destroys the objects used for the current preview tab. */
    void destroyContent() {
        if (mWebContentsObserver != null) {
            mWebContentsObserver.destroy();
            mWebContentsObserver = null;
        }
        mWebContentsDelegate = null;
        mWebContents = null;
        mSheetContent = null;
        mProfile = null;
        clearObservers();
    }
}
