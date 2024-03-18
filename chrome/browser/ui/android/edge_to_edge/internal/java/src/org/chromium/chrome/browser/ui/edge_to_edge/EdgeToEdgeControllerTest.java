// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
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
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;
import static org.mockito.hamcrest.MockitoHamcrest.intThat;
import static org.robolectric.Shadows.shadowOf;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.graphics.Color;
import android.graphics.Rect;
import android.os.Build.VERSION_CODES;
import android.os.Looper;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsCompat;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.ParameterizedRobolectricTestRunner;
import org.robolectric.ParameterizedRobolectricTestRunner.Parameters;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;

import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRule;
import org.chromium.base.test.util.DisableIf;
import org.chromium.blink.mojom.ViewportFit;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.components.browser_ui.widget.InsetObserver;
import org.chromium.components.browser_ui.widget.InsetObserver.WindowInsetsConsumer;
import org.chromium.components.browser_ui.widget.InsetObserverSupplier;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;

/**
 * Tests the EdgeToEdgeController code. Ideally this would include {@link EdgeToEdgeController},
 * {@link EdgeToEdgeControllerFactory}, along with {@link EdgeToEdgeControllerImpl}
 */
@DisableIf.Build(sdk_is_less_than = VERSION_CODES.R)
@RunWith(ParameterizedRobolectricTestRunner.class)
@Config(
        sdk = VERSION_CODES.R,
        manifest = Config.NONE,
        shadows = EdgeToEdgeControllerTest.ShadowEdgeToEdgeControllerFactory.class)
public class EdgeToEdgeControllerTest {
    @Parameters(name = "InsetManagement_{0}")
    public static Object[] data() {
        return new Object[] {false, true};
    }

    private static final int TOP_INSET = 113;
    private static final int BOTTOM_INSET = 59;

    @SuppressLint("NewApi")
    private static final Insets SYSTEM_INSETS =
            Insets.of(0, TOP_INSET, 0, BOTTOM_INSET); // Typical.

    @Rule(order = -2)
    public BaseRobolectricTestRule mBaseRule = new BaseRobolectricTestRule();

    private Activity mActivity;
    private EdgeToEdgeControllerImpl mEdgeToEdgeControllerImpl;

    private ObservableSupplierImpl<Tab> mTabProvider;

    private UserDataHost mTabDataHost = new UserDataHost();
    private boolean mEnableInsetManagement;

    @Mock private WindowAndroid mWindowAndroid;
    @Mock private InsetObserver mInsetObserver;
    @Mock private Tab mTab;

    @Mock private WebContents mWebContents;

    @Mock private EdgeToEdgeOSWrapper mOsWrapper;

    @Captor private ArgumentCaptor<WindowInsetsConsumer> mWindowInsetsListenerCaptor;

    @Captor private ArgumentCaptor<TabObserver> mTabObserverArgumentCaptor;
    @Captor private ArgumentCaptor<Rect> mSafeAreaRectCaptor;

    @Mock private View mViewMock;

    @Mock private WindowInsetsCompat mWindowInsetsMock;
    @Mock private BrowserControlsStateProvider mBrowserControlsStateProvider;

    @Implements(EdgeToEdgeControllerFactory.class)
    static class ShadowEdgeToEdgeControllerFactory extends EdgeToEdgeControllerFactory {
        @Implementation
        protected static boolean isGestureNavigationMode(Window window) {
            return true;
        }
    }

    // Test with InsetManagement enabled / disabled.
    public EdgeToEdgeControllerTest(boolean withInsetManagement) {
        mEnableInsetManagement = withInsetManagement;
    }

