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
    private boolean mIsBrowserManaged;
    private boolean mIsProfileManaged;
    private boolean mIsBrowserReportingEnabled;
    private boolean mIsProfileReportingEnabled;
    private boolean mIsLegacyTechReportingEnabled;
    private boolean mIsSecurityEventReportingEnabled;
    private boolean mIsUrlFilteringEnabled;

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
                    adjustView();
                });

        mThreatProtectionSecurityEvent = findViewById(R.id.threat_protection_security_event);
        mThreatProtectionSecurityEventDescription =
                findViewById(R.id.threat_protection_security_event_description);
        mThreatProtectionPageVisited = findViewById(R.id.threat_protection_page_visited);
        mThreatProtectionPageVisitedDescription =
                findViewById(R.id.threat_protection_page_visited_description);

        // Set default management status
        mIsBrowserManaged = false;
        mIsProfileManaged = false;
        mIsBrowserReportingEnabled = false;
        mIsProfileReportingEnabled = false;
        mIsLegacyTechReportingEnabled = false;
        mIsSecurityEventReportingEnabled = false;
        mIsUrlFilteringEnabled = false;

        adjustView();

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

    /** Sets whether browser is managed. Then updates view accordingly. */
    public void setBrowserManaged(boolean isManaged) {
        if (mIsBrowserManaged != isManaged) {
            mIsBrowserManaged = isManaged;
            adjustView();
        }
    }

    /** Sets whether profile is managed. Then updates view accordingly. */
    public void setProfileManaged(boolean isManaged) {
        if (mIsProfileManaged != isManaged) {
            mIsProfileManaged = isManaged;
            adjustView();
        }
    }

    /** Gets whether browser or profile is managed. */
    public boolean isManaged() {
        return mIsBrowserManaged || mIsProfileManaged;
    }

    /** Sets whether status reporting is enabled. Then updates view accordingly. */
    public void setBrowserReportingEnabled(boolean isEnabled) {
        if (mIsBrowserReportingEnabled != isEnabled) {
            mIsBrowserReportingEnabled = isEnabled;
            adjustView();
        }
    }

    /** Gets whether status reporting is enabled. */
    public boolean isBrowserReportingEnabled() {
        return mIsBrowserReportingEnabled;
    }

    /** Sets whether profile reporting is enabled. Then updates view accordingly. */
    public void setProfileReportingEnabled(boolean isEnabled) {
        if (mIsProfileReportingEnabled != isEnabled) {
            mIsProfileReportingEnabled = isEnabled;
            adjustView();
        }
    }

    /** Gets whether profile reporting is enabled. */
    public boolean isProfileReportingEnabled() {
        return mIsProfileReportingEnabled;
    }

    public void setProfileReportingText(SpannableStringBuilder text) {
        mProfileReportDetails.setText(text);
        mProfileReportDetails.setMovementMethod(LinkMovementMethod.getInstance());
    }

    /** Sets whether legacy tech reporting is enabled. Then updates view accordingly. */
    public void setLegacyTechReportingEnabled(boolean isEnabled) {
        if (mIsLegacyTechReportingEnabled != isEnabled) {
            mIsLegacyTechReportingEnabled = isEnabled;
            adjustView();
        }
    }

    public boolean isThreatProtectionEnabled() {
        return mIsSecurityEventReportingEnabled || mIsUrlFilteringEnabled;
    }

    public boolean shouldShowSecurityEventInfo() {
        return mIsSecurityEventReportingEnabled && mThreatProtectionMore.isChecked();
    }

    public boolean shouldShowUrlFilteringInfo() {
        return mIsUrlFilteringEnabled && mThreatProtectionMore.isChecked();
    }

    public void setSecurityEventReportingEnabled(boolean isEnabled) {
        if (mIsSecurityEventReportingEnabled != isEnabled) {
            mIsSecurityEventReportingEnabled = isEnabled;
            adjustView();
        }
    }

    public void setUrlFilteringEnabled(boolean isEnabled) {
        if (mIsUrlFilteringEnabled != isEnabled) {
            mIsUrlFilteringEnabled = isEnabled;
            adjustView();
        }
    }

    /** Gets whether legacy tech reporting is enabled. */
    public boolean isLegacyTechReportingEnabled() {
        return mIsLegacyTechReportingEnabled;
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

    /** Adjusts Title, Description, and Learn More link based on management status. */
    private void adjustView() {
        mDescription.setVisibility(isManaged() ? VISIBLE : GONE);
        if (isManaged()) {
            mDescription.setText(
                    getContext()
                            .getString(
                                    mIsBrowserManaged
                                            ? R.string.management_browser_notice
                                            : R.string.management_profile_notice));
        }
        mLearnMore.setVisibility(isManaged() ? VISIBLE : GONE);
        mBrowserReporting.setVisibility(
                mIsBrowserReportingEnabled
                                || mIsLegacyTechReportingEnabled
                                || mIsProfileReportingEnabled
                        ? VISIBLE
                        : GONE);

        mBrowserReportingExplanation.setVisibility(mIsBrowserReportingEnabled ? VISIBLE : GONE);

        mReportUsername.setVisibility(mIsBrowserReportingEnabled ? VISIBLE : GONE);
        mReportVersion.setVisibility(mIsBrowserReportingEnabled ? VISIBLE : GONE);

        mProfileReportingExplanation.setVisibility(
                !mIsBrowserReportingEnabled && mIsProfileReportingEnabled ? VISIBLE : GONE);
        mProfileReportDetails.setVisibility(
                !mIsBrowserReportingEnabled && mIsProfileReportingEnabled ? VISIBLE : GONE);

        // If Legacy tech report is enabled without browser or profile status report, show browser
        // report explanations as default.
        if (mIsLegacyTechReportingEnabled
                && !mIsBrowserReportingEnabled
                && !mIsProfileReportingEnabled) {
            mBrowserReportingExplanation.setVisibility(VISIBLE);
        }
        mReportLegacyTech.setVisibility(mIsLegacyTechReportingEnabled ? VISIBLE : GONE);

        mThreatProtectionTitle.setVisibility(isThreatProtectionEnabled() ? VISIBLE : GONE);
        mThreatProtectionDescription.setVisibility(isThreatProtectionEnabled() ? VISIBLE : GONE);
        mThreatProtectionMore.setVisibility(isThreatProtectionEnabled() ? VISIBLE : GONE);

        mThreatProtectionSecurityEvent.setVisibility(
                shouldShowSecurityEventInfo() ? VISIBLE : GONE);
        mThreatProtectionSecurityEventDescription.setVisibility(
                shouldShowSecurityEventInfo() ? VISIBLE : GONE);

        mThreatProtectionPageVisited.setVisibility(shouldShowUrlFilteringInfo() ? VISIBLE : GONE);
        mThreatProtectionPageVisitedDescription.setVisibility(
                shouldShowUrlFilteringInfo() ? VISIBLE : GONE);
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
