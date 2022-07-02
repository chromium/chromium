// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.external_intents;

import android.util.Pair;

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
import org.chromium.url.GURL;
import org.chromium.url.Origin;

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
    private final AuthenticatorNavigationInterceptor mAuthenticatorHelper;
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
        mAuthenticatorHelper = mClient.createAuthenticatorNavigationInterceptor();
        associateWithWebContents(mClient.getWebContents());
    }

    // Invoked by the client when a navigation has finished in the context in which this object is
    // operating.
    public void onNavigationFinished(NavigationHandle navigation) {
        if (!navigation.hasCommitted() || !navigation.isInPrimaryMainFrame()) return;
        maybeUpdateNavigationHistory();
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
        if (mAuthenticatorHelper != null
                && mAuthenticatorHelper.handleAuthenticatorUrl(url.getSpec())) {
            return true;
        }

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
        return result.getResultType()
                != ExternalNavigationHandler.OverrideUrlLoadingResultType.NO_OVERRIDE;
    }

    @Override
    public boolean shouldIgnoreNavigation(
            NavigationHandle navigationHandle, GURL escapedUrl, boolean applyUserGestureCarryover) {
        mClient.onNavigationStarted(navigationHandle);

        GURL url = escapedUrl;
        long lastUserInteractionTime = mClient.getLastUserInteractionTime();

        if (mAuthenticatorHelper != null
                && mAuthenticatorHelper.handleAuthenticatorUrl(url.getSpec())) {
            return true;
        }

        RedirectHandler redirectHandler = null;
        if (navigationHandle.isInPrimaryMainFrame()) {
            redirectHandler = mClient.getOrCreateRedirectHandler();
        } else if (navigationHandle.isExternalProtocol()) {
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
            redirectHandler = RedirectHandler.create();
        } else {
            assert false;
            return false;
        }

        // Temporarily apply User Gesture Carryover exception for resource requests to the
        // NavigationHandle.
        if (applyUserGestureCarryover) {
            assert !navigationHandle.hasUserGesture();
            navigationHandle.setUserGestureForCarryover(true);
        }
        redirectHandler.updateNewUrlLoading(navigationHandle.pageTransition(),
                navigationHandle.isRedirect(), navigationHandle.hasUserGesture(),
                lastUserInteractionTime, getLastCommittedEntryIndex(), isInitialNavigation());

        ExternalNavigationParams params =
                buildExternalNavigationParams(navigationHandle, redirectHandler, escapedUrl)
                        .build();
        OverrideUrlLoadingResult result = mExternalNavHandler.shouldOverrideUrlLoading(params);
        if (mResultCallbackForTesting != null) {
            mResultCallbackForTesting.onResult(Pair.create(url, result));
        }

        mClient.onDecisionReachedForNavigation(navigationHandle, result);

        if (applyUserGestureCarryover) {
            navigationHandle.setUserGestureForCarryover(false);
        }

        boolean isExternalProtocol = !UrlUtilities.isAcceptedScheme(params.getUrl());
        String protocolType = isExternalProtocol ? "ExternalProtocol" : "InternalProtocol";
        RecordHistogram.recordEnumeratedHistogram(
                "Android.TabNavigationInterceptResult.For" + protocolType, result.getResultType(),
                OverrideUrlLoadingResultType.NUM_ENTRIES);
        switch (result.getResultType()) {
            case OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT:
                assert mExternalNavHandler.canExternalAppHandleUrl(url);
                if (navigationHandle.isInPrimaryMainFrame()) {
                    onDidFinishMainFrameUrlOverriding(true, false);
                }
                return true;
            case OverrideUrlLoadingResultType.OVERRIDE_WITH_CLOBBERING_TAB:
                mShouldClearRedirectHistoryForTabClobbering = true;
                return true;
            case OverrideUrlLoadingResultType.OVERRIDE_WITH_ASYNC_ACTION:
                return true;
            case OverrideUrlLoadingResultType.NO_OVERRIDE:
            default:
                if (navigationHandle.isExternalProtocol()) {
                    logBlockedNavigationToDevToolsConsole(url);
                    return true;
                }
                return false;
        }
    }

    /**
     * Returns ExternalNavigationParams.Builder to generate ExternalNavigationParams for
     * ExternalNavigationHandler#shouldOverrideUrlLoading().
     */
    public ExternalNavigationParams.Builder buildExternalNavigationParams(
            NavigationHandle navigationHandle, RedirectHandler redirectHandler, GURL escapedUrl) {
        // http://crbug.com/448977: If this is on the initial navigation chain we set the parameter
        // to open any outgoing intents that come back to Chrome in a new tab as the existing one
        // may have been closed.
        boolean onInitialNavigationChain = isTabOnInitialNavigationChain();
        boolean isInitialTabLaunchInBackground =
                mClient.wasTabLaunchedFromLongPressInBackground() && onInitialNavigationChain;
        return new ExternalNavigationParams
                .Builder(escapedUrl, mClient.isIncognito(), navigationHandle.getReferrerUrl(),
                        navigationHandle.pageTransition(), navigationHandle.isRedirect())
                .setApplicationMustBeInForeground(true)
                .setRedirectHandler(redirectHandler)
                .setOpenInNewTab(onInitialNavigationChain)
                .setIsBackgroundTabNavigation(mClient.isHidden() && !isInitialTabLaunchInBackground)
                .setIntentLaunchesAllowedInBackgroundTabs(
                        mClient.areIntentLaunchesAllowedInHiddenTabsForNavigation(navigationHandle))
                .setIsMainFrame(navigationHandle.isInPrimaryMainFrame())
                .setHasUserGesture(navigationHandle.hasUserGesture())
                .setIsRendererInitiated(navigationHandle.isRendererInitiated())
                .setInitiatorOrigin(navigationHandle.getInitiatorOrigin())
                .setAsyncActionTakenInMainFrameCallback(this::onDidTakeMainFrameAsyncAction);
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

    @VisibleForTesting
    public AuthenticatorNavigationInterceptor getAuthenticatorNavigationInterceptor() {
        return mAuthenticatorHelper;
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
        onDidFinishMainFrameUrlOverriding(params.canCloseTab, params.willClobberTab);
    }

    /**
     * Called when Chrome decides to override URL loading and launch an intent or an asynchronous
     * action.
     */
    private void onDidFinishMainFrameUrlOverriding(boolean canCloseTab, boolean willClobberTab) {
        if (mClient.getWebContents() == null) return;

        boolean shouldCloseTab = canCloseTab && isTabOnInitialNavigationChain();

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
