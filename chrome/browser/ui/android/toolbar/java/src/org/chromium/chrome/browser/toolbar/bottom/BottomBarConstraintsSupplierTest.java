// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.bottom;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNullableObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.native_page.NativePage;
import org.chromium.ui.base.TestActivity;

/** Unit tests for {@link BottomBarConstraintsSupplier}. */
@RunWith(BaseRobolectricTestRunner.class)
@EnableFeatures({ChromeFeatureList.ANDROID_BOTTOM_BAR + ":disable_on_ntp/false"})
public class BottomBarConstraintsSupplierTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private BottomBarConstraintsSupplier mSupplier;
    private SettableNullableObservableSupplier<@BrowserControlsState Integer> mConstraintsSupplier;
    private SettableNullableObservableSupplier<Tab> mCurrentTabSupplier;
    private TestActivity mActivity;

    @Mock private Tab mTab;
    @Mock private NativePage mNativePage;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).setup().get();
        mConstraintsSupplier = ObservableSuppliers.createNullable();
        mCurrentTabSupplier = ObservableSuppliers.createNullable();

        mSupplier =
                new BottomBarConstraintsSupplier(
                        mConstraintsSupplier, mCurrentTabSupplier, mActivity);
    }

    @Test
    public void testConstraintsFollowSupplier_NonNtp() {
        when(mTab.getNativePage()).thenReturn(null);
        mCurrentTabSupplier.set(mTab);

        mConstraintsSupplier.set(BrowserControlsState.SHOWN);
        assertEquals(Integer.valueOf(BrowserControlsState.SHOWN), mSupplier.get());

        mConstraintsSupplier.set(BrowserControlsState.HIDDEN);
        assertEquals(Integer.valueOf(BrowserControlsState.HIDDEN), mSupplier.get());
    }

    @Test
    public void testConstraintsForceBoth_Ntp() {
        when(mTab.getNativePage()).thenReturn(mNativePage);
        when(mNativePage.getHost()).thenReturn("newtab");
        mCurrentTabSupplier.set(mTab);

        mConstraintsSupplier.set(BrowserControlsState.SHOWN);
        assertEquals(Integer.valueOf(BrowserControlsState.BOTH), mSupplier.get());

        mConstraintsSupplier.set(BrowserControlsState.HIDDEN);
        assertEquals(Integer.valueOf(BrowserControlsState.BOTH), mSupplier.get());
    }

    @Test
    public void testTransitionToNtp_UpdatesConstraints() {
        // Start on a normal page
        when(mTab.getNativePage()).thenReturn(null);
        mCurrentTabSupplier.set(mTab);
        mConstraintsSupplier.set(BrowserControlsState.SHOWN);

        // Verify initial state is SHOWN
        assertEquals(Integer.valueOf(BrowserControlsState.SHOWN), mSupplier.get());

        // Capture the observer
        ArgumentCaptor<org.chromium.chrome.browser.tab.TabObserver> observerCaptor =
                ArgumentCaptor.forClass(org.chromium.chrome.browser.tab.TabObserver.class);
        verify(mTab).addObserver(observerCaptor.capture());

        // Simulate navigation to NTP
        when(mTab.getNativePage()).thenReturn(mNativePage);
        when(mNativePage.getHost()).thenReturn("newtab");
        observerCaptor.getValue().onContentChanged(mTab);

        // Verify state is updated to BOTH
        assertEquals(Integer.valueOf(BrowserControlsState.BOTH), mSupplier.get());
    }
}
