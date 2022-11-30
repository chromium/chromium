// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import org.chromium.base.FeatureList;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.build.annotations.MainDex;

/**
 * Provides an API for querying the status of Message features.
 */
@JNINamespace("messages")
@MainDex
public class MessageFeatureList {
    public static final String MESSAGES_FOR_ANDROID_STACKING_ANIMATION =
            "MessagesForAndroidStackingAnimation";

    private MessageFeatureList() {}

    public static boolean isEnabled(String featureName) {
        Boolean testValue = FeatureList.getTestValueForFeature(featureName);
        if (testValue != null) return testValue;
        assert FeatureList.isNativeInitialized();
        return MessageFeatureListJni.get().isEnabled(featureName);
    }

    public static boolean isStackAnimationEnabled() {
        return isEnabled(MESSAGES_FOR_ANDROID_STACKING_ANIMATION);
    }

    @NativeMethods
    interface Natives {
        boolean isEnabled(String featureName);
    }
}
