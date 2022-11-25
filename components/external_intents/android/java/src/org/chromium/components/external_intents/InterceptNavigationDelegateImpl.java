// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.external_intents;

import android.util.Pair;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.external_intents.ExternalNavigationHandler.OverrideUrlLoadingResult;
import org.chromium.components.external_intents.ExternalNavigationHandler.OverrideUrlLoadingResultType;
import org.chromium.components.external_intents.ExternalNavigationParams.AsyncActionTakenParams;
import org.chromium.components.navigation_interception.InterceptNavigationDelegate;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ConsoleMessageLevel;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Class that controls navigations and allows to intercept them. It is used on Android to 'convert'
 * certain navigations to Intents to 3rd party applications.
 * Note the Intent is often created together with a new empty tab which then should be closed
 * immediately. Closing the tab will cancel the navigation that this delegate is running for,
 * hence can cause UAF error. It should be done in an asynchronous fashion to avoid it.
 * See https://crbug.com/732260.
 */
@JNINamespace("external_intents")
public class InterceptNavigationDelegateImpl extends InterceptNavigationDelegate {
    /**
     * Histogram for the source of a main frame intent launch.
     * This enum is used in UMA, do not reorder values.
     */
    @IntDef({MainFrameIntentLaunch.NOT_FROM_EXTERNAL_APP_TO_INTENT_SCHEME,
            MainFrameIntentLaunch.NOT_FROM_EXTERNAL_APP_TO_CUSTOM_SCHEME,
            MainFrameIntentLaunch.NOT_FROM_EXTERNAL_APP_TO_SUPPORTED_SCHEME,
            MainFrameIntentLaunch.FROM_EXTERNAL_APP_TO_INTENT_SCHEME,
            MainFrameIntentLaunch.FROM_EXTERNAL_APP_TO_CUSTOM_SCHEME,
            MainFrameIntentLaunch.FROM_EXTERNAL_APP_TO_SUPPORTED_SCHEME,
            MainFrameIntentLaunch.NUM_ENTRIES})
    @Retention(RetentionPolicy.SOURCE)
    public @interface MainFrameIntentLaunch {
        /* The tab was not opened by an external app, and the URL navigated to had an intent:
         * scheme. */
        int NOT_FROM_EXTERNAL_APP_TO_INTENT_SCHEME = 0;
        /* The tab was not opened by an external app, and the URL navigated to had a custom
         * scheme. */
        int NOT_FROM_EXTERNAL_APP_TO_CUSTOM_SCHEME = 1;
        /* The tab was not opened by an external app, and the URL navigated to had a supported
         * scheme. */
        int NOT_FROM_EXTERNAL_APP_TO_SUPPORTED_SCHEME = 2;
        /* Tab was opened by an external app, and the URL navigated to had an intent: scheme. */
        int FROM_EXTERNAL_APP_TO_INTENT_SCHEME = 3;
        /* Tab was opened by an external app, and the URL navigated to had a custom scheme. */
        int FROM_EXTERNAL_APP_TO_CUSTOM_SCHEME = 4;
        /* Tab was opened by an external app, and the URL navigated to had a supported scheme. */
        int FROM_EXTERNAL_APP_TO_SUPPORTED_SCHEME = 5;

        int NUM_ENTRIES = 6;
    }

    private static final String MAIN_FRAME_INTENT_LAUNCH_NAME =
            "Android.Intent.MainFrameIntentLaunch";

    private InterceptNavigationDelegateClient mClient;
    private Callback<Pair<GURL, OverrideUrlLoadingResult>> mResultCallbackForTesting;
    private WebContents mWebContents;
    private ExternalNavigationHandler mExternalNavHandler;

    /**
     * Whether forward history should be cleared after navigation is committed.
     */
    private boolean mClearAllForwardHistoryRequired;
    private boolean mShouldClearRedirectHistoryForTabClobbering;

