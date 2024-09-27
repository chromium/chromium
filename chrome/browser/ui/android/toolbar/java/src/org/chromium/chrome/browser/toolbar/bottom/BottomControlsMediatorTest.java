// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;

import static org.chromium.chrome.browser.toolbar.bottom.BottomControlsProperties.ANDROID_VIEW_HEIGHT;
import static org.chromium.chrome.browser.toolbar.bottom.BottomControlsProperties.ANDROID_VIEW_VISIBLE;
import static org.chromium.chrome.browser.toolbar.bottom.BottomControlsProperties.COMPOSITED_VIEW_VISIBLE;

import android.app.Activity;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeControllerImpl;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeSupplier.ChangeObserver;
import org.chromium.ui.InsetObserver;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

/** Unit tests for {@link BottomControlsMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BottomControlsMediatorTest {

    private static final int DEFAULT_HEIGHT = 80;
    private static final int DEFAULT_INSET = 56;
    private static final Insets NAVIGATION_BAR_INSETS = Insets.of(0, 0, 0, 100);
    private static final Insets STATUS_BAR_INSETS = Insets.of(0, 100, 0, 0);

    private static final WindowInsetsCompat SYSTEM_BARS_WINDOW_INSETS =
            new WindowInsetsCompat.Builder()
                    .setInsets(WindowInsetsCompat.Type.navigationBars(), NAVIGATION_BAR_INSETS)
                    .setInsets(WindowInsetsCompat.Type.statusBars(), STATUS_BAR_INSETS)
                    .build();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock BottomControlsStacker mBottomControlsStacker;
    @Mock BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock BrowserStateBrowserControlsVisibilityDelegate mBrowserControlsVisibilityDelegate;
    @Mock LayoutManager mLayoutManager;
    @Mock WindowAndroid mWindowAndroid;
    @Mock TabObscuringHandler mTabObscuringHandler;
    @Mock EdgeToEdgeController mEdgeToEdgeController;
    @Mock FullscreenManager mFullscreenManager;
    @Mock KeyboardVisibilityDelegate mKeyboardDelegate;
    @Mock Supplier<Boolean> mReadAloudRestoringSupplier;
    @Mock InsetObserver mInsetObserver;

    private ObservableSupplierImpl<EdgeToEdgeController> mEdgeToEdgeControllerSupplier;
    private ObservableSupplierImpl<Tab> mTabObservableSupplier = new ObservableSupplierImpl();
    private ObservableSupplierImpl<Boolean> mOverlayPanelVisibilitySupplier =
            new ObservableSupplierImpl();

    private PropertyModel mModel;
    private BottomControlsMediator mMediator;

    @Before
    public void setUp() {
        doReturn(mKeyboardDelegate).when(mWindowAndroid).getKeyboardDelegate();
        doReturn(SYSTEM_BARS_WINDOW_INSETS).when(mInsetObserver).getLastRawWindowInsets();
        doReturn(mInsetObserver).when(mWindowAndroid).getInsetObserver();
        doReturn(mBrowserControlsStateProvider).when(mBottomControlsStacker).getBrowserControls();
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
                        mBottomControlsStacker,
                        mBrowserControlsVisibilityDelegate,
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
                        mBottomControlsStacker,
                        mBrowserControlsVisibilityDelegate,
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
        changeObserver.onToEdgeChange(
                DEFAULT_INSET, /* isDrawingToEdge= */ false, /* isPageOptInToEdge= */ false);
        assertEquals(DEFAULT_HEIGHT, mModel.get(ANDROID_VIEW_HEIGHT));
    }

    @Test
    public void testEdgeToEdge_ToEdge_bottomChinDisabled() {
        ChromeFeatureList.sEdgeToEdgeBottomChin.setForTesting(false);

        ChangeObserver changeObserver = mMediator.getEdgeToEdgeChangeObserverForTesting();
        changeObserver.onToEdgeChange(
                DEFAULT_INSET, /* isDrawingToEdge= */ true, /* isPageOptInToEdge= */ false);
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
                        mBrowserControlsStateProvider,
                        new ObservableSupplierImpl<>(mLayoutManager),
                        mFullscreenManager);
        BottomControlsMediator plainMediator =
                new BottomControlsMediator(
                        mWindowAndroid,
                        mModel,
                        mBottomControlsStacker,
                        mBrowserControlsVisibilityDelegate,
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
                        mBrowserControlsStateProvider,
                        new ObservableSupplierImpl<>(mLayoutManager),
                        mFullscreenManager);
        new BottomControlsMediator(
                mWindowAndroid,
                mModel,
                mBottomControlsStacker,
                mBrowserControlsVisibilityDelegate,
                mFullscreenManager,
                mTabObscuringHandler,
                DEFAULT_HEIGHT,
                mOverlayPanelVisibilitySupplier,
                new ObservableSupplierImpl<>(liveEdgeToEdgeController),
                mReadAloudRestoringSupplier);
        assertNotNull(liveEdgeToEdgeController.getAnyChangeObserverForTesting());
        liveEdgeToEdgeController.setIsOptedIntoEdgeToEdgeForTesting(false);
        int toNormalHeight = mModel.get(ANDROID_VIEW_HEIGHT);
        // Go to a native page which will go ToEdge due to our enabled Feature for this test case.
        mTabObservableSupplier.set(null);
        assertEquals(toNormalHeight, mModel.get(ANDROID_VIEW_HEIGHT));
    }

    @Test
    public void testSetVisibility() {
        // The initial visibility is false, defined in #setup.
        verifyNoInteractions(mBrowserControlsVisibilityDelegate);

        mMediator.setBottomControlsVisible(true);
        assertTrue("Compositor view is not visible.", mModel.get(COMPOSITED_VIEW_VISIBLE));
        assertTrue("Android view is not visible.", mModel.get(ANDROID_VIEW_VISIBLE));
        verify(mBrowserControlsVisibilityDelegate).showControlsTransient();
    }

    @Test
    public void testSetVisibility_SwipeLayout() {
        // The initial visibility is false, defined in #setup.
        verifyNoInteractions(mBrowserControlsVisibilityDelegate);

        mMediator.onStartedShowing(LayoutType.TOOLBAR_SWIPE);
        mMediator.setBottomControlsVisible(true);
        assertTrue("Compositor view is not visible.", mModel.get(COMPOSITED_VIEW_VISIBLE));
        assertFalse(
                "Android view is not visible during toolbar swipe.",
                mModel.get(ANDROID_VIEW_VISIBLE));
        verify(mBrowserControlsVisibilityDelegate).showControlsTransient();
    }

    @Test
    public void testSetVisibility_OverviewPannel() {
        // The initial visibility is false, defined in #setup.
        verifyNoInteractions(mBrowserControlsVisibilityDelegate);

        mOverlayPanelVisibilitySupplier.set(true);
        mMediator.setBottomControlsVisible(true);
        assertTrue("Compositor view is not visible.", mModel.get(COMPOSITED_VIEW_VISIBLE));
        assertFalse(
                "Android view is not visible during overlay panel.",
                mModel.get(ANDROID_VIEW_VISIBLE));
        verify(mBrowserControlsVisibilityDelegate).showControlsTransient();
    }
}
