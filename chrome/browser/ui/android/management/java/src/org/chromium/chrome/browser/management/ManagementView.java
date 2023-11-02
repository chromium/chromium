// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.management;

import android.content.Context;
import android.content.res.Configuration;
import android.text.SpannableString;
import android.text.TextUtils;
import android.text.method.LinkMovementMethod;
import android.util.AttributeSet;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.widget.displaystyle.UiConfig;
import org.chromium.components.browser_ui.widget.displaystyle.ViewResizer;

/**
 * The View that renders the ManagementPage (chrome://management).
 * Consists of an medium size image icon over title and descriptive text.
 */
public class ManagementView extends ScrollView {
    private boolean mIsManaged;
    private @Nullable String mManagerName;

    private LinearLayout mManagementContainer;
    private TextView mTitle;
    private TextView mDescription;
    private TextView mLearnMore;
    private TextView mBrowserReporting;
    private TextView mBrowserReportingExplanation;
    private TextView mExtensionReportUsername;
    private TextView mExtensionReportVersion;

    @Nullable
    private UiConfig mUiConfig;

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
        mExtensionReportUsername = (TextView) findViewById(R.id.extension_report_username);
        mExtensionReportVersion = (TextView) findViewById(R.id.extension_report_version);

        // Set default management status
        mIsManaged = false;
        mManagerName = null;
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

    /** Sets whether account is managed. Then updates view accordingly. */
    public void setManaged(boolean isManaged) {
        if (mIsManaged != isManaged) {
            mIsManaged = isManaged;
            adjustView();
        }
    }

    /** Gets whether account is managed. */
    public boolean isManaged() {
        return mIsManaged;
    }

    /** Sets account manager name. Then updates view accordingly.  */
    public void setManagerName(@Nullable String managerName) {
        if (!TextUtils.equals(mManagerName, managerName)) {
            mManagerName = managerName;
            adjustView();
        }
    }

    /** Gets account manager name. */
    public @Nullable String getManagerName() {
        return mManagerName;
    }

    public void setLearnMoreText(SpannableString learnMoreText) {
        mLearnMore.setText(learnMoreText);
        mLearnMore.setMovementMethod(LinkMovementMethod.getInstance());
    }

    /**
     * Adjusts Title, Description, and Learn More link based on management status.
     */
    private void adjustView() {
        if (mIsManaged) {
            if (TextUtils.isEmpty(mManagerName)) {
                mTitle.setText(getResources().getString(R.string.management_subtitle));
            } else {
                mTitle.setText(getResources().getString(
                        R.string.management_subtitle_managed_by, mManagerName));
            }
        } else {
            mTitle.setText(getResources().getString(R.string.management_not_managed_subtitle));
        }

        mDescription.setVisibility(mIsManaged ? VISIBLE : INVISIBLE);
        mLearnMore.setVisibility(mIsManaged ? VISIBLE : INVISIBLE);
        mBrowserReporting.setVisibility(mIsManaged ? VISIBLE : INVISIBLE);
        mBrowserReportingExplanation.setVisibility(mIsManaged ? VISIBLE : INVISIBLE);
        mExtensionReportUsername.setVisibility(mIsManaged ? VISIBLE : INVISIBLE);
        mExtensionReportVersion.setVisibility(mIsManaged ? VISIBLE : INVISIBLE);
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
            final int minWidePadding = getResources().getDimensionPixelSize(R.dimen.cm_padding_wide);

            mUiConfig = new UiConfig(mManagementContainer);
            ViewResizer.createAndAttach(mManagementContainer, mUiConfig, minPadding, minWidePadding);
        } else {
            mUiConfig.updateDisplayStyle();
        }
    }
}