    /**
     * Default constructor of {@link InterceptNavigationDelegateImpl}.
     */
    public InterceptNavigationDelegateImpl(InterceptNavigationDelegateClient client) {
        mClient = client;
        associateWithWebContents(mClient.getWebContents());
    }

    // Invoked by the client when a navigation has finished in the context in which this object is
    // operating.
    public void onNavigationFinishedInPrimaryMainFrame(NavigationHandle navigation) {
        if (!navigation.hasCommitted()) return;
        maybeUpdateNavigationHistory();
    }

    public void onNavigationFinishedNoop(NavigationHandle navigation) {
        if (!navigation.isInPrimaryMainFrame()) return;
    }

    public void setExternalNavigationHandler(ExternalNavigationHandler handler) {
        mExternalNavHandler = handler;
    }

    public void associateWithWebContents(WebContents webContents) {
        if (mWebContents == webContents) return;
        mWebContents = webContents;
        if (mWebContents == null) return;

        // Lazily initialize the external navigation handler.
        if (mExternalNavHandler == null) {
            setExternalNavigationHandler(mClient.createExternalNavigationHandler());
        }
        InterceptNavigationDelegateImplJni.get().associateWithWebContents(this, mWebContents);
    }

    public boolean shouldIgnoreNewTab(
            GURL url, boolean incognito, boolean isRendererInitiated, Origin initiatorOrigin) {
        ExternalNavigationParams params = new ExternalNavigationParams.Builder(url, incognito)
                                                  .setOpenInNewTab(true)
                                                  .setIsRendererInitiated(isRendererInitiated)
                                                  .setInitiatorOrigin(initiatorOrigin)
                                                  .setIsMainFrame(true)
                                                  .build();
        OverrideUrlLoadingResult result = mExternalNavHandler.shouldOverrideUrlLoading(params);
        if (mResultCallbackForTesting != null) {
            mResultCallbackForTesting.onResult(Pair.create(url, result));
        }

        RecordHistogram.recordEnumeratedHistogram("Android.Intent.ShouldIgnoreNewTabResult",
                result.getResultType(), OverrideUrlLoadingResultType.NUM_ENTRIES);
        return result.getResultType()
                != ExternalNavigationHandler.OverrideUrlLoadingResultType.NO_OVERRIDE;
    }

    @Override
    public boolean shouldIgnoreNavigation(NavigationHandle navigationHandle, GURL escapedUrl) {
        // We should never get here for non-main-frame navigations.
        if (!navigationHandle.isInPrimaryMainFrame()) throw new RuntimeException();

        mClient.onNavigationStarted(navigationHandle);

        RedirectHandler redirectHandler = mClient.getOrCreateRedirectHandler();
        OverrideUrlLoadingResult result = shouldOverrideUrlLoading(redirectHandler, escapedUrl,
                navigationHandle.pageTransition(), navigationHandle.isRedirect(),
                navigationHandle.hasUserGesture(), navigationHandle.isRendererInitiated(),
                navigationHandle.getReferrerUrl(), navigationHandle.isInPrimaryMainFrame(),
                navigationHandle.getInitiatorOrigin(), navigationHandle.isExternalProtocol(),
                mClient.areIntentLaunchesAllowedInHiddenTabsForNavigation(navigationHandle));

        mClient.onDecisionReachedForNavigation(navigationHandle, result);

        switch (result.getResultType()) {
            case OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT:
                onDidFinishMainFrameUrlOverriding(true, false, escapedUrl);
                return true;
            case OverrideUrlLoadingResultType.OVERRIDE_WITH_CLOBBERING_TAB:
                mShouldClearRedirectHistoryForTabClobbering = true;
                return true;
            case OverrideUrlLoadingResultType.OVERRIDE_WITH_ASYNC_ACTION:
                return true;
            case OverrideUrlLoadingResultType.NO_OVERRIDE:
            default:
                if (navigationHandle.isExternalProtocol()) {
                    logBlockedNavigationToDevToolsConsole(escapedUrl);
                    return true;
                }
                return false;
        }
    }

