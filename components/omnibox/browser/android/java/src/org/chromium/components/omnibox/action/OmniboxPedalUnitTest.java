// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox.action;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThrows;

import com.google.common.collect.ImmutableMap;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.action.OmniboxActionType;
import org.chromium.chrome.browser.omnibox.action.OmniboxPedalType;
import org.chromium.components.browser_ui.styles.R;
import org.chromium.components.omnibox.action.OmniboxAction.ChipIcon;

import java.util.Map;

/**
 * Tests for {@link OmniboxPedal}s.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class OmniboxPedalUnitTest {
    @Test
    public void verifyDecorations() {
        ChipIcon defaultIcon = new ChipIcon(R.drawable.fre_product_logo, false);
        Map<Integer, ChipIcon> customResourceMap = ImmutableMap.of(
                OmniboxPedalType.PLAY_CHROME_DINO_GAME, new ChipIcon(R.drawable.ic_dino, true));

        for (int type = OmniboxPedalType.NONE; type < OmniboxPedalType.TOTAL_COUNT; type++) {
            var icon = new OmniboxPedal("", type).getIcon();
            var expectedIcon = customResourceMap.getOrDefault(type, defaultIcon);
            assertEquals(
                    String.format(
                            "Incorrect resource spec while evaluating OmniboxPedalType = %d", type),
                    expectedIcon.iconRes, icon.iconRes);
            assertEquals(String.format("Incorrect tint spec while evaluating OmniboxPedalType = %d",
                                 type),
                    expectedIcon.tintWithTextColor, icon.tintWithTextColor);
        }
    }

    @Test
    public void safeCasting_assertsWithNull() {
        assertThrows(AssertionError.class, () -> OmniboxPedal.from(null));
    }

    @Test
    public void safeCasting_assertsWithWrongClassType() {
        assertThrows(AssertionError.class,
                () -> OmniboxPedal.from(new OmniboxAction(OmniboxActionType.PEDAL, "") {
                    @Override
                    public ChipIcon getIcon() {
                        return null;
                    }
                }));
    }

    @Test
    public void safeCasting_successWithPedal() {
        OmniboxPedal.from(new OmniboxPedal("", OmniboxPedalType.NONE));
    }
}
