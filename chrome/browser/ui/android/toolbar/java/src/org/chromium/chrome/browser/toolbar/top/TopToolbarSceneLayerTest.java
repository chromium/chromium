// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.top;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyFloat;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Color;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.ContextUtils;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.layouts.CompositorModelChangeProcessor;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayerJni;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.theme.ToolbarThemeColorProvider;
import org.chromium.chrome.browser.toolbar.ToolbarProgressBar;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.resources.ResourceManager;

import java.util.concurrent.atomic.AtomicReference;
import java.util.function.Supplier;

/** Unit tests for {@link TopToolbarSceneLayer}. */
@NullMarked
@RunWith(BaseRobolectricTestRunner.class)
public class TopToolbarSceneLayerTest {
    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private SceneLayer.Natives mSceneLayerNatives;
    @Mock private TopToolbarSceneLayer.Natives mTopToolbarSceneLayerNatives;
    @Mock private ResourceManager mResourceManager;
    @Mock private LayoutManager mLayoutManager;
    @Mock private BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    @Mock private ToolbarThemeColorProvider mToolbarThemeColorProvider;
    @Mock private Tab mTab;

    private final SettableNonNullObservableSupplier<Long> mFrameRequestSupplier =
            ObservableSuppliers.createNonNull(0L);
    private final SettableMonotonicObservableSupplier<Tab> mTabSupplier =
            ObservableSuppliers.createMonotonic();
    private final SettableNonNullObservableSupplier<Integer> mBottomToolbarControlsOffsetSupplier =
            ObservableSuppliers.createNonNull(0);
    private final SettableNonNullObservableSupplier<Boolean> mSuppressToolbarSceneLayerSupplier =
            ObservableSuppliers.createNonNull(false);
    private final SettableMonotonicObservableSupplier<Long> mCaptureResourceIdSupplier =
            ObservableSuppliers.createMonotonic();

    private @Nullable SceneLayer mSceneLayer;
    private Context mContext;
    private ToolbarProgressBar mProgressBar;

    @Before
    public void setUp() {
        mContext = ContextUtils.getApplicationContext();
        mContext.setTheme(R.style.Theme_BrowserUI_DayNight);

        SceneLayerJni.setInstanceForTesting(mSceneLayerNatives);
        TopToolbarSceneLayerJni.setInstanceForTesting(mTopToolbarSceneLayerNatives);
        when(mTopToolbarSceneLayerNatives.init(any()))
                .thenAnswer(
                        invocation -> {
                            mSceneLayer = invocation.getArgument(0);
                            mSceneLayer.setNativePtr(1L);
                            return 1L;
                        });
        doAnswer(
                        invocation -> {
                            if (mSceneLayer != null) {
                                mSceneLayer.setNativePtr(0L);
                                mSceneLayer = null;
                            }
                            return null;
                        })
                .when(mSceneLayerNatives)
                .destroy(anyLong());

        TopToolbarOverlayMediator.setToolbarBackgroundColorForTesting(Color.RED);
        TopToolbarOverlayMediator.setUrlBarColorForTesting(Color.BLUE);
        TopToolbarOverlayMediator.setIsTabletForTesting(false);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        when(mLayoutManager.createCompositorMCP(any(), any(), any()))
                .thenAnswer(
                        invocation -> {
                            PropertyModel model = invocation.getArgument(0);
                            TopToolbarSceneLayer view = invocation.getArgument(1);
                            PropertyModelChangeProcessor.ViewBinder<
                                            PropertyModel,
                                            TopToolbarSceneLayer,
                                            @Nullable PropertyKey>
                                    binder = invocation.getArgument(2);
                            return CompositorModelChangeProcessor.create(
                                    model, view, binder, mFrameRequestSupplier, () -> {}, true);
                        });
        mTabSupplier.set(mTab);
        mProgressBar = new ToolbarProgressBar(mContext, Robolectric.buildAttributeSet().build());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.TOP_TOOLBAR_SCENE_LAYER_SKIP_UPDATE_ON_PROGRESS)
    public void testProgressBarUpdateSkipsUpdateToolbarLayer() {
        AtomicReference<@Nullable ResourceManager> resourceManagerRef = new AtomicReference<>();
        TopToolbarOverlayCoordinator coordinator = createCoordinator(resourceManagerRef::get);

        // Frame 1 with null ResourceManager skips JNI pushes and preserves dirty state.
        mFrameRequestSupplier.set(1L);
        verifyUpdateToolbarLayerCount(0);
        verifyUpdateProgressBarCount(0);

        // Frame 2 flushes preserved dirty state once ResourceManager becomes non-null.
        // Trigger a toolbar-only property change; both layers push because mProgressBarDirty
        // was preserved from frame 1.
        resourceManagerRef.set(mResourceManager);
        coordinator.setYOffset(6.0f);
        mFrameRequestSupplier.set(2L);
        verifyUpdateToolbarLayerCount(1);
        verifyUpdateProgressBarCount(1);

        // Frame 3 updating only PROGRESS_BAR_INFO pushes only updateProgressBar.
        clearInvocations(mTopToolbarSceneLayerNatives);
        mProgressBar.setAlpha(1.0f);
        mFrameRequestSupplier.set(3L);
        verifyUpdateToolbarLayerCount(0);
        verifyUpdateProgressBarCount(1);

        // Frame 4 updating only a toolbar-layer property pushes only updateToolbarLayer.
        clearInvocations(mTopToolbarSceneLayerNatives);
        mCaptureResourceIdSupplier.set(42L);
        mFrameRequestSupplier.set(4L);
        verifyUpdateToolbarLayerCount(1);
        verifyUpdateProgressBarCount(0);

        // Frame 5 updating both pushes both.
        clearInvocations(mTopToolbarSceneLayerNatives);
        mProgressBar.setAlpha(0.0f);
        coordinator.setYOffset(12.0f);
        mFrameRequestSupplier.set(5L);
        verifyUpdateToolbarLayerCount(1);
        verifyUpdateProgressBarCount(1);

        coordinator.destroy();
    }

