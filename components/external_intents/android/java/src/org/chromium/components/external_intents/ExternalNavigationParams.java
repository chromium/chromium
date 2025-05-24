// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.external_intents;

import static org.chromium.build.NullUtil.assertNonNull;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.base.RequiredCallback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** A container object for passing navigation parameters to {@link ExternalNavigationHandler}. */
@NullMarked
public class ExternalNavigationParams {
    /** A container for parameters passed to the AsyncActionTakenCallback. */
    public static class AsyncActionTakenParams {
        @IntDef({
            AsyncActionTakenType.NO_ACTION,
            AsyncActionTakenType.EXTERNAL_INTENT_LAUNCHED,
            AsyncActionTakenType.NAVIGATE
        })
        @Retention(RetentionPolicy.SOURCE)
        public @interface AsyncActionTakenType {
            /* Action was cancelled/rejected. */
            int NO_ACTION = 0;
            /* An external intent was launched as a result of the action. */
            int EXTERNAL_INTENT_LAUNCHED = 1;
            /* A navigation should occur as the result of the action */
            int NAVIGATE = 2;
        }

        @AsyncActionTakenType public int actionType;

        // Whether the async action taken allows the tab to be closed.
        public boolean canCloseTab;

        @SuppressWarnings("NullAway.Init")
        public ExternalNavigationParams externalNavigationParams;

        public @Nullable GURL targetUrl;

        private AsyncActionTakenParams() {
            this.actionType = AsyncActionTakenType.NO_ACTION;
        }

        private AsyncActionTakenParams(@Nullable GURL targetUrl, ExternalNavigationParams params) {
            this.actionType = AsyncActionTakenType.NAVIGATE;
            this.targetUrl = targetUrl;
            this.externalNavigationParams = params;
        }

        private AsyncActionTakenParams(boolean canCloseTab, ExternalNavigationParams params) {
            this.actionType = AsyncActionTakenType.EXTERNAL_INTENT_LAUNCHED;
            this.canCloseTab = canCloseTab;
            this.externalNavigationParams = params;
        }

        public static AsyncActionTakenParams forNoAction() {
            return new AsyncActionTakenParams();
        }

        public static AsyncActionTakenParams forNavigate(
                @Nullable GURL targetUrl, ExternalNavigationParams params) {
            return new AsyncActionTakenParams(targetUrl, params);
        }

