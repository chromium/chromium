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
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.ViewResizer;

/**
 * The View that renders the ManagementPage (chrome://management). Consists of an medium size image
 * icon over title and descriptive text.
 */
public class ManagementView extends ScrollView {
    private boolean mIsBrowserManaged;
    private boolean mIsProfileManaged;
    private boolean mIsBrowserReportingEnabled;
    private boolean mIsProfileReportingEnabled;
    private boolean mIsLegacyTechReportingEnabled;

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

    @Nullable private UiConfig mUiConfig;

    /** Constructor for inflating from XML. */
    public ManagementView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();

        mManagementContainer = (LinearLayout) findViewById(R.id.management_container);
        mTitle = (TextView) findViewById(R.id.title_text);
        mDescription = (TextView) findViewById(R.id.description_text);
        mLearnMore = (TextView) findViewById(R.id.learn_more);
        mBrowserReporting = (TextView) findViewById(R.id.browser_reporting);
        mBrowserReportingExplanation = (TextView) findViewById(R.id.browser_reporting_explanation);
        mReportUsername = (TextView) findViewById(R.id.report_username);
        mReportVersion = (TextView) findViewById(R.id.report_version);
        mProfileReportingExplanation = (TextView) findViewById(R.id.profile_reporting_explanation);
        mProfileReportDetails = (TextView) findViewById(R.id.profile_report_details);
        mReportLegacyTech = (TextView) findViewById(R.id.report_legacy_tech);

        // Set default management status
        mIsBrowserManaged = false;
        mIsProfileManaged = false;
        mIsBrowserReportingEnabled = false;
        mIsProfileReportingEnabled = false;
        mIsLegacyTechReportingEnabled = false;

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

    /** Adjusts Title, Description, and Learn More link based on management status. */
    private void adjustView() {
        mDescription.setVisibility(isManaged() ? VISIBLE : GONE);
        if (isManaged()) {
            mDescription.setText(
                    getContext()
                            .getResources()
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
