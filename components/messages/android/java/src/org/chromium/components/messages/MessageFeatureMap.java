// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import androidx.annotation.IntDef;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;
import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/** Java accessor for base::Features listed in {@link MessageFeatureList} */
@JNINamespace("messages")
@NullMarked
public final class MessageFeatureMap extends FeatureMap {
    private static final MessageFeatureMap sInstance = new MessageFeatureMap();

    /** Int values for the param of the MessagesAccessibilityEventInvestigations experiment. */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        AccessibilityEventInvestigationGroup.DEFAULT,
        AccessibilityEventInvestigationGroup.ENABLED_BASELINE,
        AccessibilityEventInvestigationGroup.ENABLED_WITH_ACCESSIBILITY_STATE,
        AccessibilityEventInvestigationGroup.ENABLED_WITH_RESTRICTIVE_SERVICE_CHECK,
        AccessibilityEventInvestigationGroup.ENABLED_WITH_MASK_CHECK,
        AccessibilityEventInvestigationGroup.ENABLED_WITH_DIRECT_QUERY,
    })
    public @interface AccessibilityEventInvestigationGroup {
        int DEFAULT = 0;
        int ENABLED_BASELINE = 1;
        int ENABLED_WITH_ACCESSIBILITY_STATE = 2;
        int ENABLED_WITH_RESTRICTIVE_SERVICE_CHECK = 3;
        int ENABLED_WITH_MASK_CHECK = 4;
        int ENABLED_WITH_DIRECT_QUERY = 5;
    }

    // Do not instantiate this class.
    private MessageFeatureMap() {}

    /** @return the singleton MessageFeatureMap. */
    public static MessageFeatureMap getInstance() {
        return sInstance;
    }

    /** Convenience method to call {@link #isEnabledInNative(String)} statically. */
    public static boolean isEnabled(String featureName) {
        return getInstance().isEnabledInNative(featureName);
    }

    @Override
    protected long getNativeMap() {
        return MessageFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    public interface Natives {
        long getNativeMap();
    }
}
