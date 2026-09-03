// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import android.content.Context;
import android.graphics.RectF;

import org.chromium.base.Callback;
import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browser_controls.BrowserControlsOffsetTagsInfo;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.CompositorModelChangeProcessor;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.SceneOverlay;
import org.chromium.chrome.browser.layouts.scene_layer.SceneOverlayLayer;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ToolbarThemeColorProvider;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.ToolbarProgressBar;
import org.chromium.components.browser_ui.widget.ClipDrawableProgressBar;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.resources.ResourceManager;

import java.util.Collections;
import java.util.Set;
import java.util.function.Supplier;

/** The public interface for the top toolbar texture component. */
@NullMarked
public class TopToolbarOverlayCoordinator implements SceneOverlay {
    /** The view state for this overlay. */
    private final PropertyModel mModel;

    /** A handle to the 'view' for this component as the layout manager requires access to it. */
    private final TopToolbarSceneLayer mSceneLayer;

    /** Handles processing updates to the model. */
    private final @Nullable CompositorModelChangeProcessor mChangeProcessor;

    /** Business logic for this overlay. */
    private final TopToolbarOverlayMediator mMediator;

    private final Context mContext;

    public TopToolbarOverlayCoordinator(
            Context context,
            LayoutManager layoutManager,
            Callback<ClipDrawableProgressBar.DrawingInfo> progressInfoCallback,
            NullableObservableSupplier<Tab> tabSupplier,
            BrowserControlsVisibilityManager browserControlsVisibilityManager,
            Supplier<ResourceManager> resourceManagerSupplier,
            ToolbarThemeColorProvider toolbarThemeColorProvider,
            NonNullObservableSupplier<Integer> bottomToolbarControlsOffsetSupplier,
            NonNullObservableSupplier<Boolean> suppressToolbarSceneLayerSupplier,
            int layoutsToShowOn,
            boolean isVisibilityManuallyControlled,
            MonotonicObservableSupplier<Long> captureResourceIdSupplier,
            @Nullable ToolbarProgressBar progressBar) {
        // If BCIV is enabled, we always show the hairline on the composited
        // toolbar, and let renderer+viz control the visibility during scrolls.
        mContext = context;
        mModel =
                new PropertyModel.Builder(TopToolbarOverlayProperties.ALL_KEYS)
                        .with(TopToolbarOverlayProperties.RESOURCE_ID, R.id.control_container)
                        .with(
                                TopToolbarOverlayProperties.URL_BAR_RESOURCE_ID,
                                R.drawable.modern_location_bar)
                        .with(TopToolbarOverlayProperties.VISIBLE, true)
                        .with(TopToolbarOverlayProperties.X_OFFSET, 0)
                        .with(
                                TopToolbarOverlayProperties.LEGACY_CONTENT_OFFSET,
                                browserControlsVisibilityManager.getContentOffset())
                        .with(TopToolbarOverlayProperties.ANONYMIZE, false)
                        .with(TopToolbarOverlayProperties.SHOW_SHADOW, true)
                        .build();
        mSceneLayer = new TopToolbarSceneLayer(resourceManagerSupplier);
        Set<PropertyKey> exclusions =
                ChromeFeatureList.sBottomControlsJankImprovement.isEnabled()
                        ? Set.of(
                                TopToolbarOverlayProperties.Y_OFFSET,
                                TopToolbarOverlayProperties.TOOLBAR_OFFSET_TAG,
                                TopToolbarOverlayProperties.LEGACY_CONTENT_OFFSET)
                        : Collections.emptySet();
        mChangeProcessor =
                layoutManager.createCompositorMCPWithExclusions(
                        mModel, mSceneLayer, TopToolbarSceneLayer::bind, exclusions);

        mMediator =
                new TopToolbarOverlayMediator(
                        mModel,
                        context,
                        layoutManager,
                        progressInfoCallback,
                        tabSupplier,
                        browserControlsVisibilityManager,
                        toolbarThemeColorProvider,
                        bottomToolbarControlsOffsetSupplier,
                        suppressToolbarSceneLayerSupplier,
                        layoutsToShowOn,
                        isVisibilityManuallyControlled,
                        captureResourceIdSupplier,
                        progressBar);
    }

    /**
     * Set whether the android view corresponding with this overlay is showing.
     * @param isVisible Whether the android view is visible.
     */
    public void setIsAndroidViewVisible(boolean isVisible) {
        mMediator.setIsAndroidViewVisible(isVisible);
    }

    /** Sets whether the toolbar hairline should be suppressed. */
    public void onToolbarHairlineSuppressedChanged(boolean suppressed) {
        if (mMediator != null) mMediator.onToolbarHairlineSuppressedChanged(suppressed);
    }

    /**
     * @param visible Whether the overlay and shadow should be visible despite other signals.
     */
    public void setManualVisibility(boolean visible) {
        mMediator.setManualVisibility(visible);
    }

    /** @param xOffset The x offset of the toolbar. */
    public void setXOffset(float xOffset) {
        mMediator.setXOffset(xOffset);
    }

    /** Set the yOffset */
    public void setYOffset(float yOffset) {
        mMediator.setYOffset(yOffset);
    }

    /** Set the offset tag from the current browser controls instance. */
    public void setOffsetTagInfo(@Nullable BrowserControlsOffsetTagsInfo offsetTagInfo) {
        mMediator.updateOffsetTag(offsetTagInfo);
    }

    /**
     * @param anonymize Whether the URL should be hidden when the layer is rendered.
     */
    public void setAnonymize(boolean anonymize) {
        mMediator.setAnonymize(anonymize);
    }

    /**
     * @param bookmarkBarHeightSupplier Supplier of the current Bookmark Bar height.
     */
    public void setBookmarkBarHeightSupplier(
            @Nullable Supplier<Integer> bookmarkBarHeightSupplier) {
        mMediator.setBookmarkBarHeightSupplier(bookmarkBarHeightSupplier);
    }

    /** Clean up this component. */
    public void destroy() {
        if (mChangeProcessor != null) {
            mChangeProcessor.destroy();
        }
        mMediator.destroy();
        mSceneLayer.destroy();
    }

    @Override
    public SceneOverlayLayer getUpdatedSceneOverlayTree(
            RectF viewport, RectF visibleViewport, ResourceManager resourceManager) {
        return mSceneLayer;
    }

    @Override
    public void removeFromParent() {
        mSceneLayer.removeFromParent();
    }

    @Override
    public boolean isSceneOverlayTreeShowing() {
        return mMediator.shouldBeAttachedToTree();
    }

    @Override
    public void onSizeChanged(
            float width, float height, float visibleViewportOffsetY, int orientation) {
        mMediator.setViewportHeight(height * mContext.getResources().getDisplayMetrics().density);
    }
}
