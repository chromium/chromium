// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.external_intents;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.util.Pair;

import androidx.annotation.IntDef;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.CancelableRunnable;
import org.chromium.base.ContextUtils;
import org.chromium.base.RequiredCallback;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.blink.mojom.WebFeature;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.external_intents.ExternalNavigationHandler.OverrideUrlLoadingResult;
import org.chromium.components.external_intents.ExternalNavigationHandler.OverrideUrlLoadingResultType;
import org.chromium.components.external_intents.ExternalNavigationParams.AsyncActionTakenParams;
import org.chromium.components.navigation_interception.InterceptNavigationDelegate;
import org.chromium.content_public.browser.ContentWebFeatureUsageUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.common.ConsoleMessageLevel;
import org.chromium.content_public.common.Referrer;
import org.chromium.network.mojom.ReferrerPolicy;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Class that controls navigations and allows to intercept them. It is used on Android to 'convert'
 * certain navigations to Intents to 3rd party applications. Note the Intent is often created
 * together with a new empty tab which then should be closed immediately. Closing the tab will
 * cancel the navigation that this delegate is running for, hence can cause UAF error. It should be
 * done in an asynchronous fashion to avoid it. See https://crbug.com/732260.
 */
@JNINamespace("external_intents")
@NullMarked
public class InterceptNavigationDelegateImpl extends InterceptNavigationDelegate {
    /**
     * Histogram for the source of a main frame intent launch. This enum is used in UMA, do not
     * reorder values.
     */
    @IntDef({
        MainFrameIntentLaunch.NOT_FROM_EXTERNAL_APP_TO_INTENT_SCHEME,
        MainFrameIntentLaunch.NOT_FROM_EXTERNAL_APP_TO_CUSTOM_SCHEME,
        MainFrameIntentLaunch.NOT_FROM_EXTERNAL_APP_TO_SUPPORTED_SCHEME,
        MainFrameIntentLaunch.FROM_EXTERNAL_APP_TO_INTENT_SCHEME,
        MainFrameIntentLaunch.FROM_EXTERNAL_APP_TO_CUSTOM_SCHEME,
        MainFrameIntentLaunch.FROM_EXTERNAL_APP_TO_SUPPORTED_SCHEME,
        MainFrameIntentLaunch.NUM_ENTRIES
    })
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