    @Before
    public void setUp() {
        ChromeFeatureList.sDrawEdgeToEdge.setForTesting(true);
        ChromeFeatureList.sDrawNativeEdgeToEdge.setForTesting(false);
        ChromeFeatureList.sDrawWebEdgeToEdge.setForTesting(false);
        ChromeFeatureList.sDrawEdgeToEdgeInsetsManagement.setForTesting(mEnableInsetManagement);

        MockitoAnnotations.openMocks(this);
        InsetObserverSupplier.setInstanceForTesting(mInsetObserver);

        mActivity = Robolectric.buildActivity(AppCompatActivity.class).setup().get();
        mTabProvider = new ObservableSupplierImpl<>();
        mEdgeToEdgeControllerImpl =
                new EdgeToEdgeControllerImpl(
                        mActivity,
                        mWindowAndroid,
                        mTabProvider,
                        mOsWrapper,
                        mBrowserControlsStateProvider);
        assertNotNull(mEdgeToEdgeControllerImpl);
        EdgeToEdgeControllerFactory.setHas3ButtonNavBar(false);

        doNothing().when(mTab).addObserver(any());
        when(mTab.getUserDataHost()).thenReturn(mTabDataHost);
        when(mTab.getWebContents()).thenReturn(mWebContents);

        doNothing().when(mOsWrapper).setDecorFitsSystemWindows(any(), anyBoolean());
        doNothing().when(mOsWrapper).setPadding(any(), anyInt(), anyInt(), anyInt(), anyInt());
        doNothing()
                .when(mOsWrapper)
                .setOnApplyWindowInsetsListener(any(), mWindowInsetsListenerCaptor.capture());
        // Setup needed when mEnableInsetManagement = true.
        doNothing().when(mInsetObserver).addInsetsConsumer(mWindowInsetsListenerCaptor.capture());
        doAnswer(
                        invocationOnMock -> {
                            int bottomInset = invocationOnMock.getArgument(0);
                            mWebContents.setDisplayCutoutSafeArea(new Rect(0, 0, 0, bottomInset));
                            return null;
                        })
                .when(mInsetObserver)
                .updateBottomInsetForEdgeToEdge(anyInt());

        doReturn(SYSTEM_INSETS)
                .when(mWindowInsetsMock)
                .getInsets(WindowInsets.Type.statusBars() + WindowInsets.Type.navigationBars());
    }

    @After
    public void tearDown() {
        mEdgeToEdgeControllerImpl.destroy();
        mEdgeToEdgeControllerImpl = null;
        mTabProvider = null;
    }

    @Test
    public void drawEdgeToEdge_ToNormal() {
        mEdgeToEdgeControllerImpl.drawToEdge(android.R.id.content, false);
    }

    @Test
    public void drawEdgeToEdge_ToEdge() {
        mEdgeToEdgeControllerImpl.drawToEdge(android.R.id.content, true);
    }

    @Test
    public void drawToEdge_assertsBadId() {
        final int badId = 1234;
        Assert.assertThrows(
                AssertionError.class, () -> mEdgeToEdgeControllerImpl.drawToEdge(badId, true));
    }

    /** Test nothing is done when the Feature is not enabled. */
    @Test
    public void onObservingDifferentTab_default() {
        when(mTab.isNativePage()).thenReturn(false);
        mTabProvider.set(mTab);
        verifyInteractions(mTab);
        assertFalse(mEdgeToEdgeControllerImpl.isToEdge());
        assertNoChangeExpectations();
    }

    @Test
    public void onObservingDifferentTab_changeToNative() {
        ChromeFeatureList.sDrawNativeEdgeToEdge.setForTesting(true);
        when(mTab.isNativePage()).thenReturn(true);
        mTabProvider.set(mTab);
        verifyInteractions(mTab);
        assertTrue(mEdgeToEdgeControllerImpl.isToEdge());
        assertToEdgeExpectations();
    }

    @Test
    public void onObservingDifferentTab_changeToTabSwitcher() {
        ChromeFeatureList.sDrawNativeEdgeToEdge.setForTesting(true);
        // For the Tab Switcher we need to switch from some non-null Tab to null.
        when(mTab.isNativePage()).thenReturn(false);
        mTabProvider.set(mTab);
        verifyInteractions(mTab);
        Tab nullForTabSwitcher = null;
        mTabProvider.set(nullForTabSwitcher);
        assertTrue(mEdgeToEdgeControllerImpl.isToEdge());
        assertToEdgeExpectations();
    }

    @Test
    @SuppressLint("NewApi")
    public void onObservingDifferentTab_changeToWebDisabled() {
        ChromeFeatureList.sDrawWebEdgeToEdge.setForTesting(false);
        // First go ToEdge by invoking the changeToTabSwitcher test logic.
        mEdgeToEdgeControllerImpl.setToEdgeForTesting(true);

        // Now test that a Web page causes a transition ToNormal (when Web forcing is disabled).
        mEdgeToEdgeControllerImpl.setSystemInsetsForTesting(SYSTEM_INSETS);
        when(mTab.isNativePage()).thenReturn(false);
        mTabProvider.set(mTab);
        verifyInteractions(mTab);
        assertFalse(mEdgeToEdgeControllerImpl.isToEdge());
        assertToNormalExpectations();
    }

