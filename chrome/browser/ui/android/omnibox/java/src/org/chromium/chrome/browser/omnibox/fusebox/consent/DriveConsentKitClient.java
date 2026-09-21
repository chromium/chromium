// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox.consent;

import android.text.TextUtils;

import org.json.JSONException;
import org.json.JSONObject;

import org.chromium.base.Log;
import org.chromium.base.TerminationStatus;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.fusebox.DriveDisclaimerBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.LifecycleState;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.url.GURL;

/**
 * Java client for the Drive ConsentKit dialog.
 *
 * <p>Handles web contents lifecycle callbacks and JavaScript bridge events, driving the consent
 * validation flow and reporting outcomes to the {@link Delegate}.
 */
@NullMarked
class DriveConsentKitClient extends WebContentsObserver implements DriveConsentJsBridge.Delegate {
    private static final String TAG = "DriveConsent";
    private static final String RESULT_KEY = "encodedPrivacyFlowResult";

    /** Interface for receiving lifecycle and outcome events from the client. */
    interface Delegate {
        /** Called when the web page paints its first visually non-empty content. */
        void onPageFirstPaint();

        /** Called when the web page fails to load or its renderer process crashes. */
        void onLoadFailed();

        /**
         * Called when the consent flow completes.
         *
         * @param granted Whether the user granted consent.
         */
        void onConsentComplete(boolean granted);
    }

    private final Profile mProfile;
    private final Delegate mDelegate;
    private boolean mDestroyed;
    private boolean mCompleted;
    private boolean mFirstPaintNotified;

    /**
     * Constructs a new DriveConsentKitClient.
     *
     * @param webContents The web contents hosting the ConsentKit page.
     * @param profile The current user profile.
     * @param delegate The delegate interface to receive events.
     */
    DriveConsentKitClient(@Nullable WebContents webContents, Profile profile, Delegate delegate) {
        super(webContents);
        mProfile = profile;
        mDelegate = delegate;
    }

    @Override
    public void didFirstVisuallyNonEmptyPaint() {
        if (mDestroyed || mCompleted || mFirstPaintNotified) return;
        mFirstPaintNotified = true;
        mDelegate.onPageFirstPaint();
    }

    @Override
    public void didFailLoad(
            boolean isInPrimaryMainFrame,
            int errorCode,
            GURL failingUrl,
            @LifecycleState int rfhLifecycleState) {
        if (!isInPrimaryMainFrame) return;
        notifyLoadFailed();
    }

    @Override
    public void primaryMainFrameRenderProcessGone(@TerminationStatus int terminationStatus) {
        notifyLoadFailed();
    }

    @Override
    public void webContentsDestroyed() {
        notifyLoadFailed();
        destroy();
    }

    @Override
    public void onWhenAttached(int callbackId) {
        if (mDestroyed || mCompleted) return;
        WebContents webContents = getWebContents();
        if (webContents == null) return;
        webContents.evaluateJavaScript(
                "window.ckUiCallback(" + callbackId + ")", /* callback= */ null);
    }

    @Override
    public void onCloseWithResult(@Nullable String resultJson) {
        if (mDestroyed || mCompleted) return;
        if (TextUtils.isEmpty(resultJson)) {
            notifyConsentComplete(/* granted= */ false);
            return;
        }

        try {
            JSONObject json = new JSONObject(resultJson);
            String base64Result = json.optString(RESULT_KEY);
            WebContents webContents = getWebContents();
            if (TextUtils.isEmpty(base64Result) || webContents == null) {
                notifyConsentComplete(/* granted= */ false);
                return;
            }

            notifyConsentComplete(
                    DriveDisclaimerBridge.parseAndSaveConsentResult(
                            mProfile, webContents, base64Result));
        } catch (JSONException e) {
            Log.w(TAG, "Failed to parse ConsentKit result JSON: %s", resultJson, e);
            notifyConsentComplete(/* granted= */ false);
        }
    }

    void destroy() {
        if (mDestroyed) return;
        mDestroyed = true;
        observe(null);
    }

    /** Reports a terminal load failure exactly once. */
    private void notifyLoadFailed() {
        if (mDestroyed || mCompleted) return;
        mCompleted = true;
        mDelegate.onLoadFailed();
    }

    /** Reports the terminal consent outcome exactly once. */
    private void notifyConsentComplete(boolean granted) {
        if (mDestroyed || mCompleted) return;
        mCompleted = true;
        mDelegate.onConsentComplete(granted);
    }
}
