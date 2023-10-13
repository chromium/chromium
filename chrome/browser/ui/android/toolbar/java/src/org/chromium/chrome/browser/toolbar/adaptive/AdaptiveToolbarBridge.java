// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import android.util.Pair;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Bridge between UI layer and native where segmentation platform is invoked.
 */
public class AdaptiveToolbarBridge {
    /**
     * Called to get the per-session button variant to show on the adaptive toolbar.
     * @param profile The current profile.
     * @param callback The callback to be invoked after getting the button.
     */
    public static void getSessionVariantButton(
            Profile profile, Callback<Pair<Boolean, Integer>> callback) {
        AdaptiveToolbarBridgeJni.get().getSessionVariantButton(
                profile, result -> callback.onResult(result));
    }

    @CalledByNative
    private static Object createResult(
            boolean isReady, @AdaptiveToolbarButtonVariant int buttonVariant) {
        return new Pair<>(isReady, buttonVariant);
    }

    @NativeMethods
    interface Natives {
        void getSessionVariantButton(Profile profile, Callback<Pair<Boolean, Integer>> callback);
    }
}
