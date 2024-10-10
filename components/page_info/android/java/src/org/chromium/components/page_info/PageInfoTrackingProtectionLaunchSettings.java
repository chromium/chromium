// Copyright 2024 The Chromium Authors
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
import org.chromium.components.browser_ui.site_settings.BaseSiteSettingsFragment;
import org.chromium.components.browser_ui.site_settings.ForwardingManagedPreferenceDelegate;
import org.chromium.components.browser_ui.site_settings.RWSCookieInfo;
import org.chromium.components.browser_ui.util.date.CalendarUtils;
import org.chromium.components.content_settings.CookieControlsBridge.TrackingProtectionFeature;
import org.chromium.components.content_settings.CookieControlsEnforcement;
import org.chromium.components.content_settings.TrackingProtectionFeatureType;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

import java.util.List;

/** View showing a toggle and a description for tracking protection for a site. */
public class PageInfoTrackingProtectionLaunchSettings extends BaseSiteSettingsFragment {
    private static final String COOKIE_SUMMARY_PREFERENCE = "cookie_summary";
    private static final String TP_TITLE = "tp_title";
    private static final String TP_SWITCH_PREFERENCE = "tp_switch";
    private static final String TP_STATUS_PREFERENCE = "tp_status";
    private static final String STORAGE_IN_USE_PREFERENCE = "storage_in_use";
    private static final String RWS_IN_USE_PREFERENCE = "rws_in_use";
    private static final String MANAGED_TITLE = "managed_title";
    private static final String MANAGED_STATUS = "managed_status";
    private static final int EXPIRATION_FOR_TESTING = 33;

    private ChromeSwitchPreference mTpSwitch;
    private ChromeImageViewPreference mStorageInUse;
    private ChromeImageViewPreference mRWSInUse;
    private TextMessagePreference mTpTitle;
    private TextMessagePreference mManagedTitle;
    private TrackingProtectionStatusPreference mTpStatus;
    private TrackingProtectionStatusPreference mManagedStatus;
    private Runnable mOnClearCallback;
    private Runnable mOnCookieSettingsLinkClicked;
    private Dialog mConfirmationDialog;
    private boolean mDeleteDisabled;
    private boolean mDataUsed;
    private CharSequence mHostName;
    private boolean mBlockAll3PC;
    private boolean mIsIncognito;
    // Used to have a constant # of days until expiration to prevent test flakiness.
    private boolean mFixedExpiration;

    /** Parameters to configure the cookie controls view. */
    public static class PageInfoTrackingProtectionLaunchViewParams {
        // Called when the toggle controlling third-party cookie blocking changes.
        public boolean thirdPartyCookieBlockingEnabled;
        public Callback<Boolean> onThirdPartyCookieToggleChanged;
        public Runnable onClearCallback;
        public Runnable onCookieSettingsLinkClicked;
        public Callback<Activity> onFeedbackLinkClicked;
        public boolean disableCookieDeletion;
        public CharSequence hostName;
        // Block all third-party cookies when Tracking Protection is on.
        public boolean blockAll3PC;
        public boolean isIncognito;
        public boolean fixedExpirationForTesting;
    }

    @Override
    public void onCreatePreferences(Bundle bundle, String s) {
        // Remove this Preference if it is restored without SiteSettingsDelegate.
        if (!hasSiteSettingsDelegate()) {
            getParentFragmentManager().beginTransaction().remove(this).commit();
            return;
        }
        SettingsUtils.addPreferencesFromResource(
                this, R.xml.page_info_tracking_protection_launch_preference);

        mTpSwitch = findPreference(TP_SWITCH_PREFERENCE);
        mTpSwitch.setUseSummaryAsTitle(false);

        mTpStatus = findPreference(TP_STATUS_PREFERENCE);
        mManagedStatus = findPreference(MANAGED_STATUS);
        mStorageInUse = findPreference(STORAGE_IN_USE_PREFERENCE);
        mRWSInUse = findPreference(RWS_IN_USE_PREFERENCE);
        mRWSInUse.setVisible(false);
        mTpTitle = findPreference(TP_TITLE);
        mManagedTitle = findPreference(MANAGED_TITLE);
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        if (mConfirmationDialog != null) {
            mConfirmationDialog.dismiss();
        }
    }

