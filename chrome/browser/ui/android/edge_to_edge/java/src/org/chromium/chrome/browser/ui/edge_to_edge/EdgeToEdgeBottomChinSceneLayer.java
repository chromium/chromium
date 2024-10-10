// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.edge_to_edge;

import android.graphics.RectF;

import androidx.annotation.ColorInt;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.layouts.EventFilter;
import org.chromium.chrome.browser.layouts.SceneOverlay;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.layouts.scene_layer.SceneOverlayLayer;
import org.chromium.ui.resources.ResourceManager;

import java.util.List;

/**
 * The Java component for the CC layer showing the edge-to-edge bottom chin, a scrollable view that
 * visually imitates the bottom OS navigation bar. This layer will simply show a solid block of
 * color of a particular height extending across the width of the screen viewport.
 */
@JNINamespace("android")
public class EdgeToEdgeBottomChinSceneLayer extends SceneOverlayLayer implements SceneOverlay {
    /** Handle to the native side of this class. */
    private long mNativePtr;

    /** Whether the {@link SceneLayer} is visible. */
    private boolean mIsVisible;

    /** The color used for the bottom chin. */
    private int mColor;

    /** The height for the {@link SceneLayer} in px. */
    private int mHeight;

    /** The current Y offset to apply to the bottom chin in px. */
    private int mCurrentYOffsetPx;

    /** Attributes for the divider. */
    private int mDividerColor;

    /** Build a bottom chin scene layer. */
    public EdgeToEdgeBottomChinSceneLayer() {}

    /**
     * Set the view's offset from the bottom of the screen in px. An offset of 0 means the view is
     * completely visible. An increasing offset will move the view down.
     *
     * @param offsetPx The view's offset in px.
     */
    public void setYOffset(int offsetPx) {
        mCurrentYOffsetPx = offsetPx;
    }

    /**
     * @param visible Whether this {@link SceneLayer} is visible.
     */
    public void setIsVisible(boolean visible) {
        mIsVisible = visible;
    }

    /**
     * @param height The height for this {@link SceneLayer}. This new height will apply on the next
     *     call to #getUpdatedSceneOverlayTree.
     */
    public void setHeight(int height) {
        mHeight = height;
    }

    /**
     * @param color The new color for the bottom chin. This new color will apply on the next call to
     *     #getUpdatedSceneOverlayTree.
     */
    public void setColor(@ColorInt int color) {
        mColor = color;
    }

    /**
     * Set the color for the divider.
     *
     * @see #setDividerVisible(boolean)
     */
    public void setDividerColor(@ColorInt int dividerColor) {
        mDividerColor = dividerColor;
    }

    @Override
    protected void initializeNative() {
        if (mNativePtr == 0) {
            mNativePtr =
                    EdgeToEdgeBottomChinSceneLayerJni.get()
                            .init(EdgeToEdgeBottomChinSceneLayer.this);
        }
        assert mNativePtr != 0;
    }

    @Override
    public void setContentTree(SceneLayer contentTree) {
        EdgeToEdgeBottomChinSceneLayerJni.get()
                .setContentTree(mNativePtr, EdgeToEdgeBottomChinSceneLayer.this, contentTree);
    }

    @Override
    public SceneOverlayLayer getUpdatedSceneOverlayTree(
            RectF viewport, RectF visibleViewport, ResourceManager resourceManager, float yOffset) {
        EdgeToEdgeBottomChinSceneLayerJni.get()
                .updateEdgeToEdgeBottomChinLayer(
                        mNativePtr,
                        (int) viewport.width(),
                        mHeight,
                        mColor,
                        mDividerColor,
                        viewport.height() + mCurrentYOffsetPx);

        return this;
    }

    @Override
    public boolean isSceneOverlayTreeShowing() {
        return mIsVisible;
    }

    @Override
    public EventFilter getEventFilter() {
        return null;
    }

    @Override
    public boolean shouldHideAndroidBrowserControls() {
        return false;
    }

    @Override
    public boolean updateOverlay(long time, long dt) {
        return false;
    }

    @Override
    public boolean onBackPressed() {
        return false;
    }

    @Override
    public boolean handlesTabCreating() {
        return false;
    }

    @Override
    public void onSizeChanged(
            float width, float height, float visibleViewportOffsetY, int orientation) {}

    @Override
    public void getVirtualViews(List<VirtualView> views) {}

    @NativeMethods
    interface Natives {
        long init(EdgeToEdgeBottomChinSceneLayer caller);

        void setContentTree(
                long nativeEdgeToEdgeBottomChinSceneLayer,
                EdgeToEdgeBottomChinSceneLayer caller,
                SceneLayer contentTree);

        void updateEdgeToEdgeBottomChinLayer(
                long nativeEdgeToEdgeBottomChinSceneLayer,
                int containerWidth,
                int containerHeight,
                int colorARGB,
                int dividerColor,
                float yOffset);
    }
}