    @Test
    @DisableFeatures(ChromeFeatureList.TOP_TOOLBAR_SCENE_LAYER_SKIP_UPDATE_ON_PROGRESS)
    public void testFlagDisabledPushesBothOnProgressBarUpdate() {
        TopToolbarOverlayCoordinator coordinator = createCoordinator(() -> mResourceManager);

        mFrameRequestSupplier.set(1L);
        verifyUpdateToolbarLayerCount(1);
        verifyUpdateProgressBarCount(1);

        clearInvocations(mTopToolbarSceneLayerNatives);
        mProgressBar.setAlpha(1.0f);
        mFrameRequestSupplier.set(2L);
        verifyUpdateToolbarLayerCount(1);
        verifyUpdateProgressBarCount(1);

        coordinator.destroy();
    }

    private TopToolbarOverlayCoordinator createCoordinator(
            Supplier<? extends @Nullable ResourceManager> resourceManagerSupplier) {
        return new TopToolbarOverlayCoordinator(
                mContext,
                mLayoutManager,
                info -> {},
                mTabSupplier,
                mBrowserControlsVisibilityManager,
                resourceManagerSupplier,
                mToolbarThemeColorProvider,
                mBottomToolbarControlsOffsetSupplier,
                mSuppressToolbarSceneLayerSupplier,
                LayoutType.BROWSING,
                false,
                mCaptureResourceIdSupplier,
                mProgressBar);
    }

    private void verifyUpdateToolbarLayerCount(int expectedCount) {
        verify(mTopToolbarSceneLayerNatives, times(expectedCount))
                .updateToolbarLayer(
                        anyLong(),
                        any(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyFloat(),
                        anyFloat(),
                        anyFloat(),
                        anyBoolean(),
                        anyBoolean(),
                        anyBoolean(),
                        any());
    }

    private void verifyUpdateProgressBarCount(int expectedCount) {
        verify(mTopToolbarSceneLayerNatives, times(expectedCount))
                .updateProgressBar(
                        anyLong(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyInt(),
                        anyFloat(),
                        anyBoolean(),
                        anyBoolean());
    }
}
