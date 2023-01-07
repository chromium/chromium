// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.external_intents;

import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

/**
 * A container object for passing navigation parameters to {@link ExternalNavigationHandler}.
 */
public class ExternalNavigationParams {
    /**
     * A container for parameters passed to the AsyncActionTakenInMainFrameCallback.
     */
    public static class AsyncActionTakenParams {
        // Whether the async action taken allows the tab to be closed.
        public boolean canCloseTab;

        // Whether the tab will be clobbered as a result of this async action.
        public boolean willClobberTab;

        public ExternalNavigationParams externalNavigationParams;

        public AsyncActionTakenParams(
                boolean canCloseTab, boolean willClobberTab, ExternalNavigationParams params) {
            this.canCloseTab = canCloseTab;
            this.willClobberTab = willClobberTab;
            this.externalNavigationParams = params;

            // We can't both close the tab and clobber it.
            assert !willClobberTab || !canCloseTab;
        }
    }

    private final GURL mUrl;
    private final boolean mIsIncognito;
    private final GURL mReferrerUrl;
    private final int mPageTransition;
    private final boolean mIsRedirect;
    private final boolean mApplicationMustBeInForeground;
    private final RedirectHandler mRedirectHandler;
    private final boolean mOpenInNewTab;
    private final boolean mIsBackgroundTabNavigation;
    private final boolean mIntentLaunchesAllowedInBackgroundTabs;
    private final boolean mIsMainFrame;
    private final String mNativeClientPackageName;
    private final boolean mHasUserGesture;
    private final Callback<AsyncActionTakenParams> mAsyncActionTakenInMainFrameCallback;
    private boolean mIsRendererInitiated;
    private Origin mInitiatorOrigin;

    private ExternalNavigationParams(GURL url, boolean isIncognito, GURL referrerUrl,
            int pageTransition, boolean isRedirect, boolean appMustBeInForeground,
            RedirectHandler redirectHandler, boolean openInNewTab,
            boolean isBackgroundTabNavigation, boolean intentLaunchesAllowedInBackgroundTabs,
            boolean isMainFrame, String nativeClientPackageName, boolean hasUserGesture,
            Callback<AsyncActionTakenParams> asyncActionTakenInMainFrameCallback,
            boolean isRendererInitiated, @Nullable Origin initiatorOrigin) {
        mUrl = url;
        assert mUrl != null;
        mIsIncognito = isIncognito;
        mPageTransition = pageTransition;
        mReferrerUrl = (referrerUrl == null) ? GURL.emptyGURL() : referrerUrl;
        mIsRedirect = isRedirect;
        mApplicationMustBeInForeground = appMustBeInForeground;
        mRedirectHandler = redirectHandler;
        mOpenInNewTab = openInNewTab;
        mIsBackgroundTabNavigation = isBackgroundTabNavigation;
        mIntentLaunchesAllowedInBackgroundTabs = intentLaunchesAllowedInBackgroundTabs;
        mIsMainFrame = isMainFrame;
        mNativeClientPackageName = nativeClientPackageName;
        mHasUserGesture = hasUserGesture;
        mAsyncActionTakenInMainFrameCallback = asyncActionTakenInMainFrameCallback;
        mIsRendererInitiated = isRendererInitiated;
        mInitiatorOrigin = initiatorOrigin;
    }

    /** @return The URL to potentially open externally. */
    public GURL getUrl() {
        return mUrl;
    }

    /** @return Whether we are currently in incognito mode. */
    public boolean isIncognito() {
        return mIsIncognito;
    }

    /** @return The referrer URL. */
    public GURL getReferrerUrl() {
        return mReferrerUrl;
    }

    /** @return The page transition for the current navigation. */
    public int getPageTransition() {
        return mPageTransition;
    }

    /** @return Whether the navigation is part of a redirect. */
    public boolean isRedirect() {
        return mIsRedirect;
    }

    /** @return Whether the application has to be in foreground to open the URL. */
    public boolean isApplicationMustBeInForeground() {
        return mApplicationMustBeInForeground;
    }

    /** @return The redirect handler. */
    public RedirectHandler getRedirectHandler() {
        return mRedirectHandler;
    }

    /**
     * @return Whether the external navigation should be opened in a new tab if handled by Chrome
     *         through the intent picker.
     */
    public boolean isOpenInNewTab() {
        return mOpenInNewTab;
    }

    /** @return Whether this navigation happens in background tab. */
    public boolean isBackgroundTabNavigation() {
        return mIsBackgroundTabNavigation;
    }

    /** @return Whether intent launches are allowed in background tabs. */
    public boolean areIntentLaunchesAllowedInBackgroundTabs() {
        return mIntentLaunchesAllowedInBackgroundTabs;
    }

    /** @return Whether this navigation happens in main frame. */
    public boolean isMainFrame() {
        return mIsMainFrame;
    }

    /**
     * @return The package name of the TWA or WebAPK within which the navigation is happening.
     *         Null if the navigation is not within one of these wrapping APKs.
     */
    public String nativeClientPackageName() {
        return mNativeClientPackageName;
    }

