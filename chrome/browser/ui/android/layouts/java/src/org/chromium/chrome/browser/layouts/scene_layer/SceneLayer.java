// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.layouts.scene_layer;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Java representation of a scene layer.
 */
@JNINamespace("android")
public class SceneLayer {
    public static final int INVALID_RESOURCE_ID = -1;
    private long mNativePtr;

    /**
     * Builds an instance of a {@link SceneLayer}.
     */
    public SceneLayer() {
        initializeNative();
    }

    /**
     * Initializes the native component of a {@link SceneLayer}.  Must be
     * overridden to have a custom native component.
     */
    protected void initializeNative() {
        if (mNativePtr == 0) {
            mNativePtr = SceneLayerJni.get().init(SceneLayer.this);
        }
        assert mNativePtr != 0;
    }

    /** Remove this layer from its parent in the tree. */
    public void removeFromParent() {
        if (mNativePtr == 0) return;
        SceneLayerJni.get().removeFromParent(mNativePtr, SceneLayer.this);
    }

    /**
     * Destroys this object and the corresponding native component.
     */
    public void destroy() {
        assert mNativePtr != 0;
        SceneLayerJni.get().destroy(mNativePtr, SceneLayer.this);
        assert mNativePtr == 0;
    }

    @CalledByNative
    private void setNativePtr(long nativeSceneLayerPtr) {
        assert mNativePtr == 0 || nativeSceneLayerPtr == 0;
        mNativePtr = nativeSceneLayerPtr;
    }

    @CalledByNative
    private long getNativePtr() {
        return mNativePtr;
    }

    @NativeMethods
    interface Natives {
        long init(SceneLayer caller);
        void removeFromParent(long nativeSceneLayer, SceneLayer caller);
        void destroy(long nativeSceneLayer, SceneLayer caller);
    }
}
