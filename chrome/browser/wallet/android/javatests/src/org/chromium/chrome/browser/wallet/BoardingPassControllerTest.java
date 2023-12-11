// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.wallet;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.url.GURL;

/** Tests for {@link BoardingPassController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BoardingPassControllerTest {

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private ObservableSupplier<Tab> mMockTabProvider;

    @Mock private Tab mMockTab;

    @Mock private BoardingPassBridge.Natives mMockBoardingPassBridgeJni;

    @Captor private ArgumentCaptor<Callback<Tab>> mTabSupplierCallbackCaptor;

    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;

    private BoardingPassController mController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(BoardingPassBridgeJni.TEST_HOOKS, mMockBoardingPassBridgeJni);
        createControllerAndVerify();
    }

    @After
    public void tearDown() {
        destoryControllerAndVerify();
    }

    @Test
    public void detectBoardingPass_urlAllowed() {
        when(mMockBoardingPassBridgeJni.shouldDetect(any())).thenReturn(true);
        mTabObserverCaptor
                .getValue()
                .onPageLoadFinished(mMockTab, new GURL("https://abc/boarding"));

        verify(mMockBoardingPassBridgeJni).detectBoardingPass(any(), any());
        verify(mMockBoardingPassBridgeJni).shouldDetect(eq("https://abc/boarding"));
    }

    @Test
    public void detectBoardingPass_urlNotAllowed() {
        when(mMockBoardingPassBridgeJni.shouldDetect(any())).thenReturn(false);
        mTabObserverCaptor.getValue().onPageLoadFinished(mMockTab, new GURL("https://abc/123"));

        verify(mMockBoardingPassBridgeJni, never()).detectBoardingPass(any(), any());
        verify(mMockBoardingPassBridgeJni).shouldDetect(eq("https://abc/123"));
    }

    private void createControllerAndVerify() {
        mController = new BoardingPassController(mMockTabProvider);
        verify(mMockTabProvider).addObserver(mTabSupplierCallbackCaptor.capture());
        mTabSupplierCallbackCaptor.getValue().onResult(mMockTab);
        verify(mMockTab).addObserver(mTabObserverCaptor.capture());
    }

    private void destoryControllerAndVerify() {
        mController.destroy();
        verify(mMockTab).removeObserver(mTabObserverCaptor.getValue());
        verify(mMockTabProvider).removeObserver(mTabSupplierCallbackCaptor.getValue());
    }
}
