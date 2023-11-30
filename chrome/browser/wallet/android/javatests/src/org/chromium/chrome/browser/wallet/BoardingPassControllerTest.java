// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.wallet;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;

import org.chromium.base.Callback;
import org.chromium.base.FeatureList;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.url.GURL;

import java.util.List;

/** Tests for {@link BoardingPassController}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BoardingPassControllerTest {

    @Mock private ObservableSupplier<Tab> mMockTabProvider;

    @Mock private Tab mMockTab;

    @Captor private ArgumentCaptor<Callback<Tab>> mTabSupplierCallbackCaptor;

    @Captor private ArgumentCaptor<TabObserver> mTabObserverCaptor;

    private BoardingPassController mController;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        createControllerAndVerify();
    }

    @After
    public void tearDown() {
        destoryControllerAndVerify();
    }

    @Test
    public void detectBoardingPass_urlAllowed() {
        setAllowedUrls("https://google.com, https://abc");
        mTabObserverCaptor
                .getValue()
                .onPageLoadFinished(mMockTab, new GURL("https://abc/boarding"));

        List<ShadowLog.LogItem> logs = ShadowLog.getLogs();
        assertTrue(logs.get(logs.size() - 1).msg.matches(".*Detect boarding pass on url:.*"));
    }

    @Test
    public void detectBoardingPass_urlNotAllowed() {
        setAllowedUrls("");
        mTabObserverCaptor
                .getValue()
                .onPageLoadFinished(mMockTab, new GURL("https://abc/boarding"));

        List<ShadowLog.LogItem> logs = ShadowLog.getLogs();
        assertFalse(logs.get(logs.size() - 1).msg.matches(".*Detect boarding pass on url:.*"));
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

    private void setAllowedUrls(String allowedurls) {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.BOARDING_PASS_DETECTOR,
                "boarding_pass_detector_urls",
                allowedurls);
        FeatureList.setTestValues(testValues);
    }
}
