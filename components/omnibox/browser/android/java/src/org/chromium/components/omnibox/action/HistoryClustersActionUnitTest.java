// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox.action;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThrows;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.action.OmniboxActionType;
import org.chromium.components.browser_ui.styles.R;

/**
 * Tests for {@link HistoryClustersAction}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HistoryClustersActionUnitTest {
    @Test
    public void verifyDecorations_supportedPedalTypes() {
        var action = new HistoryClustersAction("hint", "query");
        assertEquals(R.drawable.ic_journeys, action.getIcon().iconRes);
        assertTrue(action.getIcon().tintWithTextColor);
    }

    @Test
    public void safeCasting_assertsWithNull() {
        assertThrows(AssertionError.class, () -> HistoryClustersAction.from(null));
    }

    @Test
    public void safeCasting_assertsWithWrongClassType() {
        assertThrows(AssertionError.class,
                ()
                        -> HistoryClustersAction.from(
                                new OmniboxAction(OmniboxActionType.HISTORY_CLUSTERS, "") {
                                    @Override
                                    public ChipIcon getIcon() {
                                        return null;
                                    }
                                }));
    }

    @Test
    public void safeCasting_successWithHistoryClusters() {
        HistoryClustersAction.from(new HistoryClustersAction("hint", "query"));
    }
}
