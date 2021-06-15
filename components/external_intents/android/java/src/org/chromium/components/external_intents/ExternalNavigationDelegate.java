// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.external_intents;

import android.content.Context;
import android.content.Intent;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.base.Function;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A delegate for {@link ExternalNavigationHandler}.
 */
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
    Context getContext();

    /**
     * Determine if this app is the default or only handler for a given intent. If true, this app
     * will handle the intent when started.
     */
    boolean willAppHandleIntent(Intent intent);

    /**
     * Returns whether to disable forwarding URL requests to external intents for the passed-in URL.
     */
    boolean shouldDisableExternalIntentRequestsForUrl(GURL url);

    /**
     * Returns whether the embedder has custom integration with InstantApps (most embedders will not
     * have any such integration).
     */
    boolean handlesInstantAppLaunchingInternally();

    /**
     * Dispatches the intent through a proxy activity, so that startActivityForResult can be used
     * and the intent recipient can verify the caller. Will be invoked only in flows where
     * ExternalNavigationDelegate#isIntentForInstantApp() returns true for |intent|. In particular,
     * if that method always returns false in the given embedder, then the embedder's implementation
     * of this method will never be invoked and can just assert false.
     * @param intent The bare intent we were going to send.
     */
    void dispatchAuthenticatedIntent(Intent intent);

    /**
     * Informs the delegate that an Activity was started for an external intent (some embedders wish
     * to log this information, primarily for testing purposes).
     */
    void didStartActivity(Intent intent);

    /**
     * Used by maybeHandleStartActivityIfNeeded() below.
     */
    @IntDef({StartActivityIfNeededResult.HANDLED_WITH_ACTIVITY_START,
            StartActivityIfNeededResult.HANDLED_WITHOUT_ACTIVITY_START,
            StartActivityIfNeededResult.DID_NOT_HANDLE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface StartActivityIfNeededResult {
        int HANDLED_WITH_ACTIVITY_START = 0;
        int HANDLED_WITHOUT_ACTIVITY_START = 1;
        int DID_NOT_HANDLE = 2;
    }

    /**
     * Gives the embedder the opportunity to handle starting an activity for the intent. Used for
     * intents that may be handled internally or externally. If the embedder handles this intent,
     * this method should return StartActivityIfNeededResult.HANDLED_{WITH, WITHOUT}_ACTIVITY_START
     * as appropriate. To have ExternalNavigationHandler handle this intent, return
     * StartActivityIfNeededResult.NOT_HANDLED.
     * @param intent The intent we want to send.
     * @param proxy Whether we need to proxy the intent through AuthenticatedProxyActivity (this is
     *              used by Instant Apps intents).
     */
    @StartActivityIfNeededResult
    int maybeHandleStartActivityIfNeeded(Intent intent, boolean proxy);

    /**
     * Loads a URL as specified by |loadUrlParams| if possible. May fail in exceptional conditions
     * (e.g., if there is no valid tab).
     * @param loadUrlParams parameters of the URL to be loaded
     */
    void loadUrlIfPossible(LoadUrlParams loadUrlParams);

    /** Adds a window id to the intent, if necessary. */
    void maybeSetWindowId(Intent intent);

    /** Records the pending referrer if desired. */
    void maybeSetPendingReferrer(Intent intent, GURL referrerUrl);

    /**
     * Adjusts any desired extras related to intents to instant apps based on the value of
     * |insIntentToInstantApp}.
     */
    void maybeAdjustInstantAppExtras(Intent intent, boolean isIntentToInstantApp);

    /**
     * Invoked for intents with request metadata such as user gesture, whether request is renderer
     * initiated and the initiator origin. Records the information if desired.
     */
    void maybeSetRequestMetadata(Intent intent, boolean hasUserGesture, boolean isRendererInitiated,
            @Nullable Origin initiatorOrigin);

    /**
     * Records the pending incognito URL if desired. Called only if the
     * navigation is occurring in the context of incognito mode.
     */
    void maybeSetPendingIncognitoUrl(Intent intent);

    /**
     * Determine if the application of the embedder is in the foreground.
     */
    boolean isApplicationInForeground();

    /**
     * Check if the URL should be handled by an instant app, or kick off an async request for an
     * instant app banner.
     * @param url The current URL.
     * @param referrerUrl The referrer URL.
     * @param isIncomingRedirect Whether we are handling an incoming redirect to an instant app.
     * @param isSerpReferrer whether the referrer is the SERP.
     * @return Whether we launched an instant app.
     */
    boolean maybeLaunchInstantApp(
            GURL url, GURL referrerUrl, boolean isIncomingRedirect, boolean isSerpReferrer);

    /**
     * @return The WindowAndroid instance associated with this delegate instance.
     */
    WindowAndroid getWindowAndroid();

    /**
     * @return The WebContents instance associated with this delegate instance.
     */
    WebContents getWebContents();

    /**
     * @return Whether this delegate has a valid tab available.
     */
    boolean hasValidTab();

    /**
     * @return Whether it's possible to close the current tab on launching on an incognito intent.
     * TODO(blundell): Investigate whether it would be feasible to change the //chrome
     * implementation of this method to be identical to that of its implementation of
     * ExternalNavigationDelegate#hasValidTab() and then eliminate this method in favor of
     * ExternalNavigationHandler calling hasValidTab() if so.
     */
    boolean canCloseTabOnIncognitoIntentLaunch();

    /**
     * @return whether it's possible to load a URL in the current tab.
     */
    boolean canLoadUrlInCurrentTab();

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
     * @param intent The intent to launch.
     * @return Whether the Intent points to an app that we trust and that launched this app.
     */
    boolean isIntentForTrustedCallingApp(Intent intent);

    /**
     * @param intent The intent to launch.
     * @return Whether the Intent points to an instant app.
     */
    boolean isIntentToInstantApp(Intent intent);

    /**
     * @param intent The intent to launch
     * @return Whether the Intent points to Autofill Assistant
     */
    boolean isIntentToAutofillAssistant(Intent intent);

    /**
     * Used by isIntentToAutofillAssistantAllowingApp() below.
     */
    @IntDef({IntentToAutofillAllowingAppResult.NONE,
            IntentToAutofillAllowingAppResult.DEFER_TO_APP_NOW,
            IntentToAutofillAllowingAppResult.DEFER_TO_APP_LATER})
    @Retention(RetentionPolicy.SOURCE)
    public @interface IntentToAutofillAllowingAppResult {
        int NONE = 0;
        // Skip handling with Autofill Assistant and expect an external intent to be launched.
        int DEFER_TO_APP_NOW = 1;
        // Skip handling with Autofill Assistant and expect an external intent to be launched after
        // a redirect.
        int DEFER_TO_APP_LATER = 2;
    }

    /**
     * @param params The external navigation params
     * @param targetIntent The intent to launch
     * @param canExternalAppHandleIntent The checker whether or not an external app can handle the
     * provided intent
     * @return Whether the Intent to Autofill Assistant allows override with an app.
     */
    @IntentToAutofillAllowingAppResult
    int isIntentToAutofillAssistantAllowingApp(ExternalNavigationParams params, Intent targetIntent,
            Function<Intent, Boolean> canExternalAppHandleIntent);

    /**
     * Gives the embedder a chance to handle the intent via the autofill assistant.
     */
    boolean handleWithAutofillAssistant(ExternalNavigationParams params, Intent targetIntent,
            GURL browserFallbackUrl, boolean isGoogleReferrer);
}
