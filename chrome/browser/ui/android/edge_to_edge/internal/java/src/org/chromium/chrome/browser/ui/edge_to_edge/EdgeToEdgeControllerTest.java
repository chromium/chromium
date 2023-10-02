// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoInteractions;
import static org.mockito.Mockito.when;
import static org.mockito.hamcrest.MockitoHamcrest.intThat;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.graphics.Color;
import android.os.Build.VERSION_CODES;
import android.view.View;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.OnApplyWindowInsetsListener;
import androidx.core.view.WindowInsetsCompat;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.DisableIf;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Tests the EdgeToEdgeController code. Ideally this would include {@link EdgeToEdgeController},
 *  {@link EdgeToEdgeControllerFactory},  along with {@link EdgeToEdgeControllerImpl}
 */
@DisableIf.Build(sdk_is_less_than = VERSION_CODES.R)
@RunWith(BaseRobolectricTestRunner.class)
@Config(sdk = VERSION_CODES.R, manifest = Config.NONE)
public class EdgeToEdgeControllerTest {
    @SuppressLint("NewApi")
    private static final Insets SYSTEM_INSETS = Insets.of(0, 113, 0, 59); // Typical.
    private static final int SYSTEM_BARS = 519; // Actual value of WindowInsets.Type.systemBars().

    private Activity mActivity;
    private EdgeToEdgeControllerImpl mEdgeToEdgeControllerImpl;

    private ObservableSupplierImpl mObservableSupplierImpl;

    @Mock
    private Tab mTab;

    @Mock
    private EdgeToEdgeOSWrapper mOsWrapper;

    @Captor
    private ArgumentCaptor<OnApplyWindowInsetsListener> mWindowInsetsListenerCaptor;

    @Mock
    private View mViewMock;

    @Mock
    private WindowInsetsCompat mWindowInsetsMock;

    @Before
    public void setUp() {
        ChromeFeatureList.sDrawEdgeToEdge.setForTesting(true);
        ChromeFeatureList.sDrawNativeEdgeToEdge.setForTesting(false);
        ChromeFeatureList.sDrawWebEdgeToEdge.setForTesting(false);

        MockitoAnnotations.openMocks(this);

        mActivity = Robolectric.buildActivity(AppCompatActivity.class).setup().get();
        mObservableSupplierImpl = new ObservableSupplierImpl();
        mEdgeToEdgeControllerImpl =
                new EdgeToEdgeControllerImpl(mActivity, mObservableSupplierImpl, mOsWrapper);
        Assert.assertNotNull(mEdgeToEdgeControllerImpl);
        doNothing().when(mTab).addObserver(any());

        doNothing().when(mOsWrapper).setDecorFitsSystemWindows(any(), anyBoolean());
        doNothing().when(mOsWrapper).setPadding(any(), anyInt(), anyInt(), anyInt(), anyInt());
        doNothing().when(mOsWrapper).setNavigationBarColor(any(), anyInt());
        doNothing()
                .when(mOsWrapper)
                .setOnApplyWindowInsetsListener(any(), mWindowInsetsListenerCaptor.capture());

        doReturn(SYSTEM_INSETS).when(mWindowInsetsMock).getInsets(anyInt());
    }

