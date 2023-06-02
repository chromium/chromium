// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.action;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThrows;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
import org.chromium.components.omnibox.action.OmniboxActionId;

/**
 * Tests for {@link HistoryClustersAction}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class HistoryClustersActionUnitTest {
    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();
    private @Mock OmniboxActionDelegate mDelegate;

    @Test
    public void creation_usesExpectedIcon() {
        var action = new HistoryClustersAction(0, "hint", "accessibility", "query");
        assertEquals(HistoryClustersAction.JOURNEYS_ICON, action.icon);
    }

    @Test
    public void creation_failsWithNullHint() {
        assertThrows(AssertionError.class,
                () -> new HistoryClustersAction(0, null, "accessibility", "query"));
    }

    @Test
    public void creation_failsWithEmptyHint() {
        assertThrows(AssertionError.class,
                () -> new HistoryClustersAction(0, "", "accessibility", "query"));
    }

    @Test
    public void creation_failsWithNullQuery() {
        assertThrows(AssertionError.class,
                () -> new HistoryClustersAction(0, "hint", "accessibility", null));
    }

    @Test
    public void creation_failsWithEmptyQuery() {
        assertThrows(AssertionError.class,
                () -> new HistoryClustersAction(0, "hint", "accessibility", ""));
    }

    @Test
    public void safeCasting_assertsWithNull() {
        assertThrows(AssertionError.class, () -> HistoryClustersAction.from(null));
    }

    @Test
    public void safeCasting_assertsWithWrongClassType() {
        assertThrows(AssertionError.class,
                ()
                        -> HistoryClustersAction.from(new OmniboxAction(
                                OmniboxActionId.HISTORY_CLUSTERS, 0, "", "", null) {
                    @Override
                    public void execute(OmniboxActionDelegate d) {}
                }));
    }

    @Test
    public void safeCasting_successWithFactoryBuiltAction() {
        HistoryClustersAction.from(OmniboxActionFactoryImpl.get().buildHistoryClustersAction(
                0, "hint", "accessibility", "query"));
    }

    @Test
    public void executeHistoryClusters() {
        String testJourneyName = "example journey name";
        var action = new HistoryClustersAction(0, "hint", "accessibility", testJourneyName);
        action.execute(mDelegate);
        verify(mDelegate).openHistoryClustersPage(testJourneyName);
        verifyNoMoreInteractions(mDelegate);
    }
}
