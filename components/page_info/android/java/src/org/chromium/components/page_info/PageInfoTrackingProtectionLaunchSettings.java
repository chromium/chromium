// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.page_info;

import android.app.Activity;
import android.app.Dialog;
import android.os.Bundle;
import android.text.TextPaint;
import android.text.format.DateUtils;
import android.text.format.Formatter;
import android.text.style.ClickableSpan;
import android.view.View;

import androidx.appcompat.app.AlertDialog;
import androidx.preference.Preference;

import org.chromium.base.Callback;
import org.chromium.base.TimeUtils;
import org.chromium.components.browser_ui.settings.ChromeImageViewPreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.browser_ui.site_settings.BaseSiteSettingsFragment;
import org.chromium.components.browser_ui.site_settings.FPSCookieInfo;
import org.chromium.components.browser_ui.site_settings.ForwardingManagedPreferenceDelegate;
import org.chromium.components.browser_ui.util.date.CalendarUtils;
import org.chromium.components.content_settings.CookieControlsBridge.TrackingProtectionFeature;
import org.chromium.components.content_settings.CookieControlsEnforcement;
import org.chromium.components.content_settings.TrackingProtectionFeatureType;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.util.AttrUtils;

import java.util.List;

/** View showing a toggle and a description for tracking protection for a site. */
public class PageInfoTrackingProtectionLaunchSettings extends BaseSiteSettingsFragment {
    private static final String COOKIE_SUMMARY_PREFERENCE = "cookie_summary";
    private static final String TP_TITLE = "tp_title";
    private static final String TP_SWITCH_PREFERENCE = "tp_switch";
    private static final String TP_STATUS_PREFERENCE = "tp_status";
    private static final String STORAGE_IN_USE_PREFERENCE = "storage_in_use";
    private static final String FPS_IN_USE_PREFERENCE = "fps_in_use";
    private static final String TPC_SUMMARY = "tpc_summary";
    private static final int EXPIRATION_FOR_TESTING = 33;

    private ChromeSwitchPreference mTpSwitch;
    private ChromeImageViewPreference mStorageInUse;
    private ChromeImageViewPreference mFPSInUse;
    private TextMessagePreference mTpTitle;
    private TrackingProtectionStatusPreference mTpStatus;
    private Runnable mOnClearCallback;
    private Runnable mOnCookieSettingsLinkClicked;
    private Callback<Activity> mOnFeedbackClicked;
    private Dialog mConfirmationDialog;
    private boolean mDeleteDisabled;
    private boolean mDataUsed;
    private CharSequence mHostName;
    private FPSCookieInfo mFPSInfo;
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
        mStorageInUse = findPreference(STORAGE_IN_USE_PREFERENCE);
        mFPSInUse = findPreference(FPS_IN_USE_PREFERENCE);
        mFPSInUse.setVisible(false);
        mTpTitle = findPreference(TP_TITLE);
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
        mTpStatus.setBlockAll3PC(mBlockAll3PC);
        mTpSwitch.setVisible(params.thirdPartyCookieBlockingEnabled);
        mTpSwitch.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    boolean boolValue = (Boolean) newValue;
                    mTpStatus.setTrackingProtectionStatus(boolValue);
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
        // Extract the 3PC enforcement from the feature vector.
        @CookieControlsEnforcement int enforcement = CookieControlsEnforcement.NO_ENFORCEMENT;
        boolean cookiesFeaturePresent = false;
        for (TrackingProtectionFeature feature : features) {
            if (feature.featureType == TrackingProtectionFeatureType.THIRD_PARTY_COOKIES) {
                cookiesFeaturePresent = true;
                enforcement = feature.enforcement;
            }
        }
        assert cookiesFeaturePresent : "THIRD_PARTY_COOKIES must be in the features list";

        boolean isEnforced = enforcement != CookieControlsEnforcement.NO_ENFORCEMENT;

        if (enforcement == CookieControlsEnforcement.ENFORCED_BY_TPCD_GRANT) {
            // Hide all the 3PC controls.
            mTpSwitch.setVisible(false);
            mTpTitle.setVisible(false);
            findPreference(COOKIE_SUMMARY_PREFERENCE).setVisible(false);
            ClickableSpan linkSpan =
                    new ClickableSpan() {
                        @Override
                        public void onClick(View view) {
                            mOnCookieSettingsLinkClicked.run();
                        }

                        @Override
                        public void updateDrawState(TextPaint textPaint) {
                            super.updateDrawState(textPaint);
                            textPaint.setColor(
                                    AttrUtils.resolveColor(
                                            getContext().getTheme(),
                                            R.attr.globalClickableSpanColor,
                                            R.color.default_text_color_link_baseline));
                        }
                    };
            mTpSwitch.setSummary(
                    SpanApplier.applySpans(
                            getString(
                                    R.string.page_info_tracking_protection_site_grant_description),
                            new SpanApplier.SpanInfo("<link>", "</link>", linkSpan)));
            return;
        }

        mTpSwitch.setVisible(controlsVisible);
        mTpTitle.setVisible(controlsVisible);

        if (!controlsVisible) return;

        mTpSwitch.setChecked(protectionsOn);

        // Update the tracking protection status visibility depending on which features are enabled.
        for (TrackingProtectionFeature feature : features) {
            mTpStatus.setVisible(feature.featureType, true);
        }
        mTpStatus.setTrackingProtectionStatus(protectionsOn);
        mTpSwitch.setEnabled(!isEnforced);
        mTpSwitch.setManagedPreferenceDelegate(
                new ForwardingManagedPreferenceDelegate(
                        getSiteSettingsDelegate().getManagedPreferenceDelegate()) {
                    @Override
                    public boolean isPreferenceControlledByPolicy(Preference preference) {
                        return isEnforced;
                    }
                });

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
        mFPSInUse.setSummary(
                String.format(getString(R.string.cookie_info_fps_summary), fpsInfo.getOwner()));
        mFPSInUse.setIcon(SettingsUtils.getTintedIcon(getContext(), R.drawable.tenancy));
        mFPSInUse.setManagedPreferenceDelegate(
                new ForwardingManagedPreferenceDelegate(
                        getSiteSettingsDelegate().getManagedPreferenceDelegate()) {
                    @Override
                    public boolean isPreferenceControlledByPolicy(Preference preference) {
                        return getSiteSettingsDelegate()
                                .isPartOfManagedFirstPartySet(currentOrigin);
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
        return;
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
