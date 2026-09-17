// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.fusebox;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.contextual_search.DisclaimerStatus;
import org.chromium.content_public.browser.WebContents;

/**
 * Stateless bridge exposing the user's Google Drive disclaimer (consent) status and ConsentKit
 * WebView utilities to Java.
 *
 * <p>This class is not instantiated; UI callers invoke its static methods on demand when the user
 * interacts with Google Drive features. On the native side, a controller is created per request and
 * destroyed once the asynchronous check completes.
 */
@NullMarked
public class DriveDisclaimerBridge {
    private DriveDisclaimerBridge() {}

    /**
     * Asynchronously queries the Footprints backend for the user's current Drive consent status.
     *
     * <p>The backend is the source of truth: consent may have been granted or revoked on another
     * device, so callers must re-query instead of caching the result.
     *
     * @param profile The current user profile.
     * @param callback Receives the resulting {@link DisclaimerStatus}.
     */
    public static void checkConsentStatus(
            Profile profile, Callback<@DisclaimerStatus Integer> callback) {
        DriveDisclaimerBridgeJni.get().checkConsentStatus(profile, callback);
    }

    /**
     * Returns the ConsentKit landing URL for top-level WebView rendering, or an empty string if it
     * could not be built.
     */
    public static String getConsentUrl(Profile profile, boolean isDarkMode) {
        return DriveDisclaimerBridgeJni.get().getConsentUrl(profile, isDarkMode);
    }

    /**
     * Registers the ConsentKit User-Agent override, which the server uses to enable its JavaScript
     * bridge. The navigation must also opt in via {@code
     * LoadUrlParams#setOverrideUserAgent(UserAgentOverrideOption.TRUE)}.
     */
    public static void setConsentKitUserAgent(WebContents webContents, boolean isDarkMode) {
        DriveDisclaimerBridgeJni.get().setConsentKitUserAgent(webContents, isDarkMode);
    }

    /**
     * Parses the base64url-encoded {@code PrivacyFlowResult} returned by ConsentKit and records the
     * consent preference. Returns whether consent was granted.
     */
    public static boolean parseAndSaveConsentResult(
            Profile profile, WebContents webContents, String base64Result) {
        return DriveDisclaimerBridgeJni.get()
                .parseAndSaveConsentResult(profile, webContents, base64Result);
    }

    @NativeMethods
    public interface Natives {
        void checkConsentStatus(
                @JniType("Profile*") Profile profile,
                @JniType("base::OnceCallback<void(DisclaimerStatus)>")
                        Callback<@DisclaimerStatus Integer> callback);

        @JniType("std::string")
        String getConsentUrl(@JniType("Profile*") Profile profile, boolean isDarkMode);

        void setConsentKitUserAgent(
                @JniType("content::WebContents*") WebContents webContents, boolean isDarkMode);

        boolean parseAndSaveConsentResult(
                @JniType("Profile*") Profile profile,
                @JniType("content::WebContents*") WebContents webContents,
                @JniType("std::string") String base64Result);
    }
}
