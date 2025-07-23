// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.autofill.ephemeraltab;

import static org.junit.Assert.assertEquals;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.content_public.browser.WebContents;

/** Tests for {@link PaymentsWindowCoordinator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PaymentsWindowCoordinatorTest {
    @Rule public MockitoRule mockitoRule = MockitoJUnit.rule();
    @Mock private WebContents mWebContents;
    private PaymentsWindowCoordinator mCoordinator;

    @Before
    public void setUp() {
        mCoordinator = new PaymentsWindowCoordinator(mWebContents);
    }

    @Test
    public void testWebContents() {
        assertEquals(mCoordinator.getWebContentsForTesting(), mWebContents);
    }

    @Test
    public void testOpenEphemeralTab() {
        mCoordinator.openEphemeralTab();
    }
}