    /**
     * Histogram for the scheme of an overridden navigation. This enum is used in UMA, do not
     * reorder values.
     */
    @IntDef({
        InterceptScheme.NOT_INTERCEPTED,
        InterceptScheme.UNKNOWN_SCHEME,
        InterceptScheme.ACCEPTED_SCHEME,
        InterceptScheme.INTENT_SCHEME,
        InterceptScheme.MDOC_SCHEME,
        InterceptScheme.OPENID4VP_SCHEME,
        InterceptScheme.OPENID4VCI_SCHEME,
        InterceptScheme.HAIP_SCHEME,
        InterceptScheme.NUM_ENTRIES
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface InterceptScheme {
        int NOT_INTERCEPTED = 0;
        int UNKNOWN_SCHEME = 1;
        int ACCEPTED_SCHEME = 2;
        int INTENT_SCHEME = 3;
        int MDOC_SCHEME = 4;
        int OPENID4VP_SCHEME = 5;
        int OPENID4VCI_SCHEME = 6;
        int HAIP_SCHEME = 7;
        int NUM_ENTRIES = 8;
    }

    private static final String MDOC_SCHEME = "mdoc";
    private static final String HAIP_SCHEME = "haip";
    private static final String OPENID4VP_SCHEME_SUFFIX = "openid4vp";
    private static final String OPENID4VCI_SCHEME = "openid-credential-offer";

    private static final String MAIN_FRAME_INTENT_LAUNCH_NAME =
            "Android.Intent.MainFrameIntentLaunch";

    private static final String INTENT_LAUNCH_FROM_TAB_CREATION =
            "Android.Intent.IntentLaunchFromTabCreation";

    private static final long DEFER_NAVIGATION_TIMEOUT_MILLIS = 5000;

    private final InterceptNavigationDelegateClient mClient;
    private static @Nullable Callback<Pair<GURL, OverrideUrlLoadingResult>>
            sResultCallbackForTesting;

    private @Nullable WebContents mWebContents;

    private @Nullable ExternalNavigationHandler mExternalNavHandler;

    private @Nullable WebContentsObserver mWebContentsObserver;

    /** Whether forward history should be cleared after navigation is committed. */
    private boolean mClearAllForwardHistoryRequired;

    private boolean mShouldClearRedirectHistoryForTabClobbering;

    private @Nullable CancelableRunnable mPendingShouldIgnore;
    private @Nullable RequiredCallback<Boolean> mShouldIgnoreResultCallback;
    private boolean mHasAttachedToActivity;
    private boolean mTimedOutWaitingForActivity;

    /** Default constructor of {@link InterceptNavigationDelegateImpl}. */
    public InterceptNavigationDelegateImpl(InterceptNavigationDelegateClient client) {
        mClient = client;
        associateWithWebContents(mClient.getWebContents());
        mHasAttachedToActivity = mClient.getActivity() != null;
    }

    // Invoked by the client when a navigation has finished in the context in which this object is
    // operating.
    public void onNavigationFinishedInPrimaryMainFrame(NavigationHandle navigation) {
        if (!navigation.hasCommitted()) return;
        maybeUpdateNavigationHistory();
    }

    public void setExternalNavigationHandler(@Nullable ExternalNavigationHandler handler) {
        mExternalNavHandler = handler;
    }

    public void onActivityAttachmentChanged(boolean attached) {
        // Defensively cancel any pending checks if we change Activities during a check.
        if (mHasAttachedToActivity) {
            cancelPendingShouldIgnoreCheck();
            return;
        }
        // Wait until first attached.
        if (!attached) return;
        mHasAttachedToActivity = true;
        mTimedOutWaitingForActivity = false;
        requestFinishPendingShouldIgnoreCheck();
    }

    public void associateWithWebContents(@Nullable WebContents webContents) {
        if (mWebContents == webContents) return;

        // Before we attach to another WebContents, cancel any checks that were in progress.
        cancelPendingShouldIgnoreCheck();

        if (mWebContents != null) {
            assumeNonNull(mWebContentsObserver).observe(null);
            mWebContentsObserver = null;
            InterceptNavigationDelegateImplJni.get().clearWebContentsAssociation(mWebContents);
        }
        mWebContents = webContents;
        if (mWebContents == null) return;

        // Lazily initialize the external navigation handler.
        if (mExternalNavHandler == null) {
            setExternalNavigationHandler(mClient.createExternalNavigationHandler());
            if (mExternalNavHandler == null) return;
        }

        InterceptNavigationDelegateImplJni.get().associateWithWebContents(this, mWebContents);

        mWebContentsObserver =
                new WebContentsObserver(mWebContents) {
                    @Override
                    public void didStartNavigationInPrimaryMainFrame(NavigationHandle navigation) {
                        assumeNonNull(mExternalNavHandler);
                        mExternalNavHandler.onNavigationStarted(navigation.getNavigationId());
                    }

                    @Override
                    public void didFinishNavigationInPrimaryMainFrame(NavigationHandle navigation) {
                        assumeNonNull(mExternalNavHandler);
                        mExternalNavHandler.onNavigationFinished(navigation.getNavigationId());
                    }
                };
    }

    @Override
    public void shouldIgnoreNavigation(
            NavigationHandle navigationHandle,
            GURL escapedUrl,
            boolean hiddenCrossFrame,
            boolean isSandboxedFrame,
            boolean shouldRunAsync,
            RequiredCallback<Boolean> resultCallback) {
        // We should never get here for non-main-frame navigations.
        if (!navigationHandle.isInPrimaryMainFrame()) throw new RuntimeException();

        RedirectHandler redirectHandler = mClient.getOrCreateRedirectHandler();
        redirectHandler.updateNewUrlLoading(
                navigationHandle.pageTransition(),
                navigationHandle.isRedirect(),
                navigationHandle.hasUserGesture(),
                getLastCommittedEntryIndex(),
                isInitialNavigation(),
                navigationHandle.isRendererInitiated());

        // Initial navigation never leaves Chrome anyways (unless explicitly requested by a CCT
        // client) and we don't want to block background navigation on waiting for a tab (or CCT
        // pre-warming wouldn't work). Subsequent navigations and redirects can leave Chrome so
        // they'll have to wait to be attached to an Activity.
        if (!redirectHandler.canInitialNavigationLeaveChrome()
                && !mHasAttachedToActivity
                && isInitialNavigation()
                && !navigationHandle.isRedirect()) {
            resultCallback.onResult(false);
            return;
        }

        // If not attached to an Activity, we cannot check synchronously and need to defer.
        if (!mHasAttachedToActivity) {
            // Only wait to attach to an Activity for external app launches. Any other background
            // launches don't need to wait for an Activity to attach and can just block external
            // navigations (like Custom Tab prerenders).
            //
            // Also, if the previous navigation timed out waiting for Activity to attach, don't keep
            // waiting.
            if (!mClient.wasTabLaunchedFromExternalApp() || mTimedOutWaitingForActivity) {
                resultCallback.onResult(false);
                return;
            }
            shouldRunAsync = true;
            startTimeoutForDeferredNavigation();
        }

        mShouldIgnoreResultCallback = resultCallback;
        OverrideUrlLoadingResult result =
                shouldOverrideUrlLoading(
                        redirectHandler,
                        escapedUrl,
                        navigationHandle.pageTransition(),
                        navigationHandle.isRedirect(),
                        navigationHandle.hasUserGesture(),
                        navigationHandle.isRendererInitiated(),
                        navigationHandle.getReferrerUrl(),
                        navigationHandle.isInPrimaryMainFrame(),
                        navigationHandle.getInitiatorOrigin(),
                        navigationHandle.isExternalProtocol(),
                        this::onDidAsyncActionInMainFrame,
                        hiddenCrossFrame,
                        isSandboxedFrame,
                        navigationHandle.getNavigationId(),
                        shouldRunAsync);
        if (!shouldRunAsync) {
            onMainFrameShouldIgnoreNavigationResult(
                    assumeNonNull(result), escapedUrl, navigationHandle.isExternalProtocol());
        }
    }

    private void onMainFrameShouldIgnoreNavigationResult(
            OverrideUrlLoadingResult result, GURL url, boolean isExternalProtocol) {
        mPendingShouldIgnore = null;
        boolean shouldIgnore;
        switch (result.getResultType()) {
            case OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT:
                onDidFinishMainFrameIntentLaunch(true, url);
                shouldIgnore = true;
                break;
            case OverrideUrlLoadingResultType.OVERRIDE_WITH_NAVIGATE_TAB:
                clobberMainFrame(result.getTargetUrl(), result.getExternalNavigationParams());
                shouldIgnore = true;
                break;
            case OverrideUrlLoadingResultType.OVERRIDE_WITH_ASYNC_ACTION:
            case OverrideUrlLoadingResultType.OVERRIDE_CLOSING_AFTER_AUTH:
                shouldIgnore = true;
                break;
            case OverrideUrlLoadingResultType.OVERRIDE_WITH_REPARENT_TO_BROWSER:
            case OverrideUrlLoadingResultType.NO_OVERRIDE:
            default:
                if (isExternalProtocol) {
                    logBlockedNavigationToDevToolsConsole(url);
                    shouldIgnore = true;
                    break;
                }
                shouldIgnore = false;
                break;
        }
        runResultCallback(shouldIgnore);

        if (!shouldIgnore
                && result.getResultType()
                        == OverrideUrlLoadingResultType.OVERRIDE_WITH_REPARENT_TO_BROWSER
                && !mClient.isTabDetached()) {
            // Reparenting task must be executed after runResultCallback has been called.
            mClient.startReparentingTask();
        }
    }

    @Override
    public void requestFinishPendingShouldIgnoreCheck() {
        if (mPendingShouldIgnore == null) return;
        CancelableRunnable runnable = mPendingShouldIgnore;
        runnable.run();
        if (mPendingShouldIgnore != null) startTimeoutForDeferredNavigation();
        // Cancel, ensuring any pending posted tasks do not execute by converting this runnable
        // to a no-op.
        runnable.cancel();
    }

    private void cancelPendingShouldIgnoreCheck() {
        // Running the result callback could synchronously queue up more checks
        while (mPendingShouldIgnore != null) {
            mPendingShouldIgnore.cancel();
            mPendingShouldIgnore = null;
            runResultCallback(false);
        }
        assert mShouldIgnoreResultCallback == null;
    }

    private void runResultCallback(boolean shouldIgnore) {
        RequiredCallback<Boolean> callback = assumeNonNull(mShouldIgnoreResultCallback);
        // Clear before calling onResult, because onResult could queue up another callback.
        mShouldIgnoreResultCallback = null;
        callback.onResult(shouldIgnore);
    }

    @Override
    public @Nullable GURL handleSubframeExternalProtocol(
            GURL escapedUrl,
            @PageTransition int transition,
            boolean hasUserGesture,
            Origin initiatorOrigin) {
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

        boolean isRedirect = false;
        boolean isRendererInitiated = true;
        redirectHandler.updateNewUrlLoading(
                transition,
                isRedirect,
                hasUserGesture,
                getLastCommittedEntryIndex(),
                isInitialNavigation(),
                isRendererInitiated);

        OverrideUrlLoadingResult result =
                shouldOverrideUrlLoading(
                        redirectHandler,
                        escapedUrl,
                        transition,
                        isRedirect,
                        hasUserGesture,
                        isRendererInitiated,
                        GURL.emptyGURL()
                        /* referrerUrl= */ ,
                        /* isInPrimaryMainFrame= */ false,
                        initiatorOrigin,
                        /* isExternalProtocol= */ true,
                        this::onDidAsyncActionInSubFrame,
                        /* hiddenCrossFrame= */ false,
                        /* isSandboxedMainFrame= */ false,
                        /* navigationId= */ -1,
                        /* shouldRunAsync= */ false);

        switch (assumeNonNull(result).getResultType()) {
            case OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT:
                return null;
            case OverrideUrlLoadingResultType.OVERRIDE_WITH_NAVIGATE_TAB:
                assert result.getTargetUrl() != null;
                return result.getTargetUrl();
            case OverrideUrlLoadingResultType.OVERRIDE_WITH_ASYNC_ACTION:
                // Empty GURL indicates a pending async action.
                return GURL.emptyGURL();
            case OverrideUrlLoadingResultType.NO_OVERRIDE:
            default:
                logBlockedNavigationToDevToolsConsole(escapedUrl);
                return null;
        }
    }

    private @Nullable OverrideUrlLoadingResult shouldOverrideUrlLoading(
            RedirectHandler redirectHandler,
            GURL escapedUrl,
            @PageTransition int pageTransition,
            boolean isRedirect,
            boolean hasUserGesture,
            boolean isRendererInitiated,
            GURL referrerUrl,
            boolean isInPrimaryMainFrame,
            @Nullable Origin initiatorOrigin,
            boolean isExternalProtocol,
            Callback<AsyncActionTakenParams> asyncActionTakenCallback,
            boolean hiddenCrossFrame,
            boolean isSandboxedMainFrame,
            long navigationId,
            boolean shouldRunAsync) {
        assert mPendingShouldIgnore == null;

        // http://crbug.com/448977: If this is on the initial navigation chain we set the parameter
        // to open any outgoing intents that come back to Chrome in a new tab as the existing one
        // may have been closed.
        boolean onInitialNavigationChain = isTabOnInitialNavigationChain();
        boolean isWebContentsVisible =
                assumeNonNull(mClient.getWebContents()).getVisibility() == Visibility.VISIBLE;
        ExternalNavigationParams params =
                new ExternalNavigationParams.Builder(
                                escapedUrl,
                                mClient.isIncognito(),
                                referrerUrl,
                                pageTransition,
                                isRedirect)
                        .setRedirectHandler(redirectHandler)
                        .setOpenInNewTab(onInitialNavigationChain)
                        .setIsBackgroundTabNavigation(!isWebContentsVisible)
                        .setIsMainFrame(isInPrimaryMainFrame)
                        .setHasUserGesture(hasUserGesture)
                        .setIsRendererInitiated(isRendererInitiated)
                        .setInitiatorOrigin(initiatorOrigin)
                        .setAsyncActionTakenCallback(asyncActionTakenCallback)
                        .setIsInitialNavigationInFrame(isInitialNavigation())
                        .setIsHiddenCrossFrameNavigation(hiddenCrossFrame)
                        .setIsSandboxedMainFrame(isSandboxedMainFrame)
                        .setNavigationId(navigationId)
                        .setIsTabInPWA(mClient.isTabInPWA())
                        .setIsTabInBrowser(mClient.isTabInBrowser())
                        .setIsInDesktopWindowingMode(mClient.isInDesktopWindowingMode())
                        .build();
        if (!shouldRunAsync) return doShouldOverrideUrlLoading(params, isExternalProtocol);
        Runnable shouldIgnoreCheck =
                new Runnable() {
                    @Override
                    public void run() {
                        // Navigation may have been externally canceled, or something caused
                        // the Navigation Chain to be reset, so we should avoid leaving Chrome.
                        if (mWebContents == null
                                || mWebContents.isDestroyed()
                                || !params.getRedirectHandler().isOnNavigation()) {
                            cancelPendingShouldIgnoreCheck();
                            return;
                        }

                        // This may be a pre-warmed tab being navigated during startup, wait for
                        // the tab to be associated with an Activity before continuing.
                        if (!mHasAttachedToActivity) {
                            // No need to re-post this, it will be run when we attach to an
                            // Activity.
                            mPendingShouldIgnore = new CancelableRunnable(this);
                            return;
                        }
                        onMainFrameShouldIgnoreNavigationResult(
                                doShouldOverrideUrlLoading(params, isExternalProtocol),
                                params.getUrl(),
                                isExternalProtocol);
                    }
                };
        mPendingShouldIgnore = new CancelableRunnable(shouldIgnoreCheck);
        PostTask.postTask(TaskTraits.UI_DEFAULT, mPendingShouldIgnore);
        return null;
    }

    private void startTimeoutForDeferredNavigation() {
        if (mPendingShouldIgnore == null) return;
        final CancelableRunnable pendingShouldIgnore = mPendingShouldIgnore;
        PostTask.postDelayedTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    // Don't accidentally cancel subsequent navigations.
                    if (pendingShouldIgnore != mPendingShouldIgnore) return;
                    mTimedOutWaitingForActivity = true;
                    cancelPendingShouldIgnoreCheck();
                },
                DEFER_NAVIGATION_TIMEOUT_MILLIS);
    }

