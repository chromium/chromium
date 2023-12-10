// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import android.graphics.RectF;
import android.view.View;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.layouts.EventFilter;
import org.chromium.chrome.browser.layouts.SceneOverlay;
import org.chromium.chrome.browser.layouts.components.VirtualView;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.layouts.scene_layer.SceneOverlayLayer;
import org.chromium.components.browser_ui.widget.ViewResourceFrameLayout;
import org.chromium.ui.resources.ResourceManager;

import java.util.List;

/**
 * A composited view that sits at the bottom of the screen and listens to changes in the browser
 * controls. When visible, the view will mimic the behavior of the top browser controls when
 * scrolling.
 */
@JNINamespace("android")
public class ScrollingBottomViewSceneLayer extends SceneOverlayLayer implements SceneOverlay {
    /** Handle to the native side of this class. */
    private long mNativePtr;

    /** The resource ID used to reference the view bitmap in native. */
    private int mResourceId;

    /** The height of the view's top shadow. */
    private int mTopShadowHeightPx;

    /** The current Y offset of the bottom view in px. */
    private int mCurrentYOffsetPx;

    /** The current X offset of the bottom view in px. */
    private int mCurrentXOffsetPx;

    /** Whether the {@link SceneLayer}is visible. */
    private boolean mIsVisible;

    /** The {@link ViewResourceFrameLayout} that this scene layer represents. */
    private ViewResourceFrameLayout mBottomView;

    /**
     * Build a composited bottom view layer.
     * @param bottomView The view used to generate the composited version.
     * @param topShadowHeightPx The height of the shadow on the top of the view in px if it exists.
     */
    public ScrollingBottomViewSceneLayer(
            ViewResourceFrameLayout bottomView, int topShadowHeightPx) {
        mBottomView = bottomView;
        mResourceId = mBottomView.getId();
        mTopShadowHeightPx = topShadowHeightPx;
        mIsVisible = true;
    }

    /**
     * Build a copy of an existing {@link ScrollingBottomViewSceneLayer}.
     * @param sceneLayer The existing scene layer to copy. This only copies the source view,
     *                   resource ID, and shadow height. All other state is ignored.
     */
    public ScrollingBottomViewSceneLayer(ScrollingBottomViewSceneLayer sceneLayer) {
        this(sceneLayer.mBottomView, sceneLayer.mTopShadowHeightPx);
    }

    /**
     * Set the view's offset from the bottom of the screen in px. An offset of 0 means the view is
     * completely visible. An increasing offset will move the view down.
     * @param offsetPx The view's offset in px.
     */
    public void setYOffset(int offsetPx) {
        mCurrentYOffsetPx = offsetPx;
    }

    /**
     * @param offsetPx The view's X translation in px.
     */
    public void setXOffset(int offsetPx) {
        mCurrentXOffsetPx = offsetPx;
    }

    /**
     * @param visible Whether this {@link SceneLayer} is visible.
     */
    public void setIsVisible(boolean visible) {
        mIsVisible = visible;
    }

    @Override
    protected void initializeNative() {
        if (mNativePtr == 0) {
            mNativePtr =
                    ScrollingBottomViewSceneLayerJni.get().init(ScrollingBottomViewSceneLayer.this);
        }
        assert mNativePtr != 0;
    }

    @Override
    public void setContentTree(SceneLayer contentTree) {
        ScrollingBottomViewSceneLayerJni.get()
                .setContentTree(mNativePtr, ScrollingBottomViewSceneLayer.this, contentTree);
    }

    @Override
    public SceneOverlayLayer getUpdatedSceneOverlayTree(
            RectF viewport, RectF visibleViewport, ResourceManager resourceManager, float yOffset) {
        // The composited shadow should be visible if the Android toolbar's isn't.
        boolean isShadowVisible = mBottomView.getVisibility() != View.VISIBLE;

        ScrollingBottomViewSceneLayerJni.get()
                .updateScrollingBottomViewLayer(
                        mNativePtr,
                        ScrollingBottomViewSceneLayer.this,
                        resourceManager,
                        mResourceId,
                        mTopShadowHeightPx,
                        mCurrentXOffsetPx,
                        viewport.height() + mCurrentYOffsetPx,
                        isShadowVisible);

        return this;
    }

    @Override
    public boolean isSceneOverlayTreeShowing() {
        // If the offset is greater than the toolbar's height, don't draw the layer.
        return mIsVisible && mCurrentYOffsetPx < mBottomView.getHeight() - mTopShadowHeightPx;
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
        long init(ScrollingBottomViewSceneLayer caller);

        void setContentTree(
                long nativeScrollingBottomViewSceneLayer,
                ScrollingBottomViewSceneLayer caller,
                SceneLayer contentTree);

        void updateScrollingBottomViewLayer(
                long nativeScrollingBottomViewSceneLayer,
                ScrollingBottomViewSceneLayer caller,
                ResourceManager resourceManager,
                int viewResourceId,
                int shadowHeightPx,
                float xOffset,
                float yOffset,
                boolean showShadow);
    }
}
