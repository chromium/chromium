// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.management;

import android.content.Context;
import android.content.res.Configuration;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.method.LinkMovementMethod;
import android.util.AttributeSet;
import android.widget.CheckedTextView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.settings.SettingsUtils;
import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.ViewResizer;

/**
 * The View that renders the ManagementPage (chrome://management). Consists of an medium size image
 * icon over title and descriptive text.
 */
@NullMarked
public class ManagementView extends ScrollView {
    private LinearLayout mManagementContainer;

    @VisibleForTesting TextView mTitle;
    @VisibleForTesting TextView mDescription;
    @VisibleForTesting TextView mLearnMore;
    @VisibleForTesting TextView mBrowserReporting;
    @VisibleForTesting TextView mBrowserReportingExplanation;
    @VisibleForTesting TextView mProfileReportingExplanation;
    @VisibleForTesting TextView mReportUsername;
    @VisibleForTesting TextView mReportVersion;
    @VisibleForTesting TextView mProfileReportDetails;
    @VisibleForTesting TextView mReportLegacyTech;

    @VisibleForTesting TextView mThreatProtectionTitle;
    @VisibleForTesting TextView mThreatProtectionDescription;
    @VisibleForTesting CheckedTextView mThreatProtectionMore;
    @VisibleForTesting TextView mThreatProtectionSecurityEvent;
    @VisibleForTesting TextView mThreatProtectionSecurityEventDescription;
    @VisibleForTesting TextView mThreatProtectionPageVisited;
    @VisibleForTesting TextView mThreatProtectionPageVisitedDescription;
    @VisibleForTesting TextView mThreatProtectionDownload;
    @VisibleForTesting TextView mThreatProtectionDownloadDescription;

    private @Nullable UiConfig mUiConfig;

    /** Constructor for inflating from XML. */
    public ManagementView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();

        mManagementContainer = findViewById(R.id.management_container);
        mTitle = findViewById(R.id.title_text);
        mDescription = findViewById(R.id.description_text);
        mLearnMore = findViewById(R.id.learn_more);
        mBrowserReporting = findViewById(R.id.browser_reporting);
        mBrowserReportingExplanation = findViewById(R.id.browser_reporting_explanation);
        mReportUsername = findViewById(R.id.report_username);
        mReportVersion = findViewById(R.id.report_version);
        mProfileReportingExplanation = findViewById(R.id.profile_reporting_explanation);
        mProfileReportDetails = findViewById(R.id.profile_report_details);
        mReportLegacyTech = findViewById(R.id.report_legacy_tech);

        mThreatProtectionTitle = findViewById(R.id.threat_protection_title);
        mThreatProtectionDescription = findViewById(R.id.threat_protection_description);

        mThreatProtectionMore = findViewById(R.id.threat_protection_more);
        mThreatProtectionMore.setCompoundDrawablesWithIntrinsicBounds(
                /* left= */ null,
                /* top= */ null,
                /* right= */ SettingsUtils.createExpandArrow(getContext()),
                /* bottom= */ null);
        mThreatProtectionMore.setChecked(false);
        mThreatProtectionMore.setOnClickListener(
                view -> {
                    mThreatProtectionMore.toggle();
                    adjustThreatProtectionView();
                });

        mThreatProtectionSecurityEvent = findViewById(R.id.threat_protection_security_event);
        mThreatProtectionSecurityEventDescription =
                findViewById(R.id.threat_protection_security_event_description);
        mThreatProtectionPageVisited = findViewById(R.id.threat_protection_page_visited);
        mThreatProtectionPageVisitedDescription =
                findViewById(R.id.threat_protection_page_visited_description);
        mThreatProtectionDownload = findViewById(R.id.threat_protection_download);
        mThreatProtectionDownloadDescription =
                findViewById(R.id.threat_protection_download_description);

        // Set default management status.
        setManagementNotice(/* isBrowserManaged= */ false, /* isProfileManaged= */ false);
        setReportingState(
                /* isBrowserReportingEnabled= */ false,
                /* isProfileReportingEnabled= */ false,
                /* isLegacyTechReportingEnabled= */ false);