    /** @return Whether this navigation is launched by user gesture. */
    public boolean hasUserGesture() {
        return mHasUserGesture;
    }

    /**
     * @return A callback to be run when an async action is taken.
     */
    public Callback<AsyncActionTakenParams> getAsyncActionTakenInMainFrameCallback() {
        return mAsyncActionTakenInMainFrameCallback;
    }

    /**
     * @return Whether the navigation is initiated by renderer.
     */
    public boolean isRendererInitiated() {
        return mIsRendererInitiated;
    }

    /**
     * @return The origin that initiates the navigation.
     */
    @Nullable
    public Origin getInitiatorOrigin() {
        return mInitiatorOrigin;
    }

    /**
     * @return Whether the navigation is from an intent.
     */
    public boolean isFromIntent() {
        return (mPageTransition & PageTransition.FROM_API) != 0;
    }

    /** The builder for {@link ExternalNavigationParams} objects. */
    public static class Builder {
        private GURL mUrl;
        private boolean mIsIncognito;
        private GURL mReferrerUrl;
        private int mPageTransition;
        private boolean mIsRedirect;
        private boolean mApplicationMustBeInForeground;
        private RedirectHandler mRedirectHandler;
        private boolean mOpenInNewTab;
        private boolean mIsBackgroundTabNavigation;
        private boolean mIntentLaunchesAllowedInBackgroundTabs;
        private boolean mIsMainFrame;
        private String mNativeClientPackageName;
        private boolean mHasUserGesture;
        private Callback<AsyncActionTakenParams> mAsyncActionTakenInMainFrameCallback;
        private boolean mIsRendererInitiated;
        private Origin mInitiatorOrigin;

        public Builder(GURL url, boolean isIncognito) {
            mUrl = url;
            mIsIncognito = isIncognito;
        }

        public Builder(GURL url, boolean isIncognito, GURL referrer, int pageTransition,
                boolean isRedirect) {
            mUrl = url;
            mIsIncognito = isIncognito;
            mReferrerUrl = referrer;
            mPageTransition = pageTransition;
            mIsRedirect = isRedirect;
        }

        /** Specify whether the application must be in foreground to launch an external intent. */
        public Builder setApplicationMustBeInForeground(boolean v) {
            mApplicationMustBeInForeground = v;
            return this;
        }

        /** Sets a tab redirect handler. */
        public Builder setRedirectHandler(RedirectHandler handler) {
            mRedirectHandler = handler;
            return this;
        }

        /** Sets whether we want to open the intent URL in new tab, if handled by Chrome. */
        public Builder setOpenInNewTab(boolean v) {
            mOpenInNewTab = v;
            return this;
        }

        /** Sets whether this navigation happens in background tab. */
        public Builder setIsBackgroundTabNavigation(boolean v) {
            mIsBackgroundTabNavigation = v;
            return this;
        }

        /** Sets whether intent launches are allowed in background tabs. */
        public Builder setIntentLaunchesAllowedInBackgroundTabs(boolean v) {
            mIntentLaunchesAllowedInBackgroundTabs = v;
            return this;
        }

        /** Sets whether this navigation happens in main frame. */
        public Builder setIsMainFrame(boolean v) {
            mIsMainFrame = v;
            return this;
        }

        /** Sets the package name of the TWA or WebAPK within which the navigation is happening. **/
        public Builder setNativeClientPackageName(String v) {
            mNativeClientPackageName = v;
            return this;
        }

        /** Sets whether this navigation happens in main frame. */
        public Builder setHasUserGesture(boolean v) {
            mHasUserGesture = v;
            return this;
        }

        /**
         * Sets the callback to be run when an async action is taken.
         */
        public Builder setAsyncActionTakenInMainFrameCallback(Callback<AsyncActionTakenParams> v) {
            mAsyncActionTakenInMainFrameCallback = v;
            return this;
        }

        /**
         * Sets whether the navigation is initiated by renderer.
         */
        public Builder setIsRendererInitiated(boolean v) {
            mIsRendererInitiated = v;
            return this;
        }

        /**
         * Sets the origin that initiates the navigation.
         */
        public Builder setInitiatorOrigin(@Nullable Origin v) {
            mInitiatorOrigin = v;
            return this;
        }

        /** @return A fully constructed {@link ExternalNavigationParams} object. */
        public ExternalNavigationParams build() {
            return new ExternalNavigationParams(mUrl, mIsIncognito, mReferrerUrl, mPageTransition,
                    mIsRedirect, mApplicationMustBeInForeground, mRedirectHandler, mOpenInNewTab,
                    mIsBackgroundTabNavigation, mIntentLaunchesAllowedInBackgroundTabs,
                    mIsMainFrame, mNativeClientPackageName, mHasUserGesture,
                    mAsyncActionTakenInMainFrameCallback, mIsRendererInitiated, mInitiatorOrigin);
        }
    }
}
