// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.omnibox.action;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.components.browser_ui.settings.SettingsLauncher.SettingsFragment;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.omnibox.OmniboxMetrics;
import org.chromium.components.omnibox.R;

/**
 * Omnibox Actions are additional actions associated with Omnibox Matches. For more information,
 * please check on OmniboxAction class definition on native side.
 */
public class OmniboxPedal extends OmniboxAction {
    @VisibleForTesting
    static final ChipIcon DINO_GAME_ICON = new ChipIcon(R.drawable.action_dino_game, true);
    /** The type of the underlying pedal. */
    public final @OmniboxPedalType int pedalId;

    @CalledByNative
    public OmniboxPedal(@NonNull String hint, @OmniboxPedalType int pedalId) {
        super(OmniboxActionType.PEDAL, hint,
                pedalId == OmniboxPedalType.PLAY_CHROME_DINO_GAME ? DINO_GAME_ICON : null);
        this.pedalId = pedalId;
    }

    @Override
    public void execute(@NonNull OmniboxActionDelegate delegate) {
        switch (pedalId) {
            case OmniboxPedalType.MANAGE_CHROME_SETTINGS:
                delegate.openSettingsPage(SettingsFragment.MAIN);
                break;
            case OmniboxPedalType.CLEAR_BROWSING_DATA:
                delegate.openSettingsPage(SettingsFragment.CLEAR_BROWSING_DATA);
                break;
            case OmniboxPedalType.UPDATE_CREDIT_CARD:
                delegate.openSettingsPage(SettingsFragment.PAYMENT_METHODS);
                break;
            case OmniboxPedalType.RUN_CHROME_SAFETY_CHECK:
                delegate.openSettingsPage(SettingsFragment.SAFETY_CHECK);
                break;
            case OmniboxPedalType.MANAGE_SITE_SETTINGS:
                delegate.openSettingsPage(SettingsFragment.SITE);
                break;
            case OmniboxPedalType.MANAGE_CHROME_ACCESSIBILITY:
                delegate.openSettingsPage(SettingsFragment.ACCESSIBILITY);
                break;
            case OmniboxPedalType.VIEW_CHROME_HISTORY:
                delegate.loadPageInCurrentTab(UrlConstants.HISTORY_URL);
                break;
            case OmniboxPedalType.PLAY_CHROME_DINO_GAME:
                delegate.loadPageInCurrentTab(UrlConstants.CHROME_DINO_URL);
                break;
            case OmniboxPedalType.MANAGE_PASSWORDS:
                delegate.openPasswordManager();
                break;
            case OmniboxPedalType.LAUNCH_INCOGNITO:
                delegate.openIncognitoTab();
                break;
        }
        OmniboxMetrics.recordPedalUsed(pedalId);
    }

    /**
     * Cast supplied OmniboxAction to OmniboxPedal.
     * Requires the supplied input to be a valid instance of an OmniboxPedal whose
     * actionId is the PEDAL.
     */
    public static @NonNull OmniboxPedal from(@NonNull OmniboxAction action) {
        assert action != null;
        assert action.actionId == OmniboxActionType.PEDAL;
        assert action instanceof OmniboxPedal;
        return (OmniboxPedal) action;
    }
}
