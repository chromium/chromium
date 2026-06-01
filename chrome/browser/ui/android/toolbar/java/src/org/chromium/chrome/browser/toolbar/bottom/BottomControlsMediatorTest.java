// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static org.chromium.chrome.browser.toolbar.bottom.BottomControlsProperties.ANDROID_VIEW_HEIGHT_NO_PADDING;
import static org.chromium.chrome.browser.toolbar.bottom.BottomControlsProperties.ANDROID_VIEW_VISIBLE;
import static org.chromium.chrome.browser.toolbar.bottom.BottomControlsProperties.BOTTOM_PADDING;
import static org.chromium.chrome.browser.toolbar.bottom.BottomControlsProperties.COMPOSITED_VIEW_VISIBLE;

import android.app.Activity;

import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.SettableMonotonicObservableSupplier;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.cc.input.OffsetTag;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker;
import org.chromium.chrome.browser.browser_controls.BottomControlsStacker.LayerType;
import org.chromium.chrome.browser.browser_controls.BrowserControlsOffsetTagsInfo;
import org.chromium.chrome.browser.browser_controls.BrowserControlsVisibilityManager;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.overlay_panel.PanelState;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObscuringHandler;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeControllerImpl;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.edge_to_edge.EdgeToEdgeManager;
import org.chromium.ui.edge_to_edge.EdgeToEdgeStateProvider;
import org.chromium.ui.edge_to_edge.EdgeToEdgeSupplier.ChangeObserver;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.function.Supplier;

