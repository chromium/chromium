// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill.ephemeraltab;

import static org.junit.Assert.assertNotNull;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.GURL;

/** Tests for {@link PaymentsWindowBridge}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PaymentsWindowBridgeTest {
    private static final String TAB_TITLE = "Issuer Name";
    private static final GURL ISSUER_URL = new GURL("https://www.example.com/");

    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();
    @Mock private WebContents mWebContents;
    @Mock private PaymentsWindowCoordinator mPaymentsWindowCoordinator;

    private PaymentsWindowBridge mPaymentsWindowBridge;

    @Before
    public void setUp() {
        mPaymentsWindowBridge = new PaymentsWindowBridge(mWebContents);
    }

    @Test
    public void testPaymentsWindowCoordinator() {
        assertNotNull(mPaymentsWindowBridge.getPaymentsWindowCoordinatorForTesting());
    }

    @Test
    public void testOpenEphemeralTab() {
        mPaymentsWindowBridge.setPaymentsWindowCoordinatorForTesting(mPaymentsWindowCoordinator);

        mPaymentsWindowBridge.openEphemeralTab(ISSUER_URL, TAB_TITLE);

        verify(mPaymentsWindowCoordinator).openEphemeralTab(ISSUER_URL, TAB_TITLE);
    }

    @Test
    public void testCloseEphemeralTab() {
        mPaymentsWindowBridge.setPaymentsWindowCoordinatorForTesting(mPaymentsWindowCoordinator);

        mPaymentsWindowBridge.closeEphemeralTab();

        verify(mPaymentsWindowCoordinator).closeEphemeralTab();
    }
}