    private OverrideUrlLoadingResult doShouldOverrideUrlLoading(
            ExternalNavigationParams params, boolean isExternalProtocol) {
        try (TraceEvent e = TraceEvent.scoped("shouldOverrideUrlLoading")) {
            WebContents webContents = assumeNonNull(mClient.getWebContents());
            OverrideUrlLoadingResult result =
                    assumeNonNull(mExternalNavHandler).shouldOverrideUrlLoading(params);

            if (sResultCallbackForTesting != null) {
                sResultCallbackForTesting.onResult(Pair.create(params.getUrl(), result));
            }

            String protocolType = isExternalProtocol ? "ExternalProtocol" : "InternalProtocol";
            RecordHistogram.recordEnumeratedHistogram(
                    "Android.TabNavigationInterceptResult.For" + protocolType,
                    result.getResultType(),
                    OverrideUrlLoadingResultType.NUM_ENTRIES);

            int scheme = InterceptScheme.UNKNOWN_SCHEME;
            String digitalCredentialHistogramSuffix = null;
            if (result.getResultType() == OverrideUrlLoadingResultType.NO_OVERRIDE) {
                scheme = InterceptScheme.NOT_INTERCEPTED;
            } else if (UrlUtilities.isAcceptedScheme(params.getUrl())) {
                scheme = InterceptScheme.ACCEPTED_SCHEME;
            } else if (UrlUtilities.hasIntentScheme(params.getUrl())) {
                scheme = InterceptScheme.INTENT_SCHEME;
            } else if (MDOC_SCHEME.equals(params.getUrl().getScheme())) {
                scheme = InterceptScheme.MDOC_SCHEME;
                digitalCredentialHistogramSuffix = "ForMdoc";
            } else if (params.getUrl().getScheme().endsWith(OPENID4VP_SCHEME_SUFFIX)) {
                scheme = InterceptScheme.OPENID4VP_SCHEME;
                digitalCredentialHistogramSuffix = "ForOpenId4Vp";
            } else if (OPENID4VCI_SCHEME.equals(params.getUrl().getScheme())) {
                scheme = InterceptScheme.OPENID4VCI_SCHEME;
                digitalCredentialHistogramSuffix = "ForOpenId4Vci";
            } else if (HAIP_SCHEME.equals(params.getUrl().getScheme())) {
                scheme = InterceptScheme.HAIP_SCHEME;
                digitalCredentialHistogramSuffix = "ForHaip";
            }

            if (digitalCredentialHistogramSuffix != null) {
                ContentWebFeatureUsageUtils.logWebFeatureForCurrentPage(
                        webContents, WebFeature.IDENTITY_DIGITAL_CREDENTIALS_DEEP_LINK);
                // Record spread of `result` in order to get an idea of by how much the
                // IDENTITY_DIGITAL_CREDENTIALS_DEEP_LINK use counter is over counting as a user may
                // cancel the OverrideUrlLoadingResultType.OVERRIDE_WITH_ASYNC_ACTION dialog.
                RecordHistogram.recordEnumeratedHistogram(
                        "Android.TabNavigationInterceptResult." + digitalCredentialHistogramSuffix,
                        result.getResultType(),
                        OverrideUrlLoadingResultType.NUM_ENTRIES);
            }

            RecordHistogram.recordEnumeratedHistogram(
                    "Android.TabNavigationIntercept.Scheme", scheme, InterceptScheme.NUM_ENTRIES);
            return result;
        }
    }

