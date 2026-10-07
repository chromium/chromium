// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeTopScalpProperties.IS_VISIBLE;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeTopScalpProperties.OFFSET_TAG;
import static org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeTopScalpProperties.Y_OFFSET;

import android.os.Build;
import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browser_controls.TopControlsStacker;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.layouts.CompositorModelChangeProcessor;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.ui.modelutil.PropertyKey;

import java.util.Set;

// EdgeToEdgeUtils.isTopEdgeToEdgeEnabled() requires Android R+ (API 30).
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = Build.VERSION_CODES.R)
@EnableFeatures(ChromeFeatureList.EDGE_TO_EDGE_TOP_INSET)
public class EdgeToEdgeTopScalpCoordinatorTest {
    private static final Set<PropertyKey> EXPECTED_EXCLUSIONS =
            Set.of(Y_OFFSET, OFFSET_TAG, IS_VISIBLE);

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    private View mView;
    @Mock private LayoutManager mLayoutManager;
    @Mock private TopControlsStacker mTopControlsStacker;
    @Mock private EdgeToEdgeController mEdgeToEdgeController;
    @Mock private EdgeToEdgeTopScalpSceneLayer mSceneLayer;
    @Mock private FullscreenManager mFullscreenManager;
    @Mock private TopUiThemeColorProvider mTopUiThemeColorProvider;
    @Mock private CompositorModelChangeProcessor<EdgeToEdgeTopScalpSceneLayer> mCompositorMCP;

    @Before
    public void setUp() {
        when(mLayoutManager.createCompositorMCPWithExclusions(
                        any(), eq(mSceneLayer), any(), eq(EXPECTED_EXCLUSIONS)))
                .thenReturn(mCompositorMCP);
        when(mEdgeToEdgeController.isDrawingToTopEdge()).thenReturn(true);
        when(mEdgeToEdgeController.getSystemTopInsetPx()).thenReturn(100);
    }

    @Test
    public void testEdgeToEdgeTopScalpCoordinator() {
        mView = new View(ContextUtils.getApplicationContext());
        EdgeToEdgeTopScalpCoordinator coordinator =
                new EdgeToEdgeTopScalpCoordinator(
                        mView,
                        mLayoutManager,
                        mTopControlsStacker,
                        mEdgeToEdgeController,
                        mSceneLayer,
                        mFullscreenManager,
                        mTopUiThemeColorProvider);

        assertEquals(View.VISIBLE, mView.getVisibility());
        verify(mSceneLayer).setHeight(100);
        verify(mSceneLayer).setIsVisible(true);
        verify(mLayoutManager)
                .createCompositorMCPWithExclusions(
                        any(), eq(mSceneLayer), any(), eq(EXPECTED_EXCLUSIONS));
        verify(mLayoutManager).addSceneOverlay(eq(mSceneLayer));
        verify(mTopControlsStacker).addControl(any());
        verify(mEdgeToEdgeController).registerObserver(any());
        verify(mLayoutManager).addObserver(any());
        verify(mFullscreenManager).addObserver(any());
        verify(mTopUiThemeColorProvider).addThemeColorObserver(any());
        clearInvocations(mSceneLayer);

        coordinator.destroy();
        verify(mTopControlsStacker).removeControl(any());
        verify(mEdgeToEdgeController).unregisterObserver(any());
        verify(mLayoutManager).removeObserver(any());
        verify(mFullscreenManager).removeObserver(any());
        verify(mTopUiThemeColorProvider).removeThemeColorObserver(any());
        verify(mCompositorMCP).destroy();
        verify(mSceneLayer).destroy();
    }

    @Test
    public void testEdgeToEdgeTopScalpCoordinator_NullView() {
        EdgeToEdgeTopScalpCoordinator coordinator =
                new EdgeToEdgeTopScalpCoordinator(
                        /* scalpView= */ null,
                        mLayoutManager,
                        mTopControlsStacker,
                        mEdgeToEdgeController,
                        mSceneLayer,
                        mFullscreenManager,
                        mTopUiThemeColorProvider);

        verify(mSceneLayer).setHeight(100);
        verify(mSceneLayer).setIsVisible(true);
        verify(mLayoutManager).addSceneOverlay(eq(mSceneLayer));
        verify(mTopControlsStacker).addControl(any());
        verify(mEdgeToEdgeController).registerObserver(any());
        verify(mLayoutManager).addObserver(any());
        verify(mFullscreenManager).addObserver(any());
        verify(mTopUiThemeColorProvider).addThemeColorObserver(any());
        clearInvocations(mSceneLayer);

        coordinator.destroy();
        verify(mTopControlsStacker).removeControl(any());
        verify(mEdgeToEdgeController).unregisterObserver(any());
        verify(mLayoutManager).removeObserver(any());
        verify(mFullscreenManager).removeObserver(any());
        verify(mTopUiThemeColorProvider).removeThemeColorObserver(any());
        verify(mCompositorMCP).destroy();
        verify(mSceneLayer).destroy();
    }
}
