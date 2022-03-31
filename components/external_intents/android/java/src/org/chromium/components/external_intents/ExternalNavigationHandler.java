// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.external_intents;

import android.Manifest.permission;
import android.content.ActivityNotFoundException;
import android.content.ComponentName;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnCancelListener;
import android.content.DialogInterface.OnClickListener;
import android.content.Intent;
import android.content.Intent.ShortcutIconResource;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.PackageManager.NameNotFoundException;
import android.content.pm.ResolveInfo;
import android.content.res.Resources;
import android.net.Uri;
import android.os.StrictMode;
import android.os.SystemClock;
import android.provider.Browser;
import android.provider.Telephony;
import android.text.TextUtils;
import android.util.AndroidRuntimeException;
import android.util.Pair;
import android.view.WindowManager.BadTokenException;
import android.webkit.MimeTypeMap;
import android.webkit.WebView;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AlertDialog;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.PostTask;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.embedder_support.util.UrlUtilitiesJni;
import org.chromium.components.external_intents.ExternalNavigationDelegate.IntentToAutofillAllowingAppResult;
import org.chromium.components.webapk.lib.client.ChromeWebApkHostSignature;
import org.chromium.components.webapk.lib.client.WebApkValidator;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.content_public.common.Referrer;
import org.chromium.network.mojom.ReferrerPolicy;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.permissions.PermissionCallback;
import org.chromium.url.GURL;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Set;

/**
 * Logic related to the URL overriding/intercepting functionality.
 * This feature supports conversion of certain navigations to Android Intents allowing
 * applications like Youtube to direct users clicking on a http(s) link to their native app.
 */
public class ExternalNavigationHandler {
    private static final String TAG = "UrlHandler";

    // Enables debug logging on a local build.
    private static final boolean DEBUG = false;

    private static final String WTAI_URL_PREFIX = "wtai://wp/";
    private static final String WTAI_MC_URL_PREFIX = "wtai://wp/mc;";

    private static final String PLAY_PACKAGE_PARAM = "id";
    private static final String PLAY_REFERRER_PARAM = "referrer";
    private static final String PLAY_APP_PATH = "/store/apps/details";
    private static final String PLAY_HOSTNAME = "play.google.com";

    private static final String PDF_EXTENSION = "pdf";
    private static final String PDF_VIEWER = "com.google.android.apps.docs";
    private static final String PDF_MIME = "application/pdf";
    private static final String PDF_SUFFIX = ".pdf";

    /**
     * Records package names of external applications in the system that could have handled this
     * intent.
     */
    public static final String EXTRA_EXTERNAL_NAV_PACKAGES = "org.chromium.chrome.browser.eenp";

    @VisibleForTesting
    public static final String EXTRA_BROWSER_FALLBACK_URL = "browser_fallback_url";

    // An extra that may be specified on an intent:// URL that contains an encoded value for the
    // referrer field passed to the market:// URL in the case where the app is not present.
    @VisibleForTesting
    static final String EXTRA_MARKET_REFERRER = "market_referrer";

    // A mask of flags that are safe for untrusted content to use when starting an Activity.
    // This list is not exhaustive and flags not listed here are not necessarily unsafe.
    @VisibleForTesting
    static final int ALLOWED_INTENT_FLAGS = Intent.FLAG_EXCLUDE_STOPPED_PACKAGES
            | Intent.FLAG_ACTIVITY_CLEAR_TOP | Intent.FLAG_ACTIVITY_SINGLE_TOP
            | Intent.FLAG_ACTIVITY_MATCH_EXTERNAL | Intent.FLAG_ACTIVITY_NEW_TASK
            | Intent.FLAG_ACTIVITY_MULTIPLE_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT
            | Intent.FLAG_ACTIVITY_RETAIN_IN_RECENTS | Intent.FLAG_ACTIVITY_LAUNCH_ADJACENT;

    // These values are persisted in histograms. Please do not renumber. Append only.
    @IntDef({AiaIntent.FALLBACK_USED, AiaIntent.SERP, AiaIntent.OTHER})
    @Retention(RetentionPolicy.SOURCE)
    private @interface AiaIntent {
        int FALLBACK_USED = 0;
        int SERP = 1;
        int OTHER = 2;

        int NUM_ENTRIES = 3;
    }

    // Helper class to return a boolean by reference.
    private static class MutableBoolean {
        private Boolean mValue;
        public void set(boolean value) {
            mValue = value;
        }
        public Boolean get() {
            return mValue;
        }
    }

    // A Supplier that only evaluates when needed then caches the value.
    protected abstract static class LazySupplier<T> implements Supplier<T> {
        private T mValue;
        private Supplier<T> mInnerSupplier;

        public LazySupplier(Supplier<T> innerSupplier) {
            assert innerSupplier != null : "innerSupplier cannot be null";
            mInnerSupplier = innerSupplier;
        }

        @Nullable
        @Override
        public T get() {
            if (mInnerSupplier != null) {
                mValue = mInnerSupplier.get();

                // Clear the inner supplier to record that we have evaluated and to free any
                // references it may have held.
                mInnerSupplier = null;
            }
            return mValue;
        }

        @Override
        public boolean hasValue() {
            return true;
        }
    }

    // Used to ensure we only call queryIntentActivities when we really need to.
    protected class QueryIntentActivitiesSupplier extends LazySupplier<List<ResolveInfo>> {
        public QueryIntentActivitiesSupplier(Intent intent) {
            super(() -> queryIntentActivities(intent));
        }
    }

    protected static class ResolveActivitySupplier extends LazySupplier<ResolveInfo> {
        public ResolveActivitySupplier(Intent intent) {
            super(()
                            -> PackageManagerUtils.resolveActivity(
                                    intent, PackageManager.MATCH_DEFAULT_ONLY));
        }
    }

    private final ExternalNavigationDelegate mDelegate;

