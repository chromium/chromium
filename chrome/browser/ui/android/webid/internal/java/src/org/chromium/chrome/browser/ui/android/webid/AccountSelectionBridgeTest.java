// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.android.webid;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.blink.mojom.RpMode;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

/** Tests the brand icon sizes that {@link AccountSelectionBridge} reports to native. */
@RunWith(BaseRobolectricTestRunner.class)
public class AccountSelectionBridgeTest {
    /** Tests that without `FedCmIdpLinks`, the brand icons of each RP mode fit its own sheet. */
    @Test
    @DisableFeatures(ChromeFeatureList.FED_CM_IDP_LINKS)
    public void testBrandIconSizes() {
        assertTrue(
                AccountSelectionBridge.getBrandIconIdealSize(RpMode.PASSIVE)
                        < AccountSelectionBridge.getBrandIconIdealSize(RpMode.ACTIVE));
        assertTrue(
                AccountSelectionBridge.getBrandIconMinimumSize(RpMode.PASSIVE)
                        < AccountSelectionBridge.getBrandIconMinimumSize(RpMode.ACTIVE));
    }

    /**
     * Tests that with `FedCmIdpLinks`, passive mode brand icons are fetched at the active mode
     * sheet's size, since the request can ask for permission in that sheet, while the minimum size
     * stays the passive mode sheet's.
     */
    @Test
    @EnableFeatures(ChromeFeatureList.FED_CM_IDP_LINKS)
    public void testPassiveModeBrandIconSizesWithIdpLinks() {
        assertEquals(
                AccountSelectionBridge.getBrandIconIdealSize(RpMode.ACTIVE),
                AccountSelectionBridge.getBrandIconIdealSize(RpMode.PASSIVE));
        assertTrue(
                AccountSelectionBridge.getBrandIconMinimumSize(RpMode.PASSIVE)
                        < AccountSelectionBridge.getBrandIconMinimumSize(RpMode.ACTIVE));
    }
}
