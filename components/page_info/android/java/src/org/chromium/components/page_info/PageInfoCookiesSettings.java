// Copyright 2020 The Chromium Authors
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
import org.chromium.components.browser_ui.site_settings.ForwardingManagedPreferenceDelegate;
import org.chromium.components.browser_ui.site_settings.RWSCookieInfo;
import org.chromium.components.browser_ui.util.date.CalendarUtils;
import org.chromium.components.content_settings.CookieControlsEnforcement;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.util.AttrUtils;

/** View showing a toggle and a description for third-party cookie blocking for a site. */
public class PageInfoCookiesSettings extends BaseSiteSettingsFragment {
    private static final String COOKIE_SUMMARY_PREFERENCE = "cookie_summary";
    private static final String COOKIE_SWITCH_PREFERENCE = "cookie_switch";
    private static final String COOKIE_IN_USE_PREFERENCE = "cookie_in_use";
    private static final String RWS_IN_USE_PREFERENCE = "rws_in_use";
    private static final String TPC_TITLE = "tpc_title";
    private static final String TPC_SUMMARY = "tpc_summary";

    private ChromeSwitchPreference mCookieSwitch;
    private ChromeImageViewPreference mCookieInUse;
    private ChromeImageViewPreference mRWSInUse;
    private TextMessagePreference mThirdPartyCookiesTitle;
    private TextMessagePreference mThirdPartyCookiesSummary;
    private Runnable mOnClearCallback;
    private Runnable mOnCookieSettingsLinkClicked;
    private Callback<Activity> mOnFeedbackClicked;
    private Dialog mConfirmationDialog;
    private boolean mDeleteDisabled;
    private boolean mDataUsed;
    private CharSequence mHostName;
    private RWSCookieInfo mRWSInfo;
    private PageInfoControllerDelegate mPageInfoControllerDelegate;

    /** Parameters to configure the cookie controls view. */
    public static class PageInfoCookiesViewParams {
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
        mRWSInUse = findPreference(RWS_IN_USE_PREFERENCE);
        mRWSInUse.setVisible(false);
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

    private String getQuantityString(int resId, int count) {
        return getContext().getResources().getQuantityString(resId, count, count);
    }

    public void setParams(PageInfoCookiesViewParams params) {
        mOnCookieSettingsLinkClicked = params.onCookieSettingsLinkClicked;
        Preference cookieSummary = findPreference(COOKIE_SUMMARY_PREFERENCE);
        ClickableSpan linkSpan =
                new ClickableSpan() {
                    @Override
                    public void onClick(View view) {
                        mOnCookieSettingsLinkClicked.run();
                    }
                };
        cookieSummary.setSummary(
                SpanApplier.applySpans(
                        getString(R.string.page_info_cookies_description),
                        new SpanApplier.SpanInfo("<link>", "</link>", linkSpan)));

        // TODO(crbug.com/40129299): Set a ManagedPreferenceDelegate?
        mCookieSwitch.setVisible(params.thirdPartyCookieBlockingEnabled);
        mCookieSwitch.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    boolean boolValue = (Boolean) newValue;
                    // Invert since the switch is inverted.
                    boolValue = !boolValue;
                    params.onThirdPartyCookieToggleChanged.onResult(boolValue);
                    return true;
                });