        public static AsyncActionTakenParams forExternalIntentLaunched(
                boolean canCloseTab, ExternalNavigationParams params) {
            return new AsyncActionTakenParams(canCloseTab, params);
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
    private final boolean mIsMainFrame;
    private final @Nullable String mNativeClientPackageName;
    private final boolean mHasUserGesture;
    private final boolean mIsInitialNavigationInFrame;
    private final boolean mIsHiddenCrossFrameNavigation;
    private final boolean mIsSandboxedMainFrame;
    private final @Nullable Callback<AsyncActionTakenParams> mAsyncActionTakenCallback;
    private final boolean mIsRendererInitiated;
    private final @Nullable Origin mInitiatorOrigin;
    private final long mNavigationId;
    private final boolean mIsTabInPWA;
    private final boolean mIsInDesktopWindowingMode;

    // Populated when an async action is taken, ensuring the callback gets called.
    private @Nullable RequiredCallback<AsyncActionTakenParams> mRequiredAsyncActionTakenCallback;

    private ExternalNavigationParams(
            GURL url,
            boolean isIncognito,
            @Nullable GURL referrerUrl,
            int pageTransition,
            boolean isRedirect,
            boolean appMustBeInForeground,
            RedirectHandler redirectHandler,
            boolean openInNewTab,
            boolean isBackgroundTabNavigation,
            boolean isMainFrame,
            @Nullable String nativeClientPackageName,
            boolean hasUserGesture,
            @Nullable Callback<AsyncActionTakenParams> asyncActionTakenCallback,
            boolean isRendererInitiated,
            @Nullable Origin initiatorOrigin,
            boolean isInitialNavigationInFrame,
            boolean isHiddenCrossFrameNavigation,
            boolean isSandboxedMainFrame,
            long navigationId,
            boolean isTabInPWA,
            boolean isInDesktopWindowingMode) {
        mUrl = url;
        mIsIncognito = isIncognito;
        mPageTransition = pageTransition;
        mReferrerUrl = (referrerUrl == null) ? GURL.emptyGURL() : referrerUrl;
        mIsRedirect = isRedirect;
        mApplicationMustBeInForeground = appMustBeInForeground;
        mRedirectHandler = redirectHandler;
        mOpenInNewTab = openInNewTab;
        mIsBackgroundTabNavigation = isBackgroundTabNavigation;
        mIsMainFrame = isMainFrame;
        mNativeClientPackageName = nativeClientPackageName;
        mHasUserGesture = hasUserGesture;
        mAsyncActionTakenCallback = asyncActionTakenCallback;
        mIsRendererInitiated = isRendererInitiated;
        mInitiatorOrigin = initiatorOrigin;
        mIsInitialNavigationInFrame = isInitialNavigationInFrame;
        mIsHiddenCrossFrameNavigation = isHiddenCrossFrameNavigation;
        mIsSandboxedMainFrame = isSandboxedMainFrame;
        mNavigationId = navigationId;
        mIsTabInPWA = isTabInPWA;
        mIsInDesktopWindowingMode = isInDesktopWindowingMode;
    }

    public void onAsyncActionStarted() {
        if (mAsyncActionTakenCallback != null) {
            mRequiredAsyncActionTakenCallback = new RequiredCallback(mAsyncActionTakenCallback);
        }
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

    /**
     * @return Whether this navigation happens in main frame.
     */
    public boolean isMainFrame() {
        return mIsMainFrame;
    }

    /**
     * @return The package name of the TWA or WebAPK within which the navigation is happening.
     *         Null if the navigation is not within one of these wrapping APKs.
     */
    public @Nullable String nativeClientPackageName() {
        return mNativeClientPackageName;
    }

    /** @return Whether this navigation is launched by user gesture. */
    public boolean hasUserGesture() {
        return mHasUserGesture;
    }

    /** @return A callback to be run when an async action is taken. */
    public @Nullable
            RequiredCallback<AsyncActionTakenParams> getRequiredAsyncActionTakenCallback() {
        return mRequiredAsyncActionTakenCallback;
    }

    /** @return Whether the navigation is initiated by renderer. */
    public boolean isRendererInitiated() {
        return mIsRendererInitiated;
    }

    /** @return The origin that initiates the navigation. */
    public @Nullable Origin getInitiatorOrigin() {
        return mInitiatorOrigin;
    }

    /** @return Whether the navigation is from an intent. */
    public boolean isFromIntent() {
        return (mPageTransition & PageTransition.FROM_API) != 0;
    }

    /** @return Whether the navigation is the initial navigation in the frame. */
    public boolean isInitialNavigationInFrame() {
        return mIsInitialNavigationInFrame;
    }

    /** @return Whether the navigation is a cross-frame (non-browser-initiated) navigation. */
    public boolean isHiddenCrossFrameNavigation() {
        return mIsHiddenCrossFrameNavigation;
    }

    /** @return whether this navigation is taking place in a sandboxed main frame. */
    public boolean isSandboxedMainFrame() {
        return mIsSandboxedMainFrame;
    }

    /**
     * @return the id for this navigation.
     */
    public long getNavigationId() {
        return mNavigationId;
    }

    /**
     * @return whether this Tab is in a PWA (TWA or WebAPK), false otherwise.
     */
    public boolean isTabInPWA() {
        return mIsTabInPWA;
    }

    /**
     * @return whether this activity is in Android desktop windowing mode or not.
     */
    public boolean isInDesktopWindowingMode() {
        return mIsInDesktopWindowingMode;
    }

    /** The builder for {@link ExternalNavigationParams} objects. */
    public static class Builder {
        private final GURL mUrl;
        private final boolean mIsIncognito;
        private @Nullable GURL mReferrerUrl;
        private int mPageTransition;
        private boolean mIsRedirect;
        private boolean mApplicationMustBeInForeground;
        private @Nullable RedirectHandler mRedirectHandler;
        private boolean mOpenInNewTab;
        private boolean mIsBackgroundTabNavigation;
        private boolean mIsMainFrame;
        private @Nullable String mNativeClientPackageName;
        private boolean mHasUserGesture;
        private @Nullable Callback<AsyncActionTakenParams> mAsyncActionTakenCallback;
        private boolean mIsRendererInitiated;
        private @Nullable Origin mInitiatorOrigin;
        private boolean mIsInitialNavigationInFrame;
        private boolean mIsHiddenCrossFrameNavigation;
        private boolean mIsSandboxedMainFrame;
        private long mNavigationId;
        private boolean mIsTabInPWA;
        private boolean mIsInDesktopWindowingMode;

        public Builder(GURL url, boolean isIncognito) {
            mUrl = url;
            mIsIncognito = isIncognito;
        }

        public Builder(
                GURL url,
                boolean isIncognito,
                GURL referrer,
                int pageTransition,
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

        /** Sets whether this navigation happens in main frame. */
        public Builder setIsMainFrame(boolean v) {
            mIsMainFrame = v;
            return this;
        }

        /** Sets the package name of the TWA or WebAPK within which the navigation is happening. */
        public Builder setNativeClientPackageName(String v) {
            mNativeClientPackageName = v;
            return this;
        }

        /** Sets whether this navigation happens in main frame. */
        public Builder setHasUserGesture(boolean v) {
            mHasUserGesture = v;
            return this;
        }

        /** Sets the callback to be run when an async action is taken. */
        public Builder setAsyncActionTakenCallback(Callback<AsyncActionTakenParams> v) {
            mAsyncActionTakenCallback = v;
            return this;
        }

        /** Sets whether the navigation is initiated by renderer. */
        public Builder setIsRendererInitiated(boolean v) {
            mIsRendererInitiated = v;
            return this;
        }

        /** Sets the origin that initiates the navigation. */
        public Builder setInitiatorOrigin(@Nullable Origin v) {
            mInitiatorOrigin = v;
            return this;
        }

        /** Sets whether the navigation is the initial navigation in the frame. */
        public Builder setIsInitialNavigationInFrame(boolean v) {
            mIsInitialNavigationInFrame = v;
            return this;
        }

        /** Sets whether the navigation is a cross-frame (non-browser-initiated) navigation. */
        public Builder setIsHiddenCrossFrameNavigation(boolean v) {
            mIsHiddenCrossFrameNavigation = v;
            return this;
        }

        /** Sets whether this navigation is taking place in a sandboxed main frame. */
        public Builder setIsSandboxedMainFrame(boolean v) {
            mIsSandboxedMainFrame = v;
            return this;
        }

        public Builder setNavigationId(long v) {
            mNavigationId = v;
            return this;
        }

        /** Sets whether this navigation was started in a PWA (TWA or WebAPK). */
        public Builder setIsTabInPWA(boolean isTabInPWA) {
            mIsTabInPWA = isTabInPWA;
            return this;
        }

        /** Sets whether this application is in a desktop window. */
        public Builder setIsInDesktopWindowingMode(boolean v) {
            mIsInDesktopWindowingMode = v;
            return this;
        }

        /**
         * @return A fully constructed {@link ExternalNavigationParams} object.
         */
        public ExternalNavigationParams build() {
            return new ExternalNavigationParams(
                    mUrl,
                    mIsIncognito,
                    mReferrerUrl,
                    mPageTransition,
                    mIsRedirect,
                    mApplicationMustBeInForeground,
                    assertNonNull(mRedirectHandler),
                    mOpenInNewTab,
                    mIsBackgroundTabNavigation,
                    mIsMainFrame,
                    mNativeClientPackageName,
                    mHasUserGesture,
                    mAsyncActionTakenCallback,
                    mIsRendererInitiated,
                    mInitiatorOrigin,
                    mIsInitialNavigationInFrame,
                    mIsHiddenCrossFrameNavigation,
                    mIsSandboxedMainFrame,
                    mNavigationId,
                    mIsTabInPWA,
                    mIsInDesktopWindowingMode);
        }
    }
}