    @Override
    public GURL handleSubframeExternalProtocol(GURL escapedUrl, @PageTransition int transition,
            boolean hasUserGesture, Origin initiatorOrigin) {
        // Only external protocol navigations are intercepted for iframe navigations.  Since
        // we do not see all previous navigations for the iframe, we can not build a complete
        // redirect handler for each iframe.  Nor can we use the top level redirect handler as
        // that has the potential to incorrectly give access to the navigation due to previous
        // main frame gestures.
        //
        // By creating a new redirect handler for each external navigation, we are specifically
        // not covering the case where a gesture is carried over via a redirect.  This is
        // currently not feasible because we do not see all navigations for iframes and it is
        // better to error on the side of caution and require direct user gestures for iframes.
        RedirectHandler redirectHandler = RedirectHandler.create();

        OverrideUrlLoadingResult result = shouldOverrideUrlLoading(redirectHandler, escapedUrl,
                transition, false /* isRedirect */, hasUserGesture, true /* isRendererInitiated */,
                GURL.emptyGURL() /* referrerUrl */, false /* isInPrimaryMainFrame */,
                initiatorOrigin, true /* isExternalProtocol */,
                false /* areIntentLaunchesAllowedInHiddenTabsForNavigation */);

        switch (result.getResultType()) {
            case OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT:
                return null;
            case OverrideUrlLoadingResultType.OVERRIDE_WITH_CLOBBERING_TAB:
                // TODO(https://crbug.com/1365100): Pass the clobbering URL to native and do a
                // redirect rather than clobbering the tab.
                return null;
            case OverrideUrlLoadingResultType.OVERRIDE_WITH_ASYNC_ACTION:
                return null;
            case OverrideUrlLoadingResultType.NO_OVERRIDE:
            default:
                logBlockedNavigationToDevToolsConsole(escapedUrl);
                return null;
        }
    }

    private OverrideUrlLoadingResult shouldOverrideUrlLoading(RedirectHandler redirectHandler,
            GURL escapedUrl, @PageTransition int pageTransition, boolean isRedirect,
            boolean hasUserGesture, boolean isRendererInitiated, GURL referrerUrl,
            boolean isInPrimaryMainFrame, Origin initiatorOrigin, boolean isExternalProtocol,
            boolean areIntentLaunchesAllowedInHiddenTabsForNavigation) {
        redirectHandler.updateNewUrlLoading(pageTransition, isRedirect, hasUserGesture,
                mClient.getLastUserInteractionTime(), getLastCommittedEntryIndex(),
                isInitialNavigation(), isRendererInitiated);

        // http://crbug.com/448977: If this is on the initial navigation chain we set the parameter
        // to open any outgoing intents that come back to Chrome in a new tab as the existing one
        // may have been closed.
        boolean onInitialNavigationChain = isTabOnInitialNavigationChain();
        boolean isInitialTabLaunchInBackground =
                mClient.wasTabLaunchedFromLongPressInBackground() && onInitialNavigationChain;
        ExternalNavigationParams params =
                new ExternalNavigationParams
                        .Builder(escapedUrl, mClient.isIncognito(), referrerUrl, pageTransition,
                                isRedirect)
                        .setApplicationMustBeInForeground(true)
                        .setRedirectHandler(redirectHandler)
                        .setOpenInNewTab(onInitialNavigationChain)
                        .setIsBackgroundTabNavigation(
                                mClient.isHidden() && !isInitialTabLaunchInBackground)
                        .setIntentLaunchesAllowedInBackgroundTabs(
                                areIntentLaunchesAllowedInHiddenTabsForNavigation)
                        .setIsMainFrame(isInPrimaryMainFrame)
                        .setHasUserGesture(hasUserGesture)
                        .setIsRendererInitiated(isRendererInitiated)
                        .setInitiatorOrigin(initiatorOrigin)
                        .setAsyncActionTakenInMainFrameCallback(this::onDidTakeMainFrameAsyncAction)
                        .build();

        OverrideUrlLoadingResult result = mExternalNavHandler.shouldOverrideUrlLoading(params);
        if (mResultCallbackForTesting != null) {
            mResultCallbackForTesting.onResult(Pair.create(escapedUrl, result));
        }

        String protocolType = isExternalProtocol ? "ExternalProtocol" : "InternalProtocol";
        RecordHistogram.recordEnumeratedHistogram(
                "Android.TabNavigationInterceptResult.For" + protocolType, result.getResultType(),
                OverrideUrlLoadingResultType.NUM_ENTRIES);
        return result;
    }

