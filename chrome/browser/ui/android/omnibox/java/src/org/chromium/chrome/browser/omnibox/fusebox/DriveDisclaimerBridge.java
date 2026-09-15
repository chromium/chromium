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

/**
 * Stateless bridge exposing the user's Google Drive disclaimer (consent) status to Java.
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

    @NativeMethods
    public interface Natives {
        void checkConsentStatus(
                @JniType("Profile*") Profile profile,
                @JniType("base::OnceCallback<void(DisclaimerStatus)>")
                        Callback<@DisclaimerStatus Integer> callback);
    }
}
