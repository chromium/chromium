// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.enterprise_signals_disclaimer;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/** Unit tests for {@link BottomSheetDisclaimerHost}. */
@RunWith(BaseRobolectricTestRunner.class)
public class BottomSheetDisclaimerHostUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private BottomSheetController mBottomSheetController;
    @Mock private EnterpriseSignalsDisclaimerBottomSheetView mSheetContent;

    @Captor private ArgumentCaptor<Runnable> mDestroyedCallbackCaptor;

    private BottomSheetDisclaimerHost mHost;

    @Before
    public void setUp() {
        mHost = new BottomSheetDisclaimerHost(mBottomSheetController, mSheetContent);
    }

    @Test
    public void testShow_requestsShowContentAndSetsActive() {
        Assert.assertFalse(mHost.isActive());

        mHost.show();

        Assert.assertTrue(mHost.isActive());
        verify(mSheetContent).setOnDestroyedCallback(any());
        verify(mBottomSheetController).requestShowContent(eq(mSheetContent), eq(true));
    }

    @Test
    public void testHide_hidesContentAndSetsInactive() {
        mHost.show();
        Assert.assertTrue(mHost.isActive());

        mHost.hide();

        Assert.assertFalse(mHost.isActive());
        verify(mBottomSheetController).hideContent(eq(mSheetContent), eq(false));
    }

    @Test
    public void testDestroyCallback_setsInactive() {
        mHost.show();
        Assert.assertTrue(mHost.isActive());

        verify(mSheetContent).setOnDestroyedCallback(mDestroyedCallbackCaptor.capture());
        Runnable callback = mDestroyedCallbackCaptor.getValue();
        Assert.assertNotNull(callback);

        callback.run();

        Assert.assertFalse(mHost.isActive());
    }
}
