// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.page_info;

import android.app.Dialog;
import android.os.Bundle;
import android.text.format.Formatter;

import androidx.appcompat.app.AlertDialog;
import androidx.preference.Preference;

import org.chromium.base.Callback;
import org.chromium.components.browser_ui.settings.ChromeImageViewPreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.site_settings.FPSCookieInfo;
import org.chromium.components.browser_ui.site_settings.ForwardingManagedPreferenceDelegate;
import org.chromium.components.browser_ui.site_settings.SiteSettingsPreferenceFragment;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.CookieControlsStatus;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

/**
 * View showing a toggle and a description for third-party cookie blocking for a site.
 */
public class PageInfoCookiesPreference extends SiteSettingsPreferenceFragment {
    private static final String COOKIE_SUMMARY_PREFERENCE = "cookie_summary";
    private static final String COOKIE_SWITCH_PREFERENCE = "cookie_switch";
    private static final String COOKIE_IN_USE_PREFERENCE = "cookie_in_use";
    private static final String FPS_IN_USE_PREFERENCE = "fps_in_use";
    private static final String CLEAR_BUTTON_PREFERENCE = "clear_button";

    private ChromeSwitchPreference mCookieSwitch;
    private ChromeImageViewPreference mCookieInUse;
    private ChromeImageViewPreference mFPSInUse;
    private Runnable mOnClearCallback;
    private Dialog mConfirmationDialog;
    private boolean mDeleteDisabled;
    private boolean mDataUsed;
    private CharSequence mHostName;
    private FPSCookieInfo mFPSInfo;