    @Override
    public void onResourceRequestWithGesture() {
        // Browser-initiated navigations race against renderer-initiated resource requests.
        // It should be fine to just drop the resource request as it is from the previous page.
        // In rare cases this could theoretically arrive after a browser-initiated navigation
        // completes and allow an external navigation that shouldn't have been allowed but this
        // isn't exploitable so should be fine and not worth all of the complexity required to
        // properly fix it.
        if (mPendingShouldIgnore != null) return;

        // LINK is the default transition type, and is generally used for everything coming from a
        // renderer that isn't a form submission (or subframe).
        @PageTransition int transition = PageTransition.LINK;
        mClient.getOrCreateRedirectHandler()
                .updateNewUrlLoading(
                        transition, false, true, getLastCommittedEntryIndex(), false, true);
    }

    /**
     * Updates navigation history if navigation is canceled due to intent handler. We go back to the
     * last committed entry index which was saved before the navigation, and remove the empty
     * entries from the navigation history. See crbug.com/426679
     */
    public void maybeUpdateNavigationHistory() {
        WebContents webContents = mClient.getWebContents();
        assumeNonNull(webContents);
        NavigationController navigationController = webContents.getNavigationController();
        if (mClearAllForwardHistoryRequired && webContents != null) {
            navigationController.pruneForwardEntries();
        } else if (mShouldClearRedirectHistoryForTabClobbering && webContents != null) {
            // http://crbug/479056: Even if we clobber the current tab, we want to remove
            // redirect history to be consistent.
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

    /** Returns whether a Tab instance should be reparented from the PWA to the browser. */
    public boolean shouldReparentTab(GURL url) {
        if (mExternalNavHandler == null) {
            return false;
        }

        return mExternalNavHandler.shouldReparentTab(
                url,
                mClient.isTabInPWA(),
                isInitialNavigation(),
                mClient.isInDesktopWindowingMode());
    }

    private void onDidAsyncActionInMainFrame(AsyncActionTakenParams params) {
        switch (params.actionType) {
            case AsyncActionTakenParams.AsyncActionTakenType.NAVIGATE:
                clobberMainFrame(assumeNonNull(params.targetUrl), params.externalNavigationParams);
                break;
            case AsyncActionTakenParams.AsyncActionTakenType.EXTERNAL_INTENT_LAUNCHED:
                onDidFinishMainFrameIntentLaunch(
                        params.canCloseTab, params.externalNavigationParams.getUrl());
                break;
            default: // NO_ACTION
                break;
        }
    }

    private void onDidAsyncActionInSubFrame(AsyncActionTakenParams params) {
        if (mWebContents == null) return;
        GURL redirectUrl =
                (params.actionType == AsyncActionTakenParams.AsyncActionTakenType.NAVIGATE)
                        ? params.targetUrl
                        : null;
        InterceptNavigationDelegateImplJni.get()
                .onSubframeAsyncActionTaken(mWebContents, redirectUrl);
    }

    private void onDidFinishMainFrameIntentLaunch(boolean canCloseTab, GURL escapedUrl) {
        if (mClient.getWebContents() == null) return;
        boolean shouldCloseTab = canCloseTab && isTabOnInitialNavigationChain();

        @MainFrameIntentLaunch int mainFrameLaunchType;
        boolean fromApp = mClient.wasTabLaunchedFromExternalApp();
        if (UrlUtilities.hasIntentScheme(escapedUrl)) {
            mainFrameLaunchType =
                    fromApp
                            ? MainFrameIntentLaunch.FROM_EXTERNAL_APP_TO_INTENT_SCHEME
                            : MainFrameIntentLaunch.NOT_FROM_EXTERNAL_APP_TO_INTENT_SCHEME;
        } else if (UrlUtilities.isAcceptedScheme(escapedUrl)) {
            mainFrameLaunchType =
                    fromApp
                            ? MainFrameIntentLaunch.FROM_EXTERNAL_APP_TO_SUPPORTED_SCHEME
                            : MainFrameIntentLaunch.NOT_FROM_EXTERNAL_APP_TO_SUPPORTED_SCHEME;
        } else {
            mainFrameLaunchType =
                    fromApp
                            ? MainFrameIntentLaunch.FROM_EXTERNAL_APP_TO_CUSTOM_SCHEME
                            : MainFrameIntentLaunch.NOT_FROM_EXTERNAL_APP_TO_CUSTOM_SCHEME;
        }
        RecordHistogram.recordEnumeratedHistogram(
                MAIN_FRAME_INTENT_LAUNCH_NAME,
                mainFrameLaunchType,
                MainFrameIntentLaunch.NUM_ENTRIES);
        RecordHistogram.recordBooleanHistogram(
                INTENT_LAUNCH_FROM_TAB_CREATION, fromApp && isTabOnInitialNavigationChain());

        // Before leaving Chrome, close any tab created for the navigation chain.
        if (shouldCloseTab) {
            // Defer closing a tab (and the associated WebContents) until the navigation
            // request and the throttle finishes the job with it.
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    new Runnable() {
                        @Override
                        public void run() {
                            mClient.handleShouldCloseTab();
                        }
                    });
            return;
        }

        if (!mClient.getOrCreateRedirectHandler().isOnNavigation()) return;
        int lastCommittedEntryIndexBeforeNavigation =
                mClient.getOrCreateRedirectHandler()
                        .getLastCommittedEntryIndexBeforeStartingNavigation();
        if (getLastCommittedEntryIndex() <= lastCommittedEntryIndexBeforeNavigation) return;

        // Like clobbering below, changing navigation index could cancel the current navigation and
        // delete the NavigationThrottle calling this code, leading to UAFs. Do the navigation
        // asynchronously to avoid that.
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                new Runnable() {
                    @Override
                    public void run() {
                        // http://crbug.com/426679 : we want to go back to the last committed entry
                        // index which was saved before this navigation, and remove the empty
                        // entries from the navigation history.
                        mClearAllForwardHistoryRequired = true;
                        assumeNonNull(mClient.getWebContents())
                                .getNavigationController()
                                .goToNavigationIndex(lastCommittedEntryIndexBeforeNavigation);
                    }
                });
    }

