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

import java.util.List;

/**
 * Tests for {@link OmniboxPedal}s.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class OmniboxPedalUnitTest {
    private static List<Integer> sPedalsWithCustomIcons =
            List.of(OmniboxPedalType.PLAY_CHROME_DINO_GAME);

    @Test
    public void creation_usesExpectedCustomIconForDinoGame() {
        assertEquals(OmniboxPedal.DINO_GAME_ICON,
                new OmniboxPedal("hint", OmniboxPedalType.PLAY_CHROME_DINO_GAME).icon);
    }

    @Test
    public void creation_usesDefaultIconForAllNonCustomizedCases() {
        for (int type = OmniboxPedalType.NONE; type < OmniboxPedalType.TOTAL_COUNT; type++) {
            if (sPedalsWithCustomIcons.contains(type)) continue;
            assertEquals(OmniboxAction.DEFAULT_ICON, new OmniboxPedal("hint", type).icon);
        }
    }

    @Test
    public void creation_failsWithNullHint() {
        assertThrows(AssertionError.class,
                () -> new OmniboxPedal(null, OmniboxPedalType.CLEAR_BROWSING_DATA));
    }

    @Test
    public void creation_failsWithEmptyHint() {
        assertThrows(AssertionError.class,
                () -> new OmniboxPedal("", OmniboxPedalType.CLEAR_BROWSING_DATA));
    }

    @Test
    public void safeCasting_assertsWithNull() {
        assertThrows(AssertionError.class, () -> OmniboxPedal.from(null));
    }

    @Test
    public void safeCasting_assertsWithWrongClassType() {
        assertThrows(AssertionError.class,
                () -> OmniboxPedal.from(new OmniboxAction(OmniboxActionType.PEDAL, "", null)));
    }

    @Test
    public void safeCasting_successWithPedal() {
        OmniboxPedal.from(new OmniboxPedal("hint", OmniboxPedalType.NONE));
    }
}
