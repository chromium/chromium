// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import android.graphics.RectF;

import androidx.annotation.ColorInt;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.cc.input.OffsetTag;
import org.chromium.chrome.browser.layouts.SceneOverlay;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.layouts.scene_layer.SceneOverlayLayer;
import org.chromium.ui.resources.ResourceManager;

/**
 * The Java component for the CC layer showing the edge-to-edge top scalp, a view that sits behind
 * the status bar in top browser controls.
 */
@JNINamespace("android")
@NullMarked
public class EdgeToEdgeTopScalpSceneLayer extends SceneOverlayLayer implements SceneOverlay {

    /** Handle to the native side of this class. */
    private long mNativePtr;

    /** Whether the {@link SceneLayer} is visible. */
    private boolean mIsVisible;

    /** The color used for the top scalp. */
    private @ColorInt int mColor;

    /** The height for the {@link SceneLayer} in px. */
    private int mHeight;

    /** The current Y offset to apply to the top scalp in px. */
    private int mCurrentYOffsetPx;

    /** The tag indicating that this layer should be moved by viz. */
    private @Nullable OffsetTag mOffsetTag;

    private final @Nullable Runnable mRequestRenderRunnable;

    /**
     * Build a top scalp scene layer.
     *
     * @param requestRenderRunnable Optional runnable that requests a re-render of the scene
     *     overlay.
     */
    public EdgeToEdgeTopScalpSceneLayer(@Nullable Runnable requestRenderRunnable) {
        mRequestRenderRunnable = requestRenderRunnable;
    }

    /**
     * Set the view's offset from the top of the screen in px.
     *
     * @param offsetPx The view's offset in px.
     */
    public void setYOffset(int offsetPx) {
        if (mCurrentYOffsetPx == offsetPx) return;
        mCurrentYOffsetPx = offsetPx;
    }

    /**
     * @param visible Whether this {@link SceneLayer} is visible.
     */
    public void setIsVisible(boolean visible) {
        if (mIsVisible == visible) return;
        mIsVisible = visible;
        if (mRequestRenderRunnable != null) {
            mRequestRenderRunnable.run();
        }
    }

    /**
     * @param height The height for this {@link SceneLayer} in px.
     */
    public void setHeight(int height) {
        if (mHeight == height) return;
        mHeight = height;
        if (mRequestRenderRunnable != null) {
            mRequestRenderRunnable.run();
        }
    }

    /**
     * @param color The new color for the top scalp.
     */
    public void setColor(@ColorInt int color) {
        if (mColor == color) return;
        mColor = color;
        if (mRequestRenderRunnable != null) {
            mRequestRenderRunnable.run();
        }
    }

    /**
     * @param offsetTag The view's OffsetTag, indicating that this layer will be moved by viz.
     */
    public void setOffsetTag(@Nullable OffsetTag offsetTag) {
        mOffsetTag = offsetTag;
    }

    @Override
    protected void initializeNative() {
        if (mNativePtr == 0) {
            mNativePtr = EdgeToEdgeTopScalpSceneLayerJni.get().init(this);
        }
        assert mNativePtr != 0;
    }

    @Override
    public void setContentTree(SceneLayer contentTree) {
        EdgeToEdgeTopScalpSceneLayerJni.get().setContentTree(mNativePtr, contentTree);
    }

    @Override
    public SceneOverlayLayer getUpdatedSceneOverlayTree(
            RectF viewport, RectF visibleViewport, ResourceManager resourceManager) {
        EdgeToEdgeTopScalpSceneLayerJni.get()
                .updateEdgeToEdgeTopScalpLayer(
                        mNativePtr,
                        (int) viewport.width(),
                        mHeight,
                        mColor,
                        mCurrentYOffsetPx,
                        mOffsetTag);

        return this;
    }

    @Override
    public boolean isSceneOverlayTreeShowing() {
        return mIsVisible;
    }

    @Override
    public void onSizeChanged(
            float width, float height, float visibleViewportOffsetY, int orientation) {}

    @Override
    public void destroy() {
        if (mNativePtr == 0) return;
        super.destroy();
        mNativePtr = 0;
    }

    @NativeMethods
    interface Natives {
        long init(EdgeToEdgeTopScalpSceneLayer self);

        void setContentTree(long nativeEdgeToEdgeTopScalpSceneLayer, SceneLayer contentTree);

        void updateEdgeToEdgeTopScalpLayer(
                long nativeEdgeToEdgeTopScalpSceneLayer,
                int containerWidth,
                int containerHeight,
                int colorARGB,
                float yOffset,
                @Nullable OffsetTag offsetTag);
    }
}
