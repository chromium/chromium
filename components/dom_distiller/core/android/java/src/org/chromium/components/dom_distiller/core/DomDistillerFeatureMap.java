// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.dom_distiller.core;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.FeatureMap;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Maps native dom_distiller features to java. */
@JNINamespace("dom_distiller::android")
@NullMarked
public class DomDistillerFeatureMap extends FeatureMap {
    private static @Nullable DomDistillerFeatureMap sInstance;

    // Not directly instantiable.
    private DomDistillerFeatureMap() {
        super();
    }

    /**
     * @return the singleton DomDistillerFeatureMap.
     */
    public static DomDistillerFeatureMap getInstance() {
        if (sInstance == null) sInstance = new DomDistillerFeatureMap();
        return sInstance;
    }

    @Override
    protected long getNativeMap() {
        return DomDistillerFeatureMapJni.get().getNativeMap();
    }

    @NativeMethods
    protected interface Natives {
        long getNativeMap();
    }
}
