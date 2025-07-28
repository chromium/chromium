// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.layouts.scene_layer;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;

/** Java representation of a scene layer. */
@JNINamespace("android")
@NullMarked
public class SceneLayer {
    public static final int INVALID_RESOURCE_ID = -1;
    private long mNativePtr;

    /** Builds an instance of a {@link SceneLayer}. */
    public SceneLayer() {
        initializeNative();
    }

    /**
     * Initializes the native component of a {@link SceneLayer}. Must be overridden to have a custom
     * native component.
     */
    protected void initializeNative() {
        if (mNativePtr == 0) {
            mNativePtr = SceneLayerJni.get().init(this);
        }
        assert mNativePtr != 0;
    }

    /** Remove this layer from its parent in the tree. */
    public void removeFromParent() {
        if (mNativePtr == 0) return;
        SceneLayerJni.get().removeFromParent(mNativePtr);
    }

    /** Destroys this object and the corresponding native component. */
    public void destroy() {
        assert mNativePtr != 0;
        SceneLayerJni.get().destroy(mNativePtr);
        assert mNativePtr == 0;
    }

    @CalledByNative
    @VisibleForTesting
    public void setNativePtr(long nativeSceneLayerPtr) {
        assert mNativePtr == 0 || nativeSceneLayerPtr == 0;
        mNativePtr = nativeSceneLayerPtr;
    }

    @CalledByNative
    private long getNativePtr() {
        return mNativePtr;
    }

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        long init(SceneLayer self);

        void removeFromParent(long nativeSceneLayer);

        void destroy(long nativeSceneLayer);
    }
}