    @Override
    public void onResourceRequestWithGesture() {
        // LINK is the default transition type, and is generally used for everything coming from a
        // renderer that isn't a form submission (or subframe).
        @PageTransition
        int transition = PageTransition.LINK;
        mClient.getOrCreateRedirectHandler().updateNewUrlLoading(transition, false, true,
                mClient.getLastUserInteractionTime(), getLastCommittedEntryIndex(), false, true);
    }

    /**
     * Updates navigation history if navigation is canceled due to intent handler. We go back to the
     * last committed entry index which was saved before the navigation, and remove the empty
     * entries from the navigation history. See crbug.com/426679
     */
    public void maybeUpdateNavigationHistory() {
        WebContents webContents = mClient.getWebContents();
        if (mClearAllForwardHistoryRequired && webContents != null) {
            webContents.getNavigationController().pruneForwardEntries();
        } else if (mShouldClearRedirectHistoryForTabClobbering && webContents != null) {
            // http://crbug/479056: Even if we clobber the current tab, we want to remove
            // redirect history to be consistent.
            NavigationController navigationController = webContents.getNavigationController();
            int indexBeforeRedirection =
                    mClient.getOrCreateRedirectHandler()
                            .getLastCommittedEntryIndexBeforeStartingNavigation();
            int lastCommittedEntryIndex = getLastCommittedEntryIndex();
            for (int i = lastCommittedEntryIndex - 1; i > indexBeforeRedirection; --i) {
                boolean ret = navigationController.removeEntryAtIndex(i);
                assert ret;
            }
        }
        mClearAllForwardHistoryRequired = false;
        mShouldClearRedirectHistoryForTabClobbering = false;
    }

    private int getLastCommittedEntryIndex() {
        if (mClient.getWebContents() == null) return -1;
        return mClient.getWebContents().getNavigationController().getLastCommittedEntryIndex();
    }

    private boolean isInitialNavigation() {
        if (mClient.getWebContents() == null) return true;
        return mClient.getWebContents().getNavigationController().isInitialNavigation();
    }

    private boolean isTabOnInitialNavigationChain() {
        if (mClient.getWebContents() == null) return false;

        if (mClient.getWebContents().getLastCommittedUrl().isEmpty()) return true;

        // http://crbug/415948: If the user has not started a non-initial
        // navigation, this might be a JS redirect.
        if (mClient.getOrCreateRedirectHandler().isOnNavigation()) {
            return !mClient.getOrCreateRedirectHandler().hasUserStartedNonInitialNavigation();
        }
        return false;
    }

    private void onDidTakeMainFrameAsyncAction(AsyncActionTakenParams params) {
        onDidFinishMainFrameUrlOverriding(params.canCloseTab, params.willClobberTab,
                params.externalNavigationParams.getUrl());
    }