    /**
     * Result types for checking if we should override URL loading.
     * NOTE: this enum is used in UMA, do not reorder values. Changes should be append only.
     * Values should be numerated from 0 and can't have gaps.
     */
    @IntDef({OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT,
            OverrideUrlLoadingResultType.OVERRIDE_WITH_CLOBBERING_TAB,
            OverrideUrlLoadingResultType.OVERRIDE_WITH_ASYNC_ACTION,
            OverrideUrlLoadingResultType.NO_OVERRIDE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface OverrideUrlLoadingResultType {
        /* We should override the URL loading and launch an intent. */
        int OVERRIDE_WITH_EXTERNAL_INTENT = 0;
        /* We should override the URL loading and clobber the current tab. */
        int OVERRIDE_WITH_CLOBBERING_TAB = 1;
        /* We should override the URL loading.  The desired action will be determined
         * asynchronously (e.g. by requiring user confirmation). */
        int OVERRIDE_WITH_ASYNC_ACTION = 2;
        /* We shouldn't override the URL loading. */
        int NO_OVERRIDE = 3;

        int NUM_ENTRIES = 4;
    }

    /**
     * Types of async action that can be taken for a navigation.
     */
    @IntDef({OverrideUrlLoadingAsyncActionType.UI_GATING_BROWSER_NAVIGATION,
            OverrideUrlLoadingAsyncActionType.UI_GATING_INTENT_LAUNCH,
            OverrideUrlLoadingAsyncActionType.NO_ASYNC_ACTION})
    @Retention(RetentionPolicy.SOURCE)
    public @interface OverrideUrlLoadingAsyncActionType {
        /* The user has been presented with a consent dialog gating a browser navigation. */
        int UI_GATING_BROWSER_NAVIGATION = 0;
        /* The user has been presented with a consent dialog gating an intent launch. */
        int UI_GATING_INTENT_LAUNCH = 1;
        /* No async action has been taken. */
        int NO_ASYNC_ACTION = 2;

        int NUM_ENTRIES = 3;
    }

    /**
     * Packages information about the result of a check of whether we should override URL loading.
     */
    public static class OverrideUrlLoadingResult {
        @OverrideUrlLoadingResultType
        int mResultType;

        @OverrideUrlLoadingAsyncActionType
        int mAsyncActionType;

        OverrideUrlLoadingResult(@OverrideUrlLoadingResultType int resultType) {
            this(resultType, OverrideUrlLoadingAsyncActionType.NO_ASYNC_ACTION);
        }

        OverrideUrlLoadingResult(@OverrideUrlLoadingResultType int resultType,
                @OverrideUrlLoadingAsyncActionType int asyncActionType) {
            // The async action type should be set only for async actions...
            assert (asyncActionType == OverrideUrlLoadingAsyncActionType.NO_ASYNC_ACTION
                    || resultType == OverrideUrlLoadingResultType.OVERRIDE_WITH_ASYNC_ACTION);

            // ...and it *must* be set for async actions.
            assert (!(asyncActionType == OverrideUrlLoadingAsyncActionType.NO_ASYNC_ACTION
                    && resultType == OverrideUrlLoadingResultType.OVERRIDE_WITH_ASYNC_ACTION));

            mResultType = resultType;
            mAsyncActionType = asyncActionType;
        }

        public @OverrideUrlLoadingResultType int getResultType() {
            return mResultType;
        }

        public @OverrideUrlLoadingAsyncActionType int getAsyncActionType() {
            return mAsyncActionType;
        }

        public static OverrideUrlLoadingResult forAsyncAction(
                @OverrideUrlLoadingAsyncActionType int asyncActionType) {
            return new OverrideUrlLoadingResult(
                    OverrideUrlLoadingResultType.OVERRIDE_WITH_ASYNC_ACTION, asyncActionType);
        }
        public static OverrideUrlLoadingResult forNoOverride() {
            return new OverrideUrlLoadingResult(OverrideUrlLoadingResultType.NO_OVERRIDE);
        }
        public static OverrideUrlLoadingResult forClobberingTab() {
            return new OverrideUrlLoadingResult(
                    OverrideUrlLoadingResultType.OVERRIDE_WITH_CLOBBERING_TAB);
        }
        public static OverrideUrlLoadingResult forExternalIntent() {
            return new OverrideUrlLoadingResult(
                    OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT);
        }
    }

    /**
     * Constructs a new instance of {@link ExternalNavigationHandler}, using the injected
     * {@link ExternalNavigationDelegate}.
     */
    public ExternalNavigationHandler(ExternalNavigationDelegate delegate) {
        mDelegate = delegate;
    }

    /**
     * Determines whether the URL needs to be sent as an intent to the system,
     * and sends it, if appropriate.
     * @return Whether the URL generated an intent, caused a navigation in
     *         current tab, or wasn't handled at all.
     */
    public OverrideUrlLoadingResult shouldOverrideUrlLoading(ExternalNavigationParams params) {
        if (DEBUG) Log.i(TAG, "shouldOverrideUrlLoading called on " + params.getUrl().getSpec());
        Intent targetIntent;
        // Perform generic parsing of the URI to turn it into an Intent.
        if (UrlUtilities.hasIntentScheme(params.getUrl())) {
            try {
                targetIntent = Intent.parseUri(params.getUrl().getSpec(), Intent.URI_INTENT_SCHEME);
            } catch (Exception ex) {
                Log.w(TAG, "Bad URI %s", params.getUrl().getSpec(), ex);
                return OverrideUrlLoadingResult.forNoOverride();
            }
        } else {
            targetIntent = new Intent(Intent.ACTION_VIEW);
            targetIntent.setData(Uri.parse(params.getUrl().getSpec()));
        }

        GURL browserFallbackUrl =
                new GURL(IntentUtils.safeGetStringExtra(targetIntent, EXTRA_BROWSER_FALLBACK_URL));
        if (!browserFallbackUrl.isValid() || !UrlUtilities.isHttpOrHttps(browserFallbackUrl)) {
            browserFallbackUrl = GURL.emptyGURL();
        }

        // TODO(https://crbug.com/1096099): Refactor shouldOverrideUrlLoadingInternal, splitting it
        // up to separate out the notions wanting to fire an external intent vs being able to.
        MutableBoolean canLaunchExternalFallbackResult = new MutableBoolean();

        long time = SystemClock.elapsedRealtime();
        OverrideUrlLoadingResult result = shouldOverrideUrlLoadingInternal(
                params, targetIntent, browserFallbackUrl, canLaunchExternalFallbackResult);
        assert canLaunchExternalFallbackResult.get() != null;
        RecordHistogram.recordTimesHistogram(
                "Android.StrictMode.OverrideUrlLoadingTime", SystemClock.elapsedRealtime() - time);

        if (result.getResultType() != OverrideUrlLoadingResultType.NO_OVERRIDE) {
            int pageTransitionCore = params.getPageTransition() & PageTransition.CORE_MASK;
            boolean isFormSubmit = pageTransitionCore == PageTransition.FORM_SUBMIT;
            boolean isRedirectFromFormSubmit = isFormSubmit && params.isRedirect();
            if (isRedirectFromFormSubmit) {
                RecordHistogram.recordBooleanHistogram(
                        "Android.Intent.LaunchExternalAppFormSubmitHasUserGesture",
                        params.hasUserGesture());
            }
        } else {
            result = handleFallbackUrl(params, targetIntent, browserFallbackUrl,
                    canLaunchExternalFallbackResult.get());
        }
        if (DEBUG) printDebugShouldOverrideUrlLoadingResultType(result);
        return result;
    }

    private OverrideUrlLoadingResult handleFallbackUrl(ExternalNavigationParams params,
            Intent targetIntent, GURL browserFallbackUrl, boolean canLaunchExternalFallback) {
        if (browserFallbackUrl.isEmpty()
                || (params.getRedirectHandler() != null
                        && params.getRedirectHandler().isOnNavigation()
                        // For instance, if this is a chained fallback URL, we ignore it.
                        && params.getRedirectHandler().shouldNotOverrideUrlLoading())) {
            return OverrideUrlLoadingResult.forNoOverride();
        }

        if (mDelegate.isIntentToInstantApp(targetIntent)) {
            RecordHistogram.recordEnumeratedHistogram("Android.InstantApps.DirectInstantAppsIntent",
                    AiaIntent.FALLBACK_USED, AiaIntent.NUM_ENTRIES);
        }

        if (canLaunchExternalFallback) {
            if (shouldBlockAllExternalAppLaunches(params, isIncomingIntentRedirect(params))) {
                throw new SecurityException("Context is not allowed to launch an external app.");
            }
            if (!params.isIncognito()) {
                // Launch WebAPK if it can handle the URL.
                try {
                    Intent intent =
                            Intent.parseUri(browserFallbackUrl.getSpec(), Intent.URI_INTENT_SCHEME);
                    sanitizeQueryIntentActivitiesIntent(intent);
                    List<ResolveInfo> resolvingInfos = queryIntentActivities(intent);
                    if (!isAlreadyInTargetWebApk(resolvingInfos, params)
                            && launchWebApkIfSoleIntentHandler(resolvingInfos, intent)) {
                        return OverrideUrlLoadingResult.forExternalIntent();
                    }
                } catch (Exception e) {
                    if (DEBUG) Log.i(TAG, "Could not parse fallback url as intent");
                }
            }

            // If the fallback URL is a link to Play Store, send the user to Play Store app
            // instead: crbug.com/638672.
            Pair<String, String> appInfo = maybeGetPlayStoreAppIdAndReferrer(browserFallbackUrl);
            if (appInfo != null) {
                String marketReferrer = TextUtils.isEmpty(appInfo.second)
                        ? ContextUtils.getApplicationContext().getPackageName()
                        : appInfo.second;
                return sendIntentToMarket(
                        appInfo.first, marketReferrer, params, browserFallbackUrl);
            }
        }

        // For subframes, we don't support fallback url for now. If we ever do implement this, be
        // careful to prevent sandbox escapes.
        // http://crbug.com/364522.
        if (!params.isMainFrame()) {
            if (DEBUG) Log.i(TAG, "Don't support fallback url in subframes");
            return OverrideUrlLoadingResult.forNoOverride();
        }

        // NOTE: any further redirection from fall-back URL should not override URL loading.
        // Otherwise, it can be used in chain for fingerprinting multiple app installation
        // status in one shot. In order to prevent this scenario, we notify redirection
        // handler that redirection from the current navigation should stay in this app.
        if (params.getRedirectHandler() != null && params.getRedirectHandler().isOnNavigation()
                && !params.getRedirectHandler()
                            .getAndClearShouldNotBlockOverrideUrlLoadingOnCurrentRedirectionChain()) {
            params.getRedirectHandler().setShouldNotOverrideUrlLoadingOnCurrentRedirectChain();
        }
        if (DEBUG) Log.i(TAG, "clobberCurrentTab called");
        return clobberCurrentTab(browserFallbackUrl, params.getReferrerUrl());
    }

    private void printDebugShouldOverrideUrlLoadingResultType(OverrideUrlLoadingResult result) {
        String resultString;
        switch (result.getResultType()) {
            case OverrideUrlLoadingResultType.OVERRIDE_WITH_EXTERNAL_INTENT:
                resultString = "OVERRIDE_WITH_EXTERNAL_INTENT";
                break;
            case OverrideUrlLoadingResultType.OVERRIDE_WITH_CLOBBERING_TAB:
                resultString = "OVERRIDE_WITH_CLOBBERING_TAB";
                break;
            case OverrideUrlLoadingResultType.OVERRIDE_WITH_ASYNC_ACTION:
                resultString = "OVERRIDE_WITH_ASYNC_ACTION";
                break;
            case OverrideUrlLoadingResultType.NO_OVERRIDE: // Fall through.
            default:
                resultString = "NO_OVERRIDE";
                break;
        }
        Log.i(TAG, "shouldOverrideUrlLoading result: " + resultString);
    }

    private boolean resolversSubsetOf(List<ResolveInfo> infos, List<ResolveInfo> container) {
        if (container == null) return false;
        HashSet<ComponentName> containerSet = new HashSet<>();
        for (ResolveInfo info : container) {
            containerSet.add(
                    new ComponentName(info.activityInfo.packageName, info.activityInfo.name));
        }
        for (ResolveInfo info : infos) {
            if (!containerSet.contains(
                        new ComponentName(info.activityInfo.packageName, info.activityInfo.name))) {
                return false;
            }
        }
        return true;
    }

    /**
     * https://crbug.com/1094442: Don't allow any external navigation on AUTO_SUBFRAME navigation
     * (eg. initial ad frame navigation).
     */
    private boolean blockExternalNavFromAutoSubframe(ExternalNavigationParams params) {
        int pageTransitionCore = params.getPageTransition() & PageTransition.CORE_MASK;
        if (pageTransitionCore == PageTransition.AUTO_SUBFRAME) {
            if (DEBUG) Log.i(TAG, "Auto navigation in subframe");
            return true;
        }
        return false;
    }

    /**
     * http://crbug.com/441284 : Disallow firing external intent while the app is in the background.
     */
    private boolean blockExternalNavWhileBackgrounded(
            ExternalNavigationParams params, boolean incomingIntentRedirect) {
        // If the redirect is from an intent Chrome could still be transitioning to the foreground.
        // Alternatively, the user may have sent Chrome to the background by this point, but for
        // navigations started by another app that should still be safe.
        if (incomingIntentRedirect) return false;
        if (params.isApplicationMustBeInForeground() && !mDelegate.isApplicationInForeground()) {
            if (DEBUG) Log.i(TAG, "App is not in foreground");
            return true;
        }
        return false;
    }

    /** http://crbug.com/464669 : Disallow firing external intent from background tab. */
    private boolean blockExternalNavFromBackgroundTab(
            ExternalNavigationParams params, boolean incomingIntentRedirect) {
        // See #blockExternalNavWhileBackgrounded - isBackgroundTabNavigation is effectively
        // checking both that the tab is foreground, and the app is foreground, so we can skip it
        // for intent launches for the same reason.
        if (incomingIntentRedirect) return false;
        if (params.isBackgroundTabNavigation()
                && !params.areIntentLaunchesAllowedInBackgroundTabs()) {
            if (DEBUG) Log.i(TAG, "Navigation in background tab");
            return true;
        }
        return false;
    }

    /**
     * http://crbug.com/164194 . A navigation forwards or backwards should never trigger the intent
     * picker.
     */
    private boolean ignoreBackForwardNav(ExternalNavigationParams params) {
        if ((params.getPageTransition() & PageTransition.FORWARD_BACK) != 0) {
            if (DEBUG) Log.i(TAG, "Forward or back navigation");
            return true;
        }
        return false;
    }

    /** http://crbug.com/605302 : Allow embedders to handle all pdf file downloads. */
    private boolean isInternalPdfDownload(
            boolean isExternalProtocol, ExternalNavigationParams params) {
        if (!isExternalProtocol && isPdfDownload(params.getUrl())) {
            if (DEBUG) Log.i(TAG, "PDF downloads are now handled internally");
            return true;
        }
        return false;
    }

    /**
     * If accessing a file URL, ensure that the user has granted the necessary file access
     * to the app.
     */
    private boolean startFileIntentIfNecessary(ExternalNavigationParams params) {
        if (params.getUrl().getScheme().equals(UrlConstants.FILE_SCHEME)
                && shouldRequestFileAccess(params.getUrl())) {
            startFileIntent(params);
            if (DEBUG) Log.i(TAG, "Requesting filesystem access");
            return true;
        }
        return false;
    }

    /**
     * Trigger a UI affordance that will ask the user to grant file access.  After the access
     * has been granted or denied, continue loading the specified file URL.
     *
     * @param intent The intent to continue loading the file URL.
     * @param referrerUrl The HTTP referrer URL.
     * @param needsToCloseTab Whether this action should close the current tab.
     */
    @VisibleForTesting
    protected void startFileIntent(ExternalNavigationParams params) {
        PermissionCallback permissionCallback = new PermissionCallback() {
            @Override
            public void onRequestPermissionsResult(String[] permissions, int[] grantResults) {
                if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED
                        && mDelegate.hasValidTab()) {
                    clobberCurrentTab(params.getUrl(), params.getReferrerUrl());
                } else {
                    // TODO(tedchoc): Show an indication to the user that the navigation failed
                    //                instead of silently dropping it on the floor.
                    if (params.shouldCloseContentsOnOverrideUrlLoadingAndLaunchIntent()) {
                        // If the access was not granted, then close the tab if necessary.
                        mDelegate.closeTab();
                    }
                }
            }
        };
        if (!mDelegate.hasValidTab()) return;
        mDelegate.getWindowAndroid().requestPermissions(
                new String[] {permission.READ_EXTERNAL_STORAGE}, permissionCallback);
    }

    /**
     * Clobber the current tab and try not to pass an intent when it should be handled internally
     * so that we can deliver HTTP referrer information safely.
     *
     * @param url The new URL after clobbering the current tab.
     * @param referrerUrl The HTTP referrer URL.
     * @return OverrideUrlLoadingResultType (if the tab has been clobbered, or we're launching an
     *         intent.)
     */
    @VisibleForTesting
    protected OverrideUrlLoadingResult clobberCurrentTab(GURL url, GURL referrerUrl) {
        int transitionType = PageTransition.LINK;
        final LoadUrlParams loadUrlParams = new LoadUrlParams(url, transitionType);
        if (!referrerUrl.isEmpty()) {
            Referrer referrer = new Referrer(referrerUrl.getSpec(), ReferrerPolicy.ALWAYS);
            loadUrlParams.setReferrer(referrer);
        }
        assert mDelegate.hasValidTab() : "clobberCurrentTab was called with an empty tab.";
        // Loading URL will start a new navigation which cancels the current one
        // that this clobbering is being done for. It leads to UAF. To avoid that,
        // we're loading URL asynchronously. See https://crbug.com/732260.
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                mDelegate.loadUrlIfPossible(loadUrlParams);
            }
        });
        return OverrideUrlLoadingResult.forClobberingTab();
    }

    private static void loadUrlWithReferrer(
            final GURL url, final GURL referrerUrl, ExternalNavigationDelegate delegate) {
        LoadUrlParams loadUrlParams = new LoadUrlParams(url, PageTransition.AUTO_TOPLEVEL);
        if (!referrerUrl.isEmpty()) {
            Referrer referrer = new Referrer(referrerUrl.getSpec(), ReferrerPolicy.ALWAYS);
            loadUrlParams.setReferrer(referrer);
        }
        delegate.loadUrlIfPossible(loadUrlParams);
    }

    private boolean isTypedRedirectToExternalProtocol(
            ExternalNavigationParams params, int pageTransitionCore, boolean isExternalProtocol) {
        boolean isTyped = (pageTransitionCore == PageTransition.TYPED)
                || ((params.getPageTransition() & PageTransition.FROM_ADDRESS_BAR) != 0);
        return isTyped && params.isRedirect() && isExternalProtocol;
    }

    /**
     * http://crbug.com/659301: Don't stay in Chrome for Custom Tabs redirecting to Instant Apps.
     */
    private boolean handleCCTRedirectsToInstantApps(ExternalNavigationParams params,
            boolean isExternalProtocol, boolean incomingIntentRedirect) {
        RedirectHandler handler = params.getRedirectHandler();
        if (handler == null) return false;
        if (handler.isFromCustomTabIntent() && !isExternalProtocol && incomingIntentRedirect
                && !handler.shouldNavigationTypeStayInApp()
                && mDelegate.maybeLaunchInstantApp(
                        params.getUrl(), params.getReferrerUrl(), true, isSerpReferrer())) {
            if (DEBUG) {
                Log.i(TAG, "Launching redirect to an instant app");
            }
            return true;
        }
        return false;
    }

    private boolean redirectShouldStayInApp(
            ExternalNavigationParams params, boolean isExternalProtocol, Intent targetIntent) {
        RedirectHandler handler = params.getRedirectHandler();
        if (handler == null) return false;
        boolean shouldStayInApp = handler.shouldStayInApp(
                isExternalProtocol, mDelegate.isIntentForTrustedCallingApp(targetIntent));
        if (shouldStayInApp || handler.shouldNotOverrideUrlLoading()) {
            if (isExternalProtocol) handler.maybeLogExternalRedirectBlockedWithMissingGesture();
            if (DEBUG) Log.i(TAG, "RedirectHandler decision");
            return true;
        }
        return false;
    }

    /** Wrapper of check against the feature to support overriding for testing. */
    @VisibleForTesting
    boolean blockExternalFormRedirectsWithoutGesture() {
        return ExternalIntentsFeatures.INTENT_BLOCK_EXTERNAL_FORM_REDIRECT_NO_GESTURE.isEnabled();
    }

    /**
     * http://crbug.com/149218: We want to show the intent picker for ordinary links, providing
     * the link is not an incoming intent from another application, unless it's a redirect.
     */
    private boolean preferToShowIntentPicker(ExternalNavigationParams params,
            int pageTransitionCore, boolean isExternalProtocol, boolean isFormSubmit,
            boolean linkNotFromIntent, boolean incomingIntentRedirect, boolean isFromIntent,
            QueryIntentActivitiesSupplier resolveInfos) {
        // https://crbug.com/1232514: On Android S, since WebAPKs aren't verified apps they are
        // never launched as the result of a suitable Intent, the user's default browser will be
        // opened instead. As a temporary solution, have Chrome launch the WebAPK.
        if (isFromIntent && mDelegate.shouldLaunchWebApksOnInitialIntent()) {
            boolean suitableWebApk = pickWebApkIfSoleIntentHandler(resolveInfos.get()) != null;
            if (suitableWebApk) return true;
        }

        // http://crbug.com/169549 : If you type in a URL that then redirects in server side to a
        // link that cannot be rendered by the browser, we want to show the intent picker.
        if (isTypedRedirectToExternalProtocol(params, pageTransitionCore, isExternalProtocol)) {
            return true;
        }
        // http://crbug.com/181186: We need to show the intent picker when we receive a redirect
        // following a form submit.
        boolean isRedirectFromFormSubmit = isFormSubmit && params.isRedirect();

        if (!linkNotFromIntent && !incomingIntentRedirect && !isRedirectFromFormSubmit) {
            if (DEBUG) Log.i(TAG, "Incoming intent (not a redirect)");
            return false;
        }
        // http://crbug.com/839751: Require user gestures for form submits to external
        //                          protocols.
        // TODO(tedchoc): Turn this on by default once we verify this change does
        //                not break the world.
        if (isRedirectFromFormSubmit && !incomingIntentRedirect && !params.hasUserGesture()
                && blockExternalFormRedirectsWithoutGesture()) {
            if (DEBUG) {
                Log.i(TAG,
                        "Incoming form intent attempting to redirect without "
                                + "user gesture");
            }
            return false;
        }
        // http://crbug/331571 : Do not override a navigation started from user typing.
        if (params.getRedirectHandler() != null
                && params.getRedirectHandler().isNavigationFromUserTyping()) {
            if (DEBUG) Log.i(TAG, "Navigation from user typing");
            return false;
        }
        return true;
    }

    /**
     * http://crbug.com/159153: Don't override navigation from a chrome:* url to http or https. For
     * example when clicking a link in bookmarks or most visited. When navigating from such a page,
     * there is clear intent to complete the navigation in Chrome.
     */
    private boolean isLinkFromChromeInternalPage(ExternalNavigationParams params) {
        if (params.getReferrerUrl().getScheme().equals(UrlConstants.CHROME_SCHEME)
                && UrlUtilities.isHttpOrHttps(params.getUrl())) {
            if (DEBUG) Log.i(TAG, "Link from an internal chrome:// page");
            return true;
        }
        return false;
    }

    private boolean handleWtaiMcProtocol(ExternalNavigationParams params) {
        if (!params.getUrl().getSpec().startsWith(WTAI_MC_URL_PREFIX)) return false;
        // wtai://wp/mc;number
        // number=string(phone-number)
        String phoneNumber = params.getUrl().getSpec().substring(WTAI_MC_URL_PREFIX.length());
        startActivity(
                new Intent(Intent.ACTION_VIEW, Uri.parse(WebView.SCHEME_TEL + phoneNumber)), false);
        if (DEBUG) Log.i(TAG, "wtai:// link handled");
        RecordUserAction.record("Android.PhoneIntent");
        return true;
    }

    private boolean isUnhandledWtaiProtocol(ExternalNavigationParams params) {
        if (!params.getUrl().getSpec().startsWith(WTAI_URL_PREFIX)) return false;
        if (DEBUG) Log.i(TAG, "Unsupported wtai:// link");
        return true;
    }

    /**
     * The "about:", "chrome:", "chrome-native:", and "devtools:" schemes
     * are internal to the browser; don't want these to be dispatched to other apps.
     */
    private boolean hasInternalScheme(GURL targetUrl, Intent targetIntent) {
        if (isInternalScheme(targetUrl.getScheme())) {
            if (DEBUG) Log.i(TAG, "Navigating to a chrome-internal page");
            return true;
        }
        if (UrlUtilities.hasIntentScheme(targetUrl) && targetIntent.getData() != null
                && isInternalScheme(targetIntent.getData().getScheme())) {
            if (DEBUG) Log.i(TAG, "Navigating to a chrome-internal page");
            return true;
        }
        return false;
    }

    private static boolean isInternalScheme(String scheme) {
        if (TextUtils.isEmpty(scheme)) return false;
        return scheme.equals(ContentUrlConstants.ABOUT_SCHEME)
                || scheme.equals(UrlConstants.CHROME_SCHEME)
                || scheme.equals(UrlConstants.CHROME_NATIVE_SCHEME)
                || scheme.equals(UrlConstants.DEVTOOLS_SCHEME);
    }

    /**
     * The "content:" scheme is disabled in Clank. Do not try to start an external activity, or
     * load the URL in-browser.
     */
    private boolean hasContentScheme(GURL targetUrl, Intent targetIntent) {
        boolean hasContentScheme = false;
        if (UrlUtilities.hasIntentScheme(targetUrl) && targetIntent.getData() != null) {
            hasContentScheme =
                    UrlConstants.CONTENT_SCHEME.equals(targetIntent.getData().getScheme());
        } else {
            hasContentScheme = UrlConstants.CONTENT_SCHEME.equals(targetUrl.getScheme());
        }
        if (DEBUG && hasContentScheme) Log.i(TAG, "Navigation to content: URL");
        return hasContentScheme;
    }

    /**
     * Intent URIs leads to creating intents that chrome would use for firing external navigations
     * via Android. Android throws an exception [1] when an application exposes a file:// Uri to
     * another app.
     *
     * This method checks if the |targetIntent| contains the file:// scheme in its data.
     *
     * [1]: https://developer.android.com/reference/android/os/FileUriExposedException
     */
    private boolean hasFileSchemeInIntentURI(GURL targetUrl, Intent targetIntent) {
        // We are only concerned with targetIntent that was generated due to intent:// schemes only.
        if (!UrlUtilities.hasIntentScheme(targetUrl)) return false;

        Uri data = targetIntent.getData();

        if (data == null || data.getScheme() == null) return false;

        if (data.getScheme().equalsIgnoreCase(UrlConstants.FILE_SCHEME)) {
            if (DEBUG) Log.i(TAG, "Intent navigation to file: URI");
            return true;
        }
        return false;
    }

    /**
     * Special case - It makes no sense to use an external application for a YouTube
     * pairing code URL, since these match the current tab with a device (Chromecast
     * or similar) it is supposed to be controlling. Using a different application
     * that isn't expecting this (in particular YouTube) doesn't work.
     */
    @VisibleForTesting
    protected boolean isYoutubePairingCode(GURL url) {
        if (url.domainIs("youtube.com")
                && !TextUtils.isEmpty(UrlUtilities.getValueForKeyInQuery(url, "pairingCode"))) {
            if (DEBUG) Log.i(TAG, "YouTube URL with a pairing code");
            return true;
        }
        return false;
    }

    private boolean externalIntentRequestsDisabledForUrl(ExternalNavigationParams params) {
        // TODO(changwan): check if we need to handle URL even when external intent is off.
        if (CommandLine.getInstance().hasSwitch(
                    ExternalIntentsSwitches.DISABLE_EXTERNAL_INTENT_REQUESTS)) {
            Log.w(TAG, "External intent handling is disabled by a command-line flag.");
            return true;
        }

        if (mDelegate.shouldDisableExternalIntentRequestsForUrl(params.getUrl())) {
            if (DEBUG) Log.i(TAG, "Delegate disables external intent requests for URL.");
            return true;
        }
        return false;
    }

    /**
     * If the intent can't be resolved, we should fall back to the browserFallbackUrl, or try to
     * find the app on the market if no fallback is provided.
     */
    private OverrideUrlLoadingResult handleUnresolvableIntent(
            ExternalNavigationParams params, Intent targetIntent, GURL browserFallbackUrl) {
        // Fallback URL will be handled by the caller of shouldOverrideUrlLoadingInternal.
        if (!browserFallbackUrl.isEmpty()) return OverrideUrlLoadingResult.forNoOverride();
        if (targetIntent.getPackage() != null) {
            return handleWithMarketIntent(params, targetIntent);
        }

        if (DEBUG) Log.i(TAG, "Could not find an external activity to use");
        return OverrideUrlLoadingResult.forNoOverride();
    }

    private OverrideUrlLoadingResult handleWithMarketIntent(
            ExternalNavigationParams params, Intent intent) {
        String marketReferrer = IntentUtils.safeGetStringExtra(intent, EXTRA_MARKET_REFERRER);
        if (TextUtils.isEmpty(marketReferrer)) {
            marketReferrer = ContextUtils.getApplicationContext().getPackageName();
        }
        return sendIntentToMarket(intent.getPackage(), marketReferrer, params, GURL.emptyGURL());
    }

    private boolean maybeSetSmsPackage(Intent targetIntent) {
        final Uri uri = targetIntent.getData();
        if (targetIntent.getPackage() == null && uri != null
                && UrlConstants.SMS_SCHEME.equals(uri.getScheme())) {
            List<ResolveInfo> resolvingInfos = queryIntentActivities(targetIntent);
            targetIntent.setPackage(getDefaultSmsPackageName(resolvingInfos));
            return true;
        }
        return false;
    }

    private void maybeRecordPhoneIntentMetrics(Intent targetIntent) {
        final Uri uri = targetIntent.getData();
        if (uri != null && UrlConstants.TEL_SCHEME.equals(uri.getScheme())
                || (Intent.ACTION_DIAL.equals(targetIntent.getAction()))
                || (Intent.ACTION_CALL.equals(targetIntent.getAction()))) {
            RecordUserAction.record("Android.PhoneIntent");
        }
    }

    /**
     * In incognito mode, links that can be handled within the browser should just do so,
     * without asking the user.
     */
    private boolean shouldStayInIncognito(
            ExternalNavigationParams params, boolean isExternalProtocol) {
        if (params.isIncognito() && !isExternalProtocol) {
            if (DEBUG) Log.i(TAG, "Stay incognito");
            return true;
        }
        return false;
    }

    private boolean fallBackToHandlingWithInstantApp(ExternalNavigationParams params,
            boolean incomingIntentRedirect, boolean linkNotFromIntent) {
        if (incomingIntentRedirect
                && mDelegate.maybeLaunchInstantApp(
                        params.getUrl(), params.getReferrerUrl(), true, isSerpReferrer())) {
            if (DEBUG) Log.i(TAG, "Launching instant Apps redirect");
            return true;
        } else if (linkNotFromIntent && !params.isIncognito()
                && mDelegate.maybeLaunchInstantApp(
                        params.getUrl(), params.getReferrerUrl(), false, isSerpReferrer())) {
            if (DEBUG) Log.i(TAG, "Launching instant Apps link");
            return true;
        }
        return false;
    }

    /**
     * This is the catch-all path for any intent that the app can handle that doesn't have a
     * specialized external app handling it.
     */
    private OverrideUrlLoadingResult fallBackToHandlingInApp() {
        if (DEBUG) Log.i(TAG, "No specialized handler for URL");
        return OverrideUrlLoadingResult.forNoOverride();
    }

    /**
     * Returns true if an intent is an ACTION_VIEW intent targeting browsers or browser-like apps
     * (excluding the embedding app).
     */
    private boolean isViewIntentToOtherBrowser(Intent targetIntent,
            QueryIntentActivitiesSupplier resolveInfos, boolean isIntentWithSupportedProtocol,
            ResolveActivitySupplier resolveActivity) {
        // Note that up until at least Android S, an empty action will match any intent filter
        // with with an action specified. If an intent selector is specified, then don't trust the
        // action on the intent.
        if (!TextUtils.isEmpty(targetIntent.getAction())
                && !targetIntent.getAction().equals(Intent.ACTION_VIEW)
                && targetIntent.getSelector() == null) {
            return false;
        }

        if (targetIntent.getPackage() != null
                && targetIntent.getPackage().equals(
                        ContextUtils.getApplicationContext().getPackageName())) {
            return false;
        }

        String selfPackageName = mDelegate.getContext().getPackageName();
        boolean matchesOtherPackage = false;
        for (ResolveInfo resolveInfo : resolveInfos.get()) {
            ActivityInfo info = resolveInfo.activityInfo;
            if (info == null || !selfPackageName.equals(info.packageName)) {
                matchesOtherPackage = true;
                break;
            }
        }
        if (!matchesOtherPackage) return false;

        // Querying for browser packages if the intent doesn't obviously match or not
        // match a browser. This will catch custom URL schemes like googlechrome://.
        Set<String> browserPackages = getInstalledBrowserPackages();

        boolean matchesBrowser = false;
        for (ResolveInfo resolveInfo : resolveInfos.get()) {
            ActivityInfo info = resolveInfo.activityInfo;
            if (info != null && browserPackages.contains(info.packageName)) {
                matchesBrowser = true;
                break;
            }
        }
        if (!matchesBrowser) return false;
        if (resolveActivity.get().activityInfo == null) return false;

        // If the intent resolves to a non-browser even through a browser is included in
        // queryIntentActivities, it's not really targeting a browser.
        return browserPackages.contains(resolveActivity.get().activityInfo.packageName);
    }

    private static Set<String> getInstalledBrowserPackages() {
        List<ResolveInfo> browsers = PackageManagerUtils.queryAllWebBrowsersInfo();

        Set<String> packageNames = new HashSet<>();
        for (ResolveInfo browser : browsers) {
            if (browser.activityInfo == null) continue;
            packageNames.add(browser.activityInfo.packageName);
        }
        return packageNames;
    }

    /**
     * Current URL has at least one specialized handler available. For navigations
     * within the same host, keep the navigation inside the browser unless the set of
     * available apps to handle the new navigation is different. http://crbug.com/463138
     */
    private boolean shouldStayWithinHost(ExternalNavigationParams params, boolean isLink,
            boolean isFormSubmit, List<ResolveInfo> resolvingInfos, boolean isExternalProtocol) {
        if (isExternalProtocol) return false;

        GURL previousUrl = getLastCommittedUrl();
        if (previousUrl == null) previousUrl = params.getReferrerUrl();
        if (previousUrl.isEmpty() || (!isLink && !isFormSubmit)) return false;

        GURL currentUrl = params.getUrl();

        if (!TextUtils.equals(currentUrl.getHost(), previousUrl.getHost())) {
            return false;
        }

        Intent previousIntent = new Intent(Intent.ACTION_VIEW);
        previousIntent.setData(Uri.parse(previousUrl.getSpec()));

        if (resolversSubsetOf(resolvingInfos, queryIntentActivities(previousIntent))) {
            if (DEBUG) Log.i(TAG, "Same host, no new resolvers");
            return true;
        }
        return false;
    }

    /**
     * For security reasons, we disable all intent:// URLs to Instant Apps that are not coming from
     * SERP.
     */
    private boolean preventDirectInstantAppsIntent(
            boolean isDirectInstantAppsIntent, boolean shouldProxyForInstantApps) {
        if (!isDirectInstantAppsIntent || shouldProxyForInstantApps) return false;
        if (DEBUG) Log.i(TAG, "Intent URL to an Instant App");
        RecordHistogram.recordEnumeratedHistogram("Android.InstantApps.DirectInstantAppsIntent",
                AiaIntent.OTHER, AiaIntent.NUM_ENTRIES);
        return true;
    }

    /**
     * Prepare the intent to be sent. This function does not change the filtering for the intent,
     * so the list if resolveInfos for the intent will be the same before and after this function.
     */
    private void prepareExternalIntent(Intent targetIntent, ExternalNavigationParams params,
            List<ResolveInfo> resolvingInfos, boolean shouldProxyForInstantApps) {
        // Set the Browser application ID to us in case the user chooses this app
        // as the app.  This will make sure the link is opened in the same tab
        // instead of making a new one in the case of Chrome.
        targetIntent.putExtra(Browser.EXTRA_APPLICATION_ID,
                ContextUtils.getApplicationContext().getPackageName());
        if (params.isOpenInNewTab()) targetIntent.putExtra(Browser.EXTRA_CREATE_NEW_TAB, true);
        targetIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        // Ensure intents re-target potential caller activity when we run in CCT mode.
        targetIntent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TOP);
        mDelegate.maybeSetWindowId(targetIntent);
        targetIntent.putExtra(EXTRA_EXTERNAL_NAV_PACKAGES, getSpecializedHandlers(resolvingInfos));

        if (!params.getReferrerUrl().isEmpty()) {
            mDelegate.maybeSetPendingReferrer(targetIntent, params.getReferrerUrl());
        }

        if (params.isIncognito()) mDelegate.maybeSetPendingIncognitoUrl(targetIntent);

        mDelegate.maybeAdjustInstantAppExtras(targetIntent, shouldProxyForInstantApps);

        if (shouldProxyForInstantApps) {
            RecordHistogram.recordEnumeratedHistogram("Android.InstantApps.DirectInstantAppsIntent",
                    AiaIntent.SERP, AiaIntent.NUM_ENTRIES);
        }

        mDelegate.maybeSetRequestMetadata(targetIntent, params.hasUserGesture(),
                params.isRendererInitiated(), params.getInitiatorOrigin());
    }

    private OverrideUrlLoadingResult handleExternalIncognitoIntent(Intent targetIntent,
            ExternalNavigationParams params, GURL browserFallbackUrl,
            boolean shouldProxyForInstantApps) {
        // This intent may leave this app. Warn the user that incognito does not carry over
        // to external apps.
        if (startIncognitoIntent(
                    params, targetIntent, browserFallbackUrl, shouldProxyForInstantApps)) {
            if (DEBUG) Log.i(TAG, "Incognito navigation out");
            return OverrideUrlLoadingResult.forAsyncAction(
                    OverrideUrlLoadingAsyncActionType.UI_GATING_INTENT_LAUNCH);
        }
        if (DEBUG) Log.i(TAG, "Failed to show incognito alert dialog.");
        return OverrideUrlLoadingResult.forNoOverride();
    }

    /**
     * Display a dialog warning the user that they may be leaving this app by starting this
     * intent. Give the user the opportunity to cancel the action. And if it is canceled, a
     * navigation will happen in this app. Catches BadTokenExceptions caused by showing the dialog
     * on certain devices. (crbug.com/782602)
     * @param intent The intent for external application that will be sent.
     * @param referrerUrl The referrer for the current navigation.
     * @param fallbackUrl The URL to load if the user doesn't proceed with external intent.
     * @param needsToCloseTab Whether the current tab has to be closed after the intent is sent.
     * @param proxy Whether we need to proxy the intent through AuthenticatedProxyActivity (this is
     *              used by Instant Apps intents.
     * @return True if the function returned error free, false if it threw an exception.
     */
    private boolean startIncognitoIntent(
            ExternalNavigationParams params, Intent intent, GURL fallbackUrl, boolean proxy) {
        Context context = mDelegate.getContext();
        if (!canLaunchIncognitoIntent(intent, context)) return false;

        if (mDelegate.hasCustomLeavingIncognitoDialog()) {
            mDelegate.presentLeavingIncognitoModalDialog(shouldLaunch -> {
                onUserDecidedWhetherToLaunchIncognitoIntent(
                        shouldLaunch.booleanValue(), params, intent, fallbackUrl, proxy);
            });

            return true;
        }

        try {
            AlertDialog dialog =
                    showLeavingIncognitoAlert(context, params, intent, fallbackUrl, proxy);
            return dialog != null;
        } catch (BadTokenException e) {
            return false;
        }
    }

    @VisibleForTesting
    protected boolean canLaunchIncognitoIntent(Intent intent, Context context) {
        if (!mDelegate.hasValidTab()) return false;
        if (ContextUtils.activityFromContext(context) == null) return false;
        return true;
    }

    /**
     * Shows and returns an AlertDialog asking if the user would like to leave incognito.
     */
    @VisibleForTesting
    protected AlertDialog showLeavingIncognitoAlert(final Context context,
            final ExternalNavigationParams params, final Intent intent, final GURL fallbackUrl,
            final boolean proxy) {
        return new AlertDialog.Builder(context, R.style.Theme_Chromium_AlertDialog)
                .setTitle(R.string.external_app_leave_incognito_warning_title)
                .setMessage(R.string.external_app_leave_incognito_warning)
                .setPositiveButton(R.string.external_app_leave_incognito_leave,
                        new OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                onUserDecidedWhetherToLaunchIncognitoIntent(
                                        /*shouldLaunch=*/true, params, intent, fallbackUrl, proxy);
                            }
                        })
                .setNegativeButton(R.string.external_app_leave_incognito_stay,
                        new OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                onUserDecidedWhetherToLaunchIncognitoIntent(
                                        /*shouldLaunch=*/false, params, intent, fallbackUrl, proxy);
                            }
                        })
                .setOnCancelListener(new OnCancelListener() {
                    @Override
                    public void onCancel(DialogInterface dialog) {
                        onUserDecidedWhetherToLaunchIncognitoIntent(
                                /*shouldLaunch=*/false, params, intent, fallbackUrl, proxy);
                    }
                })
                .show();
    }

    private void onUserDecidedWhetherToLaunchIncognitoIntent(final boolean shouldLaunch,
            final ExternalNavigationParams params, final Intent intent, final GURL fallbackUrl,
            final boolean proxy) {
        boolean closeTab = params.shouldCloseContentsOnOverrideUrlLoadingAndLaunchIntent();
        if (shouldLaunch) {
            try {
                startActivity(intent, proxy);
                if (mDelegate.canCloseTabOnIncognitoIntentLaunch() && closeTab) {
                    mDelegate.closeTab();
                }
            } catch (ActivityNotFoundException e) {
                // The activity that we thought was going to handle the intent
                // no longer exists, so catch the exception and assume Chrome
                // can handle it.
                handleFallbackUrl(params, intent, fallbackUrl, false);
            }
        } else {
            handleFallbackUrl(params, intent, fallbackUrl, false);
        }
    }

    /**
     * If some third-party app launched this app with an intent, and the URL got redirected, and the
     * user explicitly chose this app over other intent handlers, stay in the app unless there was a
     * new intent handler after redirection or the app cannot handle it internally any more.
     * Custom tabs are an exception to this rule, since at no point, the user sees an intent picker
     * and "picking the Chrome app" is handled inside the support library.
     */
    private boolean shouldKeepIntentRedirectInApp(ExternalNavigationParams params,
            boolean incomingIntentRedirect, List<ResolveInfo> resolvingInfos,
            boolean isExternalProtocol) {
        if (params.getRedirectHandler() != null && incomingIntentRedirect && !isExternalProtocol
                && !params.getRedirectHandler().isFromCustomTabIntent()
                && !params.getRedirectHandler().hasNewResolver(
                        resolvingInfos, (Intent intent) -> queryIntentActivities(intent))) {
            if (DEBUG) Log.i(TAG, "Custom tab redirect no handled");
            return true;
        }
        return false;
    }

    /**
     * @param packageName The package to check.
     * @return Whether the package is a valid WebAPK package.
     */
    @VisibleForTesting
    protected boolean isValidWebApk(String packageName) {
        // Ensure that WebApkValidator is initialized (note: this method is a no-op after the first
        // time that it is invoked).
        WebApkValidator.init(
                ChromeWebApkHostSignature.EXPECTED_SIGNATURE, ChromeWebApkHostSignature.PUBLIC_KEY);
        return WebApkValidator.isValidWebApk(ContextUtils.getApplicationContext(), packageName);
    }

    /**
     * Returns whether the activity belongs to a WebAPK and the URL is within the scope of the
     * WebAPK. The WebAPK's main activity is a bouncer that redirects to the WebAPK Activity in
     * Chrome. In order to avoid bouncing indefinitely, we should not override the navigation if we
     * are currently showing the WebAPK (params#nativeClientPackageName()) that we will redirect to.
     */
    private boolean isAlreadyInTargetWebApk(
            List<ResolveInfo> resolveInfos, ExternalNavigationParams params) {
        String currentName = params.nativeClientPackageName();
        if (currentName == null) return false;
        for (ResolveInfo resolveInfo : resolveInfos) {
            ActivityInfo info = resolveInfo.activityInfo;
            if (info != null && currentName.equals(info.packageName)) {
                if (DEBUG) Log.i(TAG, "Already in WebAPK");
                return true;
            }
        }
        return false;
    }

    // This will handle external navigations only for intent meant for Autofill Assistant.
    private boolean handleWithAutofillAssistant(
            ExternalNavigationParams params, Intent targetIntent, GURL browserFallbackUrl) {
        if (!mDelegate.isIntentToAutofillAssistant(targetIntent)) {
            return false;
        }

        // Launching external intents is always forbidden in incognito. Handle the intent with
        // Autofill Assistant instead. Note that Autofill Assistant won't start in incognito either,
        // this will only result in navigating to the browserFallbackUrl.
        if (!params.isIncognito()) {
            @IntentToAutofillAllowingAppResult
            int intentAllowingAppResult = mDelegate.isIntentToAutofillAssistantAllowingApp(params,
                    targetIntent,
                    (intent)
                            -> getSpecializedHandlersWithFilter(queryIntentActivities(intent),
                                       /* filterPackageName= */ null,
                                       /* handlesInstantAppLaunchingInternally= */ false)
                                       .size()
                            == 1);
            switch (intentAllowingAppResult) {
                case IntentToAutofillAllowingAppResult.DEFER_TO_APP_NOW:
                    if (DEBUG) {
                        Log.i(TAG, "Autofill Assistant passed in favour of App.");
                    }
                    return false;
                case IntentToAutofillAllowingAppResult.DEFER_TO_APP_LATER:
                    if (params.getRedirectHandler() != null && isGoogleReferrer()) {
                        if (DEBUG) {
                            Log.i(TAG, "Autofill Assistant passed in favour of App later.");
                        }
                        params.getRedirectHandler()
                                .setShouldNotBlockUrlLoadingOverrideOnCurrentRedirectionChain();
                        return true;
                    }
                    break;
                case IntentToAutofillAllowingAppResult.NONE:
                    break;
            }
        }

        if (mDelegate.handleWithAutofillAssistant(
                    params, targetIntent, browserFallbackUrl, isGoogleReferrer())) {
            if (DEBUG) Log.i(TAG, "Handled with Autofill Assistant.");
        } else {
            if (DEBUG) Log.i(TAG, "Not handled with Autofill Assistant.");
        }
        return true;
    }

    // Check if we're navigating under conditions that should never launch an external app.
    private boolean shouldBlockAllExternalAppLaunches(
            ExternalNavigationParams params, boolean incomingIntentRedirect) {
        return blockExternalNavFromAutoSubframe(params)
                || blockExternalNavWhileBackgrounded(params, incomingIntentRedirect)
                || blockExternalNavFromBackgroundTab(params, incomingIntentRedirect)
                || ignoreBackForwardNav(params);
    }

    private void recordIntentSelectorMetrics(GURL targetUrl, Intent targetIntent) {
        if (UrlUtilities.hasIntentScheme(targetUrl)) {
            RecordHistogram.recordBooleanHistogram(
                    "Android.Intent.IntentUriWithSelector", targetIntent.getSelector() != null);
        }
    }

    private OverrideUrlLoadingResult shouldOverrideUrlLoadingInternal(
            ExternalNavigationParams params, Intent targetIntent, GURL browserFallbackUrl,
            MutableBoolean canLaunchExternalFallbackResult) {
        recordIntentSelectorMetrics(params.getUrl(), targetIntent);
        sanitizeQueryIntentActivitiesIntent(targetIntent);

        // Don't allow external fallback URLs by default.
        canLaunchExternalFallbackResult.set(false);

        // http://crbug.com/170925: We need to show the intent picker when we receive an intent from
        // another app that 30x redirects to a YouTube/Google Maps/Play Store/Google+ URL etc.
        boolean incomingIntentRedirect = isIncomingIntentRedirect(params);

        if (shouldBlockAllExternalAppLaunches(params, incomingIntentRedirect)) {
            return OverrideUrlLoadingResult.forNoOverride();
        }

        if (handleWithAutofillAssistant(params, targetIntent, browserFallbackUrl)) {
            return OverrideUrlLoadingResult.forNoOverride();
        }

        GURL intentDataUrl = new GURL(targetIntent.getDataString());
        boolean isExternalProtocol = !UrlUtilities.isAcceptedScheme(params.getUrl());
        // intent: URLs are considered an external protocol, but may still contain a Data URI that
        // this app does support, and may still end up launching this app.
        boolean isIntentWithSupportedProtocol = UrlUtilities.hasIntentScheme(params.getUrl())
                && UrlUtilities.isAcceptedScheme(intentDataUrl);

        if (isInternalPdfDownload(isExternalProtocol, params)) {
            return OverrideUrlLoadingResult.forNoOverride();
        }

        // This check should happen for reloads, navigations, etc..., which is why
        // it occurs before the subsequent blocks.
        if (startFileIntentIfNecessary(params)) {
            return OverrideUrlLoadingResult.forAsyncAction(
                    OverrideUrlLoadingAsyncActionType.UI_GATING_BROWSER_NAVIGATION);
        }

        // This should come after file intents, but before any returns of
        // OVERRIDE_WITH_EXTERNAL_INTENT.
        if (externalIntentRequestsDisabledForUrl(params)) {
            return OverrideUrlLoadingResult.forNoOverride();
        }

        int pageTransitionCore = params.getPageTransition() & PageTransition.CORE_MASK;
        boolean isLink = params.isLinkTransition();
        boolean isFromIntent = params.isFromIntent();
        boolean isFormSubmit = pageTransitionCore == PageTransition.FORM_SUBMIT;
        boolean linkNotFromIntent = isLink && !isFromIntent;

        if (handleCCTRedirectsToInstantApps(params, isExternalProtocol, incomingIntentRedirect)) {
            return OverrideUrlLoadingResult.forExternalIntent();
        } else if (redirectShouldStayInApp(params, isExternalProtocol, targetIntent)) {
            return OverrideUrlLoadingResult.forNoOverride();
        }

        if (!maybeSetSmsPackage(targetIntent)) maybeRecordPhoneIntentMetrics(targetIntent);

        Intent debugIntent = new Intent(targetIntent);
        QueryIntentActivitiesSupplier resolvingInfos =
                new QueryIntentActivitiesSupplier(targetIntent);
        if (!preferToShowIntentPicker(params, pageTransitionCore, isExternalProtocol, isFormSubmit,
                    linkNotFromIntent, incomingIntentRedirect, isFromIntent, resolvingInfos)) {
            return OverrideUrlLoadingResult.forNoOverride();
        }

        if (isLinkFromChromeInternalPage(params)) return OverrideUrlLoadingResult.forNoOverride();

        if (handleWtaiMcProtocol(params)) {
            return OverrideUrlLoadingResult.forExternalIntent();
        }
        // TODO: handle other WTAI schemes.
        if (isUnhandledWtaiProtocol(params)) return OverrideUrlLoadingResult.forNoOverride();

        if (hasInternalScheme(params.getUrl(), targetIntent)
                || hasContentScheme(params.getUrl(), targetIntent)
                || hasFileSchemeInIntentURI(params.getUrl(), targetIntent)) {
            return OverrideUrlLoadingResult.forNoOverride();
        }

        if (isYoutubePairingCode(params.getUrl())) return OverrideUrlLoadingResult.forNoOverride();

        if (shouldStayInIncognito(params, isExternalProtocol)) {
            return OverrideUrlLoadingResult.forNoOverride();
        }

        // From this point on, we have determined it is safe to launch an External App from a
        // fallback URL.
        canLaunchExternalFallbackResult.set(true);

        if (resolvingInfos.get().isEmpty()) {
            return handleUnresolvableIntent(params, targetIntent, browserFallbackUrl);
        }

        if (resolvesToNonExportedActivity(resolvingInfos.get())) {
            return OverrideUrlLoadingResult.forNoOverride();
        }

        if (!browserFallbackUrl.isEmpty()) targetIntent.removeExtra(EXTRA_BROWSER_FALLBACK_URL);

        boolean hasSpecializedHandler = countSpecializedHandlers(resolvingInfos.get()) > 0;
        if (!isExternalProtocol && !hasSpecializedHandler) {
            if (fallBackToHandlingWithInstantApp(
                        params, incomingIntentRedirect, linkNotFromIntent)) {
                return OverrideUrlLoadingResult.forExternalIntent();
            }
            return fallBackToHandlingInApp();
        }

        // From this point on we should only have URLs from intent URIs, or URLs for
        // apps with specialized handlers (including custom schemes).

        if (shouldStayWithinHost(
                    params, isLink, isFormSubmit, resolvingInfos.get(), isExternalProtocol)) {
            return OverrideUrlLoadingResult.forNoOverride();
        }

        boolean isDirectInstantAppsIntent =
                isExternalProtocol && mDelegate.isIntentToInstantApp(targetIntent);
        boolean shouldProxyForInstantApps = isDirectInstantAppsIntent && isSerpReferrer();
        if (preventDirectInstantAppsIntent(isDirectInstantAppsIntent, shouldProxyForInstantApps)) {
            return OverrideUrlLoadingResult.forNoOverride();
        }

        prepareExternalIntent(
                targetIntent, params, resolvingInfos.get(), shouldProxyForInstantApps);
        // As long as our intent resolution hasn't changed, resolvingInfos won't need to be
        // re-computed as it won't have changed.
        assert intentResolutionMatches(debugIntent, targetIntent);

        if (params.isIncognito()) {
            return handleIncognitoIntent(params, targetIntent, intentDataUrl, resolvingInfos.get(),
                    browserFallbackUrl, shouldProxyForInstantApps);
        }

        if (shouldKeepIntentRedirectInApp(
                    params, incomingIntentRedirect, resolvingInfos.get(), isExternalProtocol)) {
            return OverrideUrlLoadingResult.forNoOverride();
        }

        if (isAlreadyInTargetWebApk(resolvingInfos.get(), params)) {
            return OverrideUrlLoadingResult.forNoOverride();
        } else if (launchWebApkIfSoleIntentHandler(resolvingInfos.get(), targetIntent)) {
            return OverrideUrlLoadingResult.forExternalIntent();
        }

        ResolveActivitySupplier resolveActivity = new ResolveActivitySupplier(targetIntent);
        boolean requiresIntentChooser = false;
        if (isViewIntentToOtherBrowser(
                    targetIntent, resolvingInfos, isIntentWithSupportedProtocol, resolveActivity)) {
            RecordHistogram.recordBooleanHistogram("Android.Intent.WebIntentToOtherBrowser", true);
            requiresIntentChooser = true;
        }

        if (mDelegate.maybeSetTargetPackage(targetIntent)) {
            // This check was not combined with the one above to preserve the value of the
            // Android.Intent.WebIntentToOtherBrowser histogram.
            requiresIntentChooser = false;
        }

        if (shouldAvoidShowingDisambiguationPrompt(targetIntent, resolvingInfos, resolveActivity)) {
            return OverrideUrlLoadingResult.forNoOverride();
        }

        return startActivity(targetIntent, shouldProxyForInstantApps, requiresIntentChooser,
                resolvingInfos.get(), resolveActivity, browserFallbackUrl, intentDataUrl,
                params.getReferrerUrl());
    }

    // https://crbug.com/1249964
    private boolean resolvesToNonExportedActivity(List<ResolveInfo> infos) {
        for (ResolveInfo info : infos) {
            if (info.activityInfo != null && !info.activityInfo.exported) {
                Log.w(TAG, "Web Intent resolves to non-exported Activity.");
                return true;
            }
        }

        return false;
    }

    private boolean shouldAvoidShowingDisambiguationPrompt(Intent intent,
            QueryIntentActivitiesSupplier resolvingInfosSupplier,
            ResolveActivitySupplier resolveActivitySupplier) {
        // Don't bother performing the package manager checks if the delegate is fine with the
        // disambiguation prompt.
        if (!mDelegate.shouldAvoidDisambiguationDialog(intent)) return false;

        ResolveInfo resolveActivity = resolveActivitySupplier.get();

        if (resolveActivity == null) return true;

        List<ResolveInfo> possibleHandlingActivities = resolvingInfosSupplier.get();

        // If resolveActivity is contained in possibleHandlingActivities, that means the Intent
        // would launch a specialized Activity. If not, that means the Intent will launch the
        // Android disambiguation prompt.
        boolean result = !resolversSubsetOf(
                Collections.singletonList(resolveActivity), possibleHandlingActivities);
        if (DEBUG && result) Log.i(TAG, "Avoiding disambiguation dialog.");
        return result;
    }

    private OverrideUrlLoadingResult handleIncognitoIntent(ExternalNavigationParams params,
            Intent targetIntent, GURL intentDataUrl, List<ResolveInfo> resolvingInfos,
            GURL browserFallbackUrl, boolean shouldProxyForInstantApps) {
        boolean intentTargetedToApp = mDelegate.willAppHandleIntent(targetIntent);

        GURL fallbackUrl = browserFallbackUrl;
        // If we can handle the intent, then fall back to handling the target URL instead of
        // the fallbackUrl if the user decides not to leave incognito.
        if (resolveInfoContainsSelf(resolvingInfos)) {
            GURL targetUrl =
                    UrlUtilities.hasIntentScheme(params.getUrl()) ? intentDataUrl : params.getUrl();
            // Make sure the browser can handle this URL, in case the Intent targeted a
            // non-browser component for this app.
            if (UrlUtilities.isAcceptedScheme(targetUrl)) fallbackUrl = targetUrl;
        }

        // The user is about to potentially leave the app, so we should ask whether they want to
        // leave incognito or not.
        if (!intentTargetedToApp) {
            return handleExternalIncognitoIntent(
                    targetIntent, params, fallbackUrl, shouldProxyForInstantApps);
        }

        // The intent is staying in the app, so we can simply navigate to the intent's URL,
        // while staying in incognito.
        return handleFallbackUrl(params, targetIntent, fallbackUrl, false);
    }

    /**
     * Sanitize intent to be passed to {@link queryIntentActivities()}
     * ensuring that web pages cannot bypass browser security.
     */
    public static void sanitizeQueryIntentActivitiesIntent(Intent intent) {
        intent.setFlags(intent.getFlags() & ALLOWED_INTENT_FLAGS);
        intent.addCategory(Intent.CATEGORY_BROWSABLE);
        intent.setComponent(null);

        // Intent Selectors allow intents to bypass the intent filter and potentially send apps URIs
        // they were not expecting to handle. https://crbug.com/1254422
        intent.setSelector(null);
    }

    /**
     * @return OVERRIDE_WITH_EXTERNAL_INTENT when we successfully started market activity,
     *         NO_OVERRIDE otherwise.
     */
    private OverrideUrlLoadingResult sendIntentToMarket(String packageName, String marketReferrer,
            ExternalNavigationParams params, GURL fallbackUrl) {
        Uri marketUri =
                new Uri.Builder()
                        .scheme("market")
                        .authority("details")
                        .appendQueryParameter(PLAY_PACKAGE_PARAM, packageName)
                        .appendQueryParameter(PLAY_REFERRER_PARAM, Uri.decode(marketReferrer))
                        .build();
        Intent intent = new Intent(Intent.ACTION_VIEW, marketUri);
        intent.addCategory(Intent.CATEGORY_BROWSABLE);
        intent.setPackage("com.android.vending");
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        if (!params.getReferrerUrl().isEmpty()) {
            intent.putExtra(Intent.EXTRA_REFERRER, Uri.parse(params.getReferrerUrl().getSpec()));
        }

        if (!deviceCanHandleIntent(intent)) {
            // Exit early if the Play Store isn't available. (https://crbug.com/820709)
            if (DEBUG) Log.i(TAG, "Play Store not installed.");
            return OverrideUrlLoadingResult.forNoOverride();
        }

        if (params.isIncognito()) {
            if (!startIncognitoIntent(params, intent, fallbackUrl, false)) {
                if (DEBUG) Log.i(TAG, "Failed to show incognito alert dialog.");
                return OverrideUrlLoadingResult.forNoOverride();
            }
            if (DEBUG) Log.i(TAG, "Incognito intent to Play Store.");
            return OverrideUrlLoadingResult.forAsyncAction(
                    OverrideUrlLoadingAsyncActionType.UI_GATING_INTENT_LAUNCH);
        } else {
            startActivity(intent, false);
            if (DEBUG) Log.i(TAG, "Intent to Play Store.");
            return OverrideUrlLoadingResult.forExternalIntent();
        }
    }

    /**
     * If the given URL is to Google Play, extracts the package name and referrer tracking code
     * from the {@param url} and returns as a Pair in that order. Otherwise returns null.
     */
    private Pair<String, String> maybeGetPlayStoreAppIdAndReferrer(GURL url) {
        if (PLAY_HOSTNAME.equals(url.getHost()) && url.getPath().startsWith(PLAY_APP_PATH)) {
            String playPackage = UrlUtilities.getValueForKeyInQuery(url, PLAY_PACKAGE_PARAM);
            if (TextUtils.isEmpty(playPackage)) return null;
            return new Pair<String, String>(
                    playPackage, UrlUtilities.getValueForKeyInQuery(url, PLAY_REFERRER_PARAM));
        }
        return null;
    }

    /**
     * @return Whether the |url| could be handled by an external application on the system.
     */
    @VisibleForTesting
    boolean canExternalAppHandleUrl(GURL url) {
        if (url.getSpec().startsWith(WTAI_MC_URL_PREFIX)) return true;
        Intent intent;
        try {
            intent = Intent.parseUri(url.getSpec(), Intent.URI_INTENT_SCHEME);
        } catch (Exception ex) {
            // Ignore the error.
            Log.w(TAG, "Bad URI %s", url, ex);
            return false;
        }
        if (intent.getPackage() != null) return true;

        List<ResolveInfo> resolvingInfos = queryIntentActivities(intent);
        return resolvingInfos != null && !resolvingInfos.isEmpty();
    }

    /**
     * Dispatch SMS intents to the default SMS application if applicable.
     * Most SMS apps refuse to send SMS if not set as default SMS application.
     *
     * @param resolvingComponentNames The list of ComponentName that resolves the current intent.
     */
    private String getDefaultSmsPackageName(List<ResolveInfo> resolvingComponentNames) {
        String defaultSmsPackageName = getDefaultSmsPackageNameFromSystem();
        if (defaultSmsPackageName == null) return null;
        // Makes sure that the default SMS app actually resolves the intent.
        for (ResolveInfo resolveInfo : resolvingComponentNames) {
            if (defaultSmsPackageName.equals(resolveInfo.activityInfo.packageName)) {
                return defaultSmsPackageName;
            }
        }
        return null;
    }

    /**
     * Launches WebAPK if the WebAPK is the sole non-browser handler for the given intent.
     * @return Whether a WebAPK was launched.
     */
    private boolean launchWebApkIfSoleIntentHandler(
            List<ResolveInfo> resolvingInfos, Intent targetIntent) {
        String packageName = pickWebApkIfSoleIntentHandler(resolvingInfos);
        if (packageName == null) return false;

        Intent webApkIntent = new Intent(targetIntent);
        webApkIntent.setPackage(packageName);
        try {
            startActivity(webApkIntent, false);
            if (DEBUG) Log.i(TAG, "Launched WebAPK");
            return true;
        } catch (ActivityNotFoundException e) {
            // The WebApk must have been uninstalled/disabled since we queried for Activities to
            // handle this intent.
            if (DEBUG) Log.i(TAG, "WebAPK launch failed");
            return false;
        }
    }

    @Nullable
    private String pickWebApkIfSoleIntentHandler(List<ResolveInfo> resolvingInfos) {
        ArrayList<String> packages = getSpecializedHandlers(resolvingInfos);
        if (packages.size() != 1 || !isValidWebApk(packages.get(0))) return null;
        return packages.get(0);
    }

    /**
     * Returns whether or not there's an activity available to handle the intent.
     */
    private boolean deviceCanHandleIntent(Intent intent) {
        List<ResolveInfo> resolveInfos = queryIntentActivities(intent);
        return resolveInfos != null && !resolveInfos.isEmpty();
    }

    /**
     * See {@link PackageManagerUtils#queryIntentActivities(Intent, int)}
     */
    @NonNull
    private List<ResolveInfo> queryIntentActivities(Intent intent) {
        return PackageManagerUtils.queryIntentActivities(
                intent, PackageManager.GET_RESOLVED_FILTER | PackageManager.MATCH_DEFAULT_ONLY);
    }

    private static boolean intentResolutionMatches(Intent intent, Intent other) {
        return intent.filterEquals(other)
                && (intent.getSelector() == other.getSelector()
                        || intent.getSelector().filterEquals(other.getSelector()));
    }

    /**
     * @return Whether the URL is a file download.
     */
    @VisibleForTesting
    boolean isPdfDownload(GURL url) {
        String fileExtension = MimeTypeMap.getFileExtensionFromUrl(url.getSpec());
        if (TextUtils.isEmpty(fileExtension)) return false;

        return PDF_EXTENSION.equals(fileExtension);
    }

    private static boolean isPdfIntent(Intent intent) {
        if (intent == null || intent.getData() == null) return false;
        String filename = intent.getData().getLastPathSegment();
        return (filename != null && filename.endsWith(PDF_SUFFIX))
                || PDF_MIME.equals(intent.getType());
    }

    /**
     * Records the dispatching of an external intent.
     */
    private static void recordExternalNavigationDispatched(Intent intent) {
        ArrayList<String> specializedHandlers =
                intent.getStringArrayListExtra(EXTRA_EXTERNAL_NAV_PACKAGES);
        if (specializedHandlers != null && specializedHandlers.size() > 0) {
            RecordUserAction.record("MobileExternalNavigationDispatched");
        }
    }

    /**
     * If the intent is for a pdf, resolves intent handlers to find the platform pdf viewer if
     * it is available and force is for the provided |intent| so that the user doesn't need to
     * choose it from Intent picker.
     *
     * @param intent Intent to open.
     */
    private static void forcePdfViewerAsIntentHandlerIfNeeded(Intent intent) {
        if (intent == null || !isPdfIntent(intent)) return;
        resolveIntent(intent, true /* allowSelfOpen (ignored) */);
    }

    /**
     * Retrieve the best activity for the given intent. If a default activity is provided,
     * choose the default one. Otherwise, return the Intent picker if there are more than one
     * capable activities. If the intent is pdf type, return the platform pdf viewer if
     * it is available so user don't need to choose it from Intent picker.
     *
     * @param intent Intent to open.
     * @param allowSelfOpen Whether chrome itself is allowed to open the intent.
     * @return true if the intent can be resolved, or false otherwise.
     */
    public static boolean resolveIntent(Intent intent, boolean allowSelfOpen) {
        Context context = ContextUtils.getApplicationContext();
        ResolveInfo info =
                PackageManagerUtils.resolveActivity(intent, PackageManager.MATCH_DEFAULT_ONLY);
        if (info == null) return false;

        final String packageName = context.getPackageName();
        if (info.match != 0) {
            // There is a default activity for this intent, use that.
            return allowSelfOpen || !packageName.equals(info.activityInfo.packageName);
        }
        List<ResolveInfo> handlers = PackageManagerUtils.queryIntentActivities(
                intent, PackageManager.MATCH_DEFAULT_ONLY);
        if (handlers == null || handlers.isEmpty()) return false;
        boolean canSelfOpen = false;
        boolean hasPdfViewer = false;
        for (ResolveInfo resolveInfo : handlers) {
            String pName = resolveInfo.activityInfo.packageName;
            if (packageName.equals(pName)) {
                canSelfOpen = true;
            } else if (PDF_VIEWER.equals(pName)) {
                if (isPdfIntent(intent)) {
                    intent.setClassName(pName, resolveInfo.activityInfo.name);
                    Uri referrer = new Uri.Builder()
                                           .scheme(IntentUtils.ANDROID_APP_REFERRER_SCHEME)
                                           .authority(packageName)
                                           .build();
                    intent.putExtra(Intent.EXTRA_REFERRER, referrer);
                    hasPdfViewer = true;
                    break;
                }
            }
        }
        return !canSelfOpen || allowSelfOpen || hasPdfViewer;
    }

    /**
     * Start an activity for the intent. Used for intents that must be handled externally.
     * @param intent The intent we want to send.
     * @param proxy Whether we need to proxy the intent through AuthenticatedProxyActivity (this is
     *              used by Instant Apps intents).
     */
    private void startActivity(Intent intent, boolean proxy) {
        startActivity(intent, proxy, false, null, null, null, null, null);
    }

    /**
     * Start an activity for the intent. Used for intents that may be handled internally or
     * externally.
     * @param intent The intent we want to send.
     * @param proxy Whether we need to proxy the intent through AuthenticatedProxyActivity (this is
     *              used by Instant Apps intents).
     * @param requiresIntentChooser Whether, for security reasons, the Intent Chooser is required to
     *                              be shown.
     *
     * Below parameters are only used if |requiresIntentChooser| is true.
     *
     * @param resolvingInfos The queryIntentActivities |intent| matches against.
     * @param resolveActivity The resolving Activity |intent| matches against.
     * @param browserFallbackUrl The fallback URL if the user chooses not to leave this app.
     * @param intentDataUrl The URL |intent| is targeting.
     * @param referrerUrl The referrer for the navigation.
     * @returns The OverrideUrlLoadingResult for starting (or not starting) the Activity.
     */
    protected OverrideUrlLoadingResult startActivity(Intent intent, boolean proxy,
            boolean requiresIntentChooser, List<ResolveInfo> resolvingInfos,
            ResolveActivitySupplier resolveActivity, GURL browserFallbackUrl, GURL intentDataUrl,
            GURL referrerUrl) {
        // Only touches disk on Kitkat. See http://crbug.com/617725 for more context.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
        try {
            forcePdfViewerAsIntentHandlerIfNeeded(intent);
            if (proxy) {
                mDelegate.dispatchAuthenticatedIntent(intent);
                recordExternalNavigationDispatched(intent);
                return OverrideUrlLoadingResult.forExternalIntent();
            } else {
                Context context = ContextUtils.activityFromContext(mDelegate.getContext());
                if (context == null) {
                    context = ContextUtils.getApplicationContext();
                    intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                }
                if (requiresIntentChooser) {
                    return startActivityWithChooser(intent, resolvingInfos, resolveActivity,
                            browserFallbackUrl, intentDataUrl, referrerUrl, context);
                }
                return doStartActivity(intent, context);
            }
        } catch (SecurityException e) {
            // https://crbug.com/808494: Handle the URL internally if dispatching to another
            // application fails with a SecurityException. This happens due to malformed
            // manifests in another app.
        } catch (ActivityNotFoundException e) {
            // The targeted app must have been uninstalled/disabled since we queried for Activities
            // to handle this intent.
            if (DEBUG) Log.i(TAG, "Activity not found.");
        } catch (AndroidRuntimeException e) {
            // https://crbug.com/1226177: Most likely cause of this exception is Android failing
            // to start the app that we previously detected could handle the Intent.
            Log.e(TAG, "Could not start Activity for intent " + intent.toString(), e);
        } catch (RuntimeException e) {
            IntentUtils.logTransactionTooLargeOrRethrow(e, intent);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
        return OverrideUrlLoadingResult.forNoOverride();
    }

    private OverrideUrlLoadingResult doStartActivity(Intent intent, Context context) {
        if (DEBUG) Log.i(TAG, "startActivity");
        context.startActivity(intent);
        recordExternalNavigationDispatched(intent);
        return OverrideUrlLoadingResult.forExternalIntent();
    }

    @SuppressWarnings("UseCompatLoadingForDrawables")
    private OverrideUrlLoadingResult startActivityWithChooser(final Intent intent,
            List<ResolveInfo> resolvingInfos, ResolveActivitySupplier resolveActivity,
            GURL browserFallbackUrl, GURL intentDataUrl, GURL referrerUrl, Context context) {
        ResolveInfo intentResolveInfo = resolveActivity.get();
        // If this is null, then the intent was only previously matching
        // non-default filters, so just drop it.
        if (intentResolveInfo == null) return OverrideUrlLoadingResult.forNoOverride();

        // If the |resolvingInfos| from queryIntentActivities don't contain the result of
        // resolveActivity, it means the intent is resolving to the ResolverActivity, so the user
        // will already get the option to choose the target app (as there will be multiple options)
        // and we don't need to do anything. Otherwise we have to make a fake option in the chooser
        // dialog that loads the URL in the embedding app.
        if (!resolversSubsetOf(Arrays.asList(intentResolveInfo), resolvingInfos)) {
            return doStartActivity(intent, context);
        }

        Intent pickerIntent = new Intent(Intent.ACTION_PICK_ACTIVITY);
        pickerIntent.putExtra(Intent.EXTRA_INTENT, intent);

        // Add the fake entry for the embedding app. This behavior is not well documented but works
        // consistently across Android since L (and at least up to S).
        PackageManager pm = context.getPackageManager();
        ArrayList<ShortcutIconResource> icons = new ArrayList<>();
        ArrayList<String> labels = new ArrayList<>();
        String packageName = context.getPackageName();
        String label = "";
        ShortcutIconResource resource = new ShortcutIconResource();
        try {
            ApplicationInfo applicationInfo =
                    pm.getApplicationInfo(packageName, PackageManager.GET_META_DATA);
            label = (String) pm.getApplicationLabel(applicationInfo);
            Resources resources = pm.getResourcesForApplication(applicationInfo);
            resource.packageName = packageName;
            resource.resourceName = resources.getResourceName(applicationInfo.icon);
            // This will throw a Resources.NotFoundException if the package uses resource
            // name collapsing/stripping. The ActivityPicker fails to handle this exception, we have
            // have to check for it here to avoid crashes.
            resources.getDrawable(resources.getIdentifier(resource.resourceName, null, null), null);
        } catch (NameNotFoundException | Resources.NotFoundException e) {
            Log.w(TAG, "No icon resource found for package: " + packageName);
            // Most likely the app doesn't have an icon and is just a test
            // app. Android will just use a blank icon.
            resource.packageName = "";
            resource.resourceName = "";
        }
        labels.add(label);
        icons.add(resource);
        pickerIntent.putExtra(Intent.EXTRA_SHORTCUT_NAME, labels);
        pickerIntent.putExtra(Intent.EXTRA_SHORTCUT_ICON_RESOURCE, icons);

        // Call startActivityForResult on the PICK_ACTIVITY intent, which will set the component of
        // the data result to the component of the chosen app.
        mDelegate.getWindowAndroid().showCancelableIntent(
                pickerIntent, new WindowAndroid.IntentCallback() {
                    @Override
                    public void onIntentCompleted(int resultCode, Intent data) {
                        // If |data| is null, the user backed out of the intent chooser.
                        if (data == null) return;

                        // Quirk of how we use the ActivityChooser - if the embedding app is
                        // chosen we get an intent back with ACTION_CREATE_SHORTCUT.
                        if (data.getAction().equals(Intent.ACTION_CREATE_SHORTCUT)) {
                            // It's pretty arbitrary whether to prefer the data URL or the fallback
                            // URL here. We could consider preferring the fallback URL, as the URL
                            // was probably intending to leave Chrome, but loading the URL the site
                            // was trying to load in a browser seems like the better choice and
                            // matches what would have happened had the regular chooser dialog shown
                            // up and the user selected this app.
                            if (UrlUtilities.isAcceptedScheme(intentDataUrl)) {
                                clobberCurrentTab(intentDataUrl, referrerUrl);
                            } else if (!browserFallbackUrl.isEmpty()) {
                                clobberCurrentTab(browserFallbackUrl, referrerUrl);
                            }
                            return;
                        }

                        // Set the package for the original intent to the chosen app and start
                        // it. Note that a selector cannot be set at the same time as a package.
                        intent.setSelector(null);
                        intent.setPackage(data.getComponent().getPackageName());
                        startActivity(intent, false);
                    }
                }, null);
        return OverrideUrlLoadingResult.forAsyncAction(
                OverrideUrlLoadingAsyncActionType.UI_GATING_INTENT_LAUNCH);
    }

    /**
     * Returns the number of specialized intent handlers in {@params infos}. Specialized intent
     * handlers are intent handlers which handle only a few URLs (e.g. google maps or youtube).
     */
    private int countSpecializedHandlers(List<ResolveInfo> infos) {
        return getSpecializedHandlersWithFilter(
                infos, null, mDelegate.handlesInstantAppLaunchingInternally())
                .size();
    }

    /**
     * Returns the subset of {@params infos} that are specialized intent handlers.
     */
    private ArrayList<String> getSpecializedHandlers(List<ResolveInfo> infos) {
        return getSpecializedHandlersWithFilter(
                infos, null, mDelegate.handlesInstantAppLaunchingInternally());
    }

    private static boolean matchResolveInfoExceptWildCardHost(
            ResolveInfo info, String filterPackageName) {
        IntentFilter intentFilter = info.filter;
        if (intentFilter == null) {
            // Error on the side of classifying ResolveInfo as generic.
            return false;
        }
        if (intentFilter.countDataAuthorities() == 0 && intentFilter.countDataPaths() == 0) {
            // Don't count generic handlers.
            return false;
        }
        boolean isWildCardHost = false;
        Iterator<IntentFilter.AuthorityEntry> it = intentFilter.authoritiesIterator();
        while (it != null && it.hasNext()) {
            IntentFilter.AuthorityEntry entry = it.next();
            if ("*".equals(entry.getHost())) {
                isWildCardHost = true;
                break;
            }
        }
        if (isWildCardHost) {
            return false;
        }
        if (!TextUtils.isEmpty(filterPackageName)
                && (info.activityInfo == null
                        || !info.activityInfo.packageName.equals(filterPackageName))) {
            return false;
        }
        return true;
    }

    public static ArrayList<String> getSpecializedHandlersWithFilter(List<ResolveInfo> infos,
            String filterPackageName, boolean handlesInstantAppLaunchingInternally) {
        ArrayList<String> result = new ArrayList<>();
        if (infos == null) {
            return result;
        }

        for (ResolveInfo info : infos) {
            if (!matchResolveInfoExceptWildCardHost(info, filterPackageName)) {
                continue;
            }

            if (info.activityInfo != null) {
                if (handlesInstantAppLaunchingInternally
                        && IntentUtils.isInstantAppResolveInfo(info)) {
                    // Don't add the Instant Apps launcher as a specialized handler if the embedder
                    // handles launching of Instant Apps itself.
                    continue;
                }

                result.add(info.activityInfo.packageName);
            } else {
                result.add("");
            }
        }
        return result;
    }

    protected boolean resolveInfoContainsSelf(List<ResolveInfo> resolveInfos) {
        String packageName = mDelegate.getContext().getPackageName();
        for (ResolveInfo resolveInfo : resolveInfos) {
            ActivityInfo info = resolveInfo.activityInfo;
            if (info != null && packageName.equals(info.packageName)) {
                return true;
            }
        }
        return false;
    }

    /**
     * @return Default SMS application's package name at the system level. Null if there isn't any.
     */
    @VisibleForTesting
    protected String getDefaultSmsPackageNameFromSystem() {
        return Telephony.Sms.getDefaultSmsPackage(ContextUtils.getApplicationContext());
    }

    /**
     * @return The last committed URL from the WebContents.
     */
    @VisibleForTesting
    protected GURL getLastCommittedUrl() {
        if (mDelegate.getWebContents() == null) return null;
        return mDelegate.getWebContents().getLastCommittedUrl();
    }

    /**
     * @param url The requested url.
     * @return Whether we should block the navigation and request file access before proceeding.
     */
    @VisibleForTesting
    protected boolean shouldRequestFileAccess(GURL url) {
        // If the tab is null, then do not attempt to prompt for access.
        if (!mDelegate.hasValidTab()) return false;
        assert url.getScheme().equals(UrlConstants.FILE_SCHEME);
        // If the url points inside of Chromium's data directory, no permissions are necessary.
        // This is required to prevent permission prompt when uses wants to access offline pages.
        if (url.getPath().startsWith(PathUtils.getDataDirectory())) return false;

        return !mDelegate.getWindowAndroid().hasPermission(permission.READ_EXTERNAL_STORAGE)
                && mDelegate.getWindowAndroid().canRequestPermission(
                        permission.READ_EXTERNAL_STORAGE);
    }

    @Nullable
    // TODO(https://crbug.com/1194721): Investigate whether or not we can use
    // getLastCommittedUrl() instead of the NavigationController. Or maybe we can just replace this
    // with ExternalNavigationParams#getReferrerUrl?
    private GURL getReferrerUrl() {
        if (!mDelegate.hasValidTab() || mDelegate.getWebContents() == null) return null;

        NavigationController nController = mDelegate.getWebContents().getNavigationController();
        int index = nController.getLastCommittedEntryIndex();
        if (index == -1) return null;

        NavigationEntry entry = nController.getEntryAtIndex(index);
        if (entry == null) return null;

        return entry.getUrl();
    }

    /**
     * @return whether this navigation is from the search results page.
     */
    @VisibleForTesting
    protected boolean isSerpReferrer() {
        GURL referrerUrl = getReferrerUrl();
        if (referrerUrl == null || referrerUrl.isEmpty()) return false;

        return UrlUtilitiesJni.get().isGoogleSearchUrl(referrerUrl.getSpec());
    }

    /**
     * @return whether this navigation is from a Google domain.
     */
    @VisibleForTesting
    protected boolean isGoogleReferrer() {
        GURL referrerUrl = getReferrerUrl();
        if (referrerUrl == null || referrerUrl.isEmpty()) return false;

        return UrlUtilitiesJni.get().isGoogleSubDomainUrl(referrerUrl.getSpec());
    }

    /**
     * @return whether this navigation is a redirect from an intent.
     */
    private static boolean isIncomingIntentRedirect(ExternalNavigationParams params) {
        boolean isOnEffectiveIntentRedirect = params.getRedirectHandler() == null
                ? false
                : params.getRedirectHandler().isOnEffectiveIntentRedirectChain();
        return (params.isLinkTransition() && params.isFromIntent() && params.isRedirect())
                || isOnEffectiveIntentRedirect;
    }
}
