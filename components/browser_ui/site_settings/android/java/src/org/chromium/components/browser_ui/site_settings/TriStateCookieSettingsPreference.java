// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import android.content.Context;
import android.content.res.Resources;
import android.util.AttributeSet;
import android.view.View;
import android.widget.RadioGroup;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.preference.Preference;
import androidx.preference.PreferenceViewHolder;

import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionAndAuxButton;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;
import org.chromium.components.content_settings.CookieControlsMode;

/** A 3-state radio group Preference used for the Third-Party Cookies subpage of SiteSettings. */
public class TriStateCookieSettingsPreference extends Preference
        implements RadioGroup.OnCheckedChangeListener,
                RadioButtonWithDescriptionAndAuxButton.OnAuxButtonClickedListener {
    private OnCookiesDetailsRequested mListener;

    /** Used to notify cookie details subpages requests. */
    public interface OnCookiesDetailsRequested {
        /** Notify that Cookie details are requested. */
        void onCookiesDetailsRequested(@CookieControlsMode int cookieSettingsState);
    }

    /** Signals used to determine the view and button states. */
    public static class Params {
        // Whether the PrivacySandboxFirstPartySetsUI feature is enabled.
        public boolean isPrivacySandboxFirstPartySetsUIEnabled;

        // An enum indicating when to block third-party cookies.
        public @CookieControlsMode int cookieControlsMode;

        // Whether the incognito mode is enabled.
        public boolean isIncognitoModeEnabled;

        // Whether third-party blocking is enforced.
        public boolean cookieControlsModeEnforced;
        // Whether Related Website Sets are enabled.
        public boolean isRelatedWebsiteSetsDataAccessEnabled;
    }

    // Keeps the params that are applied to the UI if the params are set before the UI is ready.
    private Params mInitializationParams;

    // UI Elements.
    private RadioButtonWithDescription mAllowButton;
    private RadioButtonWithDescription mBlockThirdPartyIncognitoButton;
    private RadioButtonWithDescription mBlockThirdPartyButton;
    private RadioGroup mRadioGroup;
    private TextViewWithCompoundDrawables mManagedView;

    // Sometimes UI is initialized before the initializationParams are set. We keep this viewHolder
    // to properly adjust UI once initializationParams are set. See crbug.com/1371236.
    // TODO(tommasin) Remove this holder once the FirstPartySets UI will be enabled by default.
    private PreferenceViewHolder mViewHolder;

    public TriStateCookieSettingsPreference(Context context, AttributeSet attrs) {
        super(context, attrs);

        // Sets the layout resource that will be inflated for the view.
        setLayoutResource(R.layout.tri_state_cookie_settings_preference);

        // Make unselectable, otherwise TriStateCookieSettingsPreference is treated as one
        // selectable Preference, instead of four selectable radio buttons.
        setSelectable(false);
    }

    /** Sets the cookie settings state and updates the radio buttons. */
    public void setState(Params state) {
        if (mRadioGroup != null) {
            setRadioButtonsVisibility(state);
            configureRadioButtons(state);
        } else {
            mInitializationParams = state;
        }
    }

    /** @return The state that is currently selected. */
    public @CookieControlsMode Integer getState() {
        if (mRadioGroup == null && mInitializationParams == null) {
            return null;
        }

        // Calculate the state from mInitializationParams if the UI is not initialized yet.
        if (mInitializationParams != null) {
            return getActiveState(mInitializationParams);
        }

        if (mAllowButton.isChecked()) {
            return CookieControlsMode.OFF;
        } else if (mBlockThirdPartyIncognitoButton.isChecked()) {
            return CookieControlsMode.INCOGNITO_ONLY;
        } else {
            assert mBlockThirdPartyButton.isChecked();
            return CookieControlsMode.BLOCK_THIRD_PARTY;
        }
    }

    @Override
    public void onCheckedChanged(RadioGroup group, int checkedId) {
        callChangeListener(getState());
    }

    @Override
    public void onBindViewHolder(@NonNull PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        mViewHolder = holder;
        mAllowButton = (RadioButtonWithDescription) holder.findViewById(R.id.allow);
        mRadioGroup = (RadioGroup) holder.findViewById(R.id.radio_button_layout);
        mRadioGroup.setOnCheckedChangeListener(this);

        mManagedView =
                (TextViewWithCompoundDrawables) holder.findViewById(R.id.managed_disclaimer_text);

        if (mInitializationParams != null) {
            setRadioButtonsVisibility(mInitializationParams);
            configureRadioButtons(mInitializationParams);
        }
    }

    private Resources getResources() {
        return getContext().getResources();
    }

    private void setRadioButtonsVisibility(Params params) {
        if (params.isPrivacySandboxFirstPartySetsUIEnabled) {
            mViewHolder.findViewById(R.id.block_third_party_incognito).setVisibility(View.GONE);
            mViewHolder.findViewById(R.id.block_third_party).setVisibility(View.GONE);

            // TODO(crbug.com/40233724): Change the buttons class into a
            // RadioButtonWithDescriptionAndAuxButton and remove the following casts when the
            // PrivacySandboxFirstPartySetsUI feature is launched
            var blockTPIncognitoBtnWithDescAndAux =
                    (RadioButtonWithDescriptionAndAuxButton)
                            mViewHolder.findViewById(R.id.block_third_party_incognito_with_aux);
            var blockTPButtonWithDescAndAux =
                    (RadioButtonWithDescriptionAndAuxButton)
                            mViewHolder.findViewById(R.id.block_third_party_with_aux);

            blockTPIncognitoBtnWithDescAndAux.setVisibility(View.VISIBLE);
            blockTPButtonWithDescAndAux.setVisibility(View.VISIBLE);

            blockTPIncognitoBtnWithDescAndAux.setAuxButtonClickedListener(this);
            blockTPButtonWithDescAndAux.setAuxButtonClickedListener(this);
            mBlockThirdPartyIncognitoButton = blockTPIncognitoBtnWithDescAndAux;
            mBlockThirdPartyButton = blockTPButtonWithDescAndAux;
            setBlockThirdPartyCookieDescription(params);
        } else {
            mBlockThirdPartyIncognitoButton =
                    (RadioButtonWithDescription)
                            mViewHolder.findViewById(R.id.block_third_party_incognito);
            mBlockThirdPartyButton =
                    (RadioButtonWithDescription) mViewHolder.findViewById(R.id.block_third_party);
        }
    }

    private void setBlockThirdPartyCookieDescription(Params params) {
        if (params.isRelatedWebsiteSetsDataAccessEnabled) {
            mBlockThirdPartyButton.setDescriptionText(
                    getResources()
                            .getString(
                                    R.string
                                            .website_settings_third_party_cookies_page_block_radio_sub_label_rws_enabled));
        } else {
            mBlockThirdPartyButton.setDescriptionText(
                    getResources()
                            .getString(
                                    R.string
                                            .website_settings_third_party_cookies_page_block_radio_sub_label_rws_disabled));
        }
    }

    public void setCookiesDetailsRequestedListener(OnCookiesDetailsRequested listener) {
        mListener = listener;
    }

    @Override
    public void onAuxButtonClicked(int clickedButtonId) {
        if (clickedButtonId == mBlockThirdPartyIncognitoButton.getId()) {
            mListener.onCookiesDetailsRequested(CookieControlsMode.INCOGNITO_ONLY);
        } else if (clickedButtonId == mBlockThirdPartyButton.getId()) {
            mListener.onCookiesDetailsRequested(CookieControlsMode.BLOCK_THIRD_PARTY);
        } else {
            assert false : "Should not be reached.";
        }
    }

    private @CookieControlsMode int getActiveState(Params params) {
        if (params.cookieControlsMode == CookieControlsMode.INCOGNITO_ONLY
                && !params.isIncognitoModeEnabled) {
            return CookieControlsMode.OFF;
        }
        return params.cookieControlsMode;
    }

    private void configureRadioButtons(Params params) {
        assert (mRadioGroup != null);
        mAllowButton.setEnabled(true);
        mBlockThirdPartyIncognitoButton.setEnabled(true);
        mBlockThirdPartyButton.setEnabled(true);
        for (RadioButtonWithDescription button : getEnforcedButtons(params)) {
            button.setEnabled(false);
        }
        mManagedView.setVisibility(params.cookieControlsModeEnforced ? View.VISIBLE : View.GONE);

        RadioButtonWithDescription button = getButton(getActiveState(params));
        // Always want to enable the selected option.
        button.setEnabled(true);
        button.setChecked(true);

        mInitializationParams = null;
    }

    /** A helper function to return a button array from a variable number of arguments. */
    private RadioButtonWithDescription[] buttons(RadioButtonWithDescription... args) {
        return args;
    }

    @VisibleForTesting
    public RadioButtonWithDescription getButton(@CookieControlsMode int state) {
        switch (state) {
            case CookieControlsMode.OFF:
                return mAllowButton;
            case CookieControlsMode.INCOGNITO_ONLY:
                return mBlockThirdPartyIncognitoButton;
            case CookieControlsMode.BLOCK_THIRD_PARTY:
                return mBlockThirdPartyButton;
        }
        assert false;
        return null;
    }

    /**
     * @return An array of radio buttons that have to be disabled as they can't be selected due to
     *         policy restrictions.
     */
    private RadioButtonWithDescription[] getEnforcedButtons(Params params) {
        // Nothing is enforced.
        if (!params.cookieControlsModeEnforced) {
            if (!params.isIncognitoModeEnabled) {
                return buttons(mBlockThirdPartyIncognitoButton);
            } else {
                return buttons();
            }
        }
        return buttons(mAllowButton, mBlockThirdPartyIncognitoButton, mBlockThirdPartyButton);
    }

    public boolean isButtonEnabledForTesting(@CookieControlsMode int state) {
        assert getButton(state) != null;
        return getButton(state).isEnabled();
    }

    public boolean isButtonCheckedForTesting(@CookieControlsMode int state) {
        assert getButton(state) != null;
        return getButton(state).isChecked();
    }
}