/** Unit tests for {@link BottomControlsMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BottomControlsMediatorTest {

    private static final int DEFAULT_HEIGHT = 80;
    private static final int DEFAULT_SHADOW_HEIGHT = 10;
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
    @Mock BrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    @Mock LayoutManager mLayoutManager;
    @Mock WindowAndroid mWindowAndroid;
    @Mock TabObscuringHandler mTabObscuringHandler;
    @Mock EdgeToEdgeController mEdgeToEdgeController;
    @Mock FullscreenManager mFullscreenManager;
    @Mock KeyboardVisibilityDelegate mKeyboardDelegate;
    @Mock Supplier<Boolean> mReadAloudRestoringSupplier;
    @Mock InsetObserver mInsetObserver;
    @Mock EdgeToEdgeStateProvider mEdgeToEdgeStateProvider;
    @Mock EdgeToEdgeManager mEdgeToEdgeManager;

    private BrowserStateBrowserControlsVisibilityDelegate mBrowserControlsVisibilityDelegate;
    private SettableMonotonicObservableSupplier<EdgeToEdgeController> mEdgeToEdgeControllerSupplier;
    private final SettableNullableObservableSupplier<Tab> mTabObservableSupplier =
            ObservableSuppliers.createNullable();
    private final SettableNonNullObservableSupplier<@PanelState Integer>
            mOverlayPanelStateSupplier = ObservableSuppliers.createNonNull(PanelState.CLOSED);
    private final OneshotSupplierImpl<BottomControlsContentDelegate> mContentDelegateSupplier =
            new OneshotSupplierImpl<>();

    private PropertyModel mModel;
    private BottomControlsMediator mMediator;

    @Before
    public void setUp() {
        doReturn(mKeyboardDelegate).when(mWindowAndroid).getKeyboardDelegate();
        doReturn(SYSTEM_BARS_WINDOW_INSETS).when(mInsetObserver).getLastRawWindowInsets();
        doReturn(mInsetObserver).when(mWindowAndroid).getInsetObserver();
        doReturn(mBrowserControlsVisibilityManager)
                .when(mBottomControlsStacker)
                .getBrowserControls();
        doReturn(mEdgeToEdgeStateProvider).when(mEdgeToEdgeManager).getEdgeToEdgeStateProvider();
        mBrowserControlsVisibilityDelegate =
                new BrowserStateBrowserControlsVisibilityDelegate(
                        ObservableSuppliers.alwaysFalse());
        mModel =
                new PropertyModel.Builder(BottomControlsProperties.ALL_KEYS)
                        .with(BottomControlsProperties.ANDROID_VIEW_VISIBLE, false)
                        .with(BottomControlsProperties.COMPOSITED_VIEW_VISIBLE, false)
                        .build();
        mEdgeToEdgeControllerSupplier = ObservableSuppliers.createMonotonic(mEdgeToEdgeController);
        mMediator =
                new BottomControlsMediator(
                        mWindowAndroid,
                        mModel,
                        mBottomControlsStacker,
                        mBrowserControlsVisibilityDelegate,
                        mFullscreenManager,
                        LayerType.TABSTRIP_TOOLBAR,
                        mContentDelegateSupplier,
                        mTabObscuringHandler,
                        DEFAULT_HEIGHT,
                        DEFAULT_SHADOW_HEIGHT,
                        mOverlayPanelStateSupplier,
                        mEdgeToEdgeControllerSupplier,
                        mTabObservableSupplier,
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
                        LayerType.TABSTRIP_TOOLBAR,
                        mContentDelegateSupplier,
                        mTabObscuringHandler,
                        DEFAULT_HEIGHT,
                        DEFAULT_SHADOW_HEIGHT,
                        mOverlayPanelStateSupplier,
                        ObservableSuppliers.alwaysNull(),
                        mTabObservableSupplier,
                        mReadAloudRestoringSupplier);
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
        assertEquals(DEFAULT_HEIGHT, mModel.get(ANDROID_VIEW_HEIGHT_NO_PADDING));
        assertEquals(0, mModel.get(BOTTOM_PADDING));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_BAR})
    public void testEdgeToEdge_NtpOnly() {
        Tab tab = Mockito.mock(Tab.class);
        Activity activity = Robolectric.buildActivity(TestActivity.class).setup().get();
        when(mWindowAndroid.getContext()).thenReturn(new java.lang.ref.WeakReference<>(activity));

        when(tab.isIncognito()).thenReturn(false);
        NativePage ntp = Mockito.mock(NativePage.class);
        when(ntp.getHost()).thenReturn("newtab");
        when(tab.getNativePage()).thenReturn(ntp);

        mTabObservableSupplier.set(tab);

        when(mEdgeToEdgeController.isDrawingToEdge()).thenReturn(true);
        when(mEdgeToEdgeController.getBottomInsetPx()).thenReturn(DEFAULT_INSET);

        ChangeObserver changeObserver = mMediator.getEdgeToEdgeChangeObserverForTesting();
        changeObserver.onToEdgeChange(
                DEFAULT_INSET, /* isDrawingToEdge= */ true, /* isPageOptInToEdge= */ false);

        assertEquals(DEFAULT_HEIGHT, mModel.get(ANDROID_VIEW_HEIGHT_NO_PADDING));
        assertEquals(0, mModel.get(BOTTOM_PADDING));

        // Transition to standard web page
        Tab webTab = Mockito.mock(Tab.class);
        when(webTab.isIncognito()).thenReturn(false);
        when(webTab.getNativePage()).thenReturn(null);
        mTabObservableSupplier.set(webTab);

        assertEquals(DEFAULT_HEIGHT, mModel.get(ANDROID_VIEW_HEIGHT_NO_PADDING));
        assertEquals(0, mModel.get(BOTTOM_PADDING));

        // Transition to Incognito NTP
        Tab incognitoNtpTab = Mockito.mock(Tab.class);
        when(incognitoNtpTab.isIncognito()).thenReturn(true);
        when(incognitoNtpTab.getNativePage()).thenReturn(ntp);
        mTabObservableSupplier.set(incognitoNtpTab);

        assertEquals(DEFAULT_HEIGHT, mModel.get(ANDROID_VIEW_HEIGHT_NO_PADDING));
        assertEquals(0, mModel.get(BOTTOM_PADDING));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_BAR + ":disable_on_ntp/false"})
    public void testEdgeToEdge_NtpYTranslation() {
        Tab tab = Mockito.mock(Tab.class);
        Activity activity = Robolectric.buildActivity(TestActivity.class).setup().get();
        when(mWindowAndroid.getContext()).thenReturn(new java.lang.ref.WeakReference<>(activity));

        when(tab.isIncognito()).thenReturn(false);
        NativePage ntp = Mockito.mock(NativePage.class);
        when(ntp.getHost()).thenReturn("newtab");
        when(tab.getNativePage()).thenReturn(ntp);

        mTabObservableSupplier.set(tab);

        when(mEdgeToEdgeController.isDrawingToEdge()).thenReturn(true);
        when(mEdgeToEdgeController.getBottomInsetPx()).thenReturn(DEFAULT_INSET);

        // 1. Test TABSTRIP_TOOLBAR (default mMediator in setUp)
        mMediator.setBottomControlsVisible(true);
        mMediator.onBrowserControlsOffsetUpdate(-DEFAULT_INSET); // Y_OFFSET = -56
        // At rest (bottomControlOffset = 0), it should translate to Y_OFFSET + 0 = -56
        assertEquals(-DEFAULT_INSET, mModel.get(BottomControlsProperties.ANDROID_VIEW_TRANSLATE_Y));

        // 2. Test BOTTOM_APP_BAR
        BottomControlsMediator bottomAppBarMediator =
                new BottomControlsMediator(
                        mWindowAndroid,
                        mModel,
                        mBottomControlsStacker,
                        mBrowserControlsVisibilityDelegate,
                        mFullscreenManager,
                        LayerType.BOTTOM_APP_BAR,
                        mContentDelegateSupplier,
                        mTabObscuringHandler,
                        DEFAULT_HEIGHT,
                        DEFAULT_SHADOW_HEIGHT,
                        mOverlayPanelStateSupplier,
                        mEdgeToEdgeControllerSupplier,
                        mTabObservableSupplier,
                        mReadAloudRestoringSupplier);

        bottomAppBarMediator.setBottomControlsVisible(true);
        // At rest (bottomControlOffset = 0), it should translate to 0 (padding handles shift)
        bottomAppBarMediator.onBrowserControlsOffsetUpdate(-DEFAULT_INSET);
        assertEquals(0, mModel.get(BottomControlsProperties.ANDROID_VIEW_TRANSLATE_Y));

        // During scroll-off (bottomControlOffset = 30), it should translate to 30
        doReturn(30).when(mBrowserControlsVisibilityManager).getBottomControlOffset();
        doReturn(true).when(mBrowserControlsVisibilityManager).offsetOverridden();
        bottomAppBarMediator.onBrowserControlsOffsetUpdate(0);
        assertEquals(30, mModel.get(BottomControlsProperties.ANDROID_VIEW_TRANSLATE_Y));
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
                        mEdgeToEdgeManager,
                        mBrowserControlsVisibilityManager,
                        ObservableSuppliers.createNonNull(mLayoutManager),
                        mFullscreenManager);
        BottomControlsMediator plainMediator =
                new BottomControlsMediator(
                        mWindowAndroid,
                        mModel,
                        mBottomControlsStacker,
                        mBrowserControlsVisibilityDelegate,
                        mFullscreenManager,
                        LayerType.TABSTRIP_TOOLBAR,
                        mContentDelegateSupplier,
                        mTabObscuringHandler,
                        DEFAULT_HEIGHT,
                        DEFAULT_SHADOW_HEIGHT,
                        mOverlayPanelStateSupplier,
                        ObservableSuppliers.createNonNull(liveEdgeToEdgeController),
                        mTabObservableSupplier,
                        mReadAloudRestoringSupplier);
        assertNotNull(liveEdgeToEdgeController.getAnyChangeObserverForTesting());
        plainMediator.destroy();
        assertNull(liveEdgeToEdgeController.getAnyChangeObserverForTesting());
    }

    @Test
    public void testEdgeToEdge_ObserverCalled() {
        // Set up a mediator with a live EdgeToEdgeController.
        Activity activity = Robolectric.buildActivity(TestActivity.class).setup().get();
        EdgeToEdgeControllerImpl liveEdgeToEdgeController =
                new EdgeToEdgeControllerImpl(
                        activity,
                        mWindowAndroid,
                        mTabObservableSupplier,
                        null,
                        mEdgeToEdgeManager,
                        mBrowserControlsVisibilityManager,
                        ObservableSuppliers.createNonNull(mLayoutManager),
                        mFullscreenManager);
        new BottomControlsMediator(
                mWindowAndroid,
                mModel,
                mBottomControlsStacker,
                mBrowserControlsVisibilityDelegate,
                mFullscreenManager,
                LayerType.TABSTRIP_TOOLBAR,
                mContentDelegateSupplier,
                mTabObscuringHandler,
                DEFAULT_HEIGHT,
                DEFAULT_SHADOW_HEIGHT,
                mOverlayPanelStateSupplier,
                ObservableSuppliers.createNonNull(liveEdgeToEdgeController),
                mTabObservableSupplier,
                mReadAloudRestoringSupplier);
        assertNotNull(liveEdgeToEdgeController.getAnyChangeObserverForTesting());
        liveEdgeToEdgeController.setIsOptedIntoEdgeToEdgeForTesting(false);
        int toNormalHeight = mModel.get(ANDROID_VIEW_HEIGHT_NO_PADDING);
        // Go to a native page which will go ToEdge due to our enabled Feature for this test case.
        mTabObservableSupplier.set(null);
        assertEquals(toNormalHeight, mModel.get(ANDROID_VIEW_HEIGHT_NO_PADDING));
    }

    @Test
    public void testSetVisibility() {
        // The initial visibility is false, defined in #setup.
        assertThat(mBrowserControlsVisibilityDelegate.get()).isEqualTo(BrowserControlsState.BOTH);

        mMediator.setBottomControlsVisible(true);
        assertTrue("Compositor view is not visible.", mModel.get(COMPOSITED_VIEW_VISIBLE));
        assertTrue("Android view is not visible.", mModel.get(ANDROID_VIEW_VISIBLE));
        assertThat(mBrowserControlsVisibilityDelegate.get()).isEqualTo(BrowserControlsState.SHOWN);
    }

    @Test
    public void testSetVisibility_SwipeLayout() {
        // The initial visibility is false, defined in #setup.
        assertThat(mBrowserControlsVisibilityDelegate.get()).isEqualTo(BrowserControlsState.BOTH);

        mMediator.onStartedShowing(LayoutType.TOOLBAR_SWIPE);
        mMediator.setBottomControlsVisible(true);
        assertTrue("Compositor view is not visible.", mModel.get(COMPOSITED_VIEW_VISIBLE));
        assertFalse(
                "Android view is not visible during toolbar swipe.",
                mModel.get(ANDROID_VIEW_VISIBLE));
        assertThat(mBrowserControlsVisibilityDelegate.get()).isEqualTo(BrowserControlsState.SHOWN);
    }

    @Test
    public void testSetVisibility_OverviewPanelExpanded() {
        // The initial visibility is false, defined in #setup.
        assertThat(mBrowserControlsVisibilityDelegate.get()).isEqualTo(BrowserControlsState.BOTH);

        mOverlayPanelStateSupplier.set(PanelState.EXPANDED);
        mMediator.setBottomControlsVisible(true);
        assertTrue("Compositor view is not visible.", mModel.get(COMPOSITED_VIEW_VISIBLE));
        assertFalse(
                "Android view is not visible during overlay panel.",
                mModel.get(ANDROID_VIEW_VISIBLE));
        assertThat(mBrowserControlsVisibilityDelegate.get()).isEqualTo(BrowserControlsState.SHOWN);
    }

    @Test
    public void testSetVisibility_OverviewPanelPeeked() {
        // The initial visibility is false, defined in #setup.
        assertThat(mBrowserControlsVisibilityDelegate.get()).isEqualTo(BrowserControlsState.BOTH);

        mOverlayPanelStateSupplier.set(PanelState.PEEKED);
        mMediator.setBottomControlsVisible(true);
        assertTrue("Compositor view is not visible.", mModel.get(COMPOSITED_VIEW_VISIBLE));
        assertTrue(
                "Android view should be visible during overlay panel peek.",
                mModel.get(ANDROID_VIEW_VISIBLE));
        assertThat(mBrowserControlsVisibilityDelegate.get()).isEqualTo(BrowserControlsState.SHOWN);
    }

    @Test
    public void testShowShadow() {
        when(mBottomControlsStacker.isTopmostVisibleLayer(LayerType.TABSTRIP_TOOLBAR))
                .thenReturn(true);
        mMediator.onBrowserControlsOffsetUpdate(0);
        assertTrue(mModel.get(BottomControlsProperties.SHOW_SHADOW));

        when(mBottomControlsStacker.isTopmostVisibleLayer(LayerType.TABSTRIP_TOOLBAR))
                .thenReturn(false);
        mMediator.onBrowserControlsOffsetUpdate(0);
        assertFalse(mModel.get(BottomControlsProperties.SHOW_SHADOW));
    }

    @Test
    public void testUpdateOffsetTag() {
        when(mBottomControlsStacker.isTopmostVisibleLayer(LayerType.TABSTRIP_TOOLBAR))
                .thenReturn(true);
        mMediator.onBrowserControlsOffsetUpdate(0);

        BrowserControlsOffsetTagsInfo offsetTagsInfo =
                new BrowserControlsOffsetTagsInfo(null, null, null);
        assertEquals(DEFAULT_SHADOW_HEIGHT, mMediator.updateOffsetTag(offsetTagsInfo));

        when(mBottomControlsStacker.isTopmostVisibleLayer(LayerType.TABSTRIP_TOOLBAR))
                .thenReturn(false);
        mMediator.onBrowserControlsOffsetUpdate(0);
        assertEquals(0, mMediator.updateOffsetTag(offsetTagsInfo));
    }

    @Test
    public void testClearOffsetTag() {
        mModel.set(BottomControlsProperties.OFFSET_TAG, OffsetTag.createRandom());
        assertNotNull(mModel.get(BottomControlsProperties.OFFSET_TAG));

        mMediator.clearOffsetTag();
        assertNull(mModel.get(BottomControlsProperties.OFFSET_TAG));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_BAR})
    public void testShowShadow_DisabledWhenBottomBarEnabled() {
        Activity activity = Robolectric.buildActivity(TestActivity.class).setup().get();
        when(mWindowAndroid.getContext()).thenReturn(new java.lang.ref.WeakReference<>(activity));

        when(mBottomControlsStacker.isTopmostVisibleLayer(LayerType.TABSTRIP_TOOLBAR))
                .thenReturn(true);

        // With bottom bar enabled, SHOW_SHADOW must be false, regardless of layers
        mMediator.onBrowserControlsOffsetUpdate(0);
        assertFalse(mModel.get(BottomControlsProperties.SHOW_SHADOW));

        // updateOffsetTag must also return 0 shadow height
        BrowserControlsOffsetTagsInfo offsetTagsInfo =
                new BrowserControlsOffsetTagsInfo(null, null, null);
        assertEquals(0, mMediator.updateOffsetTag(offsetTagsInfo));
    }

    @Test
    @EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_BAR})
    public void testEdgeToEdge_BottomPadding() {
        BottomControlsMediator bottomAppBarMediator =
                new BottomControlsMediator(
                        mWindowAndroid,
                        mModel,
                        mBottomControlsStacker,
                        mBrowserControlsVisibilityDelegate,
                        mFullscreenManager,
                        LayerType.BOTTOM_APP_BAR,
                        mContentDelegateSupplier,
                        mTabObscuringHandler,
                        DEFAULT_HEIGHT,
                        DEFAULT_SHADOW_HEIGHT,
                        mOverlayPanelStateSupplier,
                        mEdgeToEdgeControllerSupplier,
                        mTabObservableSupplier,
                        mReadAloudRestoringSupplier);

        Tab tab = Mockito.mock(Tab.class);
        when(tab.isIncognito()).thenReturn(false);
        mTabObservableSupplier.set(tab);

        when(mEdgeToEdgeController.isDrawingToEdge()).thenReturn(true);
        when(mEdgeToEdgeController.getBottomInsetPx()).thenReturn(DEFAULT_INSET);

        ChangeObserver changeObserver =
                bottomAppBarMediator.getEdgeToEdgeChangeObserverForTesting();
        changeObserver.onToEdgeChange(
                DEFAULT_INSET, /* isDrawingToEdge= */ true, /* isPageOptInToEdge= */ false);

        assertEquals(DEFAULT_INSET, mModel.get(BottomControlsProperties.BOTTOM_PADDING));

        when(mEdgeToEdgeController.isDrawingToEdge()).thenReturn(false);

        changeObserver.onToEdgeChange(
                DEFAULT_INSET, /* isDrawingToEdge= */ false, /* isPageOptInToEdge= */ false);

        assertEquals(0, mModel.get(BottomControlsProperties.BOTTOM_PADDING));
    }
}
