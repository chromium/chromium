// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.page_info;

import android.app.Activity;
import android.app.Dialog;
import android.os.Bundle;
import android.text.format.DateUtils;
import android.text.format.Formatter;

import androidx.appcompat.app.AlertDialog;
import androidx.preference.Preference;

import org.chromium.base.Callback;
import org.chromium.base.TimeUtils;
import org.chromium.components.browser_ui.settings.ChromeImageViewPreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.browser_ui.site_settings.FPSCookieInfo;
import org.chromium.components.browser_ui.site_settings.ForwardingManagedPreferenceDelegate;
import org.chromium.components.browser_ui.site_settings.BaseSiteSettingsFragment;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.browser_ui.util.date.CalendarUtils;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.CookieControlsStatus;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

/**
 * View showing a toggle and a description for third-party cookie blocking for a site.
 */
public class PageInfoCookiesSettings extends BaseSiteSettingsFragment {
    private static final String COOKIE_SUMMARY_PREFERENCE = "cookie_summary";
    private static final String COOKIE_SWITCH_PREFERENCE = "cookie_switch";
    private static final String COOKIE_IN_USE_PREFERENCE = "cookie_in_use";
    private static final String FPS_IN_USE_PREFERENCE = "fps_in_use";
    private static final String TPC_TITLE = "tpc_title";
    private static final String TPC_SUMMARY = "tpc_summary";

    private ChromeSwitchPreference mCookieSwitch;
    private ChromeImageViewPreference mCookieInUse;
    private ChromeImageViewPreference mFPSInUse;
    private TextMessagePreference mThirdPartyCookiesTitle;
    private TextMessagePreference mThirdPartyCookiesSummary;
    private Runnable mOnClearCallback;
    private Callback<Activity> mOnFeedbackClicked;
    private Dialog mConfirmationDialog;
    private boolean mDeleteDisabled;
    private boolean mDataUsed;
    private int mAllowedSites;
    private int mBlockedSites;
    private CharSequence mHostName;
    private FPSCookieInfo mFPSInfo;

