// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import android.annotation.SuppressLint;
import android.view.View;
import android.view.ViewGroup;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.supplier.SupplierUtils;
import org.chromium.base.supplier.TransitiveObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.toolbar.R;
import org.chromium.chrome.browser.toolbar.bottom.BottomControlsViewBinder.ViewHolder;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.resources.ResourceManager;
import org.chromium.ui.resources.dynamics.ViewResourceAdapter;
import org.chromium.ui.widget.Toast;

import java.util.HashSet;
import java.util.Set;

/**
 * The root coordinator for the bottom controls component. This component is intended for use with
 * bottom UI that re-sizes the web contents, scrolls off-screen, and hides when the keyboard is
 * shown. This class has two primary components, an Android view and a composited texture that draws
 * when the controls are being scrolled off-screen. The Android version does not draw unless the
 * controls offset is 0.
 */
@NullMarked
public class BottomControlsCoordinator implements BackPressHandler {
    /** Interface for the BottomControls component to hide and show itself. */
    public interface BottomControlsVisibilityController {
        void setBottomControlsVisible(boolean isVisible);
    }

    /** The mediator that handles events from outside the bottom controls. */
    private final BottomControlsMediator mMediator;

    /** The Delegate for the split toolbar's bottom toolbar component UI operation. */
    private final OneshotSupplier<BottomControlsContentDelegate> mContentDelegateSupplier;

    private final OneshotSupplierImpl<Boolean> mNativeInitializedSupplier =
            new OneshotSupplierImpl<>();

    private final ObservableSupplierImpl<BottomControlsContentDelegate> mContentDelegateWrapper =
            new ObservableSupplierImpl<>();
    private final TransitiveObservableSupplier<BottomControlsContentDelegate, Boolean>
            mHandleBackPressChangedSupplier =
                    new TransitiveObservableSupplier<>(
                            mContentDelegateWrapper, cd -> cd.getHandleBackPressChangedSupplier());

    private final ScrollingBottomViewResourceFrameLayout mRootFrameLayout;
    private final ScrollingBottomViewSceneLayer mSceneLayer;

    private boolean mIsDestroyed;

