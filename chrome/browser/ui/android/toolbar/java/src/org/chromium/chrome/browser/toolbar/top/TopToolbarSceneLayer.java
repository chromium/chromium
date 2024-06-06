// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.supplier.Supplier;
import org.chromium.cc.input.OffsetTag;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.layouts.scene_layer.SceneOverlayLayer;
import org.chromium.components.browser_ui.widget.ClipDrawableProgressBar.DrawingInfo;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.resources.ResourceManager;

/** A SceneLayer to render the top toolbar. This is the "view" piece of the top toolbar overlay. */
@JNINamespace("android")
class TopToolbarSceneLayer extends SceneOverlayLayer {
    /** Pointer to native TopToolbarSceneLayer. */
    private long mNativePtr;

    /** A means of accessing a {@link ResourceManager} for textures. */
    private final Supplier<ResourceManager> mResourceManagerSupplier;

    /** A simple view binder that pushes the whole model to the view updater. */
    public static void bind(PropertyModel model, TopToolbarSceneLayer view, PropertyKey key) {
        view.pushProperties(model);
    }

    /**
     * @param resourceManagerSupplier Access to a {@link ResourceManager} for textures.
     */
    TopToolbarSceneLayer(Supplier<ResourceManager> resourceManagerSupplier) {
        mResourceManagerSupplier = resourceManagerSupplier;
    }

    /** Push all information about the texture to native at once. */
    private void pushProperties(PropertyModel model) {
        if (mResourceManagerSupplier.get() == null) return;
        TopToolbarSceneLayerJni.get()
                .updateToolbarLayer(
                        mNativePtr,
                        TopToolbarSceneLayer.this,
                        mResourceManagerSupplier.get(),
                        model.get(TopToolbarOverlayProperties.RESOURCE_ID),
                        model.get(TopToolbarOverlayProperties.TOOLBAR_BACKGROUND_COLOR),
                        model.get(TopToolbarOverlayProperties.URL_BAR_RESOURCE_ID),
                        model.get(TopToolbarOverlayProperties.URL_BAR_COLOR),
                        model.get(TopToolbarOverlayProperties.X_OFFSET),
                        model.get(TopToolbarOverlayProperties.CONTENT_OFFSET),
                        model.get(TopToolbarOverlayProperties.SHOW_SHADOW),
                        model.get(TopToolbarOverlayProperties.VISIBLE),
                        model.get(TopToolbarOverlayProperties.ANONYMIZE),
                        model.get(TopToolbarOverlayProperties.TOOLBAR_OFFSET_TAG));

        DrawingInfo progressInfo = model.get(TopToolbarOverlayProperties.PROGRESS_BAR_INFO);
        if (progressInfo == null) return;

        TopToolbarSceneLayerJni.get()
                .updateProgressBar(
                        mNativePtr,
                        TopToolbarSceneLayer.this,
                        progressInfo.progressBarRect.left,
                        progressInfo.progressBarRect.top,
                        progressInfo.progressBarRect.width(),
                        progressInfo.progressBarRect.height(),
                        progressInfo.progressBarColor,
                        progressInfo.progressBarBackgroundRect.left,
                        progressInfo.progressBarBackgroundRect.top,
                        progressInfo.progressBarBackgroundRect.width(),
                        progressInfo.progressBarBackgroundRect.height(),
                        progressInfo.progressBarBackgroundColor);
    }

    @Override
    public void setContentTree(SceneLayer contentTree) {
        TopToolbarSceneLayerJni.get()
                .setContentTree(mNativePtr, TopToolbarSceneLayer.this, contentTree);
    }

    @Override
    protected void initializeNative() {
        if (mNativePtr == 0) {
            mNativePtr = TopToolbarSceneLayerJni.get().init(TopToolbarSceneLayer.this);
        }
        assert mNativePtr != 0;
    }

    @Override
    public void destroy() {
        super.destroy();
        mNativePtr = 0;
    }

    @NativeMethods
    interface Natives {
        long init(TopToolbarSceneLayer caller);

        void setContentTree(
                long nativeTopToolbarSceneLayer,
                TopToolbarSceneLayer caller,
                SceneLayer contentTree);

        void updateToolbarLayer(
                long nativeTopToolbarSceneLayer,
                TopToolbarSceneLayer caller,
                ResourceManager resourceManager,
                int resourceId,
                int toolbarBackgroundColor,
                int urlBarResourceId,
                int urlBarColor,
                float xOffset,
                float contentOffset,
                boolean showShadow,
                boolean visible,
                boolean anonymize,
                OffsetTag offsetTag);

        void updateProgressBar(
                long nativeTopToolbarSceneLayer,
                TopToolbarSceneLayer caller,
                int progressBarX,
                int progressBarY,
                int progressBarWidth,
                int progressBarHeight,
                int progressBarColor,
                int progressBarBackgroundX,
                int progressBarBackgroundY,
                int progressBarBackgroundWidth,
                int progressBarBackgroundHeight,
                int progressBarBackgroundColor);
    }
}