    private void clobberMainFrame(GURL targetUrl, ExternalNavigationParams params) {
        // Our current tab clobbering strategy doesn't support persisting sandbox attributes, so
        // for sandboxed main frames, drop the navigation.
        if (params.isSandboxedMainFrame()) return;

        int transitionType = PageTransition.LINK;
        final LoadUrlParams loadUrlParams = new LoadUrlParams(targetUrl, transitionType);
        if (!params.getReferrerUrl().isEmpty()) {
            Referrer referrer =
                    new Referrer(params.getReferrerUrl().getSpec(), ReferrerPolicy.ALWAYS);
            loadUrlParams.setReferrer(referrer);
        }
        // Ideally this navigation would be part of the navigation chain that triggered it and get,
        // the correct SameSite cookie behavior, but this is impractical as Tab clobbering is
        // frequently async and would require complex changes that are probably not worth doing for
        // fallback URLs. Instead, we treat the navigation as coming from an opaque Origin so that
        // SameSite cookies aren't mistakenly sent.
        loadUrlParams.setIsRendererInitiated(params.isRendererInitiated());
        loadUrlParams.setInitiatorOrigin(Origin.createOpaqueOrigin());

        // Loading URL will start a new navigation which cancels the current one
        // that this clobbering is being done for. It leads to UAF. To avoid that,
        // we're loading URL asynchronously. See https://crbug.com/732260.
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                new Runnable() {
                    @Override
                    public void run() {
                        mClient.loadUrlIfPossible(loadUrlParams);
                    }
                });
        mShouldClearRedirectHistoryForTabClobbering = true;
    }

    private void logBlockedNavigationToDevToolsConsole(GURL url) {
        int resId =
                assumeNonNull(mExternalNavHandler).canExternalAppHandleUrl(url)
                        ? R.string.blocked_navigation_warning
                        : R.string.unreachable_navigation_warning;
        assumeNonNull(mClient.getWebContents())
                .addMessageToDevToolsConsole(
                        ConsoleMessageLevel.WARNING,
                        ContextUtils.getApplicationContext().getString(resId, url.getSpec()));
    }

    public static void setResultCallbackForTesting(
            Callback<Pair<GURL, OverrideUrlLoadingResult>> callback) {
        sResultCallbackForTesting = callback;
        ResettersForTesting.register(() -> sResultCallbackForTesting = null);
    }

    @NativeMethods
    interface Natives {
        void associateWithWebContents(
                InterceptNavigationDelegateImpl nativeInterceptNavigationDelegateImpl,
                WebContents webContents);

        void clearWebContentsAssociation(WebContents webContents);

        void onSubframeAsyncActionTaken(WebContents webContents, @Nullable GURL url);
    }
}
