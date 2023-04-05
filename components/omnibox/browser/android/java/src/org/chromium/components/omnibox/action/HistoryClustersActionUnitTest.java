// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox.action;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThrows;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Tests for {@link HistoryClustersAction}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HistoryClustersActionUnitTest {
    @Test
    public void creation_usesExpectedIcon() {
        var action = new HistoryClustersAction("hint", "query");
        assertEquals(HistoryClustersAction.JOURNEYS_ICON, action.icon);
    }

    @Test
    public void creation_failsWithNullHint() {
        assertThrows(AssertionError.class, () -> new HistoryClustersAction(null, "query"));
    }

    @Test
    public void creation_failsWithEmptyHint() {
        assertThrows(AssertionError.class, () -> new HistoryClustersAction("", "query"));
    }

    @Test
    public void creation_failsWithNullQuery() {
        assertThrows(AssertionError.class, () -> new HistoryClustersAction("hint", null));
    }

    @Test
    public void creation_failsWithEmptyQuery() {
        assertThrows(AssertionError.class, () -> new HistoryClustersAction("hint", ""));
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
                                new OmniboxAction(OmniboxActionType.HISTORY_CLUSTERS, "", null)));
    }

    @Test
    public void safeCasting_successWithHistoryClusters() {
        HistoryClustersAction.from(new HistoryClustersAction("hint", "query"));
    }
}