    @After
    public void tearDown() {
        mEdgeToEdgeControllerImpl.destroy();
        mEdgeToEdgeControllerImpl = null;
        mObservableSupplierImpl = null;
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

    /**
     * Test nothing is done when the Feature is not enabled.
     */
    @Test
    public void onObservingDifferentTab_default() {
        when(mTab.isNativePage()).thenReturn(false);
        mObservableSupplierImpl.set(mTab);
        verify(mTab).isNativePage();
        assertFalse(mEdgeToEdgeControllerImpl.isToEdge());
        assertNoChangeExpectations();
    }

    @Test
    public void onObservingDifferentTab_changeToNative() {
        ChromeFeatureList.sDrawNativeEdgeToEdge.setForTesting(true);
        when(mTab.isNativePage()).thenReturn(true);
        mObservableSupplierImpl.set(mTab);
        verify(mTab).isNativePage();
        assertTrue(mEdgeToEdgeControllerImpl.isToEdge());
        assertToEdgeExpectations();
    }

    @Test
    public void onObservingDifferentTab_changeToTabSwitcher() {
        ChromeFeatureList.sDrawNativeEdgeToEdge.setForTesting(true);
        // For the Tab Switcher we need to switch from some non-null Tab to null.
        when(mTab.isNativePage()).thenReturn(false);
        mObservableSupplierImpl.set(mTab);
        verify(mTab).isNativePage();
        Tab nullForTabSwitcher = null;
        mObservableSupplierImpl.set(nullForTabSwitcher);
        assertTrue(mEdgeToEdgeControllerImpl.isToEdge());
        assertToEdgeExpectations();
    }

    @Test
    @SuppressLint("NewApi")
    public void onObservingDifferentTab_changeToWebDisabled() {
        // First go ToEdge by invoking the changeToTabSwitcher test logic.
        mEdgeToEdgeControllerImpl.setToEdge(true);

        // Now test that a Web page causes a transition ToNormal (when Web forcing is disabled).
        ChromeFeatureList.sDrawWebEdgeToEdge.setForTesting(false);
        when(mTab.isNativePage()).thenReturn(false);
        mObservableSupplierImpl.set(mTab);
        verify(mTab).isNativePage(); // Verify it was false.
        assertFalse(mEdgeToEdgeControllerImpl.isToEdge());
        assertToNormalExpectations();
    }

    @Test
    public void onObservingDifferentTab_changeToWebEnabled() {
        ChromeFeatureList.sDrawWebEdgeToEdge.setForTesting(true);
        when(mTab.isNativePage()).thenReturn(false);
        mObservableSupplierImpl.set(mTab);
        verify(mTab).isNativePage();
        assertTrue(mEdgeToEdgeControllerImpl.isToEdge());
        assertToEdgeExpectations();
    }

    /** Test the OSWrapper implementation without mocking it. Native ToEdge. */
    @Test
    public void onObservingDifferentTab_osWrapperImpl() {
        ChromeFeatureList.sDrawNativeEdgeToEdge.setForTesting(true);
        EdgeToEdgeControllerImpl liveController =
                (EdgeToEdgeControllerImpl) EdgeToEdgeControllerFactory.create(
                        mActivity, mObservableSupplierImpl);
        Assert.assertNotNull(liveController);
        when(mTab.isNativePage()).thenReturn(true);
        mObservableSupplierImpl.set(mTab);
        verify(mTab, times(2)).isNativePage();
        assertTrue(liveController.isToEdge());
        // Check the Navigation Bar color, as an indicator that we really changed the window.
        assertEquals(Color.TRANSPARENT, mActivity.getWindow().getNavigationBarColor());
    }

    /** Test the OSWrapper implementation without mocking it. Native ToNormal. */
    @Test
    public void onObservingDifferentTab_osWrapperImplToNormal() {
        ChromeFeatureList.sDrawNativeEdgeToEdge.setForTesting(false);
        EdgeToEdgeControllerImpl liveController =
                (EdgeToEdgeControllerImpl) EdgeToEdgeControllerFactory.create(
                        mActivity, mObservableSupplierImpl);
        Assert.assertNotNull(liveController);
        when(mTab.isNativePage()).thenReturn(true);
        mObservableSupplierImpl.set(mTab);
        verify(mTab, times(2)).isNativePage();
        assertFalse(liveController.isToEdge());
        // Check the Navigation Bar color, as an indicator that we really changed the window.
        assertNotEquals(Color.TRANSPARENT, mActivity.getWindow().getNavigationBarColor());
    }

    void assertToEdgeExpectations() {
        verify(mOsWrapper).setNavigationBarColor(any(), eq(Color.TRANSPARENT));
        verify(mOsWrapper).setDecorFitsSystemWindows(any(), eq(false));
        verify(mOsWrapper).setOnApplyWindowInsetsListener(any(), any());
        mWindowInsetsListenerCaptor.getValue().onApplyWindowInsets(mViewMock, mWindowInsetsMock);
        // Pad the top only, bottom is ToEdge.
        verify(mOsWrapper).setPadding(any(), eq(0), intThat(Matchers.greaterThan(0)), eq(0), eq(0));
    }

    void assertToNormalExpectations() {
        verify(mOsWrapper).setNavigationBarColor(any(), eq(Color.BLACK));
        verify(mOsWrapper).setDecorFitsSystemWindows(any(), eq(true));
        verify(mOsWrapper).setOnApplyWindowInsetsListener(any(), any());
        mWindowInsetsListenerCaptor.getValue().onApplyWindowInsets(mViewMock, mWindowInsetsMock);
        // Pad the top and the bottom to keep it all normal.
        verify(mOsWrapper)
                .setPadding(any(), eq(0), intThat(Matchers.greaterThan(0)), eq(0),
                        intThat(Matchers.greaterThan(0)));
    }

    void assertNoChangeExpectations() {
        verifyNoInteractions(mOsWrapper);
    }

    // TODO: Verify that the value of the updated insets returned from the
    //  OnApplyWindowInsetsListener is correct.
}
