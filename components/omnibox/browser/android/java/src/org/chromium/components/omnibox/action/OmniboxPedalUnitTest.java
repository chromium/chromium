// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox.action;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThrows;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.settings.SettingsLauncher.SettingsFragment;
import org.chromium.components.embedder_support.util.UrlConstants;

import java.util.List;

/**
 * Tests for {@link OmniboxPedal}s.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class OmniboxPedalUnitTest {
    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();
    private @Mock OmniboxActionDelegate mDelegate;
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

    /**
     * Verify that a histogram recording the use of particular type of OmniboxPedal has been
     * recorded.
     *
     * @param type The type of Pedal to check for.
     */
    private void checkOmniboxPedalUsageRecorded(@OmniboxPedalType int type) {
        assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Omnibox.SuggestionUsed.Pedal", type));
        assertEquals(1,
                RecordHistogram.getHistogramTotalCountForTesting("Omnibox.SuggestionUsed.Pedal"));
    }

    @Test
    public void executePedal_manageChromeSettings() {
        new OmniboxPedal("hint", OmniboxPedalType.MANAGE_CHROME_SETTINGS).execute(mDelegate);
        verify(mDelegate, times(1)).openSettingsPage(SettingsFragment.MAIN);
        verifyNoMoreInteractions(mDelegate);
        checkOmniboxPedalUsageRecorded(OmniboxPedalType.MANAGE_CHROME_SETTINGS);
    }

    @Test
    public void executePedal_clearBrowsingData() {
        new OmniboxPedal("hint", OmniboxPedalType.CLEAR_BROWSING_DATA).execute(mDelegate);
        verify(mDelegate, times(1)).openSettingsPage(SettingsFragment.CLEAR_BROWSING_DATA);
        verifyNoMoreInteractions(mDelegate);
        checkOmniboxPedalUsageRecorded(OmniboxPedalType.CLEAR_BROWSING_DATA);
    }

    @Test
    public void executePedal_managePasswords() {
        new OmniboxPedal("hint", OmniboxPedalType.MANAGE_PASSWORDS).execute(mDelegate);
        verify(mDelegate, times(1)).openPasswordManager();
        verifyNoMoreInteractions(mDelegate);
        checkOmniboxPedalUsageRecorded(OmniboxPedalType.MANAGE_PASSWORDS);
    }

    @Test
    public void executePedal_updateCreditCard() {
        new OmniboxPedal("hint", OmniboxPedalType.UPDATE_CREDIT_CARD).execute(mDelegate);
        verify(mDelegate, times(1)).openSettingsPage(SettingsFragment.PAYMENT_METHODS);
        verifyNoMoreInteractions(mDelegate);
        checkOmniboxPedalUsageRecorded(OmniboxPedalType.UPDATE_CREDIT_CARD);
    }

    @Test
    public void executePedal_runChromeSafetyCheck() {
        new OmniboxPedal("hint", OmniboxPedalType.RUN_CHROME_SAFETY_CHECK).execute(mDelegate);
        verify(mDelegate, times(1)).openSettingsPage(SettingsFragment.SAFETY_CHECK);
        verifyNoMoreInteractions(mDelegate);
        checkOmniboxPedalUsageRecorded(OmniboxPedalType.RUN_CHROME_SAFETY_CHECK);
    }

    @Test
    public void executePedal_manageSiteSettings() {
        new OmniboxPedal("hint", OmniboxPedalType.MANAGE_SITE_SETTINGS).execute(mDelegate);
        verify(mDelegate, times(1)).openSettingsPage(SettingsFragment.SITE);
        verifyNoMoreInteractions(mDelegate);
        checkOmniboxPedalUsageRecorded(OmniboxPedalType.MANAGE_SITE_SETTINGS);
    }

    @Test
    public void executePedal_manageChromeAccessibility() {
        new OmniboxPedal("hint", OmniboxPedalType.MANAGE_CHROME_ACCESSIBILITY).execute(mDelegate);
        verify(mDelegate, times(1)).openSettingsPage(SettingsFragment.ACCESSIBILITY);
        verifyNoMoreInteractions(mDelegate);
        checkOmniboxPedalUsageRecorded(OmniboxPedalType.MANAGE_CHROME_ACCESSIBILITY);
    }

    @Test
    public void executePedal_launchIncognito() {
        new OmniboxPedal("hint", OmniboxPedalType.LAUNCH_INCOGNITO).execute(mDelegate);
        verify(mDelegate, times(1)).openIncognitoTab();
        verifyNoMoreInteractions(mDelegate);
        checkOmniboxPedalUsageRecorded(OmniboxPedalType.LAUNCH_INCOGNITO);
    }

    @Test
    public void executePedal_viewChromeHistory() {
        new OmniboxPedal("hint", OmniboxPedalType.VIEW_CHROME_HISTORY).execute(mDelegate);
        verify(mDelegate, times(1)).loadPageInCurrentTab(UrlConstants.HISTORY_URL);
        verifyNoMoreInteractions(mDelegate);
        checkOmniboxPedalUsageRecorded(OmniboxPedalType.VIEW_CHROME_HISTORY);
    }

    @Test
    public void executePedal_playChromeDinoGame() {
        new OmniboxPedal("hint", OmniboxPedalType.PLAY_CHROME_DINO_GAME).execute(mDelegate);
        verify(mDelegate, times(1)).loadPageInCurrentTab(UrlConstants.CHROME_DINO_URL);
        verifyNoMoreInteractions(mDelegate);
        checkOmniboxPedalUsageRecorded(OmniboxPedalType.PLAY_CHROME_DINO_GAME);
    }
}
