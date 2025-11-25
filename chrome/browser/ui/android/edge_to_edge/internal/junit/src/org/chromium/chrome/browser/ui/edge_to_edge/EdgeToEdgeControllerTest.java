// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import static android.view.Display.INVALID_DISPLAY;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;
import static org.mockito.hamcrest.MockitoHamcrest.intThat;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.os.Build.VERSION_CODES;
import android.view.View;
import android.view.Window;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.blink.mojom.ViewportFit;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeControllerImpl.SupportedConfigurationSwitch;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.content_public.browser.test.mock.MockWebContents;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.edge_to_edge.EdgeToEdgeManager;
import org.chromium.ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.ui.edge_to_edge.EdgeToEdgeStateProvider;
import org.chromium.ui.edge_to_edge.EdgeToEdgeSupplier;
import org.chromium.ui.insets.InsetObserver;
import org.chromium.ui.insets.InsetObserver.WindowInsetsConsumer;
import org.chromium.ui.insets.InsetObserver.WindowInsetsConsumer.InsetConsumerSource;

/**
 * Tests the EdgeToEdgeController code. Ideally this would include {@link EdgeToEdgeController},
 * {@link EdgeToEdgeControllerFactory}, along with {@link EdgeToEdgeControllerImpl}
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        sdk = VERSION_CODES.R,
        manifest = Config.NONE,
        shadows = EdgeToEdgeControllerTest.ShadowEdgeToEdgeControllerFactory.class)
@Features.DisableFeatures({ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE})
public class EdgeToEdgeControllerTest {

    private static final int TOP_INSET = 113;
    private static final int TOP_INSET_LANDSCAPE = 98;
    private static final int BOTTOM_INSET = 59;
    private static final int BOTTOM_INSET_LANDSCAPE = 54;
    private static final int BOTTOM_KEYBOARD_INSET = 150;
    private static final int GESTURE_SWIPE_INSET = 45;
    private static final Insets NAVIGATION_BAR_INSETS = Insets.of(0, 0, 0, BOTTOM_INSET);
    private static final Insets STATUS_BAR_INSETS = Insets.of(0, TOP_INSET, 0, 0);
    private static final Insets SYSTEM_GESTURE_INSETS_FOR_GESTURE_NAV =
            Insets.of(GESTURE_SWIPE_INSET, TOP_INSET, GESTURE_SWIPE_INSET, BOTTOM_INSET);
    private static final Insets MANDATORY_SYSTEM_GESTURE_INSETS =
            Insets.of(0, TOP_INSET, 0, BOTTOM_INSET);

    private static final Insets SYSTEM_INSETS = Insets.of(0, TOP_INSET, 0, BOTTOM_INSET);
    private static final Insets SYSTEM_INSETS_LANDSCAPE =
            Insets.of(0, TOP_INSET_LANDSCAPE, 0, BOTTOM_INSET_LANDSCAPE);
    private static final Insets IME_INSETS_NO_KEYBOARD = Insets.of(0, 0, 0, 0);
    private static final Insets IME_INSETS_KEYBOARD = Insets.of(0, 0, 0, BOTTOM_KEYBOARD_INSET);
    private static final int EDGE_TO_EDGE_STATUS_TOKEN = 12345;

    private static final WindowInsetsCompat SYSTEM_BARS_WINDOW_INSETS =
            new WindowInsetsCompat.Builder()
                    .setInsets(WindowInsetsCompat.Type.statusBars(), STATUS_BAR_INSETS)
                    .setInsets(WindowInsetsCompat.Type.navigationBars(), NAVIGATION_BAR_INSETS)
                    .setInsets(WindowInsetsCompat.Type.systemBars(), SYSTEM_INSETS)
                    .setInsets(WindowInsetsCompat.Type.ime(), IME_INSETS_NO_KEYBOARD)
                    .setInsets(
                            WindowInsetsCompat.Type.systemGestures(),
                            SYSTEM_GESTURE_INSETS_FOR_GESTURE_NAV)
                    .setInsets(
                            WindowInsetsCompat.Type.mandatorySystemGestures(),
                            MANDATORY_SYSTEM_GESTURE_INSETS)
                    .build();

    private static final WindowInsetsCompat GESTURE_NAV_INSETS_MISSING_NAVBAR =
            new WindowInsetsCompat.Builder()
                    .setInsets(WindowInsetsCompat.Type.statusBars(), STATUS_BAR_INSETS)
                    .setInsets(WindowInsetsCompat.Type.navigationBars(), Insets.NONE)
                    .setInsets(WindowInsetsCompat.Type.systemBars(), STATUS_BAR_INSETS)
                    .setInsets(WindowInsetsCompat.Type.ime(), IME_INSETS_NO_KEYBOARD)
                    .setInsets(
                            WindowInsetsCompat.Type.systemGestures(),
                            SYSTEM_GESTURE_INSETS_FOR_GESTURE_NAV)
                    .setInsets(
                            WindowInsetsCompat.Type.mandatorySystemGestures(),
                            MANDATORY_SYSTEM_GESTURE_INSETS)
                    .build();

    private static final WindowInsetsCompat GESTURE_NAV_INSETS_MISSING_ALL_BOTTOM_INSETS =
            new WindowInsetsCompat.Builder()
                    .setInsets(WindowInsetsCompat.Type.statusBars(), STATUS_BAR_INSETS)
                    .setInsets(WindowInsetsCompat.Type.navigationBars(), Insets.NONE)
                    .setInsets(WindowInsetsCompat.Type.systemBars(), STATUS_BAR_INSETS)
                    .setInsets(WindowInsetsCompat.Type.ime(), IME_INSETS_NO_KEYBOARD)
                    .setInsets(
                            WindowInsetsCompat.Type.systemGestures(),
                            Insets.of(GESTURE_SWIPE_INSET, TOP_INSET, GESTURE_SWIPE_INSET, 0))
                    .setInsets(WindowInsetsCompat.Type.mandatorySystemGestures(), STATUS_BAR_INSETS)
                    .build();

    private static final WindowInsetsCompat SYSTEM_BARS_WINDOW_INSETS_WITH_KEYBOARD =
            new WindowInsetsCompat.Builder()
                    .setInsets(WindowInsetsCompat.Type.navigationBars(), NAVIGATION_BAR_INSETS)
                    .setInsets(WindowInsetsCompat.Type.statusBars(), STATUS_BAR_INSETS)
                    .setInsets(WindowInsetsCompat.Type.systemBars(), SYSTEM_INSETS)
                    .setInsets(WindowInsetsCompat.Type.ime(), IME_INSETS_KEYBOARD)
                    .setInsets(
                            WindowInsetsCompat.Type.systemGestures(),
                            SYSTEM_GESTURE_INSETS_FOR_GESTURE_NAV)
                    .setInsets(
                            WindowInsetsCompat.Type.mandatorySystemGestures(),
                            MANDATORY_SYSTEM_GESTURE_INSETS)
                    .build();

    private static final WindowInsetsCompat SYSTEM_BARS_TOP_INSETS_ONLY =
            new WindowInsetsCompat.Builder()
                    .setInsets(WindowInsetsCompat.Type.statusBars(), STATUS_BAR_INSETS)
                    .setInsets(WindowInsetsCompat.Type.systemBars(), STATUS_BAR_INSETS)
                    .build();

    private static final WindowInsetsCompat SYSTEM_BARS_WITH_TAPPABLE_NAVBAR =
            new WindowInsetsCompat.Builder()
                    .setInsets(WindowInsetsCompat.Type.statusBars(), STATUS_BAR_INSETS)
                    .setInsets(WindowInsetsCompat.Type.navigationBars(), NAVIGATION_BAR_INSETS)
                    .setInsets(WindowInsetsCompat.Type.systemBars(), SYSTEM_INSETS)
                    .setInsets(WindowInsetsCompat.Type.tappableElement(), NAVIGATION_BAR_INSETS)
                    .setInsets(
                            WindowInsetsCompat.Type.systemGestures(),
                            MANDATORY_SYSTEM_GESTURE_INSETS)
                    .setInsets(
                            WindowInsetsCompat.Type.mandatorySystemGestures(),
                            MANDATORY_SYSTEM_GESTURE_INSETS)
                    .build();

    private static final WindowInsetsCompat SYSTEM_BARS_WITH_TAPPABLE_MISSING_NAVBAR =
            new WindowInsetsCompat.Builder()
                    .setInsets(WindowInsetsCompat.Type.statusBars(), STATUS_BAR_INSETS)
                    .setInsets(WindowInsetsCompat.Type.navigationBars(), Insets.NONE)
                    .setInsets(WindowInsetsCompat.Type.systemBars(), STATUS_BAR_INSETS)
                    .setInsets(WindowInsetsCompat.Type.tappableElement(), NAVIGATION_BAR_INSETS)
                    .setInsets(
                            WindowInsetsCompat.Type.systemGestures(),
                            MANDATORY_SYSTEM_GESTURE_INSETS)
                    .setInsets(
                            WindowInsetsCompat.Type.mandatorySystemGestures(),
                            MANDATORY_SYSTEM_GESTURE_INSETS)
                    .build();

    private static final WindowInsetsCompat SYSTEM_BARS_NEITHER_TAPPABLE_NOR_GESTURE_NAV =
            new WindowInsetsCompat.Builder()
                    .setInsets(WindowInsetsCompat.Type.statusBars(), STATUS_BAR_INSETS)
                    .setInsets(WindowInsetsCompat.Type.navigationBars(), NAVIGATION_BAR_INSETS)
                    .setInsets(WindowInsetsCompat.Type.systemBars(), SYSTEM_INSETS)
                    .setInsets(WindowInsetsCompat.Type.tappableElement(), Insets.NONE)
                    .setInsets(
                            WindowInsetsCompat.Type.systemGestures(),
                            MANDATORY_SYSTEM_GESTURE_INSETS)
                    .setInsets(
                            WindowInsetsCompat.Type.mandatorySystemGestures(),
                            MANDATORY_SYSTEM_GESTURE_INSETS)
                    .build();

    private Activity mActivity;
    private EdgeToEdgeControllerImpl mEdgeToEdgeControllerImpl;

    private final ObservableSupplierImpl<Tab> mTabProvider = new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<LayoutManager> mLayoutManagerSupplier =
            new ObservableSupplierImpl<>();

    private final UserDataHost mTabDataHost = new UserDataHost();

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private WindowAndroid mWindowAndroid;
    @Mock private InsetObserver mInsetObserver;
    @Mock private Tab mTab;
    @Mock private NativePage mKeyNativePage;

    @Mock private MockWebContents mWebContents;

    @Mock private EdgeToEdgeOSWrapper mOsWrapper;
    @Mock private EdgeToEdgeStateProvider mEdgeToEdgeStateProvider;
    @Mock private EdgeToEdgeManager mEdgeToEdgeManager;
    @Mock private EdgeToEdgeSupplier.ChangeObserver mChangeObserver;

    @Captor private ArgumentCaptor<WindowInsetsConsumer> mWindowInsetsListenerCaptor;

    @Captor private ArgumentCaptor<TabObserver> mTabObserverArgumentCaptor;

    @Mock private View mViewMock;

    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;
    @Mock private LayoutManager mLayoutManager;
    @Mock private FullscreenManager mFullscreenManager;

    @Implements(EdgeToEdgeControllerFactory.class)
    static class ShadowEdgeToEdgeControllerFactory extends EdgeToEdgeControllerFactory {
        @Implementation
        protected static boolean isGestureNavigationMode(Window window) {
            return true;
        }
    }

    @Before
    public void setUp() {
        when(mWindowAndroid.getInsetObserver()).thenReturn(mInsetObserver);
        when(mInsetObserver.getLastRawWindowInsets()).thenReturn(SYSTEM_BARS_WINDOW_INSETS);
        doAnswer(
                        (inv) -> {
                            mWindowInsetsListenerCaptor
                                    .getValue()
                                    .onApplyWindowInsets(mViewMock, SYSTEM_BARS_WINDOW_INSETS);
                            return null;
                        })
                .when(mInsetObserver)
                .retriggerOnApplyWindowInsets();

        mActivity = Mockito.spy(Robolectric.buildActivity(AppCompatActivity.class).setup().get());
        mLayoutManagerSupplier.set(mLayoutManager);

        doNothing().when(mTab).addObserver(any());
        when(mTab.getUserDataHost()).thenReturn(mTabDataHost);
        when(mTab.getWebContents()).thenReturn(mWebContents);
        when(mKeyNativePage.supportsEdgeToEdge()).thenReturn(true);

        doReturn(EDGE_TO_EDGE_STATUS_TOKEN)
                .when(mEdgeToEdgeStateProvider)
                .acquireSetDecorFitsSystemWindowToken();
        doReturn(mEdgeToEdgeStateProvider).when(mEdgeToEdgeManager).getEdgeToEdgeStateProvider();
        doNothing().when(mOsWrapper).setPadding(any(), anyInt(), anyInt(), anyInt(), anyInt());
        doNothing()
                .when(mInsetObserver)
                .addInsetsConsumer(
                        mWindowInsetsListenerCaptor.capture(),
                        eq(InsetConsumerSource.EDGE_TO_EDGE_CONTROLLER_IMPL));

        EdgeToEdgeUtils.setObservedTappableNavigationBarForTesting(false);
        mEdgeToEdgeControllerImpl =
                new EdgeToEdgeControllerImpl(
                        mActivity,
                        mWindowAndroid,
                        mTabProvider,
                        mOsWrapper,
                        mEdgeToEdgeManager,
                        mBrowserControlsStateProvider,
                        mLayoutManagerSupplier,
                        mFullscreenManager);
        verify(mEdgeToEdgeStateProvider, times(1)).acquireSetDecorFitsSystemWindowToken();

        if (!EdgeToEdgeUtils.isEdgeToEdgeEverywhereEnabled()) {
            verify(mOsWrapper, times(1))
                    .setPadding(
                            any(),
                            eq(0),
                            intThat(Matchers.greaterThan(0)),
                            eq(0),
                            intThat(Matchers.greaterThan(0)));
        }
        verify(mInsetObserver, times(1))
                .addInsetsConsumer(any(), eq(InsetConsumerSource.EDGE_TO_EDGE_CONTROLLER_IMPL));
        EdgeToEdgeUtils.setHas3ButtonNavBarForTesting(false);

        mEdgeToEdgeControllerImpl.registerObserver(mChangeObserver);
    }

    @After
    public void tearDown() {
        mEdgeToEdgeControllerImpl.destroy();
        mEdgeToEdgeControllerImpl = null;
    }

    @Test
    public void drawEdgeToEdge_ToEdgeAndToNormal() {
        mEdgeToEdgeControllerImpl.drawToEdge(true, /* changedWindowState= */ false);
        assertToEdgeExpectations();

        mEdgeToEdgeControllerImpl.drawToEdge(false, /* changedWindowState= */ false);
        assertToNormalExpectations();
    }

    @Test
    public void drawEdgeToEdge_UpdateWindowInsets_toNormal() {
        when(mTab.isNativePage()).thenReturn(false);
        mTabProvider.set(mTab);
        verifyInteractions(mTab);

        Mockito.clearInvocations(mEdgeToEdgeManager);

        mEdgeToEdgeControllerImpl.setSystemInsetsForTesting(SYSTEM_INSETS);
        mEdgeToEdgeControllerImpl.drawToEdge(false, /* changedWindowState= */ false);
        verify(mOsWrapper).setPadding(any(), eq(0), eq(TOP_INSET), eq(0), eq(BOTTOM_INSET));

        mEdgeToEdgeControllerImpl.setSystemInsetsForTesting(SYSTEM_INSETS_LANDSCAPE);
        mEdgeToEdgeControllerImpl.drawToEdge(false, /* changedWindowState= */ true);
        verify(mOsWrapper)
                .setPadding(
                        any(), eq(0), eq(TOP_INSET_LANDSCAPE), eq(0), eq(BOTTOM_INSET_LANDSCAPE));

        mEdgeToEdgeControllerImpl.setKeyboardInsetsForTesting(IME_INSETS_KEYBOARD);
        mEdgeToEdgeControllerImpl.drawToEdge(false, /* changedWindowState= */ true);
        verify(mOsWrapper)
                .setPadding(
                        any(), eq(0), eq(TOP_INSET_LANDSCAPE), eq(0), eq(BOTTOM_KEYBOARD_INSET));
        verify(mEdgeToEdgeManager, never()).setContentFitsWindowInsets(false);
        verify(mEdgeToEdgeManager, atLeastOnce()).setContentFitsWindowInsets(true);
    }

    @Test
    public void drawEdgeToEdge_UpdateWindowInsets_toEdge() {
        when(mTab.isNativePage()).thenReturn(false);
        mTabProvider.set(mTab);
        verifyInteractions(mTab);

        Mockito.clearInvocations(mEdgeToEdgeManager);

        mEdgeToEdgeControllerImpl.setSystemInsetsForTesting(SYSTEM_INSETS);
        mEdgeToEdgeControllerImpl.drawToEdge(true, /* changedWindowState= */ false);
        verify(mOsWrapper).setPadding(any(), eq(0), eq(TOP_INSET), eq(0), eq(0));

        mEdgeToEdgeControllerImpl.setSystemInsetsForTesting(SYSTEM_INSETS_LANDSCAPE);
        mEdgeToEdgeControllerImpl.drawToEdge(true, /* changedWindowState= */ true);
        verify(mOsWrapper).setPadding(any(), eq(0), eq(TOP_INSET_LANDSCAPE), eq(0), eq(0));

        mEdgeToEdgeControllerImpl.setKeyboardInsetsForTesting(IME_INSETS_KEYBOARD);
        mEdgeToEdgeControllerImpl.drawToEdge(true, /* changedWindowState= */ true);
        verify(mOsWrapper)
                .setPadding(
                        any(), eq(0), eq(TOP_INSET_LANDSCAPE), eq(0), eq(BOTTOM_KEYBOARD_INSET));
        verify(mEdgeToEdgeManager, never()).setContentFitsWindowInsets(true);
        verify(mEdgeToEdgeManager, atLeastOnce()).setContentFitsWindowInsets(false);
    }

    /** Test nothing is done when the Feature is not enabled. */
    @Test
    public void onObservingDifferentTab_default() {
        when(mTab.isNativePage()).thenReturn(false);
        mTabProvider.set(mTab);
        verifyInteractions(mTab);
        assertFalse(mEdgeToEdgeControllerImpl.isPageOptedIntoEdgeToEdge());
        assertNoChangeExpectations();
    }

    @Test
    @SuppressLint("NewApi")
    public void onObservingDifferentTab_changeToWebDisabled() {
        // First go ToEdge by invoking the changeToTabSwitcher test logic.
        mEdgeToEdgeControllerImpl.setIsOptedIntoEdgeToEdgeForTesting(true);
        mEdgeToEdgeControllerImpl.setIsDrawingToEdgeForTesting(true);
        mEdgeToEdgeControllerImpl.setSystemInsetsForTesting(SYSTEM_INSETS);

        // Now test that a Web page causes a transition ToNormal (when Web forcing is disabled).
        when(mTab.isNativePage()).thenReturn(false);
        mTabProvider.set(mTab);
        verifyInteractions(mTab);
        assertFalse(mEdgeToEdgeControllerImpl.isPageOptedIntoEdgeToEdge());
        assertFalse(mEdgeToEdgeControllerImpl.isDrawingToEdge());
        assertToNormalExpectations();
    }

    @Test
    public void onObservingDifferentTab_changeToWebEnabled() {
        EdgeToEdgeUtils.setAlwaysDrawWebEdgeToEdgeForTesting(true);
        when(mTab.isNativePage()).thenReturn(false);
        mTabProvider.set(mTab);
        verifyInteractions(mTab);
        assertTrue(mEdgeToEdgeControllerImpl.isPageOptedIntoEdgeToEdge());
        assertTrue(mEdgeToEdgeControllerImpl.isDrawingToEdge());
        assertToEdgeExpectations();
        assertBottomInsetForSafeArea(SYSTEM_INSETS.bottom);
    }

    @Test
    public void onObservingDifferentTab_changeToWebEnabled_SetsDecor() {
        EdgeToEdgeUtils.setAlwaysDrawWebEdgeToEdgeForTesting(true);
        when(mTab.isNativePage()).thenReturn(false);
        mTabProvider.set(mTab);
        verifyInteractions(mTab);
        assertTrue(mEdgeToEdgeControllerImpl.isPageOptedIntoEdgeToEdge());
        assertTrue(mEdgeToEdgeControllerImpl.isDrawingToEdge());
        assertToEdgeExpectations();
        assertBottomInsetForSafeArea(SYSTEM_INSETS.bottom);
    }

    @Test
    public void onObservingDifferentTab_viewportFitChanged() {
        // Start with web always-enabled.
        EdgeToEdgeUtils.setAlwaysDrawWebEdgeToEdgeForTesting(true);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        when(mTab.isNativePage()).thenReturn(false);
        mTabProvider.set(mTab);
        verifyInteractions(mTab);
        assertTrue(mEdgeToEdgeControllerImpl.isPageOptedIntoEdgeToEdge());
        assertTrue(mEdgeToEdgeControllerImpl.isDrawingToEdge());
        assertToEdgeExpectations();
        assertBottomInsetForSafeArea(SYSTEM_INSETS.bottom);

        // Now switch the viewport-fit value of that page back and forth,
        // with web NOT always enabled. The page opt-in should switch with the viewport fit value,
        // but the page should still draw toEdge to properly show the bottom chin.
        EdgeToEdgeUtils.setAlwaysDrawWebEdgeToEdgeForTesting(false);
        mEdgeToEdgeControllerImpl.getWebContentsObserver().viewportFitChanged(ViewportFit.AUTO);
        assertFalse(mEdgeToEdgeControllerImpl.isPageOptedIntoEdgeToEdge());
        assertTrue(mEdgeToEdgeControllerImpl.isDrawingToEdge());

        mEdgeToEdgeControllerImpl.getWebContentsObserver().viewportFitChanged(ViewportFit.COVER);
        assertTrue(mEdgeToEdgeControllerImpl.isPageOptedIntoEdgeToEdge());
        assertTrue(mEdgeToEdgeControllerImpl.isDrawingToEdge());

        mEdgeToEdgeControllerImpl.getWebContentsObserver().viewportFitChanged(ViewportFit.AUTO);
        assertFalse(mEdgeToEdgeControllerImpl.isPageOptedIntoEdgeToEdge());
        assertTrue(mEdgeToEdgeControllerImpl.isDrawingToEdge());
    }

    /** Test the OSWrapper implementation without mocking it. Native ToNormal. */
    @Test
    public void onObservingDifferentTab_osWrapperImplToNormal() {
        ObservableSupplierImpl liveSupplier = new ObservableSupplierImpl();
        EdgeToEdgeControllerImpl liveController =
                (EdgeToEdgeControllerImpl)
                        EdgeToEdgeControllerFactory.create(
                                mActivity,
                                mWindowAndroid,
                                liveSupplier,
                                mEdgeToEdgeManager,
                                mBrowserControlsStateProvider,
                                mLayoutManagerSupplier,
                                mFullscreenManager);
        assertNotNull(liveController);
        liveController.setIsOptedIntoEdgeToEdgeForTesting(true);
        liveController.setIsDrawingToEdgeForTesting(true);
        liveController.setSystemInsetsForTesting(SYSTEM_INSETS);
        when(mTab.isNativePage()).thenReturn(false);
        liveSupplier.set(mTab);
        verifyInteractions(mTab);
        assertFalse(liveController.isPageOptedIntoEdgeToEdge());
    }

    /** Test switching to the Tab Switcher, which uses a null Tab. */
    @Test
    public void onObservingDifferentTab_nullTab() {
        Mockito.clearInvocations(mEdgeToEdgeManager);

        mEdgeToEdgeControllerImpl.setIsOptedIntoEdgeToEdgeForTesting(true);
        mEdgeToEdgeControllerImpl.setIsDrawingToEdgeForTesting(true);
        mEdgeToEdgeControllerImpl.setSystemInsetsForTesting(SYSTEM_INSETS);
        mEdgeToEdgeControllerImpl.onTabSwitched(null);
        assertFalse(mEdgeToEdgeControllerImpl.isPageOptedIntoEdgeToEdge());

        // Pad the top and the bottom to keep it all normal.
        verify(mOsWrapper).setPadding(any(), eq(0), eq(TOP_INSET), eq(0), eq(BOTTOM_INSET));
        verify(mEdgeToEdgeManager).setContentFitsWindowInsets(true);
    }

    @Test
    public void onObservingDifferentTab_embeddedMediaExperience() {
        when(mTab.shouldEnableEmbeddedMediaExperience()).thenReturn(true);
        mTabProvider.set(mTab);
        verifyInteractions(mTab);
        assertToEdgeExpectations();
    }

    @Test
    public void onTabSwitched_onContentChanged() {
        // Start with a Tab with no WebContents
        when(mTab.getWebContents()).thenReturn(null);
        doNothing().when(mTab).addObserver(mTabObserverArgumentCaptor.capture());

        // When onTabSwitched is called, we capture the TabObserver created for the Tab.
        mEdgeToEdgeControllerImpl.onTabSwitched(mTab);
        TabObserver tabObserver = mTabObserverArgumentCaptor.getValue();
        assertNotNull(tabObserver);
        WebContentsObserver initialWebContentsObserver =
                mEdgeToEdgeControllerImpl.getWebContentsObserver();
        assertNull(initialWebContentsObserver);

        // Simulate the tab getting new WebContents.
        when(mTab.getWebContents()).thenReturn(mWebContents);
        tabObserver.onContentChanged(mTab);
        WebContentsObserver firstObserver = mEdgeToEdgeControllerImpl.getWebContentsObserver();
        assertNotNull(firstObserver);
    }

    @Test
    public void testNavigateFromKeyNativePageToNotOptedInWebPage() {
        Mockito.clearInvocations(mTab, mOsWrapper, mEdgeToEdgeManager);

        // Navigate to key native page, which should draw toEdge.
        when(mTab.isNativePage()).thenReturn(true);
        when(mTab.getNativePage()).thenReturn(mKeyNativePage);
        mTabProvider.set(mTab);
        assertToEdgeExpectations();

        Mockito.clearInvocations(mTab, mOsWrapper, mEdgeToEdgeManager);
        // Native to a web page that is not opted in, which should draw toNormal.
        when(mTab.isNativePage()).thenReturn(false);
        when(mTab.getNativePage()).thenReturn(null);
        mEdgeToEdgeControllerImpl.getTabObserverForTesting().onContentChanged(mTab);
        assertToNormalExpectations();
    }

    @Test
    public void testNavigateFromNotOptedInWebPageToKeyNativePage() {
        // Native to a web page that is not opted in, which should draw toNormal.
        when(mTab.isNativePage()).thenReturn(false);
        when(mTab.getNativePage()).thenReturn(null);
        mEdgeToEdgeControllerImpl.getTabObserverForTesting().onContentChanged(mTab);
        assertToNormalExpectations();

        Mockito.clearInvocations(mTab, mOsWrapper, mEdgeToEdgeManager);
        // Navigate to key native page, which should draw toEdge.
        when(mTab.isNativePage()).thenReturn(true);
        when(mTab.getNativePage()).thenReturn(mKeyNativePage);
        mTabProvider.set(mTab);
        assertToEdgeExpectations();
    }

    @Test
    public void testNavigateFromKeyNativePageToOptedInWebPage() {
        EdgeToEdgeUtils.setAlwaysDrawWebEdgeToEdgeForTesting(true);
        Mockito.clearInvocations(mTab, mOsWrapper, mEdgeToEdgeManager);

        // Navigate to key native page, which should draw toEdge.
        when(mTab.isNativePage()).thenReturn(true);
        when(mTab.getNativePage()).thenReturn(mKeyNativePage);
        mTabProvider.set(mTab);
        assertToEdgeExpectations();

        // Native to a web page that is opted in, which should draw toEdge.
        when(mTab.isNativePage()).thenReturn(false);
        when(mTab.getNativePage()).thenReturn(null);
        mEdgeToEdgeControllerImpl.getTabObserverForTesting().onContentChanged(mTab);
        assertNoChangeExpectations();
    }

    @Test
    public void testNavigateFromOptedInWebPageToKeyNativePage() {
        EdgeToEdgeUtils.setAlwaysDrawWebEdgeToEdgeForTesting(true);
        Mockito.clearInvocations(mTab, mOsWrapper, mEdgeToEdgeManager);

        // Native to a web page that is opted in, which should draw toEdge.
        when(mTab.isNativePage()).thenReturn(false);
        when(mTab.getNativePage()).thenReturn(null);
        mTabProvider.set(mTab);
        assertToEdgeExpectations();

        // Navigate to key native page, which should draw toEdge.
        when(mTab.isNativePage()).thenReturn(true);
        when(mTab.getNativePage()).thenReturn(mKeyNativePage);
        mEdgeToEdgeControllerImpl.getTabObserverForTesting().onContentChanged(mTab);
        assertNoChangeExpectations();
    }

    @Test
    public void testSwitchLayout() {
        Mockito.clearInvocations(mEdgeToEdgeManager);

        doReturn(LayoutType.TAB_SWITCHER).when(mLayoutManager).getActiveLayoutType();
        mEdgeToEdgeControllerImpl.onStartedShowing(LayoutType.TAB_SWITCHER);
        assertToEdgeExpectations();

        doReturn(LayoutType.BROWSING).when(mLayoutManager).getActiveLayoutType();
        mEdgeToEdgeControllerImpl.onStartedShowing(LayoutType.BROWSING);
        assertToEdgeExpectations();

        verify(mEdgeToEdgeManager, never()).setContentFitsWindowInsets(true);
    }

    @Test
    public void testLayoutManagerChanged() {
        doReturn(LayoutType.BROWSING).when(mLayoutManager).getActiveLayoutType();
        mEdgeToEdgeControllerImpl.onStartedShowing(LayoutType.BROWSING);
        assertToEdgeExpectations();

        mLayoutManagerSupplier.set(null);
        assertToNormalExpectations();
    }

    @Test
    public void switchFullscreenMode_NoStatusBarNoNavBar() {
        doReturn(true).when(mFullscreenManager).getPersistentFullscreenMode();
        mEdgeToEdgeControllerImpl.onEnterFullscreen(
                mTab, new FullscreenOptions(false, false, INVALID_DISPLAY));

        verify(mOsWrapper, atLeastOnce()).setPadding(any(), eq(0), eq(0), eq(0), eq(0));

        mEdgeToEdgeControllerImpl.onExitFullscreen(mTab);
        assertToNormalExpectations();
        assertBottomInsetForSafeArea(0);
    }

    @Test
    public void isSupportedConfiguration_default() {
        assertTrue(
                "The default setup should be a supported configuration but it not!",
                EdgeToEdgeUtils.isEdgeToEdgeBottomChinEnabled(
                        Robolectric.buildActivity(AppCompatActivity.class).setup().get()));
    }

    @Test
    public void disabledWhenNotActivityNotAttached() {
        Activity activity = Robolectric.buildActivity(AppCompatActivity.class).create().get();
        assertNull(activity.getWindow().getDecorView().getRootWindowInsets());
        assertFalse(
                "The activity is not supported before its root window insets is available.",
                EdgeToEdgeUtils.isEdgeToEdgeBottomChinEnabled(activity));
    }

    @Test
    @Config(qualifiers = "xlarge")
    @DisableFeatures(ChromeFeatureList.EDGE_TO_EDGE_TABLET)
    public void disabledWhenNotPhoneAndTabletFeatureDisabled() {
        // Even these always-draw flags do not override the device abilities.
        EdgeToEdgeUtils.setAlwaysDrawWebEdgeToEdgeForTesting(true);
        // Even the always-draw flags do not override the device abilities.
        assertFalse(
                EdgeToEdgeUtils.isEdgeToEdgeBottomChinEnabled(
                        Robolectric.buildActivity(AppCompatActivity.class).setup().get()));
    }

    @Test
    @Config(qualifiers = "xlarge")
    @EnableFeatures(ChromeFeatureList.EDGE_TO_EDGE_TABLET + ":e2e_tablet_width_threshold/-1")
    public void enabledOnTabletWhenFeatureEnabled() {
        // Even these always-draw flags do not override the device abilities.
        EdgeToEdgeUtils.setAlwaysDrawWebEdgeToEdgeForTesting(true);
        // Even the always-draw flags do not override the device abilities.
        assertTrue(
                EdgeToEdgeUtils.isEdgeToEdgeBottomChinEnabled(
                        Robolectric.buildActivity(AppCompatActivity.class).setup().get()));
    }

    @Test
    public void disabledWhenNotGestureEnabled() {
        // Even these always-draw flags do not override the device abilities.
        EdgeToEdgeUtils.setAlwaysDrawWebEdgeToEdgeForTesting(true);
        // Even the always-draw flags do not override the device abilities.
        EdgeToEdgeUtils.setHas3ButtonNavBarForTesting(true);
        assertFalse(
                EdgeToEdgeUtils.isEdgeToEdgeBottomChinEnabled(
                        Robolectric.buildActivity(AppCompatActivity.class).setup().get()));
    }

    @Test
    public void supportConfigurationRecorded() {
        assertTrue(EdgeToEdgeUtils.isEdgeToEdgeBottomChinEnabled(mActivity));
        try (var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords("Android.EdgeToEdge.SupportedConfigurationSwitch2")
                        .build()) {
            mEdgeToEdgeControllerImpl.handleWindowInsets(mViewMock, SYSTEM_BARS_WINDOW_INSETS);
        }
    }

    @Test
    public void supportConfigurationRecorded_supportedToUnsupported() {
        assertTrue(EdgeToEdgeUtils.isEdgeToEdgeBottomChinEnabled(mActivity));
        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.EdgeToEdge.SupportedConfigurationSwitch2",
                        SupportedConfigurationSwitch.FROM_SUPPORTED_TO_UNSUPPORTED);
        EdgeToEdgeUtils.setHas3ButtonNavBarForTesting(true);
        mEdgeToEdgeControllerImpl.handleWindowInsets(mViewMock, SYSTEM_BARS_WINDOW_INSETS);
        watcher.assertExpected();
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(ChromeFeatureList.EDGE_TO_EDGE_TABLET + ":e2e_tablet_width_threshold/-1")
    public void supportFormFactor() {
        assertTrue(DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity));
        assertTrue(
                "e2e bottom chin should be enabled on tablet",
                EdgeToEdgeUtils.isEdgeToEdgeBottomChinEnabled(mActivity));
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @DisableFeatures(ChromeFeatureList.EDGE_TO_EDGE_TABLET)
    public void supportFormFactor_disabled() {
        assertTrue(DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity));
        assertFalse(
                "e2e bottom chin should be disabled on tablet when feature is disabled",
                EdgeToEdgeUtils.isEdgeToEdgeBottomChinEnabled(mActivity));
    }

    @Test
    @Config(qualifiers = "sw600dp")
    @EnableFeatures(ChromeFeatureList.EDGE_TO_EDGE_TABLET + ":e2e_tablet_width_threshold/10000")
    public void supportFormFactor_minWidth() {
        assertTrue(DeviceFormFactor.isNonMultiDisplayContextOnTablet(mActivity));
        assertFalse(
                "e2e bottom chin should be disabled on tablet when width is less than min width",
                EdgeToEdgeUtils.isEdgeToEdgeBottomChinEnabled(mActivity));
    }

    @Test
    public void supportConfigurationRecorded_unsupportToSupported() {
        // Simulate a 3-button navbar being added without activity recreation.
        EdgeToEdgeUtils.setHas3ButtonNavBarForTesting(true);
        assertFalse(EdgeToEdgeUtils.isEdgeToEdgeBottomChinEnabled(mActivity));
        mEdgeToEdgeControllerImpl.handleWindowInsets(mViewMock, SYSTEM_BARS_WINDOW_INSETS);

        var watcher =
                HistogramWatcher.newSingleRecordWatcher(
                        "Android.EdgeToEdge.SupportedConfigurationSwitch2",
                        SupportedConfigurationSwitch.FROM_UNSUPPORTED_TO_SUPPORTED);
        // Simulate a 3-button navbar being removed without activity recreation.
        EdgeToEdgeUtils.setHas3ButtonNavBarForTesting(false);
        mEdgeToEdgeControllerImpl.handleWindowInsets(mViewMock, SYSTEM_BARS_WINDOW_INSETS);
        watcher.assertExpected();
    }

    @Test
    public void testViewportFitAfterListenerSet_ToNormal() {
        when(mTab.isNativePage()).thenReturn(false);
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        mTabProvider.set(mTab);
        verifyInteractions(mTab);
        assertFalse(
                "Shouldn't be opted into edge-to-edge.",
                mEdgeToEdgeControllerImpl.isPageOptedIntoEdgeToEdge());
        assertTrue(
                "Should be drawing edge-to-edge for the bottom chin.",
                mEdgeToEdgeControllerImpl.isDrawingToEdge());

        // Simulate a viewport fit change to kick off WindowInsetConsumer being hooked up.
        mEdgeToEdgeControllerImpl.getWebContentsObserver().viewportFitChanged(ViewportFit.COVER);

        Mockito.clearInvocations(mEdgeToEdgeManager);

        // Simulate another viewport fit change prior to #handleWindowInsets being called.
        mEdgeToEdgeControllerImpl.getWebContentsObserver().viewportFitChanged(ViewportFit.CONTAIN);

        // Simulate insets being available.
        assertNotNull(mWindowInsetsListenerCaptor.getValue());
        mWindowInsetsListenerCaptor
                .getValue()
                .onApplyWindowInsets(mViewMock, SYSTEM_BARS_WINDOW_INSETS);
        assertFalse(
                "Shouldn't be opted into edge-to-edge after toggling viewport-fit.",
                mEdgeToEdgeControllerImpl.isPageOptedIntoEdgeToEdge());
        assertTrue(
                "Should still be drawing edge-to-edge after toggling viewport-fit to account for"
                        + " the bottom chin.",
                mEdgeToEdgeControllerImpl.isDrawingToEdge());
        verify(mEdgeToEdgeManager, atLeastOnce()).setContentFitsWindowInsets(false);
        verify(mOsWrapper).setPadding(any(), eq(0), eq(TOP_INSET), eq(0), eq(0));
    }

    @Test
    public void testViewportFitAfterListenerSet_ToEdge() {
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        when(mTab.isNativePage()).thenReturn(false);
        mTabProvider.set(mTab);
        verifyInteractions(mTab);
        assertFalse("Shouldn't be toEdge.", mEdgeToEdgeControllerImpl.isPageOptedIntoEdgeToEdge());
        verify(mChangeObserver, times(1))
                .onToEdgeChange(eq(BOTTOM_INSET), anyBoolean(), anyBoolean());

        // Simulate a viewport fit change to kick off WindowInsetConsumer being hooked up.
        mEdgeToEdgeControllerImpl.getWebContentsObserver().viewportFitChanged(ViewportFit.COVER);
        verify(mChangeObserver, times(2))
                .onToEdgeChange(eq(BOTTOM_INSET), anyBoolean(), anyBoolean());

        // Simulate another viewport fit change prior to #handleWindowInsets being called.
        mEdgeToEdgeControllerImpl.getWebContentsObserver().viewportFitChanged(ViewportFit.CONTAIN);
        verify(mChangeObserver, times(3))
                .onToEdgeChange(eq(BOTTOM_INSET), anyBoolean(), anyBoolean());

        Mockito.clearInvocations(mEdgeToEdgeManager);

        // Go back to edge.
        mEdgeToEdgeControllerImpl.getWebContentsObserver().viewportFitChanged(ViewportFit.COVER);
        verify(mChangeObserver, times(4))
                .onToEdgeChange(eq(BOTTOM_INSET), anyBoolean(), anyBoolean());

        // Simulate insets being available.
        assertNotNull(mWindowInsetsListenerCaptor.getValue());
        mWindowInsetsListenerCaptor
                .getValue()
                .onApplyWindowInsets(mViewMock, SYSTEM_BARS_WINDOW_INSETS);
        assertTrue(
                "Should be opted into edge-to-edge after toggling viewport-fit.",
                mEdgeToEdgeControllerImpl.isPageOptedIntoEdgeToEdge());
        assertTrue(
                "Should be drawing toEdge after toggling viewport-fit.",
                mEdgeToEdgeControllerImpl.isDrawingToEdge());
        verify(mEdgeToEdgeManager, atLeastOnce()).setContentFitsWindowInsets(false);
        verify(mOsWrapper).setPadding(any(), eq(0), eq(TOP_INSET), eq(0), eq(0));
    }

    @Test
    public void safeAreaConstraint() {
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        when(mTab.isNativePage()).thenReturn(false);
        mTabProvider.set(mTab);
        verifyInteractions(mTab);
        assertFalse(
                "Safe area constraint should default to false.",
                mEdgeToEdgeControllerImpl.getHasSafeAreaConstraintForTesting());

        mEdgeToEdgeControllerImpl.getWebContentsObserver().safeAreaConstraintChanged(true);
        assertTrue(
                "Safe area constraint should be set by observer.",
                mEdgeToEdgeControllerImpl.getHasSafeAreaConstraintForTesting());
        verify(mChangeObserver).onSafeAreaConstraintChanged(true);

        mEdgeToEdgeControllerImpl.getWebContentsObserver().safeAreaConstraintChanged(false);
        assertFalse(
                "Safe area constraint should be removed by observer.",
                mEdgeToEdgeControllerImpl.getHasSafeAreaConstraintForTesting());
        verify(mChangeObserver).onSafeAreaConstraintChanged(false);
    }

    @Test
    public void noPadAdjustmentWhenNotDrawingToEdge() {
        mEdgeToEdgeControllerImpl.setIsOptedIntoEdgeToEdgeForTesting(false);
        mEdgeToEdgeControllerImpl.setIsDrawingToEdgeForTesting(false);
        mEdgeToEdgeControllerImpl.setSystemInsetsForTesting(SYSTEM_INSETS);
        mEdgeToEdgeControllerImpl.setKeyboardInsetsForTesting(null);

        MockPadAdjuster mockPadAdjuster = new MockPadAdjuster();
        mEdgeToEdgeControllerImpl.registerAdjuster(mockPadAdjuster);
        mockPadAdjuster.checkInsets(0);
    }

    @Test
    public void toggleKeyboard_properlyPadAdjusters() {
        mEdgeToEdgeControllerImpl.setIsOptedIntoEdgeToEdgeForTesting(true);
        mEdgeToEdgeControllerImpl.setIsDrawingToEdgeForTesting(true);
        mEdgeToEdgeControllerImpl.setSystemInsetsForTesting(SYSTEM_INSETS);
        mEdgeToEdgeControllerImpl.setKeyboardInsetsForTesting(null);

        // Register a new pad adjuster. Without the keyboard or browser controls visible, the insets
        // should just match the system bottom inset.
        MockPadAdjuster mockPadAdjuster = new MockPadAdjuster();
        mEdgeToEdgeControllerImpl.registerAdjuster(mockPadAdjuster);
        mockPadAdjuster.checkInsets(BOTTOM_INSET);

        mEdgeToEdgeControllerImpl.handleWindowInsets(
                mViewMock, SYSTEM_BARS_WINDOW_INSETS_WITH_KEYBOARD);
        mockPadAdjuster.checkInsets(0);

        mEdgeToEdgeControllerImpl.handleWindowInsets(mViewMock, SYSTEM_BARS_WINDOW_INSETS);
        mockPadAdjuster.checkInsets(BOTTOM_INSET);
    }

    @Test
    public void handleBrowserControls_properlyPadAdjusters() {
        int unused = -1;
        int browserControlsHeight = BOTTOM_INSET * 2;

        verify(mBrowserControlsStateProvider, atLeastOnce())
                .addObserver(eq(mEdgeToEdgeControllerImpl));

        mEdgeToEdgeControllerImpl.setIsOptedIntoEdgeToEdgeForTesting(true);
        mEdgeToEdgeControllerImpl.setIsDrawingToEdgeForTesting(true);
        mEdgeToEdgeControllerImpl.setSystemInsetsForTesting(SYSTEM_INSETS);
        mEdgeToEdgeControllerImpl.setKeyboardInsetsForTesting(null);

        // Register a new pad adjuster. Without the keyboard or browser controls visible, the insets
        // should just match the system bottom inset.
        MockPadAdjuster mockPadAdjuster = new MockPadAdjuster();
        mEdgeToEdgeControllerImpl.registerAdjuster(mockPadAdjuster);
        mockPadAdjuster.checkInsets(BOTTOM_INSET);

        // Sometimes, the controls offset can change even when browser controls aren't visible. This
        // should be a no-op.
        mEdgeToEdgeControllerImpl.onControlsOffsetChanged(
                unused,
                unused,
                /* topControlsMinHeightChanged= */ false,
                /* bottomOffset= */ browserControlsHeight,
                unused,
                /* bottomControlsMinHeightChanged= */ false,
                false,
                false);
        mockPadAdjuster.checkInsets(BOTTOM_INSET);

        // Show browser controls.
        mEdgeToEdgeControllerImpl.onBottomControlsHeightChanged(browserControlsHeight, unused);
        mockPadAdjuster.checkInsets(0);

        // Scroll off browser controls gradually.
        mEdgeToEdgeControllerImpl.onControlsOffsetChanged(
                unused,
                unused,
                /* topControlsMinHeightChanged= */ false,
                /* bottomOffset= */ browserControlsHeight / 4,
                unused,
                /* bottomControlsMinHeightChanged= */ false,
                false,
                false);
        mockPadAdjuster.checkInsets(0);
        mEdgeToEdgeControllerImpl.onControlsOffsetChanged(
                unused,
                unused,
                /* topControlsMinHeightChanged= */ false,
                /* bottomOffset= */ browserControlsHeight / 2,
                unused,
                /* bottomControlsMinHeightChanged= */ false,
                false,
                false);
        mockPadAdjuster.checkInsets(0);
        mEdgeToEdgeControllerImpl.onControlsOffsetChanged(
                unused,
                unused,
                /* topControlsMinHeightChanged= */ false,
                /* bottomOffset= */ browserControlsHeight,
                unused,
                /* bottomControlsMinHeightChanged= */ false,
                false,
                false);
        mockPadAdjuster.checkInsets(BOTTOM_INSET);

        // Scroll the browser controls back up.
        mEdgeToEdgeControllerImpl.onControlsOffsetChanged(
                unused,
                unused,
                /* topControlsMinHeightChanged= */ false,
                /* bottomOffset= */ browserControlsHeight / 2,
                unused,
                /* bottomControlsMinHeightChanged= */ false,
                false,
                false);
        mockPadAdjuster.checkInsets(0);
        mEdgeToEdgeControllerImpl.onControlsOffsetChanged(
                unused,
                unused,
                /* topControlsMinHeightChanged= */ false,
                /* bottomOffset= */ 0,
                unused,
                /* bottomControlsMinHeightChanged= */ false,
                false,
                false);
        mockPadAdjuster.checkInsets(0);

        // Hide browser controls.
        mEdgeToEdgeControllerImpl.onBottomControlsHeightChanged(0, unused);
        mockPadAdjuster.checkInsets(BOTTOM_INSET);

        mEdgeToEdgeControllerImpl.unregisterAdjuster(mockPadAdjuster);
    }

    @Test
    public void handleBrowserControlsInTabSwitcher_properlyPadAdjusters() {
        int unused = -1;
        int browserControlsHeight = BOTTOM_INSET * 2;

        mEdgeToEdgeControllerImpl.setIsOptedIntoEdgeToEdgeForTesting(true);
        mEdgeToEdgeControllerImpl.setIsDrawingToEdgeForTesting(true);
        mEdgeToEdgeControllerImpl.setSystemInsetsForTesting(SYSTEM_INSETS);
        mEdgeToEdgeControllerImpl.setKeyboardInsetsForTesting(null);

        // Register a new pad adjuster. Without the keyboard or browser controls visible, the insets
        // should just match the system bottom inset.
        MockPadAdjuster mockPadAdjuster = new MockPadAdjuster();
        mEdgeToEdgeControllerImpl.registerAdjuster(mockPadAdjuster);
        mockPadAdjuster.checkInsets(BOTTOM_INSET);

        // Case 1: Tab Switcher active, Bottom Controls visible. Skip browser control check.
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.TAB_SWITCHER);
        mEdgeToEdgeControllerImpl.onBottomControlsHeightChanged(browserControlsHeight, unused);
        mockPadAdjuster.checkInsets(BOTTOM_INSET);

        // Case 2: Browsing active, Bottom Controls visible. Should check browser control.
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        // Hide browser controls.
        mEdgeToEdgeControllerImpl.onBottomControlsHeightChanged(0, unused);
        mockPadAdjuster.checkInsets(BOTTOM_INSET);
        // Show browser controls.
        mEdgeToEdgeControllerImpl.onBottomControlsHeightChanged(browserControlsHeight, unused);
        mockPadAdjuster.checkInsets(0);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE)
    public void drawToEdge_EdgeToEdgeEverywhereEnabled() {
        Mockito.clearInvocations(mEdgeToEdgeManager);
        mEdgeToEdgeControllerImpl.drawToEdge(
                /* pageOptedIntoEdgeToEdge= */ false, /* changedWindowState= */ true);
        verify(mEdgeToEdgeManager, never()).setContentFitsWindowInsets(anyBoolean());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.EDGE_TO_EDGE_EVERYWHERE)
    public void drawToEdge_EdgeToEdgeEverywhereDisabled() {
        Mockito.clearInvocations(mEdgeToEdgeManager);
        mEdgeToEdgeControllerImpl.drawToEdge(
                /* pageOptedIntoEdgeToEdge= */ false, /* changedWindowState= */ true);
        // #setContentFitsWindowInsets should be called once when EdgeToEdgeEverywhere is disabled.
        verify(mEdgeToEdgeManager, times(1)).setContentFitsWindowInsets(anyBoolean());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.EDGE_TO_EDGE_MONITOR_CONFIGURATIONS)
    public void drawToEdge_configurationChanges_tappable() {
        EdgeToEdgeUtils.setHas3ButtonNavBarForTesting(null);

        assertTrue(EdgeToEdgeUtils.isEdgeToEdgeBottomChinEnabled(mActivity));
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        when(mTab.isNativePage()).thenReturn(false);
        mTabProvider.set(mTab);

        assertTrue(EdgeToEdgeControllerImpl.isSupportedByConfiguration(mActivity, mInsetObserver));
        mEdgeToEdgeControllerImpl.handleWindowInsets(mViewMock, SYSTEM_BARS_WINDOW_INSETS);
        assertTrue(mEdgeToEdgeControllerImpl.isDrawingToEdge());

        // Simulate a tappable navigation bar.
        when(mInsetObserver.getLastRawWindowInsets()).thenReturn(SYSTEM_BARS_WITH_TAPPABLE_NAVBAR);

        assertFalse(EdgeToEdgeControllerImpl.isSupportedByConfiguration(mActivity, mInsetObserver));
        mEdgeToEdgeControllerImpl.handleWindowInsets(mViewMock, SYSTEM_BARS_WITH_TAPPABLE_NAVBAR);
        assertFalse(
                "Drawing to edge should be false when the configuration is not supported.",
                mEdgeToEdgeControllerImpl.isDrawingToEdge());
        assertFalse(
                "Page opted into edge-to-edge should be false when the configuration is not"
                        + " supported.",
                mEdgeToEdgeControllerImpl.isPageOptedIntoEdgeToEdge());
        assertEquals(
                Insets.of(0, TOP_INSET, 0, BOTTOM_INSET),
                mEdgeToEdgeControllerImpl.getAppliedContentViewPaddingForTesting());
    }

    @Test
    @EnableFeatures(ChromeFeatureList.EDGE_TO_EDGE_MONITOR_CONFIGURATIONS)
    public void drawToEdge_configurationChanges_neitherTappableNorGesture() {
        EdgeToEdgeUtils.setHas3ButtonNavBarForTesting(null);

        assertTrue(EdgeToEdgeUtils.isEdgeToEdgeBottomChinEnabled(mActivity));
        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        when(mTab.isNativePage()).thenReturn(false);
        mTabProvider.set(mTab);

        assertTrue(EdgeToEdgeControllerImpl.isSupportedByConfiguration(mActivity, mInsetObserver));
        mEdgeToEdgeControllerImpl.handleWindowInsets(mViewMock, SYSTEM_BARS_WINDOW_INSETS);
        assertTrue(mEdgeToEdgeControllerImpl.isDrawingToEdge());

        // Simulate a tappable navigation bar.
        when(mInsetObserver.getLastRawWindowInsets())
                .thenReturn(SYSTEM_BARS_NEITHER_TAPPABLE_NOR_GESTURE_NAV);

        assertFalse(EdgeToEdgeControllerImpl.isSupportedByConfiguration(mActivity, mInsetObserver));
        mEdgeToEdgeControllerImpl.handleWindowInsets(
                mViewMock, SYSTEM_BARS_NEITHER_TAPPABLE_NOR_GESTURE_NAV);
        assertFalse(
                "Drawing to edge should be false when the configuration is not supported.",
                mEdgeToEdgeControllerImpl.isDrawingToEdge());
        assertFalse(
                "Page opted into edge-to-edge should be false when the configuration is not"
                        + " supported.",
                mEdgeToEdgeControllerImpl.isPageOptedIntoEdgeToEdge());
        assertEquals(
                Insets.of(0, TOP_INSET, 0, BOTTOM_INSET),
                mEdgeToEdgeControllerImpl.getAppliedContentViewPaddingForTesting());
    }

    @Test
    @DisableFeatures(ChromeFeatureList.EDGE_TO_EDGE_USE_BACKUP_NAVBAR_INSETS)
    @EnableFeatures(ChromeFeatureList.EDGE_TO_EDGE_MONITOR_CONFIGURATIONS)
    public void handleWindowInsets_backupInsetsDisabled() {
        TestChangeObserver changeObserver = new TestChangeObserver();
        mEdgeToEdgeControllerImpl.registerObserver(changeObserver);

        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        when(mTab.isNativePage()).thenReturn(false);
        mTabProvider.set(mTab);

        try (var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                "Android.EdgeToEdge.BackupNavbarInsets.EdgeToEdgeController")
                        .build()) {
            when(mInsetObserver.getLastRawWindowInsets()).thenReturn(SYSTEM_BARS_WINDOW_INSETS);
            mEdgeToEdgeControllerImpl.handleWindowInsets(mViewMock, SYSTEM_BARS_WINDOW_INSETS);
        }
        assertTrue(mEdgeToEdgeControllerImpl.isDrawingToEdge());
        assertEquals(
                "The controller should be using the system bar insets.",
                Insets.of(0, TOP_INSET, 0, 0),
                mEdgeToEdgeControllerImpl.getAppliedContentViewPaddingForTesting());
        changeObserver.verify(
                "The system bars are providing a bottom inset and the controller should be"
                        + " drawing toEdge.",
                BOTTOM_INSET,
                /* isDrawingToEdge= */ true,
                /* isPageOptInToEdge= */ false);

        try (var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                "Android.EdgeToEdge.BackupNavbarInsets.EdgeToEdgeController")
                        .build()) {
            when(mInsetObserver.getLastRawWindowInsets())
                    .thenReturn(GESTURE_NAV_INSETS_MISSING_NAVBAR);
            mEdgeToEdgeControllerImpl.handleWindowInsets(
                    mViewMock, GESTURE_NAV_INSETS_MISSING_NAVBAR);
        }
        assertFalse(mEdgeToEdgeControllerImpl.isDrawingToEdge());
        assertEquals(
                "The controller should be using the system bar insets, even though the navbar"
                        + " insets are zero.",
                Insets.of(0, TOP_INSET, 0, 0),
                mEdgeToEdgeControllerImpl.getAppliedContentViewPaddingForTesting());
        changeObserver.verify(
                "The system bars have no bottom inset, and backup insets are disabled, so the"
                        + " controller should not be drawing toEdge.",
                0,
                /* isDrawingToEdge= */ false,
                /* isPageOptInToEdge= */ false);
    }

    @Test
    @EnableFeatures(ChromeFeatureList.EDGE_TO_EDGE_MONITOR_CONFIGURATIONS)
    public void handleWindowInsets_useTappableElementForBackupInsets() {
        TestChangeObserver changeObserver = new TestChangeObserver();
        mEdgeToEdgeControllerImpl.registerObserver(changeObserver);

        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        when(mTab.isNativePage()).thenReturn(false);
        mTabProvider.set(mTab);

        try (var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                "Android.EdgeToEdge.BackupNavbarInsets.EdgeToEdgeController")
                        .build()) {
            when(mInsetObserver.getLastRawWindowInsets())
                    .thenReturn(SYSTEM_BARS_WITH_TAPPABLE_NAVBAR);
            mEdgeToEdgeControllerImpl.handleWindowInsets(
                    mViewMock, SYSTEM_BARS_WITH_TAPPABLE_NAVBAR);
        }
        assertFalse(mEdgeToEdgeControllerImpl.isDrawingToEdge());
        assertEquals(
                "The controller should be using the system bar insets.",
                Insets.of(0, TOP_INSET, 0, BOTTOM_INSET),
                mEdgeToEdgeControllerImpl.getAppliedContentViewPaddingForTesting());
        changeObserver.verify(
                "The system bars are providing a bottom inset.",
                BOTTOM_INSET,
                /* isDrawingToEdge= */ false,
                /* isPageOptInToEdge= */ false);

        try (var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                "Android.EdgeToEdge.BackupNavbarInsets.EdgeToEdgeController")
                        .build()) {
            when(mInsetObserver.getLastRawWindowInsets())
                    .thenReturn(SYSTEM_BARS_WITH_TAPPABLE_MISSING_NAVBAR);
            mEdgeToEdgeControllerImpl.handleWindowInsets(
                    mViewMock, SYSTEM_BARS_WITH_TAPPABLE_MISSING_NAVBAR);
        }
        assertFalse(mEdgeToEdgeControllerImpl.isDrawingToEdge());
        assertEquals(
                "The controller should receive backup navbar insets sourced from the mandatory"
                        + " system gesture insets.",
                Insets.of(0, TOP_INSET, 0, BOTTOM_INSET),
                mEdgeToEdgeControllerImpl.getAppliedContentViewPaddingForTesting());
        changeObserver.verify(
                "The tappable element should be used as a backup for the bottom inset, and the"
                        + " controller should not be drawing toEdge.",
                BOTTOM_INSET,
                /* isDrawingToEdge= */ false,
                /* isPageOptInToEdge= */ false);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.EDGE_TO_EDGE_USE_BACKUP_NAVBAR_INSETS,
        ChromeFeatureList.EDGE_TO_EDGE_MONITOR_CONFIGURATIONS
    })
    public void handleWindowInsets_hasSeenNonZeroNavBar_doNotUseBackupInsets() {
        TestChangeObserver changeObserver = new TestChangeObserver();
        mEdgeToEdgeControllerImpl.registerObserver(changeObserver);

        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        when(mTab.isNativePage()).thenReturn(false);
        mTabProvider.set(mTab);
        when(mInsetObserver.hasSeenNonZeroNavigationBarInsets()).thenReturn(true);

        try (var watcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(
                                "Android.EdgeToEdge.BackupNavbarInsets.EdgeToEdgeController")
                        .build()) {
            when(mInsetObserver.getLastRawWindowInsets()).thenReturn(SYSTEM_BARS_WINDOW_INSETS);
            mEdgeToEdgeControllerImpl.handleWindowInsets(mViewMock, SYSTEM_BARS_WINDOW_INSETS);
        }
        assertTrue(mEdgeToEdgeControllerImpl.isDrawingToEdge());
        assertEquals(
                "The controller should be using the system bar insets.",
                Insets.of(0, TOP_INSET, 0, 0),
                mEdgeToEdgeControllerImpl.getAppliedContentViewPaddingForTesting());
        changeObserver.verify(
                "The system bars are providing a bottom inset, and the controller should be drawing"
                        + " toEdge.",
                BOTTOM_INSET,
                /* isDrawingToEdge= */ true,
                /* isPageOptInToEdge= */ false);

        try (var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.EdgeToEdge.BackupNavbarInsets.EdgeToEdgeController",
                                EdgeToEdgeManager.BackupNavbarInsetsSource.FILTERED_WEAKER_SIGNALS)
                        .build()) {
            when(mInsetObserver.getLastRawWindowInsets())
                    .thenReturn(GESTURE_NAV_INSETS_MISSING_NAVBAR);
            mEdgeToEdgeControllerImpl.handleWindowInsets(
                    mViewMock, GESTURE_NAV_INSETS_MISSING_NAVBAR);
        }
        assertFalse(mEdgeToEdgeControllerImpl.isDrawingToEdge());
        assertEquals(
                "The controller should receive backup navbar insets sourced from the mandatory"
                        + " system gesture insets.",
                Insets.of(0, TOP_INSET, 0, 0),
                mEdgeToEdgeControllerImpl.getAppliedContentViewPaddingForTesting());
        changeObserver.verify(
                "The mandatory system gestures should not be used as a backup after a non-zero"
                        + " navigation bar inset has been seen, and the controller should not be"
                        + " drawing toEdge.",
                0,
                /* isDrawingToEdge= */ false,
                /* isPageOptInToEdge= */ false);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.EDGE_TO_EDGE_USE_BACKUP_NAVBAR_INSETS + ":use_gesture_insets/true",
        ChromeFeatureList.EDGE_TO_EDGE_MONITOR_CONFIGURATIONS
    })
    public void handleWindowInsets_useMandatoryGesturesForBackupInsets() {
        TestChangeObserver changeObserver = new TestChangeObserver();
        mEdgeToEdgeControllerImpl.registerObserver(changeObserver);

        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        when(mTab.isNativePage()).thenReturn(false);
        mTabProvider.set(mTab);
        when(mInsetObserver.hasSeenNonZeroNavigationBarInsets()).thenReturn(false);

        try (var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.EdgeToEdge.BackupNavbarInsets.EdgeToEdgeController",
                                EdgeToEdgeManager.BackupNavbarInsetsSource
                                        .MANDATORY_SYSTEM_GESTURES)
                        .build()) {
            when(mInsetObserver.getLastRawWindowInsets())
                    .thenReturn(GESTURE_NAV_INSETS_MISSING_NAVBAR);
            mEdgeToEdgeControllerImpl.handleWindowInsets(
                    mViewMock, GESTURE_NAV_INSETS_MISSING_NAVBAR);
        }
        assertTrue(mEdgeToEdgeControllerImpl.isDrawingToEdge());
        assertEquals(
                "The controller should receive backup navbar insets sourced from the mandatory"
                        + " system gesture insets.",
                Insets.of(0, TOP_INSET, 0, 0),
                mEdgeToEdgeControllerImpl.getAppliedContentViewPaddingForTesting());
        changeObserver.verify(
                "The mandatory system gestures should be used as a backup for the bottom inset, and"
                        + " the controller should be drawing toEdge.",
                BOTTOM_INSET,
                /* isDrawingToEdge= */ true,
                /* isPageOptInToEdge= */ false);
    }

    @Test
    @EnableFeatures({
        ChromeFeatureList.EDGE_TO_EDGE_USE_BACKUP_NAVBAR_INSETS,
        ChromeFeatureList.EDGE_TO_EDGE_MONITOR_CONFIGURATIONS
    })
    public void handleWindowInsets_noOptionsForBackupInsets() {
        TestChangeObserver changeObserver = new TestChangeObserver();
        mEdgeToEdgeControllerImpl.registerObserver(changeObserver);

        when(mLayoutManager.getActiveLayoutType()).thenReturn(LayoutType.BROWSING);
        when(mTab.isNativePage()).thenReturn(false);
        mTabProvider.set(mTab);
        when(mInsetObserver.hasSeenNonZeroNavigationBarInsets()).thenReturn(false);

        try (var watcher =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(
                                "Android.EdgeToEdge.BackupNavbarInsets.EdgeToEdgeController",
                                EdgeToEdgeManager.BackupNavbarInsetsSource.NO_APPLICABLE_BACKUP)
                        .build()) {
            when(mInsetObserver.getLastRawWindowInsets())
                    .thenReturn(GESTURE_NAV_INSETS_MISSING_ALL_BOTTOM_INSETS);
            mEdgeToEdgeControllerImpl.handleWindowInsets(
                    mViewMock, GESTURE_NAV_INSETS_MISSING_ALL_BOTTOM_INSETS);
        }
        // In practice, it doesn't matter if the controller is drawing toEdge or not, as any applied
        // padding will be zero anyways.
        assertFalse(mEdgeToEdgeControllerImpl.isDrawingToEdge());
        assertEquals(
                "The controller should receive backup navbar insets sourced from the mandatory"
                        + " system gesture insets.",
                Insets.of(0, TOP_INSET, 0, 0),
                mEdgeToEdgeControllerImpl.getAppliedContentViewPaddingForTesting());
        changeObserver.verify(
                "No options have possible backup insets.",
                0,
                /* isDrawingToEdge= */ false,
                /* isPageOptInToEdge= */ false);
    }

    @Test
    @DisableFeatures(ChromeFeatureList.EDGE_TO_EDGE_MONITOR_CONFIGURATIONS)
    public void hasSeenTappableNavigationBarInsets_disabled() {
        EdgeToEdgeUtils.setHas3ButtonNavBarForTesting(null);
        Window window = mockWindowWithRootInsets(SYSTEM_BARS_WITH_TAPPABLE_NAVBAR);
        assertTrue(
                "Insets should be considered has tappable nav bar.",
                EdgeToEdgeUtils.hasTappableNavigationBar(window));

        window = mockWindowWithRootInsets(SYSTEM_BARS_WINDOW_INSETS);
        assertFalse(
                "Insets should be considered not has tappable nav bar.",
                EdgeToEdgeUtils.hasTappableNavigationBar(window));
    }

    @Test
    @EnableFeatures(ChromeFeatureList.EDGE_TO_EDGE_MONITOR_CONFIGURATIONS)
    public void hasSeenTappableNavigationBarInsets() {
        EdgeToEdgeUtils.setHas3ButtonNavBarForTesting(null);
        Window window = mockWindowWithRootInsets(SYSTEM_BARS_WITH_TAPPABLE_NAVBAR);
        assertTrue(
                "Insets should be considered has tappable nav bar.",
                EdgeToEdgeUtils.hasTappableNavigationBar(window));

        window = mockWindowWithRootInsets(SYSTEM_BARS_WINDOW_INSETS);
        assertTrue(
                "Has tappable nav bar is seen, so check should be true.",
                EdgeToEdgeUtils.hasTappableNavigationBar(window));
    }

    @Test
    public void firstContentfulPaint_uploadDebuggingReport() {
        // Standard setup of a Web Tab ToEdge
        when(mTab.isNativePage()).thenReturn(false);
        mTabProvider.set(mTab);
        verifyInteractions(mTab);

        WebContentsObserver webContentsObserver =
                mEdgeToEdgeControllerImpl.getWebContentsObserver();
        assertNotNull(webContentsObserver);
        webContentsObserver.firstContentfulPaintInPrimaryMainFrame(null, /* durationUs= */ 0);
    }

    @Test
    public void pushSafeAreaInsetUpdate_notDrawingToEdge() {
        mEdgeToEdgeControllerImpl.setIsDrawingToEdgeForTesting(false);
        doReturn(false).when(mFullscreenManager).getPersistentFullscreenMode();
        mEdgeToEdgeControllerImpl.onBottomControlsHeightChanged(0, 0);
        assertBottomInsetForSafeArea(0);
    }

    @Test
    public void pushSafeAreaInsetUpdate_drawingToEdgeInFullscreen() {
        mEdgeToEdgeControllerImpl.setIsDrawingToEdgeForTesting(true);
        doReturn(true).when(mFullscreenManager).getPersistentFullscreenMode();
        mEdgeToEdgeControllerImpl.setSystemInsetsForTesting(SYSTEM_INSETS);
        mEdgeToEdgeControllerImpl.onBottomControlsHeightChanged(0, 0);
        assertBottomInsetForSafeArea(0);
    }

    @Test
    public void pushSafeAreaInsetUpdate_drawingToEdgeNoPadding() {
        mEdgeToEdgeControllerImpl.setIsDrawingToEdgeForTesting(true);
        doReturn(false).when(mFullscreenManager).getPersistentFullscreenMode();
        mEdgeToEdgeControllerImpl.setSystemInsetsForTesting(SYSTEM_INSETS);
        mEdgeToEdgeControllerImpl.onBottomControlsHeightChanged(0, 0);
        assertBottomInsetForSafeArea(BOTTOM_INSET);
    }

    @Test
    public void pushSafeAreaInsetUpdate_drawingToEdgeWithKeyboardPadding() {
        mEdgeToEdgeControllerImpl.setIsDrawingToEdgeForTesting(true);
        doReturn(false).when(mFullscreenManager).getPersistentFullscreenMode();
        mEdgeToEdgeControllerImpl.setSystemInsetsForTesting(SYSTEM_INSETS);
        mEdgeToEdgeControllerImpl.setKeyboardInsetsForTesting(IME_INSETS_KEYBOARD);

        mEdgeToEdgeControllerImpl.onBottomControlsHeightChanged(0, 0);

        assertBottomInsetForSafeArea(0);
    }

    void assertToEdgeExpectations() {
        // Pad the top only, bottom is ToEdge.
        verify(mOsWrapper, atLeastOnce())
                .setPadding(any(), eq(0), intThat(Matchers.greaterThan(0)), eq(0), eq(0));
    }

    void assertToNormalExpectations() {
        // Pad the top and the bottom to keep it all normal.
        verify(mOsWrapper, atLeastOnce())
                .setPadding(
                        any(),
                        eq(0),
                        intThat(Matchers.greaterThan(0)),
                        eq(0),
                        intThat(Matchers.greaterThan(0)));
        verify(mEdgeToEdgeManager, atLeastOnce()).setContentFitsWindowInsets(true);
    }

    void assertNoChangeExpectations() {
        verifyNoMoreInteractions(mOsWrapper);
    }

    void verifyInteractions(Tab tab) {
        verify(tab, atLeastOnce()).addObserver(any());
        verify(tab, atLeastOnce()).getWebContents();
    }

    void assertBottomInsetForSafeArea(int bottomInset) {
        ArgumentCaptor<Integer> captor = ArgumentCaptor.forClass(Integer.class);
        verify(mInsetObserver, atLeastOnce()).updateBottomInsetForEdgeToEdge(captor.capture());
        assertEquals(
                "Bottom insets for safe area does not match.",
                bottomInset,
                (int) captor.getValue());
    }

    Window mockWindowWithRootInsets(WindowInsetsCompat rootInsets) {
        View mockView = Mockito.mock(View.class);
        doReturn(rootInsets.toWindowInsets()).when(mockView).getRootWindowInsets();

        Window mockWindow = Mockito.mock(Window.class);
        doReturn(mockView).when(mockWindow).getDecorView();
        return mockWindow;
    }

    // TODO: Verify that the value of the updated insets returned from the
    //  OnApplyWindowInsetsListener is correct.

    private static class MockPadAdjuster implements EdgeToEdgePadAdjuster {
        private int mInset;

        MockPadAdjuster() {}

        @Override
        public void overrideBottomInset(int inset) {
            mInset = inset;
        }

        @Override
        public void destroy() {}

        void checkInsets(int expected) {
            assertEquals("The pad adjuster does not have the expected inset.", expected, mInset);
        }
    }

    private static class TestChangeObserver implements EdgeToEdgeSupplier.ChangeObserver {
        private int mBottomInset;
        private boolean mIsDrawingToEdge;
        private boolean mIsPageOptInToEdge;

        @Override
        public void onToEdgeChange(
                int bottomInset, boolean isDrawingToEdge, boolean isPageOptInToEdge) {
            mBottomInset = bottomInset;
            mIsDrawingToEdge = isDrawingToEdge;
            mIsPageOptInToEdge = isPageOptInToEdge;
        }

        void verify(
                String message,
                int bottomInset,
                boolean isDrawingToEdge,
                boolean isPageOptInToEdge) {
            assertEquals(message, bottomInset, mBottomInset);
            assertEquals(message, isDrawingToEdge, mIsDrawingToEdge);
            assertEquals(message, isPageOptInToEdge, mIsPageOptInToEdge);
        }
    }
}
