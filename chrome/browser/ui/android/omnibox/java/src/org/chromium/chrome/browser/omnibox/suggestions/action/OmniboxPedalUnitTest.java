// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.action;

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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.components.browser_ui.settings.SettingsNavigation.SettingsFragment;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
import org.chromium.components.omnibox.action.OmniboxActionId;
import org.chromium.components.omnibox.action.OmniboxPedalId;

import java.util.List;

/** Tests for {@link OmniboxPedal}s. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class OmniboxPedalUnitTest {
    public @Rule MockitoRule mockitoRule = MockitoJUnit.rule();
    private @Mock OmniboxActionDelegate mDelegate;
    private static List<Integer> sPedalsWithCustomIcons =
            List.of(OmniboxPedalId.PLAY_CHROME_DINO_GAME);

    @Test
    public void creation_usesExpectedCustomIconForDinoGame() {
        assertEquals(
                OmniboxPedal.DINO_GAME_ICON,
                new OmniboxPedal(0, "hint", "accessibility", OmniboxPedalId.PLAY_CHROME_DINO_GAME)
                        .icon);
    }

    @Test
    public void creation_usesDefaultIconForAllNonCustomizedCases() {
        for (int type = OmniboxPedalId.NONE; type < OmniboxPedalId.TOTAL_COUNT; type++) {
            if (sPedalsWithCustomIcons.contains(type)) continue;
            assertEquals(
                    OmniboxAction.DEFAULT_ICON,
                    new OmniboxPedal(0, "hint", "accessibility", type).icon);
        }
    }

    @Test
    public void creation_failsWithNullHint() {
        assertThrows(
                AssertionError.class,
                () ->
                        new OmniboxPedal(
                                0, null, "accessibility", OmniboxPedalId.CLEAR_BROWSING_DATA));
    }

    @Test
    public void creation_failsWithEmptyHint() {
        assertThrows(
                AssertionError.class,
                () -> new OmniboxPedal(0, "", "accessibility", OmniboxPedalId.CLEAR_BROWSING_DATA));
    }

    @Test
    public void safeCasting_assertsWithNull() {
        assertThrows(AssertionError.class, () -> OmniboxPedal.from(null));
    }

    @Test
    public void safeCasting_assertsWithWrongClassType() {
        assertThrows(
                AssertionError.class,
                () ->
                        OmniboxPedal.from(
                                new OmniboxAction(
                                        OmniboxActionId.PEDAL,
                                        0,
                                        "",
                                        "",
                                        null,
                                        R.style.TextAppearance_ChipText) {
                                    @Override
                                    public void execute(OmniboxActionDelegate d) {}
                                }));
    }

    @Test
    public void safeCasting_successWithFactoryBuiltAction() {
        OmniboxPedal.from(
                OmniboxActionFactoryImpl.get()
                        .buildOmniboxPedal(0, "hint", "accessibility", OmniboxPedalId.NONE));
    }

    @Test
    public void executePedal_manageChromeSettings() {
        new OmniboxPedal(0, "hint", "", OmniboxPedalId.MANAGE_CHROME_SETTINGS).execute(mDelegate);
        verify(mDelegate, times(1)).openSettingsPage(SettingsFragment.MAIN);
        verifyNoMoreInteractions(mDelegate);
    }

    @Test
    public void executePedal_clearBrowsingData() {
        new OmniboxPedal(0, "hint", "", OmniboxPedalId.CLEAR_BROWSING_DATA).execute(mDelegate);
        verify(mDelegate).handleClearBrowsingData();
        verifyNoMoreInteractions(mDelegate);
    }

    @Test
    public void executePedal_managePasswords() {
        new OmniboxPedal(0, "hint", "", OmniboxPedalId.MANAGE_PASSWORDS).execute(mDelegate);
        verify(mDelegate, times(1)).openPasswordManager();
        verifyNoMoreInteractions(mDelegate);
    }

    @Test
    public void executePedal_updateCreditCard() {
        new OmniboxPedal(0, "hint", "", OmniboxPedalId.UPDATE_CREDIT_CARD).execute(mDelegate);
        verify(mDelegate, times(1)).openSettingsPage(SettingsFragment.PAYMENT_METHODS);
        verifyNoMoreInteractions(mDelegate);
    }

    @Test
    public void executePedal_runChromeSafetyCheck() {
        new OmniboxPedal(0, "hint", "", OmniboxPedalId.RUN_CHROME_SAFETY_CHECK).execute(mDelegate);
        verify(mDelegate, times(1)).openSettingsPage(SettingsFragment.SAFETY_CHECK);
        verifyNoMoreInteractions(mDelegate);
    }

    @Test
    public void executePedal_manageSiteSettings() {
        new OmniboxPedal(0, "hint", "", OmniboxPedalId.MANAGE_SITE_SETTINGS).execute(mDelegate);
        verify(mDelegate, times(1)).openSettingsPage(SettingsFragment.SITE);
        verifyNoMoreInteractions(mDelegate);
    }

    @Test
    public void executePedal_manageChromeAccessibility() {
        new OmniboxPedal(0, "hint", "", OmniboxPedalId.MANAGE_CHROME_ACCESSIBILITY)
                .execute(mDelegate);
        verify(mDelegate, times(1)).openSettingsPage(SettingsFragment.ACCESSIBILITY);
        verifyNoMoreInteractions(mDelegate);
    }

    @Test
    public void executePedal_launchIncognito() {
        new OmniboxPedal(0, "hint", "", OmniboxPedalId.LAUNCH_INCOGNITO).execute(mDelegate);
        verify(mDelegate, times(1)).openIncognitoTab();
        verifyNoMoreInteractions(mDelegate);
    }

    @Test
    public void executePedal_viewChromeHistory() {
        new OmniboxPedal(0, "hint", "", OmniboxPedalId.VIEW_CHROME_HISTORY).execute(mDelegate);
        verify(mDelegate, times(1)).loadPageInCurrentTab(UrlConstants.HISTORY_URL);
        verifyNoMoreInteractions(mDelegate);
    }

    @Test
    public void executePedal_playChromeDinoGame() {
        new OmniboxPedal(0, "hint", "", OmniboxPedalId.PLAY_CHROME_DINO_GAME).execute(mDelegate);
        verify(mDelegate, times(1)).loadPageInCurrentTab(UrlConstants.CHROME_DINO_URL);
        verifyNoMoreInteractions(mDelegate);
    }
}