    /**  Parameters to configure the cookie controls view. */
    public static class PageInfoCookiesViewParams {
        // Called when the toggle controlling third-party cookie blocking changes.
        public boolean thirdPartyCookieBlockingEnabled;
        public Callback<Boolean> onCheckedChangedCallback;
        public Runnable onClearCallback;
        public Runnable onCookieSettingsLinkClicked;
        public boolean disableCookieDeletion;
        public CharSequence hostName;
    }

    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        // Remove this Preference if it is restored without SiteSettingsDelegate.
        if (!hasSiteSettingsDelegate()) {
            getParentFragmentManager().beginTransaction().remove(this).commit();
            return;
        }
        SettingsUtils.addPreferencesFromResource(this, R.xml.page_info_cookie_preference);
        mCookieSwitch = findPreference(COOKIE_SWITCH_PREFERENCE);
        mCookieInUse = findPreference(COOKIE_IN_USE_PREFERENCE);
        mFPSInUse = findPreference(FPS_IN_USE_PREFERENCE);
        mFPSInUse.setVisible(false);
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        if (mConfirmationDialog != null) {
            mConfirmationDialog.dismiss();
        }
    }

    public void setParams(PageInfoCookiesViewParams params) {
        Preference cookieSummary = findPreference(COOKIE_SUMMARY_PREFERENCE);
        NoUnderlineClickableSpan linkSpan = new NoUnderlineClickableSpan(
                getContext(), (view) -> { params.onCookieSettingsLinkClicked.run(); });
        cookieSummary.setSummary(
                SpanApplier.applySpans(getString(R.string.page_info_cookies_description),
                        new SpanApplier.SpanInfo("<link>", "</link>", linkSpan)));

        // TODO(crbug.com/1077766): Set a ManagedPreferenceDelegate?
        mCookieSwitch.setVisible(params.thirdPartyCookieBlockingEnabled);
        mCookieSwitch.setOnPreferenceChangeListener((preference, newValue) -> {
            params.onCheckedChangedCallback.onResult((Boolean) newValue);
            return true;
        });
        boolean areAllCookiesBlocked = !WebsitePreferenceBridge.isCategoryEnabled(
                getSiteSettingsDelegate().getBrowserContextHandle(), ContentSettingsType.COOKIES);
        if (areAllCookiesBlocked) {
            mCookieSwitch.setTitle(R.string.page_info_all_cookies_block);
        }

        mCookieInUse.setIcon(
                SettingsUtils.getTintedIcon(getContext(), R.drawable.permission_cookie));
        mCookieInUse.setImageView(
                R.drawable.ic_delete_white_24dp, R.string.page_info_cookies_clear, null);
        // Disabling enables passthrough of clicks to the main preference.
        mCookieInUse.setImageViewEnabled(false);
        mDeleteDisabled = params.disableCookieDeletion;
        mCookieInUse.setOnPreferenceClickListener(preference -> {
            showClearCookiesConfirmation();
            return true;
        });
        updateCookieDeleteButton();

        mOnClearCallback = params.onClearCallback;
        mHostName = params.hostName;
    }

    private void showClearCookiesConfirmation() {
        if (mDeleteDisabled || !mDataUsed) return;

        mConfirmationDialog =
                new AlertDialog.Builder(getContext(), R.style.ThemeOverlay_BrowserUI_AlertDialog)
                        .setTitle(R.string.page_info_cookies_clear)
                        .setMessage(R.string.page_info_cookies_clear_confirmation)
                        .setMessage(
                                getString(R.string.page_info_cookies_clear_confirmation, mHostName))
                        .setPositiveButton(R.string.page_info_cookies_clear_confirmation_button,
                                (dialog, which) -> mOnClearCallback.run())
                        .setNegativeButton(
                                R.string.cancel, (dialog, which) -> mConfirmationDialog = null)
                        .show();
    }

    public void setCookieBlockingStatus(@CookieControlsStatus int status, boolean isEnforced) {
        boolean visible = status != CookieControlsStatus.DISABLED;
        boolean enabled = status == CookieControlsStatus.ENABLED;
        mCookieSwitch.setVisible(visible);
        if (visible) {
            mCookieSwitch.setIcon(
                    SettingsUtils.getTintedIcon(getContext(), R.drawable.ic_eye_crossed));
            mCookieSwitch.setChecked(enabled);
            mCookieSwitch.setEnabled(!isEnforced);
        }
    }

    public void setCookiesCount(int allowedCookies, int blockedCookies) {
        mCookieSwitch.setSummary(
                blockedCookies > 0 ? getContext().getResources().getQuantityString(
                        R.plurals.cookie_controls_blocked_cookies, blockedCookies, blockedCookies)
                                   : null);
        mCookieInUse.setTitle(getContext().getResources().getQuantityString(
                R.plurals.page_info_cookies_in_use, allowedCookies, allowedCookies));

        mDataUsed |= allowedCookies != 0;
        updateCookieDeleteButton();
    }

    public void setStorageUsage(long storageUsage) {
        mCookieInUse.setSummary(
                storageUsage > 0 ? String.format(
                        getContext().getString(R.string.origin_settings_storage_usage_brief),
                        Formatter.formatShortFileSize(getContext(), storageUsage))
                                 : null);

        mDataUsed |= storageUsage != 0;
        updateCookieDeleteButton();
    }

    /**
     * Returns a boolean indicating if the FPS info has been shown or not.
     *
     * @param fpsInfo First Party Sets info to show.
     * @param currentOrigin PageInfo current origin.
     * @return a boolean indicating if the FPS info has been shown or not.
     */
    public boolean maybeShowFPSInfo(FPSCookieInfo fpsInfo, String currentOrigin) {
        mFPSInfo = fpsInfo;
        if (fpsInfo == null || mFPSInUse == null) {
            return false;
        }

        assert getSiteSettingsDelegate().isPrivacySandboxFirstPartySetsUIFeatureEnabled()
                && getSiteSettingsDelegate().isFirstPartySetsDataAccessEnabled()
            : "First Party Sets UI and access should be enabled to show FPS info.";

        mFPSInUse.setVisible(true);
        mFPSInUse.setTitle(R.string.cookie_info_fps_title);
        mFPSInUse.setSummary(String.format(
                getContext().getString(R.string.cookie_info_fps_summary), fpsInfo.getOwner()));
        mFPSInUse.setIcon(SettingsUtils.getTintedIcon(getContext(), R.drawable.tenancy));
        mFPSInUse.setManagedPreferenceDelegate(new ForwardingManagedPreferenceDelegate(
                getSiteSettingsDelegate().getManagedPreferenceDelegate()) {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                return getSiteSettingsDelegate().isPartOfManagedFirstPartySet(currentOrigin);
            }
        });

        return true;
    }

    private void updateCookieDeleteButton() {
        mCookieInUse.setImageColor(!mDeleteDisabled && mDataUsed
                        ? R.color.default_icon_color_accent1_tint_list
                        : R.color.default_icon_color_disabled);
    }
}