    /**  Parameters to configure the cookie controls view. */
    public static class PageInfoCookiesViewParams {
        // Called when the toggle controlling third-party cookie blocking changes.
        public boolean thirdPartyCookieBlockingEnabled;
        public Callback<Boolean> onThirdPartyCookieToggleChanged;
        public Runnable onClearCallback;
        public Runnable onCookieSettingsLinkClicked;
        public Callback<Activity> onFeedbackLinkClicked;
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
        if (PageInfoFeatures.USER_BYPASS_UI.isEnabled()) {
            SettingsUtils.addPreferencesFromResource(
                    this, R.xml.page_info_cookie_preference_user_bypass);
        } else {
            SettingsUtils.addPreferencesFromResource(this, R.xml.page_info_cookie_preference);
        }
        mCookieSwitch = findPreference(COOKIE_SWITCH_PREFERENCE);
        mCookieInUse = findPreference(COOKIE_IN_USE_PREFERENCE);
        mFPSInUse = findPreference(FPS_IN_USE_PREFERENCE);
        mFPSInUse.setVisible(false);
        mThirdPartyCookiesTitle = findPreference(TPC_TITLE);
        mThirdPartyCookiesSummary = findPreference(TPC_SUMMARY);
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
            boolean boolValue = (Boolean) newValue;
            // Invert when the flag is on, since the switch is inverted.
            if (PageInfoFeatures.USER_BYPASS_UI.isEnabled()) {
                boolValue = !boolValue;
            }
            params.onThirdPartyCookieToggleChanged.onResult(boolValue);
            return true;
        });
        boolean areAllCookiesBlocked = !WebsitePreferenceBridge.isCategoryEnabled(
                getSiteSettingsDelegate().getBrowserContextHandle(), ContentSettingsType.COOKIES);
        if (areAllCookiesBlocked && !PageInfoFeatures.USER_BYPASS_UI.isEnabled()) {
            mCookieSwitch.setTitle(R.string.page_info_all_cookies_block);
        }

        mCookieInUse.setIcon(SettingsUtils.getTintedIcon(getContext(),
                PageInfoFeatures.USER_BYPASS_UI.isEnabled() ? R.drawable.gm_database_24
                                                            : R.drawable.permission_cookie));
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
        mOnFeedbackClicked = params.onFeedbackLinkClicked;
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

    // Only used when UserBypassUI flag is off.
    public void setCookieBlockingStatus(@CookieControlsStatus int status, boolean isEnforced) {
        assert !PageInfoFeatures.USER_BYPASS_UI.isEnabled()
            : "This should only be invoked when UserBypassUI is off.";

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

    // Only used when UserBypassUI flag is on.
    public void setCookieStatus(
            @CookieControlsStatus int status, final boolean isEnforced, long expiration) {
        assert PageInfoFeatures.USER_BYPASS_UI.isEnabled()
            : "This should only be invoked when UserBypassUI is enabled.";

        boolean visible = status != CookieControlsStatus.DISABLED;
        boolean blockingEnabled = status == CookieControlsStatus.ENABLED;

        mCookieSwitch.setVisible(visible);
        mThirdPartyCookiesTitle.setVisible(visible);
        mThirdPartyCookiesSummary.setVisible(visible);

        if (!visible) return;

        mCookieSwitch.setIcon(SettingsUtils.getTintedIcon(getContext(),
                blockingEnabled ? R.drawable.ic_visibility_off_black
                                : R.drawable.ic_visibility_black));
        mCookieSwitch.setChecked(!blockingEnabled);
        mCookieSwitch.setEnabled(!isEnforced);
        mCookieSwitch.setManagedPreferenceDelegate(new ForwardingManagedPreferenceDelegate(
                getSiteSettingsDelegate().getManagedPreferenceDelegate()) {
            @Override
            public boolean isPreferenceControlledByPolicy(Preference preference) {
                return isEnforced;
            }
        });

        boolean permanentException = (expiration == 0);

        NoUnderlineClickableSpan feedbackSpan = new NoUnderlineClickableSpan(
                getContext(), (view) -> { mOnFeedbackClicked.onResult(this.getActivity()); });

        if (blockingEnabled) {
            mThirdPartyCookiesTitle.setTitle(
                    getContext().getString(R.string.page_info_cookies_site_not_working_title));
            mThirdPartyCookiesSummary.setSummary(getContext().getString(
                    willCreatePermanentException()
                            ? R.string.page_info_cookies_site_not_working_description_permanent
                            : R.string.page_info_cookies_site_not_working_description_temporary));
        } else if (permanentException) {
            mThirdPartyCookiesTitle.setTitle(
                    getContext().getString(R.string.page_info_cookies_permanent_allowed_title));
            mThirdPartyCookiesSummary.setSummary(SpanApplier.applySpans(
                    getContext().getString(R.string.page_info_cookies_send_feedback_description),
                    new SpanApplier.SpanInfo("<link>", "</link>", feedbackSpan)));
        } else { // Not blocking and temporary exception.
            int days = calculateDaysUntilExpiration(TimeUtils.currentTimeMillis(), expiration);
            mThirdPartyCookiesTitle.setTitle(days == 0
                            ? getContext().getString(
                                    R.string.page_info_cookies_blocking_restart_today_title)
                            : getContext().getResources().getQuantityString(
                                    R.plurals.page_info_cookies_blocking_restart_title, days,
                                    days));
            mThirdPartyCookiesSummary.setSummary(SpanApplier.applySpans(
                    getContext().getString(R.string.page_info_cookies_send_feedback_description),
                    new SpanApplier.SpanInfo("<link>", "</link>", feedbackSpan)));
        }

        updateCookieSwitch();
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

    public void setSitesCount(int allowedSites, int blockedSites) {
        mAllowedSites = allowedSites;
        mBlockedSites = blockedSites;

        mDataUsed |= allowedSites != 0;
        updateCookieDeleteButton();
        updateCookieSwitch();
    }

    public void setStorageUsage(long storageUsage) {
        if (PageInfoFeatures.USER_BYPASS_UI.isEnabled()) {
            mCookieInUse.setTitle(String.format(
                    getContext().getString(R.string.origin_settings_storage_usage_brief),
                    Formatter.formatShortFileSize(getContext(), storageUsage)));
        } else {
            mCookieInUse.setSummary(
                    storageUsage > 0 ? String.format(
                            getContext().getString(R.string.origin_settings_storage_usage_brief),
                            Formatter.formatShortFileSize(getContext(), storageUsage))
                                     : null);
        }

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

    /**
     * Returns the number of days left until the exception expiration.
     * @param currentTime Current timestamps (can be obtained using TimeUtils.currentTimeMillis())
     * @param expiration A timestamp for the expiration.
     * @return Number of days until expiration. Day boundary is considered to be the local midnight.
     */
    public static int calculateDaysUntilExpiration(long currentTime, long expiration) {
        long currentMidnight = CalendarUtils.getStartOfDay(currentTime).getTime().getTime();
        long expirationMidnight = CalendarUtils.getStartOfDay(expiration).getTime().getTime();
        return (int) ((expirationMidnight - currentMidnight) / DateUtils.DAY_IN_MILLIS);
    }

    private void updateCookieDeleteButton() {
        mCookieInUse.setImageColor(!mDeleteDisabled && mDataUsed
                        ? R.color.default_icon_color_accent1_tint_list
                        : R.color.default_icon_color_disabled);
    }

    // Only invoked when UserBypassUI is on.
    private void updateCookieSwitch() {
        assert PageInfoFeatures.USER_BYPASS_UI.isEnabled()
            : "This should only be invoked when UserBypassUI is enabled.";
        // TODO(crbug.com/1446230): Update the strings for when FPS are on.
        if (!mCookieSwitch.isChecked()) {
            mCookieSwitch.setSummary(getContext().getResources().getQuantityString(
                    R.plurals.page_info_sites_blocked, mBlockedSites, mBlockedSites));
        } else {
            mCookieSwitch.setSummary(getContext().getResources().getQuantityString(
                    R.plurals.page_info_sites_allowed, mAllowedSites, mAllowedSites));
        }
    }

    private boolean willCreatePermanentException() {
        return "0d".equals(PageInfoFeatures.getUserBypassExpiration());
    }
}
