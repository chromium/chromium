// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.messages;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;

/** Java accessor for base::Features listed in {@link MessageFeatureList} */
@JNINamespace("messages")
public final class MessageFeatureMap extends FeatureMap {
    private static final MessageFeatureMap sInstance = new MessageFeatureMap();

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
