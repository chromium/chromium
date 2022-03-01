// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.external_intents;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.task.PostTask;
import org.chromium.components.external_intents.ExternalNavigationHandler.OverrideUrlLoadingResult;
import org.chromium.components.external_intents.ExternalNavigationHandler.OverrideUrlLoadingResultType;
import org.chromium.components.navigation_interception.InterceptNavigationDelegate;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ConsoleMessageLevel;
import org.chromium.url.GURL;

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
    private @OverrideUrlLoadingResultType int mLastOverrideUrlLoadingResultType =
            OverrideUrlLoadingResultType.NO_OVERRIDE;
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

    public boolean shouldIgnoreNewTab(GURL url, boolean incognito) {
        if (mAuthenticatorHelper != null
                && mAuthenticatorHelper.handleAuthenticatorUrl(url.getSpec())) {
            return true;
        }

        ExternalNavigationParams params =
                new ExternalNavigationParams.Builder(url, incognito).setOpenInNewTab(true).build();
        mLastOverrideUrlLoadingResultType =
                mExternalNavHandler.shouldOverrideUrlLoading(params).getResultType();
        return mLastOverrideUrlLoadingResultType
                != ExternalNavigationHandler.OverrideUrlLoadingResultType.NO_OVERRIDE;
    }

    @VisibleForTesting
    public @OverrideUrlLoadingResultType int getLastOverrideUrlLoadingResultTypeForTests() {
        return mLastOverrideUrlLoadingResultType;
    }

    @Override
    public boolean shouldIgnoreNavigation(NavigationHandle navigationHandle, GURL escapedUrl) {
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
        redirectHandler.updateNewUrlLoading(navigationHandle.pageTransition(),
                navigationHandle.isRedirect(), navigationHandle.hasUserGesture(),
                lastUserInteractionTime, getLastCommittedEntryIndex(), isInitialNavigation());

        boolean shouldCloseTab = shouldCloseContentsOnOverrideUrlLoadingAndLaunchIntent();
        ExternalNavigationParams params = buildExternalNavigationParams(
                navigationHandle, redirectHandler, shouldCloseTab, escapedUrl)
                                                  .build();
        OverrideUrlLoadingResult result = mExternalNavHandler.shouldOverrideUrlLoading(params);
        mLastOverrideUrlLoadingResultType = result.getResultType();

        mClient.onDecisionReachedForNavigation(navigationHandle, result);

        switch (mLastOverrideUrlLoadingResultType) {
            case OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT:
                assert mExternalNavHandler.canExternalAppHandleUrl(url);
                if (navigationHandle.isInPrimaryMainFrame()) {
                    onOverrideUrlLoadingAndLaunchIntent(shouldCloseTab);
                }
                return true;
            case OverrideUrlLoadingResultType.OVERRIDE_WITH_CLOBBERING_TAB:
                mShouldClearRedirectHistoryForTabClobbering = true;
                return true;
            case OverrideUrlLoadingResultType.OVERRIDE_WITH_ASYNC_ACTION:
                if (!shouldCloseTab && navigationHandle.isInPrimaryMainFrame()) {
                    onOverrideUrlLoadingAndLaunchIntent(shouldCloseTab);
                }
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
            NavigationHandle navigationHandle, RedirectHandler redirectHandler,
            boolean shouldCloseTab, GURL escapedUrl) {
        boolean isInitialTabLaunchInBackground =
                mClient.wasTabLaunchedFromLongPressInBackground() && shouldCloseTab;
        // http://crbug.com/448977: If a new tab is closed by this overriding, we should open an
        // Intent in a new tab when Chrome receives it again.
        return new ExternalNavigationParams
                .Builder(escapedUrl, mClient.isIncognito(), navigationHandle.getReferrerUrl(),
                        navigationHandle.pageTransition(), navigationHandle.isRedirect())
                .setApplicationMustBeInForeground(true)
                .setRedirectHandler(redirectHandler)
                .setOpenInNewTab(shouldCloseTab)
                .setIsBackgroundTabNavigation(mClient.isHidden() && !isInitialTabLaunchInBackground)
                .setIntentLaunchesAllowedInBackgroundTabs(
                        mClient.areIntentLaunchesAllowedInHiddenTabsForNavigation(navigationHandle))
                .setIsMainFrame(navigationHandle.isInPrimaryMainFrame())
                .setHasUserGesture(navigationHandle.hasUserGesture())
                .setShouldCloseContentsOnOverrideUrlLoadingAndLaunchIntent(
                        shouldCloseTab && navigationHandle.isInPrimaryMainFrame())
                .setIsRendererInitiated(navigationHandle.isRendererInitiated())
                .setInitiatorOrigin(navigationHandle.getInitiatorOrigin());
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

    private boolean shouldCloseContentsOnOverrideUrlLoadingAndLaunchIntent() {
        if (mClient.getWebContents() == null) return false;
        // If no navigation has committed, close the tab.
        if (mClient.getWebContents().getLastCommittedUrl().isEmpty()) return true;

        // http://crbug/415948: If the user has not started a non-initial
        // navigation, this might be a JS redirect.
        // In such case, we would like to close this tab.
        if (mClient.getOrCreateRedirectHandler().isOnNavigation()) {
            return !mClient.getOrCreateRedirectHandler().hasUserStartedNonInitialNavigation();
        }
        return false;
    }

    /**
     * Called when Chrome decides to override URL loading and launch an intent or an asynchronous
     * action.
     * @param shouldCloseTab
     */
    private void onOverrideUrlLoadingAndLaunchIntent(boolean shouldCloseTab) {
        if (mClient.getWebContents() == null) return;

        // Before leaving Chrome, close the empty child tab.
        // If a new tab is created through JavaScript open to load this
        // url, we would like to close it as we will load this url in a
        // different Activity.
        if (shouldCloseTab) {
            // Defer closing a tab (and the associated WebContents) till the navigation
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
        } else if (mClient.getOrCreateRedirectHandler().isOnNavigation()) {
            int lastCommittedEntryIndexBeforeNavigation =
                    mClient.getOrCreateRedirectHandler()
                            .getLastCommittedEntryIndexBeforeStartingNavigation();
            if (getLastCommittedEntryIndex() > lastCommittedEntryIndexBeforeNavigation) {
                // http://crbug/426679 : we want to go back to the last committed entry index which
                // was saved before this navigation, and remove the empty entries from the
                // navigation history.
                mClearAllForwardHistoryRequired = true;
                mClient.getWebContents().getNavigationController().goToNavigationIndex(
                        lastCommittedEntryIndexBeforeNavigation);
            }
        }
    }

    private void logBlockedNavigationToDevToolsConsole(GURL url) {
        int resId = mExternalNavHandler.canExternalAppHandleUrl(url)
                ? R.string.blocked_navigation_warning
                : R.string.unreachable_navigation_warning;
        mClient.getWebContents().addMessageToDevToolsConsole(ConsoleMessageLevel.WARNING,
                ContextUtils.getApplicationContext().getString(resId, url.getSpec()));
    }

    @NativeMethods
    interface Natives {
        void associateWithWebContents(
                InterceptNavigationDelegateImpl nativeInterceptNavigationDelegateImpl,
                WebContents webContents);
    }
}
