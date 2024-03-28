// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.toolbar.bottom.BottomControlsProperties.ANDROID_VIEW_HEIGHT;

import android.app.Activity;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeControllerImpl;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeSupplier.ChangeObserver;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link BottomControlsMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@DisableFeatures({ChromeFeatureList.TOTALLY_EDGE_TO_EDGE})
public class BottomControlsMediatorTest {
    @Rule public TestRule mProcessor = new Features.JUnitProcessor();

    private static final int DEFAULT_HEIGHT = 80;
    private static final int DEFAULT_INSET = 56;
    @Mock BrowserControlsSizer mBrowserControlsSizer;
    @Mock BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock WindowAndroid mWindowAndroid;
    @Mock TabObscuringHandler mTabObscuringHandler;
    @Mock ObservableSupplier<Boolean> mOverlayPanelVisibilitySupplier;
    @Mock EdgeToEdgeController mEdgeToEdgeController;
    @Mock FullscreenManager mFullscreenManager;
    @Mock KeyboardVisibilityDelegate mKeyboardDelegate;
    @Mock Supplier<Boolean> mReadAloudRestoringSupplier;

    private ObservableSupplierImpl<EdgeToEdgeController> mEdgeToEdgeControllerSupplier;
    private ObservableSupplierImpl<Tab> mTabObservableSupplier = new ObservableSupplierImpl();

    private PropertyModel mModel;
    private BottomControlsMediator mMediator;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(mKeyboardDelegate).when(mWindowAndroid).getKeyboardDelegate();
        mModel =
                new PropertyModel.Builder(BottomControlsProperties.ALL_KEYS)
                        .with(BottomControlsProperties.ANDROID_VIEW_VISIBLE, false)
                        .with(BottomControlsProperties.COMPOSITED_VIEW_VISIBLE, false)
                        .build();
        mEdgeToEdgeControllerSupplier = new ObservableSupplierImpl<>(mEdgeToEdgeController);
        mMediator =
                new BottomControlsMediator(
                        mWindowAndroid,
                        mModel,
                        mBrowserControlsSizer,
                        mFullscreenManager,
                        mTabObscuringHandler,
                        DEFAULT_HEIGHT,
                        mOverlayPanelVisibilitySupplier,
                        mEdgeToEdgeControllerSupplier,
                        mReadAloudRestoringSupplier);
    }

    @Test
    public void testNoEdgeToEdge() {
        BottomControlsMediator plainMediator =
                new BottomControlsMediator(
                        mWindowAndroid,
                        mModel,
                        mBrowserControlsSizer,
                        mFullscreenManager,
                        mTabObscuringHandler,
                        DEFAULT_HEIGHT,
                        mOverlayPanelVisibilitySupplier,
                        new ObservableSupplierImpl<>(null),
                        mReadAloudRestoringSupplier);
        assertNull(plainMediator.getEdgeToEdgeChangeObserverForTesting());
    }

    @Test
    public void testEdgeToEdge_simple() {
        assertNotNull(mMediator.getEdgeToEdgeChangeObserverForTesting());
        verify(mEdgeToEdgeController).registerObserver(any());
    }

    @Test
    public void testEdgeToEdge_ToNormal() {
        ChangeObserver changeObserver = mMediator.getEdgeToEdgeChangeObserverForTesting();
        changeObserver.onToEdgeChange(0);
        assertEquals(DEFAULT_HEIGHT, mModel.get(ANDROID_VIEW_HEIGHT));
    }

    @Test
    public void testEdgeToEdge_ToEdge() {
        ChangeObserver changeObserver = mMediator.getEdgeToEdgeChangeObserverForTesting();
        changeObserver.onToEdgeChange(DEFAULT_INSET);
        assertEquals(DEFAULT_HEIGHT + DEFAULT_INSET, mModel.get(ANDROID_VIEW_HEIGHT));
    }

    @Test
    public void testEdgeToEdge_ObserverDestroyed() {
        // Set up a mediator with a live EdgeToEdgeController.
        Activity activity = Robolectric.buildActivity(TestActivity.class).setup().get();
        EdgeToEdgeControllerImpl liveEdgeToEdgeController =
                new EdgeToEdgeControllerImpl(
                        activity,
                        mWindowAndroid,
                        mTabObservableSupplier,
                        null,
                        mBrowserControlsStateProvider);
        BottomControlsMediator plainMediator =
                new BottomControlsMediator(
                        mWindowAndroid,
                        mModel,
                        mBrowserControlsSizer,
                        mFullscreenManager,
                        mTabObscuringHandler,
                        DEFAULT_HEIGHT,
                        mOverlayPanelVisibilitySupplier,
                        new ObservableSupplierImpl<>(liveEdgeToEdgeController),
                        mReadAloudRestoringSupplier);
        assertNotNull(liveEdgeToEdgeController.getAnyChangeObserverForTesting());
        plainMediator.destroy();
        assertNull(liveEdgeToEdgeController.getAnyChangeObserverForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.DRAW_NATIVE_EDGE_TO_EDGE)
    public void testEdgeToEdge_ObserverCalled() {
        // Set up a mediator with a live EdgeToEdgeController.
        Activity activity = Robolectric.buildActivity(TestActivity.class).setup().get();
        EdgeToEdgeControllerImpl liveEdgeToEdgeController =
                new EdgeToEdgeControllerImpl(
                        activity,
                        mWindowAndroid,
                        mTabObservableSupplier,
                        null,
                        mBrowserControlsStateProvider);
        new BottomControlsMediator(
                mWindowAndroid,
                mModel,
                mBrowserControlsSizer,
                mFullscreenManager,
                mTabObscuringHandler,
                DEFAULT_HEIGHT,
                mOverlayPanelVisibilitySupplier,
                new ObservableSupplierImpl<>(liveEdgeToEdgeController),
                mReadAloudRestoringSupplier);
        assertNotNull(liveEdgeToEdgeController.getAnyChangeObserverForTesting());
        liveEdgeToEdgeController.setToEdgeForTesting(false);
        int toNormalHeight = mModel.get(ANDROID_VIEW_HEIGHT);
        // Go to a native page which will go ToEdge due to our enabled Feature for this test case.
        mTabObservableSupplier.set(null);
        assertEquals(toNormalHeight, mModel.get(ANDROID_VIEW_HEIGHT));
    }
}
