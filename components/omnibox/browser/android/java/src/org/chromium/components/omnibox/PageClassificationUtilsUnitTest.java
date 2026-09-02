// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.metrics.OmniboxEventProtosIntDef.PageClassification;

/** Tests for {@link PageClassificationUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PageClassificationUtilsUnitTest {
    @Test
    public void testIsHubOrTabSearch() {
        assertTrue(PageClassificationUtils.isHubOrTabSearch(PageClassification.ANDROID_HUB));
        assertTrue(
                PageClassificationUtils.isHubOrTabSearch(
                        PageClassification.ANDROID_TAB_SEARCH_OVERLAY));
        assertFalse(PageClassificationUtils.isHubOrTabSearch(PageClassification.NTP));
        assertFalse(PageClassificationUtils.isHubOrTabSearch(PageClassification.BLANK));
    }
}