    /**
     * Called when Chrome decides to override URL loading and launch an intent or an asynchronous
     * action.
     */
    private void onDidFinishMainFrameUrlOverriding(
            boolean canCloseTab, boolean willClobberTab, GURL escapedUrl) {
        if (mClient.getWebContents() == null) return;

        boolean shouldCloseTab = canCloseTab && isTabOnInitialNavigationChain();

        @MainFrameIntentLaunch
        int mainFrameLaunchType;
        boolean fromApp = mClient.wasTabLaunchedFromExternalApp();
        if (UrlUtilities.hasIntentScheme(escapedUrl)) {
            mainFrameLaunchType = fromApp
                    ? MainFrameIntentLaunch.FROM_EXTERNAL_APP_TO_INTENT_SCHEME
                    : MainFrameIntentLaunch.NOT_FROM_EXTERNAL_APP_TO_INTENT_SCHEME;
        } else if (UrlUtilities.isAcceptedScheme(escapedUrl)) {
            mainFrameLaunchType = fromApp
                    ? MainFrameIntentLaunch.FROM_EXTERNAL_APP_TO_SUPPORTED_SCHEME
                    : MainFrameIntentLaunch.NOT_FROM_EXTERNAL_APP_TO_SUPPORTED_SCHEME;
        } else {
            mainFrameLaunchType = fromApp
                    ? MainFrameIntentLaunch.FROM_EXTERNAL_APP_TO_CUSTOM_SCHEME
                    : MainFrameIntentLaunch.NOT_FROM_EXTERNAL_APP_TO_CUSTOM_SCHEME;
        }
        RecordHistogram.recordEnumeratedHistogram(MAIN_FRAME_INTENT_LAUNCH_NAME,
                mainFrameLaunchType, MainFrameIntentLaunch.NUM_ENTRIES);

        // Before leaving Chrome, close any tab created for the navigation chain.
        if (shouldCloseTab) {
            // Defer closing a tab (and the associated WebContents) until the navigation
            // request and the throttle finishes the job with it.
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
                @Override
                public void run() {
                    // Tab was destroyed before this task ran.
                    if (mClient.getWebContents() == null) return;

                    // If the launch was from an External app, Chrome came from the background and
                    // acted as an intermediate link redirector between two apps (crbug.com/487938).
                    if (mClient.wasTabLaunchedFromExternalApp()) {
                        if (mClient.getOrCreateRedirectHandler().wasTaskStartedByExternalIntent()) {
                            // If Chrome was only launched to perform a redirect, don't keep its
                            // task in history.
                            mClient.getActivity().finishAndRemoveTask();
                        } else {
                            // Takes Chrome out of the back stack.
                            mClient.getActivity().moveTaskToBack(false);
                        }
                    }
                    // Closing tab must happen after we potentially call finishAndRemoveTask, as
                    // closing tabs can lead to the Activity being finished, which would cause
                    // Android to ignore the finishAndRemoveTask call, leaving the task around.
                    mClient.closeTab();
                }
            });
            return;
        }

        if (!mClient.getOrCreateRedirectHandler().isOnNavigation()) return;
        int lastCommittedEntryIndexBeforeNavigation =
                mClient.getOrCreateRedirectHandler()
                        .getLastCommittedEntryIndexBeforeStartingNavigation();
        if (getLastCommittedEntryIndex() <= lastCommittedEntryIndexBeforeNavigation) return;
        if (willClobberTab) {
            mShouldClearRedirectHistoryForTabClobbering = true;
        } else {
            // http://crbug/426679 : we want to go back to the last committed entry index which
            // was saved before this navigation, and remove the empty entries from the
            // navigation history.
            mClearAllForwardHistoryRequired = true;
            mClient.getWebContents().getNavigationController().goToNavigationIndex(
                    lastCommittedEntryIndexBeforeNavigation);
        }
    }

    private void logBlockedNavigationToDevToolsConsole(GURL url) {
        int resId = mExternalNavHandler.canExternalAppHandleUrl(url)
                ? R.string.blocked_navigation_warning
                : R.string.unreachable_navigation_warning;
        mClient.getWebContents().addMessageToDevToolsConsole(ConsoleMessageLevel.WARNING,
                ContextUtils.getApplicationContext().getString(resId, url.getSpec()));
    }

    @VisibleForTesting
    public void setResultCallbackForTesting(
            Callback<Pair<GURL, OverrideUrlLoadingResult>> callback) {
        mResultCallbackForTesting = callback;
    }

    @NativeMethods
    interface Natives {
        void associateWithWebContents(
                InterceptNavigationDelegateImpl nativeInterceptNavigationDelegateImpl,
                WebContents webContents);
    }
}
