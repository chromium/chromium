// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.page_info;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.app.Dialog;
import android.os.Bundle;
import android.text.format.DateUtils;
import android.text.format.Formatter;
import android.view.View;

import androidx.appcompat.app.AlertDialog;
import androidx.preference.Preference;

import org.chromium.base.Callback;
import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.settings.ChromeBasePreference;
import org.chromium.components.browser_ui.settings.ChromeImageViewPreference;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.settings.TextMessagePreference;
import org.chromium.components.browser_ui.site_settings.BaseSiteSettingsFragment;
import org.chromium.components.browser_ui.site_settings.ForwardingManagedPreferenceDelegate;
import org.chromium.components.browser_ui.site_settings.RwsCookieInfo;
import org.chromium.components.browser_ui.site_settings.Website;
import org.chromium.components.browser_ui.util.date.CalendarUtils;
import org.chromium.components.content_settings.CookieControlsEnforcement;
import org.chromium.components.content_settings.CookieControlsState;
import org.chromium.ui.text.ChromeClickableSpan;
import org.chromium.ui.text.SpanApplier;

/** View showing a toggle and a description for third-party cookie blocking for a site. */
@NullMarked
public class PageInfoCookiesSettings extends BaseSiteSettingsFragment {
    private static final String COOKIE_SUMMARY_PREFERENCE = "cookie_summary";
    private static final String COOKIE_SWITCH_PREFERENCE = "cookie_switch";
    private static final String COOKIE_IN_USE_PREFERENCE = "cookie_in_use";
    private static final String RWS_IN_USE_PREFERENCE = "rws_in_use";
    private static final String TPC_TITLE = "tpc_title";
    private static final String TPC_SUMMARY = "tpc_summary";

    private ChromeSwitchPreference mCookieSwitch;
    private ChromeImageViewPreference mCookieInUse;
    private ChromeBasePreference mRwsInUse;
    private TextMessagePreference mThirdPartyCookiesTitle;
    private TextMessagePreference mThirdPartyCookiesSummary;
    private Runnable mOnClearCallback;
    private Runnable mOnCookieSettingsLinkClicked;
    private Callback<Activity> mOnFeedbackClicked;
    private @Nullable Dialog mConfirmationDialog;
    private boolean mDeleteDisabled;
    private boolean mDataUsed;
    private @Nullable CharSequence mHostName;
    private boolean mIsModeBUi;
    private boolean mBlockAll3pc;
    private boolean mIsIncognito;
    private PageInfoControllerDelegate mPageInfoControllerDelegate;
    // Sets a constant # of days until expiration to prevent test flakiness.
    private boolean mFixedExpirationForTesting;
    private int mDaysUntilExpirationForTesting;

    /** Parameters to configure the cookie controls view. */
    static class PageInfoCookiesViewParams {
        public final Callback<Boolean> onThirdPartyCookieToggleChanged;
        public final Runnable onClearCallback;
        public final Runnable onCookieSettingsLinkClicked;
        public final Callback<Activity> onFeedbackLinkClicked;
        public final boolean disableCookieDeletion;
        public final CharSequence hostName;
        public final boolean blockAll3pc;
        public final boolean isIncognito;
        public final boolean isModeBUi;
        public final boolean fixedExpirationForTesting;
        public final int daysUntilExpirationForTesting;

        public PageInfoCookiesViewParams(
                Callback<Boolean> onThirdPartyCookieToggleChanged,
                Runnable onClearCallback,
                Runnable onCookieSettingsLinkClicked,
                Callback<Activity> onFeedbackLinkClicked,
                boolean disableCookieDeletion,
                CharSequence hostName,
                boolean blockAll3pc,
                boolean isIncognito,
                boolean isModeBUi,
                boolean fixedExpirationForTesting,
                int daysUntilExpirationForTesting) {
            this.onThirdPartyCookieToggleChanged = onThirdPartyCookieToggleChanged;
            this.onClearCallback = onClearCallback;
            this.onCookieSettingsLinkClicked = onCookieSettingsLinkClicked;
            this.onFeedbackLinkClicked = onFeedbackLinkClicked;
            this.disableCookieDeletion = disableCookieDeletion;
            this.hostName = hostName;
            this.blockAll3pc = blockAll3pc;
            this.isIncognito = isIncognito;
            this.isModeBUi = isModeBUi;
            this.fixedExpirationForTesting = fixedExpirationForTesting;
            this.daysUntilExpirationForTesting = daysUntilExpirationForTesting;
        }
    }

