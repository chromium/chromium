// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.action;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.components.browser_ui.settings.SettingsNavigation.SettingsFragment;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.omnibox.R;
import org.chromium.components.omnibox.action.OmniboxAction;
import org.chromium.components.omnibox.action.OmniboxActionDelegate;
import org.chromium.components.omnibox.action.OmniboxActionId;
import org.chromium.components.omnibox.action.OmniboxPedalId;

/**
 * Omnibox Actions are additional actions associated with Omnibox Matches. For more information,
 * please check on OmniboxAction class definition on native side.
 */
public class OmniboxPedal extends OmniboxAction {
    @VisibleForTesting
    static final ChipIcon DINO_GAME_ICON = new ChipIcon(R.drawable.action_dino_game, true);

    /** The type of the underlying pedal. */
    public final @OmniboxPedalId int pedalId;

    public OmniboxPedal(
            long nativeInstance,
            @NonNull String hint,
            @NonNull String accessibilityHint,
            @OmniboxPedalId int pedalId) {
        super(
                OmniboxActionId.PEDAL,
                nativeInstance,
                hint,
                accessibilityHint,
                pedalId == OmniboxPedalId.PLAY_CHROME_DINO_GAME
                        ? DINO_GAME_ICON
                        : OmniboxAction.DEFAULT_ICON,
                R.style.TextAppearance_ChipText);
        this.pedalId = pedalId;
    }

    @Override
    public void execute(@NonNull OmniboxActionDelegate delegate) {
        switch (pedalId) {
            case OmniboxPedalId.MANAGE_CHROME_SETTINGS:
                delegate.openSettingsPage(SettingsFragment.MAIN);
                break;
            case OmniboxPedalId.CLEAR_BROWSING_DATA:
                delegate.handleClearBrowsingData();
                break;
            case OmniboxPedalId.UPDATE_CREDIT_CARD:
                delegate.openSettingsPage(SettingsFragment.PAYMENT_METHODS);
                break;
            case OmniboxPedalId.RUN_CHROME_SAFETY_CHECK:
                delegate.openSettingsPage(SettingsFragment.SAFETY_CHECK);
                break;
            case OmniboxPedalId.MANAGE_SITE_SETTINGS:
                delegate.openSettingsPage(SettingsFragment.SITE);
                break;
            case OmniboxPedalId.MANAGE_CHROME_ACCESSIBILITY:
                delegate.openSettingsPage(SettingsFragment.ACCESSIBILITY);
                break;
            case OmniboxPedalId.VIEW_CHROME_HISTORY:
                delegate.loadPageInCurrentTab(UrlConstants.HISTORY_URL);
                break;
            case OmniboxPedalId.PLAY_CHROME_DINO_GAME:
                delegate.loadPageInCurrentTab(UrlConstants.CHROME_DINO_URL);
                break;
            case OmniboxPedalId.MANAGE_PASSWORDS:
                delegate.openPasswordManager();
                break;
            case OmniboxPedalId.LAUNCH_INCOGNITO:
                delegate.openIncognitoTab();
                break;
        }
    }

    /**
     * Cast supplied OmniboxAction to OmniboxPedal. Requires the supplied input to be a valid
     * instance of an OmniboxPedal whose actionId is the PEDAL.
     */
    public static @NonNull OmniboxPedal from(@NonNull OmniboxAction action) {
        assert action != null;
        assert action.actionId == OmniboxActionId.PEDAL;
        assert action instanceof OmniboxPedal;
        return (OmniboxPedal) action;
    }
}
