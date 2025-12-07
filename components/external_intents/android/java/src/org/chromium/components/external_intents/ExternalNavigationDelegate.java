// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.external_intents;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ResolveInfo;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

import java.util.List;
import java.util.function.Supplier;

/** A delegate for {@link ExternalNavigationHandler}. */
@NullMarked
public interface ExternalNavigationDelegate {
    /**
     * Returns the Context with which this delegate is associated, or null if there is no such
     * Context at the time of invocation. The returned Context may or may not be a wrapper around an
     * Activity with which the delegate is associated. Note that when obtaining resources, however,
     * the handler should do so directly via the returned Context (i.e., not via the Activity that
     * it is wrapping, even if it is in fact wrapping one). The reason is that some embedders handle
     * resource fetching via special logic in the ContextWrapper object that is wrapping the
     * Activity.
     */
    @Nullable Context getContext();

    /**
     * Determine if this app is the default or only handler for a given intent. If true, this app
     * will handle the intent when started.
     */
    boolean willAppHandleIntent(Intent intent);

    /**
     * Returns whether to disable forwarding URL requests to external intents for the passed-in URL.
     *
     * <p>This method may have the side effect of modifying the given {@link Intent}. For specific
     * navigation scenarios defined by {@code params}, it may add the {@link
     * Intent#FLAG_ACTIVITY_MULTIPLE_TASK} flag. This ensures that the external application is
     * launched in a new task.
     *
     * @param params The parameters describing the navigation.
     * @param intent The external {@link Intent} that will be used for the navigation. This object
     *     may be modified by this method.
     * @return true to disable the external intent request, false to allow it.
     */
    boolean shouldDisableExternalIntentRequestsForUrl(
            ExternalNavigationParams params, Intent intent);

    /** Adds a window id to the intent, if necessary. */
    void maybeSetWindowId(Intent intent);

    /** Records the pending referrer if desired. */
    void maybeSetPendingReferrer(Intent intent, GURL referrerUrl);

    /**
     * Invoked for intents with request metadata such as user gesture, whether request is renderer
     * initiated and the initiator origin. Records the information if desired.
     */
    void maybeSetRequestMetadata(
            Intent intent, boolean hasUserGesture, boolean isRendererInitiated);

    /**
     * Records the pending incognito URL if desired. Called only if the
     * navigation is occurring in the context of incognito mode.
     */
    void maybeSetPendingIncognitoUrl(Intent intent);

    /** Determine if the application of the embedder is in the foreground. */
    boolean isApplicationInForeground();

    /**
     * @return The WindowAndroid instance associated with this delegate instance.
     */
    @Nullable WindowAndroid getWindowAndroid();

    /**
     * @return The WebContents instance associated with this delegate instance.
     */
    @Nullable WebContents getWebContents();

    /**
     * @return Whether this delegate has a valid tab available.
     */
    boolean hasValidTab();

    /**
     * @return Whether it's possible to close the current tab on launching on an intent.
     *     TODO(blundell): Investigate whether it would be feasible to change the //chrome
     *     implementation of this method to be identical to that of its implementation of
     *     ExternalNavigationDelegate#hasValidTab() and then eliminate this method in favor of
     *     ExternalNavigationHandler calling hasValidTab() if so.
     */
    boolean canCloseTabOnIntentLaunch();

    /* Invoked when the tab associated with this delegate should be closed. */
    void closeTab();

    /* Returns whether whether the tab associated with this delegate is incognito. */
    boolean isIncognito();

    /* Returns whether the delegate implementation wishes to present its own warning dialog gating
     * the user launching an intent in incognito mode. If this method returns true,
     * ExternalNavigationHandler will invoke presentLeavingIncognitoModalDialog(). If this method
     * returns false, ExternalNavigationHandler will present its own dialog. */
    boolean hasCustomLeavingIncognitoDialog();

    /* Invoked when the user initiates a launch of an intent in incognito mode and the delegate has
     * returned true for hasCustomLeavingIncognitoDialog(). The delegate should
     * invoke onUserDecision() with the user's decision once obtained, passing true if the user has
     * consented to launch the intent and false otherwise.
     * NOTE: The dialog presented should be modal, as confusion of state can otherwise occur. */
    void presentLeavingIncognitoModalDialog(Callback<Boolean> onUserDecision);

    /**
     * @param resolveInfoSupplier The resolveInfos to check.
     * @return Whether the Intent points to an app that we trust and that launched this app.
     */
    boolean isForTrustedCallingApp(Supplier<List<ResolveInfo>> resolveInfoSupplier);

    /** Whether WebAPKs should be launched even on the initial Intent. */
    boolean shouldLaunchWebApksOnInitialIntent();

    /** Adds a target package to the Intent. Only called if isForTrustedCallingApp is true. */
    void setPackageForTrustedCallingApp(Intent intent);

    /**
     * Whether the Activity launch should be aborted if the disambiguation prompt is going to be
     * shown and Chrome is able to handle the navigation.
     */
    boolean shouldAvoidDisambiguationDialog(GURL intentDataUrl);

    /**
     * Returns the scheme (or null) used by web pages to start up the browser (Chrome Stable for
     * Chrome) without an explicit Intent.
     */
    @Nullable String getSelfScheme();

    /** Returns whether all the external intents are supposed to be disabled per embedder. */
    boolean shouldDisableAllExternalIntents();

    /**
     * Returns whether the url should be returned as the result of the current activity.
     *
     * @param url The {@link GURL} to return as activity result.
     */
    boolean shouldReturnAsActivityResult(GURL url);

    /**
     * Sets the url as the result of the current activity and finishes it if conditions are met.
     *
     * @param url The {@link GURL} to return as activity result.
     */
    void returnAsActivityResult(GURL url);

    /**
     * Records the scheme of the external navigation if this is likely a CCT launched for auth
     * purposes.
     *
     * @param url The {@link GURL} of the external navigation.
     */
    void maybeRecordExternalNavigationSchemeHistogram(GURL url);

    /**
     * Records metrics relevant to password saving in CCTs if the recorder exists. A recorder might
     * not exist if there was no form submission preceding the external navigation.
     */
    void notifyCctPasswordSavingRecorderOfExternalNavigation();

    void reportIntentToSafeBrowsing(Intent intent);

    /**
     * Returns an intent that targets the embedder application if opening the url in incognito
     * should be prevented. Returns null otherwise.
     */
    @Nullable Intent createIntentToPreventIncognitoAccess(GURL url);

    /**
     * Returns true if the tab associated with this client was launched from a link opening a new
     * foreground tab.
     */
    boolean wasTabLaunchedFromLinkCreatingNewForegroundTab();

    /**
     * Returns true if the tab associated with this client was launched from a link opening a new
     * window.
     */
    boolean wasTabLaunchedFromLinkCreatingNewWindow();
}