        mCookieInUse.setIcon(SettingsUtils.getTintedIcon(getContext(), R.drawable.gm_database_24));
        mCookieInUse.setImageView(
                R.drawable.ic_delete_white_24dp, R.string.page_info_cookies_clear, null);
        // Disabling enables passthrough of clicks to the main preference.
        mCookieInUse.setImageViewEnabled(false);
        mDeleteDisabled = params.disableCookieDeletion;
        mCookieInUse.setOnPreferenceClickListener(
                preference -> {
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
                        .setPositiveButton(
                                R.string.page_info_cookies_clear_confirmation_button,
                                (dialog, which) -> mOnClearCallback.run())
                        .setNegativeButton(
                                R.string.cancel, (dialog, which) -> mConfirmationDialog = null)
                        .show();
    }

    public void setCookieStatus(
            boolean controlsVisible,
            boolean protectionsOn,
            @CookieControlsEnforcement int enforcement,
            long expiration) {
        if (enforcement == CookieControlsEnforcement.ENFORCED_BY_TPCD_GRANT) {
            // Hide all the 3PC controls.
            mCookieSwitch.setVisible(false);
            mThirdPartyCookiesTitle.setVisible(false);
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
            mThirdPartyCookiesSummary.setSummary(
                    SpanApplier.applySpans(
                            getString(
                                    R.string.page_info_tracking_protection_site_grant_description),
                            new SpanApplier.SpanInfo("<link>", "</link>", linkSpan)));
            mThirdPartyCookiesSummary.setDividerAllowedAbove(true);
            return;
        }

        mCookieSwitch.setVisible(controlsVisible);
        mThirdPartyCookiesTitle.setVisible(controlsVisible);
        mThirdPartyCookiesSummary.setVisible(controlsVisible);

        if (!controlsVisible) return;

        mCookieSwitch.setIcon(
                SettingsUtils.getTintedIcon(
                        getContext(),
                        protectionsOn
                                ? R.drawable.ic_visibility_off_black
                                : R.drawable.ic_visibility_black));
        mCookieSwitch.setChecked(!protectionsOn);
        mCookieSwitch.setEnabled(enforcement == CookieControlsEnforcement.NO_ENFORCEMENT);
        mCookieSwitch.setManagedPreferenceDelegate(
                new ForwardingManagedPreferenceDelegate(
                        getSiteSettingsDelegate().getManagedPreferenceDelegate()) {
                    @Override
                    public boolean isPreferenceControlledByPolicy(Preference preference) {
                        return enforcement == CookieControlsEnforcement.ENFORCED_BY_POLICY;
                    }
                });

        boolean permanentException = (expiration == 0);

        ClickableSpan feedbackSpan =
                new ClickableSpan() {
                    @Override
                    public void onClick(View view) {
                        mOnFeedbackClicked.onResult(getActivity());
                    }
                };

        if (protectionsOn) {
            mThirdPartyCookiesTitle.setTitle(
                    getString(R.string.page_info_cookies_site_not_working_title));
            int resId =
                    willCreatePermanentException()
                            ? R.string.page_info_cookies_site_not_working_description_permanent
                            : R.string.page_info_cookies_site_not_working_description_temporary;
            mThirdPartyCookiesSummary.setSummary(getString(resId));
        } else if (permanentException) {
            mThirdPartyCookiesTitle.setTitle(
                    getString(R.string.page_info_cookies_permanent_allowed_title));
            int resId =
                    R.string.page_info_cookies_tracking_protection_permanent_allowed_description;
            mThirdPartyCookiesSummary.setSummary(
                    SpanApplier.applySpans(
                            getString(resId),
                            new SpanApplier.SpanInfo("<link>", "</link>", feedbackSpan)));
        } else { // Not blocking and temporary exception.
            int days = calculateDaysUntilExpiration(TimeUtils.currentTimeMillis(), expiration);
            updateThirdPartyCookiesTitleTemporary(days);
            int resId = R.string.page_info_cookies_send_feedback_description;
            mThirdPartyCookiesSummary.setSummary(
                    SpanApplier.applySpans(
                            getString(resId),
                            new SpanApplier.SpanInfo("<link>", "</link>", feedbackSpan)));
        }
        updateCookieSwitch();
    }

    public void setStorageUsage(long storageUsage) {
        mCookieInUse.setTitle(
                String.format(
                        getString(R.string.origin_settings_storage_usage_brief),
                        Formatter.formatShortFileSize(getContext(), storageUsage)));

        mDataUsed |= storageUsage != 0;
        updateCookieDeleteButton();
    }

    /**
     * @param delegate {@link PageInfoControllerDelegate} for showing filtered RWS (Related Website
     *     Sets) in settings.
     */
    public void setPageInfoDelegate(PageInfoControllerDelegate delegate) {
        mPageInfoControllerDelegate = delegate;
    }

    /**
     * Returns a boolean indicating if the RWS info has been shown or not.
     *
     * @param rwsInfo Related Website Sets info to show.
     * @param currentOrigin PageInfo current origin.
     * @return a boolean indicating if the RWS info has been shown or not.
     */
    public boolean maybeShowRWSInfo(RWSCookieInfo rwsInfo, String currentOrigin) {
        mRWSInfo = rwsInfo;
        if (rwsInfo == null || mRWSInUse == null) {
            return false;
        }

        assert getSiteSettingsDelegate().isPrivacySandboxFirstPartySetsUIFeatureEnabled()
                        && getSiteSettingsDelegate().isRelatedWebsiteSetsDataAccessEnabled()
                : "Related Website Sets UI and access should be enabled to show RWS info.";

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
        if (getSiteSettingsDelegate().shouldShowPrivacySandboxRwsUi()) {
            mRWSInUse.setOnPreferenceClickListener(
                    preference -> {
                        mPageInfoControllerDelegate.showAllSettingsForRws(mRWSInfo.getOwner());
                        return false;
                    });
        }

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
        mCookieInUse.setImageColor(
                !mDeleteDisabled && mDataUsed
                        ? R.color.default_icon_color_accent1_tint_list
                        : R.color.default_icon_color_disabled);
    }

    private void updateCookieSwitch() {
        // TODO(crbug.com/40064612): Update the strings for when FPS are on.
        if (!mCookieSwitch.isChecked()) {
            mCookieSwitch.setSummary(
                    getString(R.string.page_info_tracking_protection_toggle_blocked));
        } else {
            mCookieSwitch.setSummary(
                    getString(R.string.page_info_tracking_protection_toggle_allowed));
        }
    }

    private void updateThirdPartyCookiesTitleTemporary(int days) {
        mThirdPartyCookiesTitle.setTitle(
                days == 0
                        ? getString(R.string.page_info_cookies_blocking_restart_today_title)
                        : getQuantityString(
                                R.plurals.page_info_cookies_blocking_restart_title, days));
    }

    private boolean willCreatePermanentException() {
        return "0d".equals(PageInfoFeatures.getUserBypassExpiration());
    }
}
