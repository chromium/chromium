// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.adaptive;

import android.util.Pair;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.ArrayList;
import java.util.List;

/** Bridge between UI layer and native where segmentation platform is invoked. */
public class AdaptiveToolbarBridge {
    /**
     * Called to get the per-session button variant to show on the adaptive toolbar.
     *
     * @param profile The current profile.
     * @param callback The callback to be invoked after getting the button.
     */
    public static void getSessionVariantButton(
            Profile profile, Callback<Pair<Boolean, Integer>> callback) {
        AdaptiveToolbarBridgeJni.get()
                .getSessionVariantButton(profile, result -> callback.onResult(result));
    }

    /**
     * Called to get the per-session button list to show on the adaptive toolbar.
     *
     * @param profile The current profile.
     * @param useRawResults If true it'll get the raw model results, without applying any
     *     thresholds, should only be used on tablets.
     * @param callback Callback to be invoked after getting the button. It returns a boolean
     *     indicating whether the model was ready to execute and a sorted list of
     *     AdaptiveToolbarButtonVariant values, where the first element is the highest ranked. This
     *     list will contain a single value of AdaptiveToolbarButtonVariant.UNKNOWN if the model was
     *     not executed.
     */
    public static void getSessionVariantButtons(
            Profile profile,
            boolean useRawResults,
            Callback<Pair<Boolean, List<Integer>>> callback) {
        AdaptiveToolbarBridgeJni.get()
                .getRankedSessionVariantButtons(
                        profile, useRawResults, result -> callback.onResult(result));
    }

    @CalledByNative
    private static Object createResult(
            boolean isReady, @AdaptiveToolbarButtonVariant int buttonVariant) {
        return new Pair<>(isReady, buttonVariant);
    }

    @CalledByNative
    private static Object createResultList(
            boolean isReady, @AdaptiveToolbarButtonVariant int[] buttonVariants) {
        ArrayList<Integer> buttonRankingList = new ArrayList<>();
        for (int button : buttonVariants) {
            buttonRankingList.add(button);
        }
        return new Pair<>(isReady, buttonRankingList);
    }

    @NativeMethods
    interface Natives {
        void getSessionVariantButton(
                @JniType("Profile*") Profile profile, Callback<Pair<Boolean, Integer>> callback);

        void getRankedSessionVariantButtons(
                @JniType("Profile*") Profile profile,
                boolean useRawResults,
                Callback<Pair<Boolean, List<Integer>>> callback);
    }
}
