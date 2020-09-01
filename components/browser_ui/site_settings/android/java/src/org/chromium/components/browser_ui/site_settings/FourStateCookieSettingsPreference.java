// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.RadioGroup;

import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.components.browser_ui.settings.ManagedPreferencesUtils;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;
import org.chromium.components.content_settings.CookieControlsMode;

/**
 * A 4-state radio group Preference used for the Cookies subpage of SiteSettings.
 */
public class FourStateCookieSettingsPreference
        extends Preference implements RadioGroup.OnCheckedChangeListener {
    public enum CookieSettingsState {
        UNINITIALIZED,
        ALLOW,
        BLOCK_THIRD_PARTY_INCOGNITO,
        BLOCK_THIRD_PARTY,
        BLOCK
    }

    /**
     * Signals used to determine the view and button states.
     */
    public static class Params {
        // Whether the cookies content setting is enabled.
        public boolean allowCookies;
        // An enum indicating when to block third-party cookies.
        public @CookieControlsMode int cookieControlsMode;

        // Whether the cookies content setting is enforced.
        public boolean cookiesContentSettingEnforced;
        //  Whether third-party blocking is enforced.
        public boolean cookieControlsModeEnforced;
    }

    // Keeps the params that are applied to the UI if the params are set before the UI is ready.
    private Params mInitializationParams;

    // UI Elements.
    private RadioButtonWithDescription mAllowButton;
    private RadioButtonWithDescription mBlockThirdPartyIncognitoButton;
    private RadioButtonWithDescription mBlockThirdPartyButton;
    private RadioButtonWithDescription mBlockButton;
    private RadioGroup mRadioGroup;
    private TextViewWithCompoundDrawables mManagedView;

    public FourStateCookieSettingsPreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        // Sets the layout resource that will be inflated for the view.
        setLayoutResource(R.layout.four_state_cookie_settings_preference);

        // Make unselectable, otherwise FourStateCookieSettingsPreference is treated as one
        // selectable Preference, instead of four selectable radio buttons.
        setSelectable(false);
    }

    /**
     * Sets the cookie settings state and updates the radio buttons.
     */
    public void setState(Params state) {
        if (mRadioGroup != null) {
            configureRadioButtons(state);
        } else {
            mInitializationParams = state;
        }
    }

    /**
     * @return The state that is currently selected.
     */
    public CookieSettingsState getState() {
        if (mRadioGroup == null && mInitializationParams == null) {
            return CookieSettingsState.UNINITIALIZED;
        }

        // Calculate the state from mInitializationParams if the UI is not initialized yet.
        if (mInitializationParams != null) {
            return getActiveState(mInitializationParams);
        }

        if (mAllowButton.isChecked()) {
            return CookieSettingsState.ALLOW;
        } else if (mBlockThirdPartyIncognitoButton.isChecked()) {
            return CookieSettingsState.BLOCK_THIRD_PARTY_INCOGNITO;
        } else if (mBlockThirdPartyButton.isChecked()) {
            return CookieSettingsState.BLOCK_THIRD_PARTY;
        } else {
            assert mBlockButton.isChecked();
            return CookieSettingsState.BLOCK;
        }
    }

    @Override
    public void onCheckedChanged(RadioGroup group, int checkedId) {
        callChangeListener(getState());
    }

    @Override
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        mAllowButton = (RadioButtonWithDescription) holder.findViewById(R.id.allow);
        mBlockThirdPartyIncognitoButton =
                (RadioButtonWithDescription) holder.findViewById(R.id.block_third_party_incognito);
        mBlockThirdPartyButton =
                (RadioButtonWithDescription) holder.findViewById(R.id.block_third_party);
        mBlockButton = (RadioButtonWithDescription) holder.findViewById(R.id.block);
        mRadioGroup = (RadioGroup) holder.findViewById(R.id.radio_button_layout);
        mRadioGroup.setOnCheckedChangeListener(this);

        mManagedView = (TextViewWithCompoundDrawables) holder.findViewById(R.id.managed_view);
        Drawable[] drawables = mManagedView.getCompoundDrawablesRelative();
        Drawable managedIcon = ApiCompatibilityUtils.getDrawable(getContext().getResources(),
                ManagedPreferencesUtils.getManagedByEnterpriseIconId());
        mManagedView.setCompoundDrawablesRelativeWithIntrinsicBounds(
                managedIcon, drawables[1], drawables[2], drawables[3]);

        if (mInitializationParams != null) {
            configureRadioButtons(mInitializationParams);
        }
    }

    private CookieSettingsState getActiveState(Params params) {
        // These conditions only check the preference combinations that deterministically decide
        // your cookie settings state. In the future we would refactor the backend preferences to
        // reflect the only possible states you can be in
        // (Allow/BlockThirdPartyIncognito/BlockThirdParty/Block), instead of using this
        // combination of multiple signals.
        if (!params.allowCookies) {
            return CookieSettingsState.BLOCK;
        } else if (params.cookieControlsMode == CookieControlsMode.BLOCK_THIRD_PARTY) {
            return CookieSettingsState.BLOCK_THIRD_PARTY;
        } else if (params.cookieControlsMode == CookieControlsMode.INCOGNITO_ONLY) {
            return CookieSettingsState.BLOCK_THIRD_PARTY_INCOGNITO;
        } else {
            return CookieSettingsState.ALLOW;
        }
    }

    private void configureRadioButtons(Params params) {
        assert (mRadioGroup != null);
        mAllowButton.setEnabled(true);
        mBlockThirdPartyIncognitoButton.setEnabled(true);
        mBlockThirdPartyButton.setEnabled(true);
        mBlockButton.setEnabled(true);
        for (RadioButtonWithDescription button : getEnforcedButtons(params)) {
            button.setEnabled(false);
        }
        mManagedView.setVisibility(
                (params.cookiesContentSettingEnforced || params.cookieControlsModeEnforced)
                        ? View.VISIBLE
                        : View.GONE);

        RadioButtonWithDescription button = getButton(getActiveState(params));
        // Always want to enable the selected option.
        button.setEnabled(true);
        button.setChecked(true);

        mInitializationParams = null;
    }

    /**
     * A helper function to return a button array from a variable number of arguments.
     */
    private RadioButtonWithDescription[] buttons(RadioButtonWithDescription... args) {
        return args;
    }

    private RadioButtonWithDescription getButton(CookieSettingsState state) {
        switch (state) {
            case ALLOW:
                return mAllowButton;
            case BLOCK_THIRD_PARTY_INCOGNITO:
                return mBlockThirdPartyIncognitoButton;
            case BLOCK_THIRD_PARTY:
                return mBlockThirdPartyButton;
            case BLOCK:
                return mBlockButton;
            case UNINITIALIZED:
                assert false;
                return null;
        }
        assert false;
        return null;
    }

    /**
     * @return An array of radio buttons that have to be disabled as they can't be selected due to
     *         policy restrictions.
     */
    private RadioButtonWithDescription[] getEnforcedButtons(Params params) {
        if (!params.cookiesContentSettingEnforced && !params.cookieControlsModeEnforced) {
            // Nothing is enforced.
            return buttons();
        }
        if (params.cookiesContentSettingEnforced && params.cookieControlsModeEnforced) {
            return buttons(mAllowButton, mBlockThirdPartyIncognitoButton, mBlockThirdPartyButton,
                    mBlockButton);
        }
        if (params.cookiesContentSettingEnforced) {
            if (params.allowCookies) {
                return buttons(mBlockButton);
            } else {
                return buttons(mAllowButton, mBlockThirdPartyIncognitoButton,
                        mBlockThirdPartyButton, mBlockButton);
            }
        }
        // cookieControlsModeEnforced must be true.
        if (params.cookieControlsMode == CookieControlsMode.BLOCK_THIRD_PARTY) {
            return buttons(mAllowButton, mBlockThirdPartyIncognitoButton);
        } else {
            return buttons(mBlockThirdPartyIncognitoButton, mBlockThirdPartyButton);
        }
    }

    @VisibleForTesting
    public boolean isButtonEnabledForTesting(CookieSettingsState state) {
        assert getButton(state) != null;
        return getButton(state).isEnabled();
    }
}