    @Test
    public void onObservingDifferentTab_changeToWebEnabled() {
        ChromeFeatureList.sDrawWebEdgeToEdge.setForTesting(true);
        when(mTab.isNativePage()).thenReturn(false);
        mTabProvider.set(mTab);
        verifyInteractions(mTab);
        assertTrue(mEdgeToEdgeControllerImpl.isToEdge());
        assertToEdgeExpectations();
        assertBottomInsetForSafeArea(SYSTEM_INSETS.bottom);
    }

    @Test
    public void onObservingDifferentTab_changeToWebEnabled_SetsDecor() {
        ChromeFeatureList.sDrawWebEdgeToEdge.setForTesting(true);
        when(mTab.isNativePage()).thenReturn(false);
        mTabProvider.set(mTab);
        verifyInteractions(mTab);
        assertTrue(mEdgeToEdgeControllerImpl.isToEdge());
        assertToEdgeExpectations();
        assertBottomInsetForSafeArea(SYSTEM_INSETS.bottom);
    }

    @Test
    public void onObservingDifferentTab_viewportFitChanged() {
        // Start with web always-enabled.
        ChromeFeatureList.sDrawWebEdgeToEdge.setForTesting(true);
        when(mTab.isNativePage()).thenReturn(false);
        mTabProvider.set(mTab);
        verifyInteractions(mTab);
        assertTrue(mEdgeToEdgeControllerImpl.isToEdge());
        assertToEdgeExpectations();
        assertBottomInsetForSafeArea(SYSTEM_INSETS.bottom);

        // Now switch the viewport-fit value of that page back and forth,
        // with web NOT always enabled
        ChromeFeatureList.sDrawWebEdgeToEdge.setForTesting(false);
        mEdgeToEdgeControllerImpl.getWebContentsObserver().viewportFitChanged(ViewportFit.AUTO);
        assertFalse(mEdgeToEdgeControllerImpl.isToEdge());

        mEdgeToEdgeControllerImpl.getWebContentsObserver().viewportFitChanged(ViewportFit.COVER);
        assertTrue(mEdgeToEdgeControllerImpl.isToEdge());

        mEdgeToEdgeControllerImpl.getWebContentsObserver().viewportFitChanged(ViewportFit.AUTO);
        assertFalse(mEdgeToEdgeControllerImpl.isToEdge());
    }