    @Override
    public void onCreatePreferences(@Nullable Bundle bundle, @Nullable String s) {
        // Remove this Preference if it is restored without SiteSettingsDelegate.
        if (!hasSiteSettingsDelegate()) {
            getParentFragmentManager().beginTransaction().remove(this).commit();
            // Appease NullAway.
            assumeNonNull(mRwsInUse);
            return;
        }
        SettingsUtils.addPreferencesFromResource(this, R.xml.page_info_cookie_preference);
        mCookieSwitch = assertNonNull(findPreference(COOKIE_SWITCH_PREFERENCE));
        mCookieInUse = assertNonNull(findPreference(COOKIE_IN_USE_PREFERENCE));
        mRwsInUse = assertNonNull(findPreference(RWS_IN_USE_PREFERENCE));
        mRwsInUse.setVisible(false);
        mThirdPartyCookiesTitle = assertNonNull(findPreference(TPC_TITLE));
        mThirdPartyCookiesSummary = assertNonNull(findPreference(TPC_SUMMARY));
        // Set accessibility properties on the region that will change with the toggle.
        // Two a11y live regions don't work at the same time. Using a workaround of setting the
        // content description for both the title and the summary on one of them.
        // See crbug.com/388844792 for more background.
        updateContentDescriptionsForA11y();
        mThirdPartyCookiesSummary.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        if (mConfirmationDialog != null) {
            mConfirmationDialog.dismiss();
        }
    }

    /**
     * @param delegate {@link PageInfoControllerDelegate} for showing filtered RWS (Related Website
     *     Sets) in settings.
     */
    @Initializer
    public void setParams(PageInfoCookiesViewParams params, PageInfoControllerDelegate delegate) {
        mPageInfoControllerDelegate = delegate;
        mOnCookieSettingsLinkClicked = params.onCookieSettingsLinkClicked;
        mFixedExpirationForTesting = params.fixedExpirationForTesting;
        mBlockAll3pc = params.blockAll3pc;
        mIsIncognito = params.isIncognito;
        mIsModeBUi = params.isModeBUi;
        mDaysUntilExpirationForTesting = params.daysUntilExpirationForTesting;
        mDeleteDisabled = params.disableCookieDeletion;
        mOnClearCallback = params.onClearCallback;
        mOnFeedbackClicked = params.onFeedbackLinkClicked;
        mHostName = params.hostName;

        // Initialize UI elements that are based on params.
        Preference cookieSummary = assertNonNull(findPreference(COOKIE_SUMMARY_PREFERENCE));
        cookieSummary.setSummary(
                SpanApplier.applySpans(
                        getSummaryString(),
                        new SpanApplier.SpanInfo(
                                "<link>",
                                "</link>",
                                new ChromeClickableSpan(
                                        getContext(),
                                        (view) -> {
                                            mOnCookieSettingsLinkClicked.run();
                                        }))));

        mCookieSwitch.setOnPreferenceChangeListener(
                (preference, newValue) -> {
                    boolean boolValue = (Boolean) newValue;
                    // Invert since the switch is inverted.
                    boolValue = !boolValue;
                    params.onThirdPartyCookieToggleChanged.onResult(boolValue);
                    return true;
                });

        initCookieInUse();
        updateCookieDeleteButton();
    }

    private String getSummaryString() {
        if (!mIsModeBUi) {
            // Pre Mode B description: "Cookies and other site data are used to remember you..."
            return getString(R.string.page_info_cookies_description);
        } else if (mIsIncognito) {
            // Description of chrome blocking sites: "Chrome blocks sites..."
            return getString(
                    R.string.page_info_tracking_protection_incognito_blocked_cookies_description);
        } else if (mBlockAll3pc) {
            // Description of user blocking sites: "You blocked sites..."
            return getString(R.string.page_info_tracking_protection_blocked_cookies_description);
        } else {
            // Description of Chrome limiting cookies: "Chrome limits most sites...""
            return getString(R.string.page_info_tracking_protection_description);
        }
    }

