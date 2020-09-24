// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.page_info;

import android.os.Bundle;
import android.text.format.Formatter;

import androidx.appcompat.app.AlertDialog;
import androidx.preference.Preference;
import androidx.preference.PreferenceFragmentCompat;

import org.chromium.base.Callback;
import org.chromium.components.browser_ui.settings.ButtonPreference;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.content_settings.CookieControlsStatus;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

/**
 * View showing a toggle and a description for third-party cookie blocking for a site.
 */
public class PageInfoCookiesPreference extends PreferenceFragmentCompat {
    private static final String COOKIE_SUMMARY_PREFERENCE = "cookie_summary";
    private static final String COOKIE_SWITCH_PREFERENCE = "cookie_switch";
    private static final String COOKIE_IN_USE_PREFERENCE = "cookie_in_use";
    private static final String CLEAR_BUTTON_PREFERENCE = "clear_button";

    private ChromeSwitchPreference mCookieSwitch;
    private ChromeBasePreference mCookieInUse;
    private Runnable mOnClearCallback;

    /**  Parameters to configure the cookie controls view. */
    public static class PageInfoCookiesViewParams {
        // Called when the toggle controlling third-party cookie blocking changes.
        public boolean thirdPartyCookieBlockingEnabled;
        public Callback<Boolean> onCheckedChangedCallback;
        public Runnable onClearCallback;
        public Runnable onCookieSettingsLinkClicked;
        public boolean disableCookieDeletion;
    }

    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        SettingsUtils.addPreferencesFromResource(this, R.xml.page_info_cookie_preference);
        mCookieSwitch = findPreference(COOKIE_SWITCH_PREFERENCE);
        mCookieInUse = findPreference(COOKIE_IN_USE_PREFERENCE);
    }

    public void setParams(PageInfoCookiesViewParams params) {
        Preference cookieSummary = findPreference(COOKIE_SUMMARY_PREFERENCE);
        NoUnderlineClickableSpan linkSpan = new NoUnderlineClickableSpan(
                getResources(), (view) -> { params.onCookieSettingsLinkClicked.run(); });
        cookieSummary.setSummary(
                SpanApplier.applySpans(getString(R.string.page_info_cookies_description),
                        new SpanApplier.SpanInfo("<link>", "</link>", linkSpan)));

        // TODO(crbug.com/1077766): Set a ManagedPreferenceDelegate?
        mCookieSwitch.setVisible(params.thirdPartyCookieBlockingEnabled);
        mCookieSwitch.setOnPreferenceChangeListener((preference, newValue) -> {
            params.onCheckedChangedCallback.onResult((Boolean) newValue);
            return true;
        });

        mCookieInUse.setIcon(
                SettingsUtils.getTintedIcon(getContext(), R.drawable.permission_cookie));

        mOnClearCallback = params.onClearCallback;
        ButtonPreference clearButton = findPreference(CLEAR_BUTTON_PREFERENCE);
        clearButton.setOnPreferenceClickListener(preference -> {
            showClearCookiesConfirmation();
            return true;
        });
        if (params.disableCookieDeletion) {
            clearButton.setEnabled(false);
        }
    }

    private void showClearCookiesConfirmation() {
        new AlertDialog.Builder(getActivity(), R.style.Theme_Chromium_AlertDialog)
                .setTitle(R.string.page_info_cookies_clear)
                .setMessage(R.string.page_info_cookies_clear_confirmation)
                .setPositiveButton(R.string.page_info_cookies_clear_confirmation_button,
                        (dialog, which) -> mOnClearCallback.run())
                .setNegativeButton(R.string.cancel, null)
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
    }

    public void setStorageUsage(long storageUsage) {
        mCookieInUse.setSummary(
                storageUsage > 0 ? String.format(
                        getContext().getString(R.string.origin_settings_storage_usage_brief),
                        Formatter.formatShortFileSize(getContext(), storageUsage))
                                 : null);
    }
}
