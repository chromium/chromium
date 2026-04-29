// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.media;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Java accessor for base/android/feature_map.h state. */
@JNINamespace("browser_ui")
@NullMarked
public final class MediaFeatureMap extends FeatureMap {
    private static @Nullable MediaFeatureMap sInstance;

    private MediaFeatureMap() {}

    /** Returns the singleton MediaFeatureMap. */
    public static MediaFeatureMap getInstance() {
        if (sInstance == null) {
            sInstance = new MediaFeatureMap();
        }
        return sInstance;
    }

    @Override
    protected long getNativeMap() {
        return MediaFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    public interface Natives {
        long getNativeMap();
    }
}