    private String getQuantityString(int resId, int count) {
        return getContext().getResources().getQuantityString(resId, count, count);
    }

    public void setParams(PageInfoTrackingProtectionLaunchViewParams params) {
        mBlockAll3PC = params.blockAll3PC;
        mIsIncognito = params.isIncognito;
        mFixedExpiration = params.fixedExpirationForTesting;
        mOnCookieSettingsLinkClicked = params.onCookieSettingsLinkClicked;
        Preference cookieSummary = findPreference(COOKIE_SUMMARY_PREFERENCE);
        NoUnderlineClickableSpan linkSpan =
                new NoUnderlineClickableSpan(
                        getContext(),
                        (view) -> {
                            mOnCookieSettingsLinkClicked.run();
                        });
        int summaryString;
        if (mIsIncognito) {
            summaryString =
                    R.string.page_info_tracking_protection_incognito_blocked_cookies_description;
        } else if (mBlockAll3PC) {
            summaryString = R.string.page_info_tracking_protection_blocked_cookies_description;
        } else {
            summaryString = R.string.page_info_tracking_protection_description;
        }
        cookieSummary.setSummary(
                SpanApplier.applySpans(
                        getString(summaryString),
                        new SpanApplier.SpanInfo("<link>", "</link>", linkSpan)));

        // TODO(crbug.com/40129299): Set a ManagedPreferenceDelegate?
        mTpSwitch.setVisible(params.thirdPartyCookieBlockingEnabled);
        mTpSwitch.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    boolean boolValue = (Boolean) newValue;
                    params.onThirdPartyCookieToggleChanged.onResult(boolValue);
                    return true;
                });

        mStorageInUse.setIcon(SettingsUtils.getTintedIcon(getContext(), R.drawable.gm_database_24));
        mStorageInUse.setImageView(
                R.drawable.ic_delete_white_24dp, R.string.page_info_cookies_clear, null);
        // Disabling enables passthrough of clicks to the main preference.
        mStorageInUse.setImageViewEnabled(false);
        mDeleteDisabled = params.disableCookieDeletion;
        mStorageInUse.setOnPreferenceClickListener(
                preference -> {
                    showClearCookiesConfirmation();
                    return true;
                });
        updateStorageDeleteButton();

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
                        .setPositiveButton(
                                R.string.page_info_cookies_clear_confirmation_button,
                                (dialog, which) -> mOnClearCallback.run())
                        .setNegativeButton(
                                R.string.cancel, (dialog, which) -> mConfirmationDialog = null)
                        .show();
    }

    public void setTrackingProtectionStatus(
            boolean controlsVisible,
            boolean protectionsOn,
            long expiration,
            List<TrackingProtectionFeature> features) {
        boolean cookiesFeaturePresent = false;
        int regularCount = 0;
        for (TrackingProtectionFeature feature : features) {
            if (feature.enforcement == CookieControlsEnforcement.NO_ENFORCEMENT) {
                regularCount++;
                mTpStatus.updateStatus(feature, true);
                mManagedStatus.updateStatus(feature, false);
            } else {
                // Set the managed title and status to visible if they're not already.
                mManagedTitle.setVisible(true);
                mManagedStatus.setVisible(true);
                mManagedStatus.updateStatus(feature, true);
                mTpStatus.updateStatus(feature, false);
            }
            if (feature.featureType == TrackingProtectionFeatureType.THIRD_PARTY_COOKIES) {
                cookiesFeaturePresent = true;
            }
        }

        assert cookiesFeaturePresent : "THIRD_PARTY_COOKIES must be in the features list";

        // No unmanaged protections - should hide the controls UI.
        if (regularCount == 0) {
            mTpSwitch.setVisible(false);
            mTpTitle.setVisible(false);
            mTpStatus.setVisible(false);
            findPreference(COOKIE_SUMMARY_PREFERENCE).setVisible(false);
            return;
        }

        mTpSwitch.setVisible(controlsVisible);
        mTpTitle.setVisible(controlsVisible);

        if (!controlsVisible) return;

        mTpSwitch.setChecked(protectionsOn);

        boolean permanentException = (expiration == 0);

        if (protectionsOn) {
            mTpTitle.setTitle(getString(R.string.page_info_tracking_protection_title_on));
            mTpSwitch.setSummary(R.string.page_info_tracking_protection_toggle_on);
        } else if (permanentException) {
            mTpTitle.setTitle(
                    getString(R.string.page_info_tracking_protection_title_off_permanent));
            mTpSwitch.setSummary(R.string.page_info_tracking_protection_toggle_off);
        } else { // Not blocking and temporary exception.
            int days =
                    mFixedExpiration
                            ? EXPIRATION_FOR_TESTING
                            : calculateDaysUntilExpiration(
                                    TimeUtils.currentTimeMillis(), expiration);
            updateTrackingProtectionTitleTemporary(days);
            mTpSwitch.setSummary(R.string.page_info_tracking_protection_toggle_off);
        }
        updateCookieSwitch();
    }

    public void setStorageUsage(long storageUsage) {
        mStorageInUse.setTitle(
                String.format(
                        getString(R.string.origin_settings_storage_usage_brief),
                        Formatter.formatShortFileSize(getContext(), storageUsage)));

        mDataUsed |= storageUsage != 0;
        updateStorageDeleteButton();
    }

    /**
     * Returns a boolean indicating if the RWS info has been shown or not.
     *
     * @param rwsInfo Related Website Sets info to show.
     * @param currentOrigin PageInfo current origin.
     * @return a boolean indicating if the RWS info has been shown or not.
     */
    public boolean maybeShowRWSInfo(RWSCookieInfo rwsInfo, String currentOrigin) {
        if (rwsInfo == null || mRWSInUse == null) {
            return false;
        }

        assert getSiteSettingsDelegate().isPrivacySandboxFirstPartySetsUIFeatureEnabled()
                        && getSiteSettingsDelegate().isRelatedWebsiteSetsDataAccessEnabled()
                : "First Party Sets UI and access should be enabled to show FPS info.";

        mRWSInUse.setVisible(true);
        mRWSInUse.setTitle(R.string.cookie_info_rws_title);
        mRWSInUse.setSummary(
                String.format(getString(R.string.cookie_info_rws_summary), rwsInfo.getOwner()));
        mRWSInUse.setIcon(SettingsUtils.getTintedIcon(getContext(), R.drawable.tenancy));
        mRWSInUse.setManagedPreferenceDelegate(
                new ForwardingManagedPreferenceDelegate(
                        getSiteSettingsDelegate().getManagedPreferenceDelegate()) {
                    @Override
                    public boolean isPreferenceControlledByPolicy(Preference preference) {
                        return getSiteSettingsDelegate()
                                .isPartOfManagedRelatedWebsiteSet(currentOrigin);
                    }
                });

        return true;
    }

    /**
     * Returns the number of days left until the exception expiration.
     *
     * @param currentTime Current timestamps (can be obtained using TimeUtils.currentTimeMillis())
     * @param expiration A timestamp for the expiration.
     * @return Number of days until expiration. Day boundary is considered to be the local midnight.
     */
    public static int calculateDaysUntilExpiration(long currentTime, long expiration) {
        long currentMidnight = CalendarUtils.getStartOfDay(currentTime).getTime().getTime();
        long expirationMidnight = CalendarUtils.getStartOfDay(expiration).getTime().getTime();
        return (int) ((expirationMidnight - currentMidnight) / DateUtils.DAY_IN_MILLIS);
    }

    private void updateStorageDeleteButton() {
        mStorageInUse.setImageColor(
                !mDeleteDisabled && mDataUsed
                        ? R.color.default_icon_color_accent1_tint_list
                        : R.color.default_icon_color_disabled);
    }

    private void updateCookieSwitch() {
        // TODO(b/337310050): Put the logic for the tracking protection switch here.

    }

    private void updateTrackingProtectionTitleTemporary(int days) {
        if (days == 0) {
            mTpTitle.setTitle(getString(R.string.page_info_tracking_protection_title_off_today));
            return;
        }
        mTpTitle.setTitle(
                getQuantityString(R.plurals.page_info_tracking_protection_title_off, days));
    }

    private boolean willCreatePermanentException() {
        return "0d".equals(PageInfoFeatures.getUserBypassExpiration());
    }
}
