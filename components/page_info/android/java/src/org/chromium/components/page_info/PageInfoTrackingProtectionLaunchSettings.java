// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.page_info;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Dialog;
import android.os.Bundle;
import android.text.format.DateUtils;
import android.text.format.Formatter;
import android.view.View;

import androidx.appcompat.app.AlertDialog;
import androidx.preference.Preference;

import org.chromium.base.Callback;
import org.chromium.base.TimeUtils;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.settings.ChromeImageViewPreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.browser_ui.site_settings.BaseSiteSettingsFragment;
import org.chromium.components.browser_ui.site_settings.ForwardingManagedPreferenceDelegate;
import org.chromium.components.browser_ui.site_settings.RwsCookieInfo;
import org.chromium.components.browser_ui.util.date.CalendarUtils;
import org.chromium.components.content_settings.CookieControlsBridge.TrackingProtectionFeature;
import org.chromium.components.content_settings.CookieControlsEnforcement;
import org.chromium.components.content_settings.TrackingProtectionFeatureType;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;

import java.util.List;

/** View showing a toggle and a description for tracking protection for a site. */
@NullMarked
@SuppressWarnings("NullAway.Init") // onCreatePreferences() has an early return.
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
    private @Nullable ChromeImageViewPreference mRwsInUse;
    private TextMessagePreference mTpTitle;
    private @Nullable TextMessagePreference mManagedTitle;
    private TrackingProtectionStatusPreference mTpStatus;
    private TrackingProtectionStatusPreference mManagedStatus;
    private Runnable mOnClearCallback;
    private Runnable mOnCookieSettingsLinkClicked;
    private @Nullable Dialog mConfirmationDialog;
    private boolean mDeleteDisabled;
    private boolean mDataUsed;
    private CharSequence mHostName;
    private boolean mBlockAll3pc;
    private boolean mIsIncognito;
    // Used to have a constant # of days until expiration to prevent test flakiness.
    private boolean mFixedExpiration;

    /** Parameters to configure the cookie controls view. */
    static class PageInfoTrackingProtectionLaunchViewParams {
        // Called when the toggle controlling third-party cookie blocking changes.
        public final boolean thirdPartyCookieBlockingEnabled;
        public final Callback<Boolean> onThirdPartyCookieToggleChanged;
        public final Runnable onClearCallback;
        public final Runnable onCookieSettingsLinkClicked;
        public final boolean disableCookieDeletion;
        public final CharSequence hostName;
        // Block all third-party cookies when Tracking Protection is on.
        public final boolean blockAll3pc;
        public final boolean isIncognito;
        public final boolean fixedExpirationForTesting;

        public PageInfoTrackingProtectionLaunchViewParams(
                boolean thirdPartyCookieBlockingEnabled,
                Callback<Boolean> onThirdPartyCookieToggleChanged,
                Runnable onClearCallback,
                Runnable onCookieSettingsLinkClicked,
                boolean disableCookieDeletion,
                CharSequence hostName,
                boolean blockAll3pc,
                boolean isIncognito,
                boolean fixedExpirationForTesting) {
            this.thirdPartyCookieBlockingEnabled = thirdPartyCookieBlockingEnabled;
            this.onThirdPartyCookieToggleChanged = onThirdPartyCookieToggleChanged;
            this.onClearCallback = onClearCallback;
            this.onCookieSettingsLinkClicked = onCookieSettingsLinkClicked;
            this.disableCookieDeletion = disableCookieDeletion;
            this.hostName = hostName;
            this.blockAll3pc = blockAll3pc;
            this.isIncognito = isIncognito;
            this.fixedExpirationForTesting = fixedExpirationForTesting;
        }
    }

    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        // Remove this Preference if it is restored without SiteSettingsDelegate.
        if (!hasSiteSettingsDelegate()) {
            getParentFragmentManager().beginTransaction().remove(this).commit();
            return;
        }
        SettingsUtils.addPreferencesFromResource(
                this, R.xml.page_info_tracking_protection_launch_preference);

        mTpSwitch = findPreference(TP_SWITCH_PREFERENCE);
        mTpSwitch.setUseSummaryAsTitle(false);

        mTpStatus = assertNonNull(findPreference(TP_STATUS_PREFERENCE));
        mManagedStatus = assertNonNull(findPreference(MANAGED_STATUS));
        mStorageInUse = assertNonNull(findPreference(STORAGE_IN_USE_PREFERENCE));
        mRwsInUse = findPreference(RWS_IN_USE_PREFERENCE);
        mRwsInUse.setVisible(false);
        mManagedTitle = findPreference(MANAGED_TITLE);
        // This part is above the toggle and changes with it, so it has to be an a11y live region.
        mTpTitle = findPreference(TP_TITLE);
        mTpTitle.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);
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

    @Initializer
    public void setParams(PageInfoTrackingProtectionLaunchViewParams params) {
        mBlockAll3pc = params.blockAll3pc;
        mIsIncognito = params.isIncognito;
        mFixedExpiration = params.fixedExpirationForTesting;
        mOnCookieSettingsLinkClicked = params.onCookieSettingsLinkClicked;
        Preference cookieSummary = findPreference(COOKIE_SUMMARY_PREFERENCE);
        ChromeClickableSpan linkSpan =
                new ChromeClickableSpan(
                        getContext(),
                        (view) -> {
                            mOnCookieSettingsLinkClicked.run();
                        });
        int summaryString;
        if (mIsIncognito) {
            summaryString =
                    R.string.page_info_tracking_protection_incognito_blocked_cookies_description;
        } else if (mBlockAll3pc) {
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
                assumeNonNull(mManagedTitle);
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
            Preference cookieSummary = findPreference(COOKIE_SUMMARY_PREFERENCE);
            cookieSummary.setVisible(false);
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
    public boolean maybeShowRwsInfo(@Nullable RwsCookieInfo rwsInfo, String currentOrigin) {
        if (rwsInfo == null || mRwsInUse == null) {
            return false;
        }

        assert getSiteSettingsDelegate().isRelatedWebsiteSetsDataAccessEnabled()
                : "RWS access should be enabled to show info.";

        mRwsInUse.setVisible(true);
        mRwsInUse.setTitle(R.string.cookie_info_rws_title);
        mRwsInUse.setSummary(
                String.format(getString(R.string.cookie_info_rws_summary), rwsInfo.getOwner()));
        mRwsInUse.setIcon(SettingsUtils.getTintedIcon(getContext(), R.drawable.tenancy));
        mRwsInUse.setManagedPreferenceDelegate(
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
}
