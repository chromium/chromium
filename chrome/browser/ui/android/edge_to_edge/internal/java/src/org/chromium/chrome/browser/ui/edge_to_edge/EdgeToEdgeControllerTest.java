// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.edge_to_edge;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Build.VERSION_CODES;

import androidx.appcompat.app.AppCompatActivity;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
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
    private EdgeToEdgeControllerImpl mEdgeToEdgeControllerImpl;

    private ObservableSupplierImpl mObservableSupplierImpl;

    @Mock
    private Tab mTab;

    @Before
    public void setUp() {
        ChromeFeatureList.sDrawEdgeToEdge.setForTesting(true);
        ChromeFeatureList.sDrawNativeEdgeToEdge.setForTesting(false);
        ChromeFeatureList.sDrawWebEdgeToEdge.setForTesting(false);
        var activity = Robolectric.buildActivity(AppCompatActivity.class).setup().get();
        mObservableSupplierImpl = new ObservableSupplierImpl();
        mEdgeToEdgeControllerImpl = (EdgeToEdgeControllerImpl) EdgeToEdgeControllerFactory.create(
                activity, mObservableSupplierImpl);
        Assert.assertNotNull(mEdgeToEdgeControllerImpl);
        MockitoAnnotations.openMocks(this);
        doNothing().when(mTab).addObserver(any());
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

    @Test
    public void onObservingDifferentTab_default() {
        when(mTab.isNativePage()).thenReturn(false);
        mObservableSupplierImpl.set(mTab);
        verify(mTab).isNativePage();
        Assert.assertFalse(mEdgeToEdgeControllerImpl.isToEdge());
    }

    @Test
    public void onObservingDifferentTab_changeToNative() {
        ChromeFeatureList.sDrawNativeEdgeToEdge.setForTesting(true);
        when(mTab.isNativePage()).thenReturn(true);
        mObservableSupplierImpl.set(mTab);
        verify(mTab).isNativePage();
        Assert.assertTrue(mEdgeToEdgeControllerImpl.isToEdge());
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
        Assert.assertTrue(mEdgeToEdgeControllerImpl.isToEdge());
    }

    @Test
    public void onObservingDifferentTab_changeToWeb() {
        ChromeFeatureList.sDrawWebEdgeToEdge.setForTesting(true);
        when(mTab.isNativePage()).thenReturn(false);
        mObservableSupplierImpl.set(mTab);
        verify(mTab).isNativePage();
        Assert.assertTrue(mEdgeToEdgeControllerImpl.isToEdge());
    }

    // TODO: Verify inset or drawn under
}