    /**
     * Build the coordinator that manages the bottom controls.
     *
     * @param windowAndroid A {@link WindowAndroid} for watching keyboard visibility events.
     * @param layoutManager A {@link LayoutManager} to attach overlays to.
     * @param resourceManager A {@link ResourceManager} for loading textures into the compositor.
     * @param controlsStacker A {@link BottomControlsStacker} to update the bottom controls.
     * @param fullscreenManager A {@link FullscreenManager} to listen for fullscreen changes.
     * @param edgeToEdgeControllerSupplier A supplier to control drawing to the edge of the screen.
     * @param root The parent {@link ViewGroup} for the bottom controls.
     * @param contentDelegateSupplier Supplier of delegate for bottom controls UI operations.
     * @param tabObscuringHandler Delegate object handling obscuring views.
     * @param overlayPanelVisibilitySupplier Notifies overlay panel visibility event.
     * @param constraintsSupplier Used to access current constraints of the browser controls.
     * @param readAloudRestoringSupplier Supplier that returns true if Read Aloud is currently
     *     restoring its player, e.g. after theme change.
     */
    @SuppressLint("CutPasteId") // Not actually cut and paste since it's View vs ViewGroup.
    public BottomControlsCoordinator(
            WindowAndroid windowAndroid,
            LayoutManager layoutManager,
            ResourceManager resourceManager,
            BottomControlsStacker controlsStacker,
            BrowserStateBrowserControlsVisibilityDelegate browserControlsVisibilityDelegate,
            FullscreenManager fullscreenManager,
            ObservableSupplier<EdgeToEdgeController> edgeToEdgeControllerSupplier,
            ScrollingBottomViewResourceFrameLayout root,
            OneshotSupplier<BottomControlsContentDelegate> contentDelegateSupplier,
            TabObscuringHandler tabObscuringHandler,
            ObservableSupplier<Boolean> overlayPanelVisibilitySupplier,
            ObservableSupplier<Integer> constraintsSupplier,
            Supplier<Boolean> readAloudRestoringSupplier) {
        mRootFrameLayout = root;
        root.setConstraintsSupplier(constraintsSupplier);
        PropertyModel model = new PropertyModel(BottomControlsProperties.ALL_KEYS);

        mSceneLayer = new ScrollingBottomViewSceneLayer(root, root.getTopShadowHeight());
        PropertyModelChangeProcessor.create(
                model, new ViewHolder(root, mSceneLayer), BottomControlsViewBinder::bind);
        if (ChromeFeatureList.sBcivBottomControls.isEnabled()) {
            Set<PropertyKey> exclusions = new HashSet();
            exclusions.add(BottomControlsProperties.ANDROID_VIEW_VISIBLE);
            layoutManager.createCompositorMCPWithExclusions(
                    model, mSceneLayer, BottomControlsViewBinder::bindCompositorMCP, exclusions);
        } else {
            layoutManager.createCompositorMCP(
                    model, mSceneLayer, BottomControlsViewBinder::bindCompositorMCP);
        }
        int bottomControlsHeightId = R.dimen.bottom_controls_height;

        View container = root.findViewById(R.id.bottom_container_slot);
        ViewGroup.LayoutParams params = container.getLayoutParams();

        int bottomControlsHeightRes =
                root.getResources().getDimensionPixelOffset(bottomControlsHeightId);
        params.height = bottomControlsHeightRes;

        mMediator =
                new BottomControlsMediator(
                        windowAndroid,
                        model,
                        controlsStacker,
                        browserControlsVisibilityDelegate,
                        fullscreenManager,
                        tabObscuringHandler,
                        bottomControlsHeightRes,
                        root.getTopShadowHeight(),
                        overlayPanelVisibilitySupplier,
                        edgeToEdgeControllerSupplier,
                        readAloudRestoringSupplier);
        resourceManager
                .getDynamicResourceLoader()
                .registerResource(root.getId(), root.getResourceAdapter());

        mContentDelegateSupplier = contentDelegateSupplier;
        Toast.setGlobalExtraYOffset(
                root.getResources().getDimensionPixelSize(bottomControlsHeightId));

        // Set the visibility of BottomControls to false by default. Components within
        // BottomControls should update the visibility explicitly if needed.
        setBottomControlsVisible(false);

        mSceneLayer.setIsVisible(mMediator.isCompositedViewVisible());
        layoutManager.addSceneOverlay(mSceneLayer);

        SupplierUtils.waitForAll(
                () -> {
                    if (mIsDestroyed) return;

                    BottomControlsContentDelegate contentDelegate = mContentDelegateSupplier.get();
                    assert contentDelegate != null;

                    contentDelegate.initializeWithNative(
                            new BottomControlsVisibilityController() {
                                @Override
                                public void setBottomControlsVisible(boolean isVisible) {
                                    mMediator.setBottomControlsVisible(isVisible);
                                }
                            },
                            root::onModelTokenChange);
                    mContentDelegateWrapper.set(contentDelegate);
                },
                mContentDelegateSupplier,
                mNativeInitializedSupplier);
    }

    /**
     * @param layoutStateProvider {@link LayoutStateProvider} object.
     */
    public void setLayoutStateProvider(LayoutStateProvider layoutStateProvider) {
        mMediator.setLayoutStateProvider(layoutStateProvider);
    }

    /**
     * @param isVisible Whether the bottom control is visible.
     */
    public void setBottomControlsVisible(boolean isVisible) {
        mMediator.setBottomControlsVisible(isVisible);
    }

    /**
     * Handles system back press action if needed.
     *
     * @return Whether or not the back press event is consumed here.
     */
    public boolean onBackPressed() {
        return mContentDelegateSupplier.hasValue()
                ? mContentDelegateSupplier.get().onBackPressed()
                : false;
    }

    @Override
    public @BackPressResult int handleBackPress() {
        return mContentDelegateSupplier.hasValue()
                ? mContentDelegateSupplier.get().handleBackPress()
                : BackPressResult.FAILURE;
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mHandleBackPressChangedSupplier;
    }

    /** Initializes any native dependencies. */
    public void initializeWithNative() {
        mNativeInitializedSupplier.set(true);
    }

    /** Clean up any state when the bottom controls component is destroyed. */
    public void destroy() {
        mIsDestroyed = true;

        if (mContentDelegateSupplier.hasValue()) mContentDelegateSupplier.get().destroy();
        mMediator.destroy();
    }

    public void simulateEdgeToEdgeChangeForTesting(
            int bottomInset, boolean isDrawingToEdge, boolean isPageOptedIntoEdgeToEdge) {
        mMediator.simulateEdgeToEdgeChangeForTesting( // IN-TEST
                bottomInset, isDrawingToEdge, isPageOptedIntoEdgeToEdge); // IN-TEST
    }

    public ScrollingBottomViewSceneLayer getSceneLayerForTesting() {
        return mSceneLayer;
    }

    public ViewResourceAdapter getResourceAdapterForTesting() {
        return mRootFrameLayout.getResourceAdapter();
    }
}