    private void initCookieInUse() {
        mCookieInUse.setIcon(SettingsUtils.getTintedIcon(getContext(), R.drawable.gm_database_24));
        mCookieInUse.setImageView(
                R.drawable.ic_delete_white_24dp, R.string.page_info_cookies_clear, null);
        // Disabling enables passthrough of clicks to the main preference.
        mCookieInUse.setImageViewEnabled(false);
        mCookieInUse.setOnPreferenceClickListener(
                preference -> {
                    showClearCookiesConfirmation();
                    return true;
                });
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

    public void updateState(
            @CookieControlsState int controlsState,
            @CookieControlsEnforcement int enforcement,
            long expiration) {
        if (controlsState == CookieControlsState.ACTIVE_TP
                || controlsState == CookieControlsState.PAUSED_TP) {
            updateTrackingProtectionState();
        } else {
            update3pcState(controlsState, enforcement, expiration);
        }
    }

    public void updateTrackingProtectionState() {
        // TODO(crbug.com/388294499): Add support for TP UI.
        mCookieSwitch.setVisible(false);
    }

    public void update3pcState(
            @CookieControlsState int controlsState,
            @CookieControlsEnforcement int enforcement,
            long expiration) {
        if (enforcement == CookieControlsEnforcement.ENFORCED_BY_TPCD_GRANT) {
            setTpcdGrantState();
            updateContentDescriptionsForA11y();
            return;
        }
        boolean visible = controlsState != CookieControlsState.HIDDEN;
        mCookieSwitch.setVisible(visible);
        mThirdPartyCookiesTitle.setVisible(visible);
        mThirdPartyCookiesSummary.setVisible(visible);

        if (!visible) return;

        updateCookieSwitch(controlsState == CookieControlsState.ALLOWED3PC, enforcement);
        updateTitleAndSummary(controlsState, expiration);
        updateContentDescriptionsForA11y();
    }

    private void setTpcdGrantState() {
        mCookieSwitch.setVisible(false);
        mThirdPartyCookiesTitle.setVisible(false);
        Preference cookieSummary = assertNonNull(findPreference(COOKIE_SUMMARY_PREFERENCE));
        cookieSummary.setVisible(false);
        mThirdPartyCookiesSummary.setSummary(
                SpanApplier.applySpans(
                        getString(R.string.page_info_tracking_protection_site_grant_description),
                        new SpanApplier.SpanInfo(
                                "<link>",
                                "</link>",
                                new ChromeClickableSpan(
                                        getContext(),
                                        (view) -> {
                                            mOnCookieSettingsLinkClicked.run();
                                        }))));
        mThirdPartyCookiesSummary.setDividerAllowedAbove(true);
    }

    private void updateCookieSwitch(
            boolean cookiesAllowed, @CookieControlsEnforcement int enforcement) {
        mCookieSwitch.setIcon(
                SettingsUtils.getTintedIcon(
                        getContext(),
                        cookiesAllowed
                                ? R.drawable.ic_visibility_black
                                : R.drawable.ic_visibility_off_black));
        mCookieSwitch.setChecked(cookiesAllowed);
        mCookieSwitch.setEnabled(enforcement == CookieControlsEnforcement.NO_ENFORCEMENT);
        mCookieSwitch.setManagedPreferenceDelegate(
                new ForwardingManagedPreferenceDelegate(
                        getSiteSettingsDelegate().getManagedPreferenceDelegate()) {
                    @Override
                    public boolean isPreferenceControlledByPolicy(Preference preference) {
                        return enforcement == CookieControlsEnforcement.ENFORCED_BY_POLICY;
                    }
                });
        int resId;
        if (cookiesAllowed) {
            resId = R.string.page_info_tracking_protection_toggle_allowed;
        } else if (mIsModeBUi && !mBlockAll3pc) {
            resId = R.string.page_info_tracking_protection_toggle_limited;
        } else {
            resId = R.string.page_info_tracking_protection_toggle_blocked;
        }
        mCookieSwitch.setSummary(getString(resId));
    }

    private void updateTitleAndSummary(@CookieControlsState int controlsState, long expiration) {
        int resId;
        if (controlsState == CookieControlsState.BLOCKED3PC) {
            mThirdPartyCookiesTitle.setTitle(
                    getString(R.string.page_info_cookies_site_not_working_title));
            resId = R.string.page_info_cookies_site_not_working_description_tracking_protection;
            mThirdPartyCookiesSummary.setSummary(getString(resId));
            return;
        }

        mThirdPartyCookiesTitle.setTitle(getThirdPartyCookiesAllowedTitle(expiration));
        if (expiration == 0) {
            resId = R.string.page_info_cookies_tracking_protection_permanent_allowed_description;
        } else if (mIsModeBUi) {
            resId = R.string.page_info_cookies_tracking_protection_description;
        } else {
            resId = R.string.page_info_cookies_send_feedback_description;
        }
        mThirdPartyCookiesSummary.setSummary(
                SpanApplier.applySpans(
                        getString(resId),
                        new SpanApplier.SpanInfo(
                                "<link>",
                                "</link>",
                                new ChromeClickableSpan(
                                        getContext(),
                                        (view) -> {
                                            mOnFeedbackClicked.onResult(this.getActivity());
                                        }))));
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

    private String getThirdPartyCookiesAllowedTitle(long expiration) {
        if (expiration == 0) {
            return getString(R.string.page_info_cookies_permanent_allowed_title);
        }
        int days =
                mFixedExpirationForTesting
                        ? mDaysUntilExpirationForTesting
                        : calculateDaysUntilExpiration(TimeUtils.currentTimeMillis(), expiration);
        if (mIsModeBUi && !mBlockAll3pc) {
            if (days == 0) {
                return getString(R.string.page_info_cookies_limiting_restart_today_title);
            }
            return getQuantityString(R.plurals.page_info_cookies_limiting_restart_title, days);
        }
        if (days == 0) {
            return getString(R.string.page_info_cookies_blocking_restart_today_title);
        }
        return getQuantityString(
                R.plurals.page_info_cookies_blocking_restart_tracking_protection_title, days);
    }

    private String getQuantityString(int resId, int count) {
        return getContext().getResources().getQuantityString(resId, count, count);
    }

    // TODO(crbug.com/388844792): Revert back to two live regions once that's supported.
    private void updateContentDescriptionsForA11y() {
        // Combine both the title and the summary into a content description inside of a single a11y
        // live region.
        mThirdPartyCookiesTitle.setTitleContentDescription("");
        mThirdPartyCookiesSummary.setSummaryContentDescription(
                mThirdPartyCookiesTitle.getTitle() + " " + mThirdPartyCookiesSummary.getSummary());
    }

    public void setStorageUsage(long storageUsage) {
        mCookieInUse.setTitle(
                String.format(
                        getString(R.string.origin_settings_storage_usage_brief),
                        Formatter.formatShortFileSize(getContext(), storageUsage)));

        mDataUsed |= storageUsage != 0;
        updateCookieDeleteButton();
    }

    private void updateCookieDeleteButton() {
        mCookieInUse.setImageColor(
                !mDeleteDisabled && mDataUsed
                        ? R.color.default_icon_color_accent1_tint_list
                        : R.color.default_icon_color_disabled);
    }

    /**
     * Returns a boolean indicating if the RWS info has been shown or not.
     *
     * @param rwsInfo Related Website Sets info to show.
     * @param currentOrigin PageInfo current origin.
     * @return a boolean indicating if the RWS info has been shown or not.
     */
    public boolean maybeShowRwsInfo(@Nullable RwsCookieInfo rwsInfo, String currentOrigin) {
        if (rwsInfo == null) {
            return false;
        }

        assert getSiteSettingsDelegate().isRelatedWebsiteSetsDataAccessEnabled()
                : "RWS access should be enabled to show info.";

        mRwsInUse.setVisible(true);
        mRwsInUse.setIcon(SettingsUtils.getTintedIcon(getContext(), R.drawable.tenancy));
        if (getSiteSettingsDelegate().shouldShowPrivacySandboxRwsUi()) {
            mRwsInUse.setTitle(R.string.page_info_rws_v2_button_title);
            mRwsInUse.setSummary(
                    String.format(
                            getString(R.string.page_info_rws_v2_button_subtitle_android),
                            rwsInfo.getOwner()));
            mRwsInUse.setOnPreferenceClickListener(
                    preference -> {
                        Website currentWebsite = rwsInfo.findWebsiteForOrigin(currentOrigin);
                        if (currentWebsite != null) {
                            mPageInfoControllerDelegate.showSiteSettings(currentWebsite);
                            RecordUserAction.record(
                                    "PageInfo.CookiesSubpage.RwsSiteSettingsOpened");
                        }
                        return false;
                    });
            mRwsInUse.setManagedPreferenceDelegate(
                    new ForwardingManagedPreferenceDelegate(
                            getSiteSettingsDelegate().getManagedPreferenceDelegate()) {
                        @Override
                        public boolean isPreferenceControlledByPolicy(Preference preference) {
                            return getSiteSettingsDelegate()
                                    .isPartOfManagedRelatedWebsiteSet(currentOrigin);
                        }

                        /*
                         * The entrypoint to site settings should work even for sites in a
                         * managed set.
                         */
                        @Override
                        public boolean isPreferenceClickDisabled(Preference preference) {
                            return false;
                        }
                    });
        } else {
            mRwsInUse.setTitle(R.string.cookie_info_rws_title);
            mRwsInUse.setSummary(
                    String.format(getString(R.string.cookie_info_rws_summary), rwsInfo.getOwner()));
            mRwsInUse.setManagedPreferenceDelegate(
                    new ForwardingManagedPreferenceDelegate(
                            getSiteSettingsDelegate().getManagedPreferenceDelegate()) {
                        @Override
                        public boolean isPreferenceControlledByPolicy(Preference preference) {
                            return getSiteSettingsDelegate()
                                    .isPartOfManagedRelatedWebsiteSet(currentOrigin);
                        }
                    });
        }

        return true;
    }
}
