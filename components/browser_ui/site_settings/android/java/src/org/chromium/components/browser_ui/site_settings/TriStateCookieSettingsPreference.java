// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.site_settings;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.Resources;
import android.util.AttributeSet;
import android.view.View;
import android.widget.RadioGroup;

import androidx.annotation.VisibleForTesting;
import androidx.preference.PreferenceViewHolder;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.NullUnmarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.settings.ContainedRadioButtonGroupPreference;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescription;
import org.chromium.components.browser_ui.widget.RadioButtonWithDescriptionAndAuxButton;
import org.chromium.components.browser_ui.widget.text.TextViewWithCompoundDrawables;
import org.chromium.components.content_settings.CookieControlsMode;

/** A 3-state radio group Preference used for the Third-Party Cookies subpage of SiteSettings. */
@NullMarked
public class TriStateCookieSettingsPreference extends ContainedRadioButtonGroupPreference
        implements RadioGroup.OnCheckedChangeListener,
                RadioButtonWithDescriptionAndAuxButton.OnAuxButtonClickedListener {
    @SuppressWarnings("NullAway.Init")
    private OnCookiesDetailsRequested mListener;

    /** Used to notify cookie details subpages requests. */
    public interface OnCookiesDetailsRequested {
        /** Notify that Cookie details are requested. */
        void onCookiesDetailsRequested(@CookieControlsMode int cookieSettingsState);
    }

    /** Signals used to determine the view and button states. */
    public static class Params {
        // An enum indicating when to block third-party cookies.
        public @CookieControlsMode int cookieControlsMode;

        // Whether third-party blocking is enforced.
        public boolean cookieControlsModeEnforced;
        // Whether Related Website Sets are enabled.
        public boolean isRelatedWebsiteSetsDataAccessEnabled;
    }

    // Keeps the params that are applied to the UI if the params are set before the UI is ready.
    private @Nullable Params mParams;

    // UI Elements.
    private RadioButtonWithDescriptionAndAuxButton mBlockThirdPartyIncognitoButton;
    private RadioButtonWithDescriptionAndAuxButton mBlockThirdPartyButton;
    private @Nullable RadioGroup mRadioGroup;
    private TextViewWithCompoundDrawables mManagedView;

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
        mParams = state;
        if (mRadioGroup != null) {
            setBlockThirdPartyCookieDescription(mParams);
            configureRadioButtons(mParams);
        }
        notifyChanged();
    }

    /**
     * @return The state that is currently selected.
     */
    public @CookieControlsMode @Nullable Integer getState() {
        if (mRadioGroup == null && mParams == null) {
            return null;
        }

        // Calculate the state from mParams if the UI is not initialized yet.
        if (mParams != null) {
            return getActiveState(mParams);
        }

        if (mBlockThirdPartyIncognitoButton.isChecked()) {
            return CookieControlsMode.INCOGNITO_ONLY;
        } else {
            assert mBlockThirdPartyButton.isChecked();
            return CookieControlsMode.BLOCK_THIRD_PARTY;
        }
    }

    @Override
    public void onCheckedChanged(RadioGroup group, int checkedId) {
        @CookieControlsMode int state;
        if (checkedId == mBlockThirdPartyButton.getId()) {
            RecordUserAction.record("Settings.ThirdPartyCookies.Block");
            state = CookieControlsMode.BLOCK_THIRD_PARTY;
        } else if (checkedId == mBlockThirdPartyIncognitoButton.getId()) {
            RecordUserAction.record("Settings.ThirdPartyCookies.Allow");
            state = CookieControlsMode.INCOGNITO_ONLY;
        } else {
            return;
        }

        if (mParams != null) {
            setBlockThirdPartyCookieDescription(mParams);
        }
        callChangeListener(state);
    }

    @Override
    @Initializer
    @NullUnmarked
    public void onBindViewHolder(PreferenceViewHolder holder) {
        super.onBindViewHolder(holder);

        mBlockThirdPartyIncognitoButton =
                (RadioButtonWithDescriptionAndAuxButton)
                        holder.findViewById(R.id.block_third_party_incognito_with_aux);
        mBlockThirdPartyIncognitoButton.setAuxButtonClickedListener(this);
        mBlockThirdPartyButton =
                (RadioButtonWithDescriptionAndAuxButton)
                        holder.findViewById(R.id.block_third_party_with_aux);
        mBlockThirdPartyButton.setAuxButtonClickedListener(this);

        mRadioGroup = (RadioGroup) holder.findViewById(R.id.radio_button_layout);
        mRadioGroup.setOnCheckedChangeListener(this);

        mManagedView =
                (TextViewWithCompoundDrawables)
                        assertNonNull(holder.findViewById(R.id.managed_disclaimer_text));

        if (mParams != null) {
            setBlockThirdPartyCookieDescription(mParams);
            configureRadioButtons(mParams);
        }
    }

    private Resources getResources() {
        return getContext().getResources();
    }

    private void setBlockThirdPartyCookieDescription(Params params) {
        final int blockSublabelId =
                params.isRelatedWebsiteSetsDataAccessEnabled
                        ? R.string
                        .settings_cookies_block_third_party_settings_block_sublabel_rws_enabled
                        : R.string
                                .website_settings_third_party_cookies_page_block_radio_sub_label_rws_disabled;
        mBlockThirdPartyButton.setDescriptionText(getResources().getString(blockSublabelId));
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
        return params.cookieControlsMode == CookieControlsMode.OFF
                ? CookieControlsMode.INCOGNITO_ONLY
                : params.cookieControlsMode;
    }

    private void configureRadioButtons(Params params) {
        assert (mRadioGroup != null);
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
    }

    /** A helper function to return a button array from a variable number of arguments. */
    private RadioButtonWithDescription[] buttons(RadioButtonWithDescription... args) {
        return args;
    }

    @VisibleForTesting
    public RadioButtonWithDescription getButton(@CookieControlsMode int state) {
        switch (state) {
            case CookieControlsMode.OFF:
            case CookieControlsMode.INCOGNITO_ONLY:
                return mBlockThirdPartyIncognitoButton;
            case CookieControlsMode.BLOCK_THIRD_PARTY:
                return mBlockThirdPartyButton;
        }
        assert false;
        return assumeNonNull(null);
    }

    /**
     * @return An array of radio buttons that have to be disabled as they can't be selected due to
     *     policy restrictions.
     */
    private RadioButtonWithDescription[] getEnforcedButtons(Params params) {
        if (params.cookieControlsModeEnforced) {
            return buttons(mBlockThirdPartyIncognitoButton, mBlockThirdPartyButton);
        }
        return buttons();
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
