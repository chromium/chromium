// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.web_distiller.core;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Maps native web_distiller features to java. */
@JNINamespace("web_distiller::android")
@NullMarked
public class WebDistillerFeatureMap extends FeatureMap {
    private static @Nullable WebDistillerFeatureMap sInstance;

    // Not directly instantiable.
    private WebDistillerFeatureMap() {
        super();
    }

    /**
     * @return the singleton WebDistillerFeatureMap.
     */
    public static WebDistillerFeatureMap getInstance() {
        if (sInstance == null) sInstance = new WebDistillerFeatureMap();
        return sInstance;
    }

    @Override
    protected long getNativeMap() {
        return WebDistillerFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    protected interface Natives {
        long getNativeMap();
    }
}