        mThreatProtectionSecurityEvent.setEnabled(false);
        mThreatProtectionPageVisited.setEnabled(false);
        mThreatProtectionDownload.setEnabled(false);

        adjustThreatProtectionView();

        // Making the view focusable ensures that it will be presented to the user once they select
        // the page on the Omnibox. When the view is not focusable, the keyboard needs to be
        // dismissed before the page is shown.
        setFocusable(true);
        setFocusableInTouchMode(true);

        // Set width constraints.
        configureWideDisplayStyle();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        // Set width constraints.
        configureWideDisplayStyle();
    }

    /**
     * Shows or hides the management notice and "learn more" link based on management status.
     *
     * @param isBrowserManaged Whether the browser is managed.
     * @param isProfileManaged Whether the profile is managed.
     */
    public void setManagementNotice(boolean isBrowserManaged, boolean isProfileManaged) {
        boolean isManaged = isBrowserManaged || isProfileManaged;
        mDescription.setVisibility(isManaged ? VISIBLE : GONE);
        if (isManaged) {
            mDescription.setText(
                    getContext()
                            .getString(
                                    isBrowserManaged
                                            ? R.string.management_browser_notice
                                            : R.string.management_profile_notice));
        }
        mLearnMore.setVisibility(isManaged ? VISIBLE : GONE);
    }

    /**
     * Shows or hides the reporting section based on which reporting features are enabled.
     *
     * @param isBrowserReportingEnabled Whether browser status reporting is enabled.
     * @param isProfileReportingEnabled Whether profile reporting is enabled.
     * @param isLegacyTechReportingEnabled Whether legacy tech reporting is enabled.
     */
    public void setReportingState(
            boolean isBrowserReportingEnabled,
            boolean isProfileReportingEnabled,
            boolean isLegacyTechReportingEnabled) {
        mBrowserReporting.setVisibility(
                isBrowserReportingEnabled
                                || isLegacyTechReportingEnabled
                                || isProfileReportingEnabled
                        ? VISIBLE
                        : GONE);

        mReportUsername.setVisibility(isBrowserReportingEnabled ? VISIBLE : GONE);
        mReportVersion.setVisibility(isBrowserReportingEnabled ? VISIBLE : GONE);

        boolean showProfileReporting = !isBrowserReportingEnabled && isProfileReportingEnabled;
        mProfileReportingExplanation.setVisibility(showProfileReporting ? VISIBLE : GONE);
        mProfileReportDetails.setVisibility(showProfileReporting ? VISIBLE : GONE);

        // If Legacy tech report is enabled without browser or profile status report, show browser
        // report explanations as default.
        boolean showBrowserReportingExplanation =
                isBrowserReportingEnabled
                        || (isLegacyTechReportingEnabled && !isProfileReportingEnabled);
        mBrowserReportingExplanation.setVisibility(
                showBrowserReportingExplanation ? VISIBLE : GONE);

        mReportLegacyTech.setVisibility(isLegacyTechReportingEnabled ? VISIBLE : GONE);
    }

    public void setProfileReportingText(SpannableStringBuilder text) {
        mProfileReportDetails.setText(text);
        mProfileReportDetails.setMovementMethod(LinkMovementMethod.getInstance());
    }

    public boolean isThreatProtectionEnabled() {
        return mThreatProtectionSecurityEvent.isEnabled()
                || mThreatProtectionPageVisited.isEnabled()
                || mThreatProtectionDownload.isEnabled();
    }

    public void setSecurityEventReportingEnabled(boolean isEnabled) {
        mThreatProtectionSecurityEvent.setEnabled(isEnabled);
        adjustThreatProtectionView();
    }

    public void setUrlFilteringEnabled(boolean isEnabled) {
        mThreatProtectionPageVisited.setEnabled(isEnabled);
        adjustThreatProtectionView();
    }

    public void setDownloadEnterpriseScanEnabled(boolean isEnabled) {
        mThreatProtectionDownload.setEnabled(isEnabled);
        adjustThreatProtectionView();
    }

    public void setLearnMoreText(SpannableString learnMoreText) {
        mLearnMore.setText(learnMoreText);
        mLearnMore.setMovementMethod(LinkMovementMethod.getInstance());
    }

    public void setLegacyTechReportingText(SpannableString text) {
        mReportLegacyTech.setText(text);
        mReportLegacyTech.setMovementMethod(LinkMovementMethod.getInstance());
    }

    public void setTitleText(String title) {
        mTitle.setText(title);
    }

    public void setDescriptionText(String description) {
        mDescription.setText(description);
    }

    public void setSecurityEventReportingText(SpannableStringBuilder text) {
        mThreatProtectionSecurityEvent.setText(text);
    }

    public void setSecurityEventReportingDescriptionText(SpannableStringBuilder text) {
        mThreatProtectionSecurityEventDescription.setText(text);
    }

    public void setUrlFilteringText(SpannableStringBuilder text) {
        mThreatProtectionPageVisited.setText(text);
    }

    public void setUrlFilteringDescriptionText(SpannableStringBuilder text) {
        mThreatProtectionPageVisitedDescription.setText(text);
    }

    public void setDownloadEnterpriseScanText(SpannableStringBuilder text) {
        mThreatProtectionDownload.setText(text);
    }

    public void setDownloadEnterpriseScanDescriptionText(SpannableStringBuilder text) {
        mThreatProtectionDownloadDescription.setText(text);
    }

    /** Adjusts the threat protection section based on which features are enabled. */
    private void adjustThreatProtectionView() {
        mThreatProtectionTitle.setVisibility(isThreatProtectionEnabled() ? VISIBLE : GONE);
        mThreatProtectionDescription.setVisibility(isThreatProtectionEnabled() ? VISIBLE : GONE);
        mThreatProtectionMore.setVisibility(isThreatProtectionEnabled() ? VISIBLE : GONE);

        mThreatProtectionSecurityEvent.setVisibility(
                mThreatProtectionSecurityEvent.isEnabled() && mThreatProtectionMore.isChecked()
                        ? VISIBLE
                        : GONE);
        mThreatProtectionSecurityEventDescription.setVisibility(
                mThreatProtectionSecurityEvent.isEnabled() && mThreatProtectionMore.isChecked()
                        ? VISIBLE
                        : GONE);

        mThreatProtectionPageVisited.setVisibility(
                mThreatProtectionPageVisited.isEnabled() && mThreatProtectionMore.isChecked()
                        ? VISIBLE
                        : GONE);
        mThreatProtectionPageVisitedDescription.setVisibility(
                mThreatProtectionPageVisited.isEnabled() && mThreatProtectionMore.isChecked()
                        ? VISIBLE
                        : GONE);

        mThreatProtectionDownload.setVisibility(
                mThreatProtectionDownload.isEnabled() && mThreatProtectionMore.isChecked()
                        ? VISIBLE
                        : GONE);
        mThreatProtectionDownloadDescription.setVisibility(
                mThreatProtectionDownload.isEnabled() && mThreatProtectionMore.isChecked()
                        ? VISIBLE
                        : GONE);
    }

    /**
     * When this layout has a wide display style, it will be width constrained to
     * {@link UiConfig#WIDE_DISPLAY_STYLE_MIN_WIDTH_DP}. If the current screen width is greater than
     * UiConfig#WIDE_DISPLAY_STYLE_MIN_WIDTH_DP, the settings layout will be visually centered
     * by adding padding to both sides.
     */
    private void configureWideDisplayStyle() {
        if (mUiConfig == null) {
            final int minPadding = getResources().getDimensionPixelSize(R.dimen.cm_padding);
            final int minWidePadding =
                    getResources().getDimensionPixelSize(R.dimen.cm_padding_wide);

            mUiConfig = new UiConfig(mManagementContainer);
            ViewResizer.createAndAttach(
                    mManagementContainer, mUiConfig, minPadding, minWidePadding);
        } else {
            mUiConfig.updateDisplayStyle();
        }
    }
}