    /** Test the OSWrapper implementation without mocking it. Native ToEdge. */
    @Test
    public void onObservingDifferentTab_osWrapperImpl() {
        // Force a shift ToEdge by enabling all native and saying it is native.
        ChromeFeatureList.sDrawNativeEdgeToEdge.setForTesting(true);
        ChromeFeatureList.sDrawEdgeToEdgeInsetsManagement.setForTesting(false);
        ObservableSupplierImpl liveSupplier = new ObservableSupplierImpl();
        EdgeToEdgeControllerImpl liveController =
                (EdgeToEdgeControllerImpl)
                        EdgeToEdgeControllerFactory.create(
                                mActivity,
                                mWindowAndroid,
                                liveSupplier,
                                mBrowserControlsStateProvider);
        assertNotNull(liveController);
        liveController.setToEdgeForTesting(false);
        when(mTab.isNativePage()).thenReturn(true);
        liveSupplier.set(mTab);
        // Process the WindowInsetsListener callback.
        shadowOf(Looper.getMainLooper()).idle();
        verifyInteractions(mTab);
        assertTrue(liveController.isToEdge());
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
                                mBrowserControlsStateProvider);
        assertNotNull(liveController);
        liveController.setToEdgeForTesting(true);
        liveController.setSystemInsetsForTesting(SYSTEM_INSETS);
        when(mTab.isNativePage()).thenReturn(false);
        liveSupplier.set(mTab);
        verifyInteractions(mTab);
        assertFalse(liveController.isToEdge());
        // Check the Navigation Bar color, as an indicator that we really changed the window,
        // since we didn't use the OS Wrapper mock.
        assertNotEquals(Color.TRANSPARENT, mActivity.getWindow().getNavigationBarColor());
    }

    /** Test switching to the Tab Switcher, which uses a null Tab. */
    @Test
    public void onObservingDifferentTab_nullTabSwitcher() {
        mEdgeToEdgeControllerImpl.setToEdgeForTesting(true);
        mEdgeToEdgeControllerImpl.setSystemInsetsForTesting(SYSTEM_INSETS);
        mEdgeToEdgeControllerImpl.onTabSwitched(null);
        assertFalse(mEdgeToEdgeControllerImpl.isToEdge());
        // Check the Navigation Bar color, as an indicator that we really changed the window.
        assertNotEquals(Color.TRANSPARENT, mActivity.getWindow().getNavigationBarColor());
        verify(mOsWrapper, times(0)).setDecorFitsSystemWindows(any(), anyBoolean());
        verify(mOsWrapper, times(0)).setOnApplyWindowInsetsListener(any(), any());
        verify(mInsetObserver, times(0)).addInsetsConsumer(any());
        // Pad the top and the bottom to keep it all normal.
        verify(mOsWrapper, times(1))
                .setPadding(
                        any(),
                        eq(0),
                        intThat(Matchers.greaterThan(0)),
                        eq(0),
                        intThat(Matchers.greaterThan(0)));
    }

    /** Test that we update WebContentsObservers when a Tab changes WebContents. */
    @Test
    public void onTabSwitched_onWebContentsSwapped() {
        // Standard setup of a Web Tab ToEdge
        ChromeFeatureList.sDrawWebEdgeToEdge.setForTesting(true);
        when(mTab.isNativePage()).thenReturn(false);
        mTabProvider.set(mTab);
        verifyInteractions(mTab);
        assertTrue(mEdgeToEdgeControllerImpl.isToEdge());

        // Grab the WebContentsObserver, and setup.
        WebContentsObserver initialWebContentsObserver =
                mEdgeToEdgeControllerImpl.getWebContentsObserver();
        when(mTab.getWebContents()).thenReturn(mWebContents);
        doNothing().when(mTab).addObserver(mTabObserverArgumentCaptor.capture());

        // When onTabSwitched is called, we capture the TabObserver created for the Tab.
        mEdgeToEdgeControllerImpl.onTabSwitched(mTab);
        // Simulate the tab getting new WebContents.
        mTabObserverArgumentCaptor.getValue().onWebContentsSwapped(mTab, true, true);
        assertNotNull(initialWebContentsObserver);
        assertNotNull(mEdgeToEdgeControllerImpl.getWebContentsObserver());
        assertNotEquals(
                "onWebContentsSwapped not handling WebContentsObservers correctly",
                initialWebContentsObserver,
                mEdgeToEdgeControllerImpl.getWebContentsObserver());
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

        // Make sure we can still swap to another WebContents.
        tabObserver.onWebContentsSwapped(mTab, true, true);
        WebContentsObserver secondObserver = mEdgeToEdgeControllerImpl.getWebContentsObserver();
        assertNotNull(secondObserver);
        assertNotEquals(firstObserver, secondObserver);
    }

    @Test
    public void onObservingDifferentTab_simple() {
        ChromeFeatureList.sDrawNativeEdgeToEdge.setForTesting(true);
        // For the Tab Switcher we need to switch from some non-null Tab to null.
        when(mTab.isNativePage()).thenReturn(true);
        mTabProvider.set(mTab);
        verifyInteractions(mTab);
        assertToEdgeExpectations();
        assertBottomInsetForSafeArea(SYSTEM_INSETS.bottom);
    }

    @Test
    public void isSupportedConfiguration_default() {
        assertTrue(
                "The default setup should be a supported configuration but it not!",
                EdgeToEdgeControllerFactory.isSupportedConfiguration(
                        Robolectric.buildActivity(AppCompatActivity.class).setup().get()));
    }

    @Test
    @Config(qualifiers = "xlarge")
    public void disabledWhenNotPhone() {
        // Even these always-draw flags do not override the device abilities.
        ChromeFeatureList.sDrawNativeEdgeToEdge.setForTesting(true);
        ChromeFeatureList.sDrawWebEdgeToEdge.setForTesting(true);

        assertFalse(
                EdgeToEdgeControllerFactory.isSupportedConfiguration(
                        Robolectric.buildActivity(AppCompatActivity.class).setup().get()));
    }

    @Test
    public void disabledWhenNotGestureEnabled() {
        // Even these always-draw flags do not override the device abilities.
        ChromeFeatureList.sDrawNativeEdgeToEdge.setForTesting(true);
        ChromeFeatureList.sDrawWebEdgeToEdge.setForTesting(true);

        EdgeToEdgeControllerFactory.setHas3ButtonNavBar(true);
        assertFalse(
                EdgeToEdgeControllerFactory.isSupportedConfiguration(
                        Robolectric.buildActivity(AppCompatActivity.class).setup().get()));
    }

    // Regression test for https://crbug.com/329875254.
    @Test
    public void testViewportFitAfterListenerSet_ToNormal() {
        when(mTab.isNativePage()).thenReturn(false);
        mTabProvider.set(mTab);
        verifyInteractions(mTab);
        assertFalse("Shouldn't be toEdge.", mEdgeToEdgeControllerImpl.isToEdge());

        // Simulate a viewport fit change to kick off WindowInsetConsumer being hooked up.
        mEdgeToEdgeControllerImpl.getWebContentsObserver().viewportFitChanged(ViewportFit.COVER);
        // Simulate another viewport fit change prior to #handleWindowInsets being called.
        mEdgeToEdgeControllerImpl.getWebContentsObserver().viewportFitChanged(ViewportFit.CONTAIN);

        // Simulate insets being available.
        assertNotNull(mWindowInsetsListenerCaptor.getValue());
        mWindowInsetsListenerCaptor.getValue().onApplyWindowInsets(mViewMock, mWindowInsetsMock);
        assertFalse(
                "Shouldn't be toEdge after toggling viewport-fit.",
                mEdgeToEdgeControllerImpl.isToEdge());
        verify(mOsWrapper).setPadding(any(), eq(0), eq(TOP_INSET), eq(0), eq(BOTTOM_INSET));
    }

    @Test
    public void testViewportFitAfterListenerSet_ToEdge() {
        when(mTab.isNativePage()).thenReturn(false);
        mTabProvider.set(mTab);
        verifyInteractions(mTab);
        assertFalse("Shouldn't be toEdge.", mEdgeToEdgeControllerImpl.isToEdge());

        // Simulate a viewport fit change to kick off WindowInsetConsumer being hooked up.
        mEdgeToEdgeControllerImpl.getWebContentsObserver().viewportFitChanged(ViewportFit.COVER);
        // Simulate another viewport fit change prior to #handleWindowInsets being called.
        mEdgeToEdgeControllerImpl.getWebContentsObserver().viewportFitChanged(ViewportFit.CONTAIN);
        // Go back to edge.
        mEdgeToEdgeControllerImpl.getWebContentsObserver().viewportFitChanged(ViewportFit.COVER);

        // Simulate insets being available.
        assertNotNull(mWindowInsetsListenerCaptor.getValue());
        mWindowInsetsListenerCaptor.getValue().onApplyWindowInsets(mViewMock, mWindowInsetsMock);
        assertTrue(
                "Should be toEdge after toggling viewport-fit.",
                mEdgeToEdgeControllerImpl.isToEdge());
        verify(mOsWrapper).setPadding(any(), eq(0), eq(TOP_INSET), eq(0), eq(0));
    }

    void assertToEdgeExpectations() {
        assertNotNull(mWindowInsetsListenerCaptor.getValue());
        mWindowInsetsListenerCaptor.getValue().onApplyWindowInsets(mViewMock, mWindowInsetsMock);
        // Pad the top only, bottom is ToEdge.
        verify(mOsWrapper).setPadding(any(), eq(0), intThat(Matchers.greaterThan(0)), eq(0), eq(0));
        verify(mOsWrapper).setDecorFitsSystemWindows(any(), eq(false));
    }

    void assertToNormalExpectations() {
        verify(mOsWrapper, times(0)).setDecorFitsSystemWindows(any(), anyBoolean());
        verify(mOsWrapper, times(0)).setOnApplyWindowInsetsListener(any(), any());
        verify(mInsetObserver, times(0)).addInsetsConsumer(any());
        // Pad the top and the bottom to keep it all normal.
        verify(mOsWrapper)
                .setPadding(
                        any(),
                        eq(0),
                        intThat(Matchers.greaterThan(0)),
                        eq(0),
                        intThat(Matchers.greaterThan(0)));
    }

    void assertNoChangeExpectations() {
        verifyNoMoreInteractions(mOsWrapper);
    }

    void verifyInteractions(Tab tab) {
        verify(tab, atLeastOnce()).addObserver(any());
        verify(tab, atLeastOnce()).getWebContents();
    }

    void assertBottomInsetForSafeArea(int bottomInset) {
        verify(mWebContents, atLeastOnce()).setDisplayCutoutSafeArea(mSafeAreaRectCaptor.capture());
        Rect safeAreaRect = mSafeAreaRectCaptor.getValue();
        assertEquals(
                "Bottom insets for safe area does not match.", bottomInset, safeAreaRect.bottom);
    }

    // TODO: Verify that the value of the updated insets returned from the
    //  OnApplyWindowInsetsListener is correct.
}
