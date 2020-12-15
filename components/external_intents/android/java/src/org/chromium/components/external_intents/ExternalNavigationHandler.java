// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.external_intents;

import android.Manifest.permission;
import android.app.Activity;
import android.content.ActivityNotFoundException;
import android.content.ComponentName;
import android.content.Context;
import android.content.DialogInterface;
import android.content.DialogInterface.OnCancelListener;
import android.content.DialogInterface.OnClickListener;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.StrictMode;
import android.os.SystemClock;
import android.provider.Browser;
import android.provider.Telephony;
import android.text.TextUtils;
import android.util.Pair;
import android.view.WindowManager.BadTokenException;
import android.webkit.MimeTypeMap;
import android.webkit.WebView;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.embedder_support.util.UrlUtilitiesJni;
import org.chromium.components.webapk.lib.client.ChromeWebApkHostSignature;
import org.chromium.components.webapk.lib.client.WebApkValidator;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationEntry;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.common.ContentUrlConstants;
import org.chromium.content_public.common.Referrer;
import org.chromium.network.mojom.ReferrerPolicy;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.PermissionCallback;
import org.chromium.url.URI;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.net.URISyntaxException;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;

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

    // Standard Activity Actions, as defined by:
    // https://developer.android.com/reference/android/content/Intent.html#standard-activity-actions
    // These values are persisted in histograms. Please do not renumber.
    @IntDef({StandardActions.MAIN, StandardActions.VIEW, StandardActions.ATTACH_DATA,
            StandardActions.EDIT, StandardActions.PICK, StandardActions.CHOOSER,
            StandardActions.GET_CONTENT, StandardActions.DIAL, StandardActions.CALL,
            StandardActions.SEND, StandardActions.SENDTO, StandardActions.ANSWER,
            StandardActions.INSERT, StandardActions.DELETE, StandardActions.RUN,
            StandardActions.SYNC, StandardActions.PICK_ACTIVITY, StandardActions.SEARCH,
            StandardActions.WEB_SEARCH, StandardActions.FACTORY_TEST, StandardActions.OTHER})
    @Retention(RetentionPolicy.SOURCE)
    @VisibleForTesting
    @interface StandardActions {
        int MAIN = 0;
        int VIEW = 1;
        int ATTACH_DATA = 2;
        int EDIT = 3;
        int PICK = 4;
        int CHOOSER = 5;
        int GET_CONTENT = 6;
        int DIAL = 7;
        int CALL = 8;
        int SEND = 9;
        int SENDTO = 10;
        int ANSWER = 11;
        int INSERT = 12;
        int DELETE = 13;
        int RUN = 14;
        int SYNC = 15;
        int PICK_ACTIVITY = 16;
        int SEARCH = 17;
        int WEB_SEARCH = 18;
        int FACTORY_TEST = 19;
        int OTHER = 20;

        int NUM_ENTRIES = 21;
    }

    @VisibleForTesting
    static final String INTENT_ACTION_HISTOGRAM = "Android.Intent.OverrideUrlLoadingIntentAction";

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
        if (DEBUG) Log.i(TAG, "shouldOverrideUrlLoading called on " + params.getUrl());
        Intent targetIntent;
        // Perform generic parsing of the URI to turn it into an Intent.
        try {
            targetIntent = Intent.parseUri(params.getUrl(), Intent.URI_INTENT_SCHEME);
        } catch (Exception ex) {
            Log.w(TAG, "Bad URI %s", params.getUrl(), ex);
            return OverrideUrlLoadingResult.forNoOverride();
        }

        String browserFallbackUrl =
                IntentUtils.safeGetStringExtra(targetIntent, EXTRA_BROWSER_FALLBACK_URL);
        if (browserFallbackUrl != null
                && !UrlUtilities.isValidForIntentFallbackNavigation(browserFallbackUrl)) {
            browserFallbackUrl = null;
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
        } else if (result.getResultType() == OverrideUrlLoadingResultType.NO_OVERRIDE
                && browserFallbackUrl != null
                && (params.getRedirectHandler() == null
                        // For instance, if this is a chained fallback URL, we ignore it.
                        || !params.getRedirectHandler().shouldNotOverrideUrlLoading())) {
            result = handleFallbackUrl(params, targetIntent, browserFallbackUrl,
                    canLaunchExternalFallbackResult.get());
        }
        if (DEBUG) printDebugShouldOverrideUrlLoadingResultType(result);
        return result;
    }

    private OverrideUrlLoadingResult handleFallbackUrl(ExternalNavigationParams params,
            Intent targetIntent, String browserFallbackUrl, boolean canLaunchExternalFallback) {
        if (mDelegate.isIntentToInstantApp(targetIntent)) {
            RecordHistogram.recordEnumeratedHistogram("Android.InstantApps.DirectInstantAppsIntent",
                    AiaIntent.FALLBACK_USED, AiaIntent.NUM_ENTRIES);
        }

        if (canLaunchExternalFallback) {
            if (shouldBlockAllExternalAppLaunches(params) || params.isIncognito()) {
                throw new SecurityException("Context is not allowed to launch an external app.");
            }
            // Launch WebAPK if it can handle the URL.
            try {
                Intent intent = Intent.parseUri(browserFallbackUrl, Intent.URI_INTENT_SCHEME);
                sanitizeQueryIntentActivitiesIntent(intent);
                List<ResolveInfo> resolvingInfos = queryIntentActivities(intent);
                if (!isAlreadyInTargetWebApk(resolvingInfos, params)
                        && launchWebApkIfSoleIntentHandler(resolvingInfos, intent)) {
                    return OverrideUrlLoadingResult.forExternalIntent();
                }
            } catch (Exception e) {
                if (DEBUG) Log.i(TAG, "Could not parse fallback url as intent");
            }

            // If the fallback URL is a link to Play Store, send the user to Play Store app
            // instead: crbug.com/638672.
            Pair<String, String> appInfo = maybeGetPlayStoreAppIdAndReferrer(browserFallbackUrl);
            if (appInfo != null) {
                String marketReferrer = TextUtils.isEmpty(appInfo.second)
                        ? ContextUtils.getApplicationContext().getPackageName()
                        : appInfo.second;
                return sendIntentToMarket(appInfo.first, marketReferrer, params);
            }
        }

        // For subframes, we don't support fallback url for now.
        // http://crbug.com/364522.
        if (!params.isMainFrame()) {
            if (DEBUG) Log.i(TAG, "Don't support fallback url in subframes");
            return OverrideUrlLoadingResult.forNoOverride();
        }

        // NOTE: any further redirection from fall-back URL should not override URL loading.
        // Otherwise, it can be used in chain for fingerprinting multiple app installation
        // status in one shot. In order to prevent this scenario, we notify redirection
        // handler that redirection from the current navigation should stay in this app.
        if (params.getRedirectHandler() != null) {
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
    private boolean blockExternalNavWhileBackgrounded(ExternalNavigationParams params) {
        if (params.isApplicationMustBeInForeground() && !mDelegate.isApplicationInForeground()) {
            if (DEBUG) Log.i(TAG, "App is not in foreground");
            return true;
        }
        return false;
    }

    /** http://crbug.com/464669 : Disallow firing external intent from background tab. */
    private boolean blockExternalNavFromBackgroundTab(ExternalNavigationParams params) {
        if (params.isBackgroundTabNavigation()) {
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
    private boolean startFileIntentIfNecessary(
            ExternalNavigationParams params, Intent targetIntent) {
        if (params.getUrl().startsWith(UrlConstants.FILE_URL_SHORT_PREFIX)
                && shouldRequestFileAccess(params.getUrl())) {
            startFileIntent(targetIntent, params.getReferrerUrl(),
                    params.shouldCloseContentsOnOverrideUrlLoadingAndLaunchIntent());
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
    protected void startFileIntent(
            final Intent intent, final String referrerUrl, final boolean needsToCloseTab) {
        PermissionCallback permissionCallback = new PermissionCallback() {
            @Override
            public void onRequestPermissionsResult(String[] permissions, int[] grantResults) {
                if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED
                        && mDelegate.hasValidTab()) {
                    loadUrlFromIntent(referrerUrl, intent.getDataString(), null, mDelegate,
                            needsToCloseTab, mDelegate.isIncognito());
                } else {
                    // TODO(tedchoc): Show an indication to the user that the navigation failed
                    //                instead of silently dropping it on the floor.
                    if (needsToCloseTab) {
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
    protected OverrideUrlLoadingResult clobberCurrentTab(String url, String referrerUrl) {
        int transitionType = PageTransition.LINK;
        final LoadUrlParams loadUrlParams = new LoadUrlParams(url, transitionType);
        if (!TextUtils.isEmpty(referrerUrl)) {
            Referrer referrer = new Referrer(referrerUrl, ReferrerPolicy.ALWAYS);
            loadUrlParams.setReferrer(referrer);
        }
        if (mDelegate.hasValidTab()) {
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
        } else {
            assert false : "clobberCurrentTab was called with an empty tab.";
            Uri uri = Uri.parse(url);
            Intent intent = new Intent(Intent.ACTION_VIEW, uri);
            String packageName = ContextUtils.getApplicationContext().getPackageName();
            intent.putExtra(Browser.EXTRA_APPLICATION_ID, packageName);
            intent.addCategory(Intent.CATEGORY_BROWSABLE);
            intent.setPackage(packageName);
            startActivity(intent, false, mDelegate);
            return OverrideUrlLoadingResult.forExternalIntent();
        }
    }

    private static void loadUrlWithReferrer(
            final String url, final String referrerUrl, ExternalNavigationDelegate delegate) {
        LoadUrlParams loadUrlParams = new LoadUrlParams(url, PageTransition.AUTO_TOPLEVEL);
        if (!TextUtils.isEmpty(referrerUrl)) {
            Referrer referrer = new Referrer(referrerUrl, ReferrerPolicy.ALWAYS);
            loadUrlParams.setReferrer(referrer);
        }
        delegate.loadUrlIfPossible(loadUrlParams);
    }

    /**
     * Loads the URL from an intent, either in the current tab or a new tab, falling back to the
     * |alternateUrl| if the |primaryUrl| is unsupported.
     *
     * Handling is determined as follows:
     *
     * If the url scheme is not supported we do nothing.
     * If the url can be loaded in the current tab then we load the url there.
     * If the url can't be loaded in the current tab then we launch a new tab and load it there.
     *
     * @param referrerUrl The string containing the original url from where the intent was referred.
     * @param primaryUrl The primary url to load.
     * @param alternateUrl The fallback url to use if the primary url is null or invalid.
     * @param delegate The delegate instance with this request is associated.
     * @param launchIncognito Whether the url should be loaded in an incognito tab.
     * @return true if the url is loaded in the current tab.
     */
    public static boolean loadUrlFromIntent(String referrerUrl, String primaryUrl,
            String alternateUrl, ExternalNavigationDelegate delegate, boolean needsToCloseTab,
            boolean launchIncognito) {
        // Check whether we should load this URL in the current tab or in a new tab.
        if (!delegate.supportsCreatingNewTabs() && !delegate.canLoadUrlInCurrentTab()) return false;
        boolean loadInNewTab = delegate.supportsCreatingNewTabs()
                && (!delegate.canLoadUrlInCurrentTab() || needsToCloseTab);

        boolean isPrimaryUrlValid =
                (primaryUrl != null) ? UrlUtilities.isAcceptedScheme(primaryUrl) : false;
        boolean isAlternateUrlValid =
                (alternateUrl != null) ? UrlUtilities.isAcceptedScheme(alternateUrl) : false;

        if (!isPrimaryUrlValid && !isAlternateUrlValid) return false;

        String url = (isPrimaryUrlValid) ? primaryUrl : alternateUrl;

        if (loadInNewTab) {
            delegate.loadUrlInNewTab(url, launchIncognito);
            // Explicit request to close the tab.
            if (needsToCloseTab) delegate.closeTab();
            return false;
        }

        loadUrlWithReferrer(url, referrerUrl, delegate);
        return true;
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
            if (DEBUG) Log.i(TAG, "RedirectHandler decision");
            return true;
        }
        return false;
    }

    /** Wrapper of check against the feature to support overriding for testing. */
    @VisibleForTesting
    boolean blockExternalFormRedirectsWithoutGesture() {
        return ExternalIntentsFeatureList.isEnabled(
                ExternalIntentsFeatureList.INTENT_BLOCK_EXTERNAL_FORM_REDIRECT_NO_GESTURE);
    }

    /**
     * http://crbug.com/149218: We want to show the intent picker for ordinary links, providing
     * the link is not an incoming intent from another application, unless it's a redirect.
     */
    private boolean preferToShowIntentPicker(ExternalNavigationParams params,
            int pageTransitionCore, boolean isExternalProtocol, boolean isFormSubmit,
            boolean linkNotFromIntent, boolean incomingIntentRedirect) {
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
        if (params.getReferrerUrl() == null) return false;
        if (params.getReferrerUrl().startsWith(UrlConstants.CHROME_URL_PREFIX)
                && (params.getUrl().startsWith(UrlConstants.HTTP_URL_PREFIX)
                        || params.getUrl().startsWith(UrlConstants.HTTPS_URL_PREFIX))) {
            if (DEBUG) Log.i(TAG, "Link from an internal chrome:// page");
            return true;
        }
        return false;
    }

    private boolean handleWtaiMcProtocol(ExternalNavigationParams params) {
        if (!params.getUrl().startsWith(WTAI_MC_URL_PREFIX)) return false;
        // wtai://wp/mc;number
        // number=string(phone-number)
        startActivity(new Intent(Intent.ACTION_VIEW,
                              Uri.parse(WebView.SCHEME_TEL
                                      + params.getUrl().substring(WTAI_MC_URL_PREFIX.length()))),
                false, mDelegate);
        if (DEBUG) Log.i(TAG, "wtai:// link handled");
        RecordUserAction.record("Android.PhoneIntent");
        return true;
    }

    private boolean isUnhandledWtaiProtocol(ExternalNavigationParams params) {
        if (!params.getUrl().startsWith(WTAI_URL_PREFIX)) return false;
        if (DEBUG) Log.i(TAG, "Unsupported wtai:// link");
        return true;
    }

    /**
     * The "about:", "chrome:", "chrome-native:", and "devtools:" schemes
     * are internal to the browser; don't want these to be dispatched to other apps.
     */
    private boolean hasInternalScheme(
            ExternalNavigationParams params, Intent targetIntent, boolean hasIntentScheme) {
        String url;
        if (hasIntentScheme) {
            // TODO(https://crbug.com/783819): When this function is converted to GURL, we should
            // also call fixUpUrl on this user-provided URL as the fixed-up URL is what we would end
            // up navigating to.
            url = targetIntent.getDataString();
            if (url == null) return false;
        } else {
            url = params.getUrl();
        }
        if (url.startsWith(ContentUrlConstants.ABOUT_SCHEME)
                || url.startsWith(UrlConstants.CHROME_URL_SHORT_PREFIX)
                || url.startsWith(UrlConstants.CHROME_NATIVE_URL_SHORT_PREFIX)
                || url.startsWith(UrlConstants.DEVTOOLS_URL_SHORT_PREFIX)) {
            if (DEBUG) Log.i(TAG, "Navigating to a chrome-internal page");
            return true;
        }
        return false;
    }

    /** The "content:" scheme is disabled in Clank. Do not try to start an activity. */
    private boolean hasContentScheme(
            ExternalNavigationParams params, Intent targetIntent, boolean hasIntentScheme) {
        String url;
        if (hasIntentScheme) {
            url = targetIntent.getDataString();
            if (url == null) return false;
        } else {
            url = params.getUrl();
        }
        if (!url.startsWith(UrlConstants.CONTENT_URL_SHORT_PREFIX)) return false;
        if (DEBUG) Log.i(TAG, "Navigation to content: URL");
        return true;
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
    private boolean hasFileSchemeInIntentURI(Intent targetIntent, boolean hasIntentScheme) {
        // We are only concerned with targetIntent that was generated due to intent:// schemes only.
        if (!hasIntentScheme) return false;

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
    private boolean isYoutubePairingCode(ExternalNavigationParams params) {
        // TODO(https://crbug.com/1009539): Replace this regex with proper URI parsing.
        if (params.getUrl().matches(".*youtube\\.com(\\/.*)?\\?(.+&)?pairingCode=[^&].+")) {
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
            ExternalNavigationParams params, Intent targetIntent, String browserFallbackUrl) {
        // Fallback URL will be handled by the caller of shouldOverrideUrlLoadingInternal.
        if (browserFallbackUrl != null) return OverrideUrlLoadingResult.forNoOverride();
        if (targetIntent.getPackage() != null) return handleWithMarketIntent(params, targetIntent);

        if (DEBUG) Log.i(TAG, "Could not find an external activity to use");
        return OverrideUrlLoadingResult.forNoOverride();
    }

    private OverrideUrlLoadingResult handleWithMarketIntent(
            ExternalNavigationParams params, Intent intent) {
        String marketReferrer = IntentUtils.safeGetStringExtra(intent, EXTRA_MARKET_REFERRER);
        if (TextUtils.isEmpty(marketReferrer)) {
            marketReferrer = ContextUtils.getApplicationContext().getPackageName();
        }
        return sendIntentToMarket(intent.getPackage(), marketReferrer, params);
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
     * Current URL has at least one specialized handler available. For navigations
     * within the same host, keep the navigation inside the browser unless the set of
     * available apps to handle the new navigation is different. http://crbug.com/463138
     */
    private boolean shouldStayWithinHost(ExternalNavigationParams params, boolean isLink,
            boolean isFormSubmit, List<ResolveInfo> resolvingInfos, boolean isExternalProtocol) {
        if (isExternalProtocol) return false;

        // TODO(https://crbug.com/1009539): Replace this host parsing with a UrlUtilities or GURL
        //   function call.
        String lastCommittedUrl = getLastCommittedUrl();
        String previousUriString =
                lastCommittedUrl != null ? lastCommittedUrl : params.getReferrerUrl();
        if (previousUriString == null || (!isLink && !isFormSubmit)) return false;

        URI currentUri;
        URI previousUri;

        try {
            currentUri = new URI(params.getUrl());
            previousUri = new URI(previousUriString);
        } catch (Exception e) {
            return false;
        }

        if (currentUri == null || previousUri == null
                || !TextUtils.equals(currentUri.getHost(), previousUri.getHost())) {
            return false;
        }

        Intent previousIntent;
        try {
            previousIntent = Intent.parseUri(previousUriString, Intent.URI_INTENT_SCHEME);
        } catch (Exception e) {
            return false;
        }

        if (previousIntent != null
                && resolversSubsetOf(resolvingInfos, queryIntentActivities(previousIntent))) {
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

        if (params.getReferrerUrl() != null) {
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
            ExternalNavigationParams params, String browserFallbackUrl,
            boolean shouldProxyForInstantApps) {
        // This intent may leave this app. Warn the user that incognito does not carry over
        // to external apps.
        if (startIncognitoIntent(targetIntent, params.getReferrerUrl(), browserFallbackUrl,
                    params.shouldCloseContentsOnOverrideUrlLoadingAndLaunchIntent(),
                    shouldProxyForInstantApps)) {
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
    private boolean startIncognitoIntent(final Intent intent, final String referrerUrl,
            final String fallbackUrl, final boolean needsToCloseTab, final boolean proxy) {
        try {
            return startIncognitoIntentInternal(
                    intent, referrerUrl, fallbackUrl, needsToCloseTab, proxy);
        } catch (BadTokenException e) {
            return false;
        }
    }

    /**
     * Internal implementation of startIncognitoIntent(), with all the same parameters.
     */
    @VisibleForTesting
    protected boolean startIncognitoIntentInternal(final Intent intent, final String referrerUrl,
            final String fallbackUrl, final boolean needsToCloseTab, final boolean proxy) {
        if (!mDelegate.hasValidTab()) return false;
        Context context = mDelegate.getContext();
        if (ContextUtils.activityFromContext(context) == null) return false;

        new UiUtils.CompatibleAlertDialogBuilder(context, R.style.Theme_Chromium_AlertDialog)
                .setTitle(R.string.external_app_leave_incognito_warning_title)
                .setMessage(R.string.external_app_leave_incognito_warning)
                .setPositiveButton(R.string.external_app_leave_incognito_leave,
                        new OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                try {
                                    startActivity(intent, proxy, mDelegate);
                                    if (mDelegate.canCloseTabOnIncognitoIntentLaunch()
                                            && needsToCloseTab) {
                                        mDelegate.closeTab();
                                    }
                                } catch (ActivityNotFoundException e) {
                                    // The activity that we thought was going to handle the intent
                                    // no longer exists, so catch the exception and assume Chrome
                                    // can handle it.
                                    loadUrlFromIntent(referrerUrl, fallbackUrl,
                                            intent.getDataString(), mDelegate, needsToCloseTab,
                                            true);
                                }
                            }
                        })
                .setNegativeButton(R.string.external_app_leave_incognito_stay,
                        new OnClickListener() {
                            @Override
                            public void onClick(DialogInterface dialog, int which) {
                                loadUrlFromIntent(referrerUrl, fallbackUrl, intent.getDataString(),
                                        mDelegate, needsToCloseTab, true);
                            }
                        })
                .setOnCancelListener(new OnCancelListener() {
                    @Override
                    public void onCancel(DialogInterface dialog) {
                        loadUrlFromIntent(referrerUrl, fallbackUrl, intent.getDataString(),
                                mDelegate, needsToCloseTab, true);
                    }
                })
                .show();
        return true;
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
                && !params.getRedirectHandler().hasNewResolver(resolvingInfos)) {
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

    private boolean launchExternalIntent(Intent targetIntent, boolean shouldProxyForInstantApps) {
        try {
            if (!startActivityIfNeeded(targetIntent, shouldProxyForInstantApps)) {
                if (DEBUG) Log.i(TAG, "The current Activity was the only targeted Activity.");
                return false;
            }
        } catch (ActivityNotFoundException e) {
            // The targeted app must have been uninstalled/disabled since we queried for Activities
            // to handle this intent.
            if (DEBUG) Log.i(TAG, "Activity not found.");
            return false;
        }
        if (DEBUG) Log.i(TAG, "startActivityIfNeeded");
        return true;
    }

    // This will handle external navigations only for intent meant for Autofill Assistant.
    private boolean handleWithAutofillAssistant(
            ExternalNavigationParams params, Intent targetIntent, String browserFallbackUrl) {
        if (mDelegate.isIntentToAutofillAssistant(targetIntent)) {
            if (mDelegate.handleWithAutofillAssistant(
                        params, targetIntent, browserFallbackUrl, isGoogleReferrer())) {
                if (DEBUG) Log.i(TAG, "Handled with Autofill Assistant.");
            } else {
                if (DEBUG) Log.i(TAG, "Not handled with Autofill Assistant.");
            }
            return true;
        }
        return false;
    }

    // Check if we're navigating under conditions that should never launch an external app.
    private boolean shouldBlockAllExternalAppLaunches(ExternalNavigationParams params) {
        return blockExternalNavFromAutoSubframe(params) || blockExternalNavWhileBackgrounded(params)
                || blockExternalNavFromBackgroundTab(params) || ignoreBackForwardNav(params);
    }

    private OverrideUrlLoadingResult shouldOverrideUrlLoadingInternal(
            ExternalNavigationParams params, Intent targetIntent,
            @Nullable String browserFallbackUrl, MutableBoolean canLaunchExternalFallbackResult) {
        sanitizeQueryIntentActivitiesIntent(targetIntent);
        // Don't allow external fallback URLs by default.
        canLaunchExternalFallbackResult.set(false);

        if (shouldBlockAllExternalAppLaunches(params)) {
            return OverrideUrlLoadingResult.forNoOverride();
        }

        if (handleWithAutofillAssistant(params, targetIntent, browserFallbackUrl)) {
            return OverrideUrlLoadingResult.forNoOverride();
        }

        boolean isExternalProtocol = !UrlUtilities.isAcceptedScheme(params.getUrl());

        if (isInternalPdfDownload(isExternalProtocol, params)) {
            return OverrideUrlLoadingResult.forNoOverride();
        }

        // This check should happen for reloads, navigations, etc..., which is why
        // it occurs before the subsequent blocks.
        if (startFileIntentIfNecessary(params, targetIntent)) {
            return OverrideUrlLoadingResult.forAsyncAction(
                    OverrideUrlLoadingAsyncActionType.UI_GATING_BROWSER_NAVIGATION);
        }

        // This should come after file intents, but before any returns of
        // OVERRIDE_WITH_EXTERNAL_INTENT.
        if (externalIntentRequestsDisabledForUrl(params)) {
            return OverrideUrlLoadingResult.forNoOverride();
        }

        int pageTransitionCore = params.getPageTransition() & PageTransition.CORE_MASK;
        boolean isLink = pageTransitionCore == PageTransition.LINK;
        boolean isFormSubmit = pageTransitionCore == PageTransition.FORM_SUBMIT;
        boolean isFromIntent = (params.getPageTransition() & PageTransition.FROM_API) != 0;
        boolean linkNotFromIntent = isLink && !isFromIntent;

        boolean isOnEffectiveIntentRedirect = params.getRedirectHandler() == null
                ? false
                : params.getRedirectHandler().isOnEffectiveIntentRedirectChain();

        // http://crbug.com/170925: We need to show the intent picker when we receive an intent from
        // another app that 30x redirects to a YouTube/Google Maps/Play Store/Google+ URL etc.
        boolean incomingIntentRedirect =
                (isLink && isFromIntent && params.isRedirect()) || isOnEffectiveIntentRedirect;

        if (handleCCTRedirectsToInstantApps(params, isExternalProtocol, incomingIntentRedirect)) {
            return OverrideUrlLoadingResult.forExternalIntent();
        } else if (redirectShouldStayInApp(params, isExternalProtocol, targetIntent)) {
            return OverrideUrlLoadingResult.forNoOverride();
        }

        if (!preferToShowIntentPicker(params, pageTransitionCore, isExternalProtocol, isFormSubmit,
                    linkNotFromIntent, incomingIntentRedirect)) {
            return OverrideUrlLoadingResult.forNoOverride();
        }

        if (isLinkFromChromeInternalPage(params)) return OverrideUrlLoadingResult.forNoOverride();

        if (handleWtaiMcProtocol(params)) {
            return OverrideUrlLoadingResult.forExternalIntent();
        }
        // TODO: handle other WTAI schemes.
        if (isUnhandledWtaiProtocol(params)) return OverrideUrlLoadingResult.forNoOverride();

        boolean hasIntentScheme = params.getUrl().startsWith(UrlConstants.INTENT_URL_SHORT_PREFIX)
                || params.getUrl().startsWith(UrlConstants.APP_INTENT_URL_SHORT_PREFIX);
        if (hasInternalScheme(params, targetIntent, hasIntentScheme)) {
            return OverrideUrlLoadingResult.forNoOverride();
        }

        if (hasContentScheme(params, targetIntent, hasIntentScheme)) {
            return OverrideUrlLoadingResult.forNoOverride();
        }

        if (hasFileSchemeInIntentURI(targetIntent, hasIntentScheme)) {
            return OverrideUrlLoadingResult.forNoOverride();
        }

        if (isYoutubePairingCode(params)) return OverrideUrlLoadingResult.forNoOverride();

        if (shouldStayInIncognito(params, isExternalProtocol)) {
            return OverrideUrlLoadingResult.forNoOverride();
        }

        if (!maybeSetSmsPackage(targetIntent)) maybeRecordPhoneIntentMetrics(targetIntent);

        if (hasIntentScheme) recordIntentActionMetrics(targetIntent);

        // From this point on, we have determined it is safe to launch an External App from a
        // fallback URL, provided the user isn't in incognito.
        if (!params.isIncognito()) canLaunchExternalFallbackResult.set(true);

        Intent debugIntent = new Intent(targetIntent);
        List<ResolveInfo> resolvingInfos = queryIntentActivities(targetIntent);
        if (resolvingInfos.isEmpty()) {
            return handleUnresolvableIntent(params, targetIntent, browserFallbackUrl);
        }

        if (browserFallbackUrl != null) targetIntent.removeExtra(EXTRA_BROWSER_FALLBACK_URL);

        boolean hasSpecializedHandler = countSpecializedHandlers(resolvingInfos) > 0;
        if (!isExternalProtocol && !hasSpecializedHandler) {
            if (fallBackToHandlingWithInstantApp(
                        params, incomingIntentRedirect, linkNotFromIntent)) {
                return OverrideUrlLoadingResult.forExternalIntent();
            }
            return fallBackToHandlingInApp();
        }

        // From this point on we should only have intents that this app can't handle, or intents for
        // apps with specialized handlers.

        if (shouldStayWithinHost(
                    params, isLink, isFormSubmit, resolvingInfos, isExternalProtocol)) {
            return OverrideUrlLoadingResult.forNoOverride();
        }

        boolean isDirectInstantAppsIntent =
                isExternalProtocol && mDelegate.isIntentToInstantApp(targetIntent);
        boolean shouldProxyForInstantApps = isDirectInstantAppsIntent && isSerpReferrer();
        if (preventDirectInstantAppsIntent(isDirectInstantAppsIntent, shouldProxyForInstantApps)) {
            return OverrideUrlLoadingResult.forNoOverride();
        }

        prepareExternalIntent(targetIntent, params, resolvingInfos, shouldProxyForInstantApps);
        // As long as our intent resolution hasn't changed, resolvingInfos won't need to be
        // re-computed as it won't have changed.
        assert intentResolutionMatches(debugIntent, targetIntent);

        if (params.isIncognito()) {
            boolean intentTargetedToApp = mDelegate.willAppHandleIntent(targetIntent);

            // The user is about to potentially leave the app, so we should ask whether they want to
            // leave incognito or not.
            if (!intentTargetedToApp) {
                return handleExternalIncognitoIntent(
                        targetIntent, params, browserFallbackUrl, shouldProxyForInstantApps);
            }

            // The intent is staying in the app, so we can simply navigate to the intent's URL,
            // while staying in incognito.
            return mDelegate.handleIncognitoIntentTargetingSelf(
                    targetIntent, params.getReferrerUrl(), browserFallbackUrl);
        }

        if (shouldKeepIntentRedirectInApp(
                    params, incomingIntentRedirect, resolvingInfos, isExternalProtocol)) {
            return OverrideUrlLoadingResult.forNoOverride();
        }

        if (isAlreadyInTargetWebApk(resolvingInfos, params)) {
            return OverrideUrlLoadingResult.forNoOverride();
        } else if (launchWebApkIfSoleIntentHandler(resolvingInfos, targetIntent)) {
            return OverrideUrlLoadingResult.forExternalIntent();
        }
        if (launchExternalIntent(targetIntent, shouldProxyForInstantApps)) {
            return OverrideUrlLoadingResult.forExternalIntent();
        }
        return OverrideUrlLoadingResult.forNoOverride();
    }

    /**
     * Sanitize intent to be passed to {@link queryIntentActivities()}
     * ensuring that web pages cannot bypass browser security.
     */
    private void sanitizeQueryIntentActivitiesIntent(Intent intent) {
        intent.setFlags(intent.getFlags() & ALLOWED_INTENT_FLAGS);
        intent.addCategory(Intent.CATEGORY_BROWSABLE);
        intent.setComponent(null);
        Intent selector = intent.getSelector();
        if (selector != null) {
            selector.addCategory(Intent.CATEGORY_BROWSABLE);
            selector.setComponent(null);
        }
    }

    /**
     * @return OVERRIDE_WITH_EXTERNAL_INTENT when we successfully started market activity,
     *         NO_OVERRIDE otherwise.
     */
    private OverrideUrlLoadingResult sendIntentToMarket(
            String packageName, String marketReferrer, ExternalNavigationParams params) {
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
        if (params.getReferrerUrl() != null) {
            intent.putExtra(Intent.EXTRA_REFERRER, Uri.parse(params.getReferrerUrl()));
        }

        if (!deviceCanHandleIntent(intent)) {
            // Exit early if the Play Store isn't available. (https://crbug.com/820709)
            if (DEBUG) Log.i(TAG, "Play Store not installed.");
            return OverrideUrlLoadingResult.forNoOverride();
        }

        if (params.isIncognito()) {
            if (!startIncognitoIntent(intent, params.getReferrerUrl(), null,

                        params.shouldCloseContentsOnOverrideUrlLoadingAndLaunchIntent(), false)) {
                if (DEBUG) Log.i(TAG, "Failed to show incognito alert dialog.");
                return OverrideUrlLoadingResult.forNoOverride();
            }
            if (DEBUG) Log.i(TAG, "Incognito intent to Play Store.");
            return OverrideUrlLoadingResult.forAsyncAction(
                    OverrideUrlLoadingAsyncActionType.UI_GATING_INTENT_LAUNCH);
        } else {
            startActivity(intent, false, mDelegate);
            if (DEBUG) Log.i(TAG, "Intent to Play Store.");
            return OverrideUrlLoadingResult.forExternalIntent();
        }
    }

    /**
     * If the given URL is to Google Play, extracts the package name and referrer tracking code
     * from the {@param url} and returns as a Pair in that order. Otherwise returns null.
     */
    private Pair<String, String> maybeGetPlayStoreAppIdAndReferrer(String url) {
        Uri uri = Uri.parse(url);
        if (PLAY_HOSTNAME.equals(uri.getHost()) && uri.getPath() != null
                && uri.getPath().startsWith(PLAY_APP_PATH)
                && !TextUtils.isEmpty(uri.getQueryParameter(PLAY_PACKAGE_PARAM))) {
            return new Pair<String, String>(uri.getQueryParameter(PLAY_PACKAGE_PARAM),
                    uri.getQueryParameter(PLAY_REFERRER_PARAM));
        }
        return null;
    }

    /**
     * @return Whether the |url| could be handled by an external application on the system.
     */
    @VisibleForTesting
    boolean canExternalAppHandleUrl(String url) {
        if (url.startsWith(WTAI_MC_URL_PREFIX)) return true;
        Intent intent;
        try {
            intent = Intent.parseUri(url, Intent.URI_INTENT_SCHEME);
        } catch (URISyntaxException ex) {
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
        ArrayList<String> packages = getSpecializedHandlers(resolvingInfos);
        if (packages.size() != 1 || !isValidWebApk(packages.get(0))) return false;
        Intent webApkIntent = new Intent(targetIntent);
        webApkIntent.setPackage(packages.get(0));
        try {
            startActivity(webApkIntent, false, mDelegate);
            if (DEBUG) Log.i(TAG, "Launched WebAPK");
            return true;
        } catch (ActivityNotFoundException e) {
            // The WebApk must have been uninstalled/disabled since we queried for Activities to
            // handle this intent.
            if (DEBUG) Log.i(TAG, "WebAPK launch failed");
            return false;
        }
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
                intent, PackageManager.GET_RESOLVED_FILTER);
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
    boolean isPdfDownload(String url) {
        String fileExtension = MimeTypeMap.getFileExtensionFromUrl(url);
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
        ResolveInfo info = PackageManagerUtils.resolveActivity(intent, 0);
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
    public static void startActivity(
            Intent intent, boolean proxy, ExternalNavigationDelegate delegate) {
        try {
            forcePdfViewerAsIntentHandlerIfNeeded(intent);
            if (proxy) {
                delegate.dispatchAuthenticatedIntent(intent);
            } else {
                // Start the activity via the current activity if possible, and otherwise as a new
                // task from the application context.
                Context context = ContextUtils.activityFromContext(delegate.getContext());
                if (context == null) {
                    context = ContextUtils.getApplicationContext();
                    intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                }
                context.startActivity(intent);
            }
            recordExternalNavigationDispatched(intent);
        } catch (RuntimeException e) {
            IntentUtils.logTransactionTooLargeOrRethrow(e, intent);
        }

        delegate.didStartActivity(intent);
    }

    /**
     * Start an activity for the intent. Used for intents that may be handled internally or
     * externally.
     * @param intent The intent we want to send.
     * @param proxy Whether we need to proxy the intent through AuthenticatedProxyActivity (this is
     *              used by Instant Apps intents).
     * @returns whether an activity was started for the intent.
     */
    private boolean startActivityIfNeeded(Intent intent, boolean proxy) {
        @ExternalNavigationDelegate.StartActivityIfNeededResult
        int delegateResult = mDelegate.maybeHandleStartActivityIfNeeded(intent, proxy);

        switch (delegateResult) {
            case ExternalNavigationDelegate.StartActivityIfNeededResult.HANDLED_WITH_ACTIVITY_START:
                return true;
            case ExternalNavigationDelegate.StartActivityIfNeededResult
                    .HANDLED_WITHOUT_ACTIVITY_START:
                return false;
            case ExternalNavigationDelegate.StartActivityIfNeededResult.DID_NOT_HANDLE:
                return startActivityIfNeededInternal(intent, proxy);
        }

        assert false;
        return false;
    }

    /**
     * Implementation of startActivityIfNeeded() that is used when the delegate does not handle the
     * event.
     */
    private boolean startActivityIfNeededInternal(Intent intent, boolean proxy) {
        boolean activityWasLaunched;
        // Only touches disk on Kitkat. See http://crbug.com/617725 for more context.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
        try {
            forcePdfViewerAsIntentHandlerIfNeeded(intent);
            if (proxy) {
                mDelegate.dispatchAuthenticatedIntent(intent);
                activityWasLaunched = true;
            } else {
                Activity activity = ContextUtils.activityFromContext(mDelegate.getContext());
                if (activity != null) {
                    activityWasLaunched = activity.startActivityIfNeeded(intent, -1);
                } else {
                    activityWasLaunched = false;
                }
            }
            if (activityWasLaunched) {
                recordExternalNavigationDispatched(intent);
            }
            return activityWasLaunched;
        } catch (SecurityException e) {
            // https://crbug.com/808494: Handle the URL internally if dispatching to another
            // application fails with a SecurityException. This happens due to malformed manifests
            // in another app.
            return false;
        } catch (RuntimeException e) {
            IntentUtils.logTransactionTooLargeOrRethrow(e, intent);
            return false;
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }
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
    protected String getLastCommittedUrl() {
        if (mDelegate.getWebContents() == null) return null;
        return mDelegate.getWebContents().getLastCommittedUrl();
    }

    private void recordIntentActionMetrics(Intent intent) {
        String action = intent.getAction();
        @StandardActions
        int standardAction;
        if (TextUtils.isEmpty(action)) {
            standardAction = StandardActions.VIEW;
        } else {
            standardAction = getStandardAction(action);
        }
        RecordHistogram.recordEnumeratedHistogram(
                INTENT_ACTION_HISTOGRAM, standardAction, StandardActions.NUM_ENTRIES);
    }

    /**
     * @param url The requested url.
     * @return Whether we should block the navigation and request file access before proceeding.
     */
    @VisibleForTesting
    protected boolean shouldRequestFileAccess(String url) {
        // If the tab is null, then do not attempt to prompt for access.
        if (!mDelegate.hasValidTab()) return false;

        // If the url points inside of Chromium's data directory, no permissions are necessary.
        // This is required to prevent permission prompt when uses wants to access offline pages.
        if (url.startsWith(UrlConstants.FILE_URL_PREFIX + PathUtils.getDataDirectory())) {
            return false;
        }

        return !mDelegate.getWindowAndroid().hasPermission(permission.READ_EXTERNAL_STORAGE)
                && mDelegate.getWindowAndroid().canRequestPermission(
                        permission.READ_EXTERNAL_STORAGE);
    }

    @Nullable
    private String getReferrerUrl() {
        // TODO (thildebr): Investigate whether or not we can use getLastCommittedUrl() instead of
        // the NavigationController.
        if (!mDelegate.hasValidTab() || mDelegate.getWebContents() == null) return null;

        NavigationController nController = mDelegate.getWebContents().getNavigationController();
        int index = nController.getLastCommittedEntryIndex();
        if (index == -1) return null;

        NavigationEntry entry = nController.getEntryAtIndex(index);
        if (entry == null) return null;

        return entry.getUrl().getSpec();
    }

    /**
     * @return whether this navigation is from the search results page.
     */
    @VisibleForTesting
    protected boolean isSerpReferrer() {
        String referrerUrl = getReferrerUrl();
        if (referrerUrl == null) return false;

        return UrlUtilitiesJni.get().isGoogleSearchUrl(referrerUrl);
    }

    private boolean isGoogleReferrer() {
        String referrerUrl = getReferrerUrl();
        if (referrerUrl == null) return false;

        return UrlUtilitiesJni.get().isGoogleSubDomainUrl(referrerUrl);
    }

    private @StandardActions int getStandardAction(String action) {
        switch (action) {
            case Intent.ACTION_MAIN:
                return StandardActions.MAIN;
            case Intent.ACTION_VIEW:
                return StandardActions.VIEW;
            case Intent.ACTION_ATTACH_DATA:
                return StandardActions.ATTACH_DATA;
            case Intent.ACTION_EDIT:
                return StandardActions.EDIT;
            case Intent.ACTION_PICK:
                return StandardActions.PICK;
            case Intent.ACTION_CHOOSER:
                return StandardActions.CHOOSER;
            case Intent.ACTION_GET_CONTENT:
                return StandardActions.GET_CONTENT;
            case Intent.ACTION_DIAL:
                return StandardActions.DIAL;
            case Intent.ACTION_CALL:
                return StandardActions.CALL;
            case Intent.ACTION_SEND:
                return StandardActions.SEND;
            case Intent.ACTION_SENDTO:
                return StandardActions.SENDTO;
            case Intent.ACTION_ANSWER:
                return StandardActions.ANSWER;
            case Intent.ACTION_INSERT:
                return StandardActions.INSERT;
            case Intent.ACTION_DELETE:
                return StandardActions.DELETE;
            case Intent.ACTION_RUN:
                return StandardActions.RUN;
            case Intent.ACTION_SYNC:
                return StandardActions.SYNC;
            case Intent.ACTION_PICK_ACTIVITY:
                return StandardActions.PICK_ACTIVITY;
            case Intent.ACTION_SEARCH:
                return StandardActions.SEARCH;
            case Intent.ACTION_WEB_SEARCH:
                return StandardActions.WEB_SEARCH;
            case Intent.ACTION_FACTORY_TEST:
                return StandardActions.FACTORY_TEST;
            default:
                return StandardActions.OTHER;
        }
    }
}
