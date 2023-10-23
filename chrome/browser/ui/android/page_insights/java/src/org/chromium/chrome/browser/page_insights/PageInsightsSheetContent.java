// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_insights;

import android.content.Context;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

import java.text.DateFormat;
import java.util.Date;

public class PageInsightsSheetContent implements BottomSheetContent {
    /** Ratio of the height when in full mode. */
    private static final float FULL_HEIGHT_RATIO = 0.9f;

    @VisibleForTesting static final float PEEK_HEIGHT_RATIO_WITHOUT_PRIVACY_NOTICE = 0.201f;

    @VisibleForTesting static final float PEEK_HEIGHT_RATIO_WITH_PRIVACY_NOTICE = 0.263f;

    private ViewGroup mToolbarView;
    private ViewGroup mSheetContentView;
    private boolean mShouldPrivacyNoticeBeShown;
    private int mFullScreenHeight;
    private Context mContext;
    private Callback<View> mOnPrivacyNoticeLinkClickCallback;
    private static final SharedPreferencesManager sSharedPreferencesManager =
            ChromeSharedPreferences.getInstance();
    private static final long INVALID_TIMESTAMP = -1;

    /**
     * Constructor.
     *
     * @param context An Android context.
     */
    public PageInsightsSheetContent(
            Context context, Callback<View> onPrivacyNoticeLinkClickCallback) {
        // TODO(kamalchoudhury): Inflate with loading indicator instead
        mToolbarView = (ViewGroup) LayoutInflater.from(context).inflate(
            R.layout.page_insights_sheet_toolbar, null);
        mToolbarView
            .findViewById(R.id.page_insights_back_button)
            .setOnClickListener((view)-> onBackButtonPressed());
        mSheetContentView = (ViewGroup) LayoutInflater.from(context).inflate(
                R.layout.page_insights_sheet_content, null);
        mContext = context;
        mOnPrivacyNoticeLinkClickCallback = onPrivacyNoticeLinkClickCallback;
        mFullScreenHeight = context.getResources().getDisplayMetrics().heightPixels;
    }

    @Override
    public boolean hasCustomLifecycle() {
        // Lifecycle is controlled by triggering logic.
        return true;
    }

    @Override
    public boolean hasCustomScrimLifecycle() {
        // We use the standard scrim that open when going beyond the peeking state.
        return false;
    }

    @Override
    public boolean hideOnScroll() {
        // PIH scrolls away in sync with tab scroll.
        return true;
    }

    @Override
    public View getContentView() {
        return mSheetContentView;
    }

    @Nullable
    @Override
    public View getToolbarView() {
        return mToolbarView;
    }

    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    @Override
    public void destroy() {}

    @Override
    public int getPeekHeight() {
        // TODO(b/282739536): Find the right peeking height value from the feed view dimension.
        if (mShouldPrivacyNoticeBeShown) {
            return (int) (PEEK_HEIGHT_RATIO_WITH_PRIVACY_NOTICE * mFullScreenHeight);
        }
        return (int) (PEEK_HEIGHT_RATIO_WITHOUT_PRIVACY_NOTICE * mFullScreenHeight);
    }

    @Override
    public float getHalfHeightRatio() {
        return HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        return FULL_HEIGHT_RATIO;
    }

    @Override
    public int getPriority() {
        return ContentPriority.LOW;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        // Swiping down hard/tapping on scrim closes the sheet.
        return true;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.page_insights_sheet_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        // Half-height is disabled so no need for an accessibility string.
        assert false : "This method should not be called";
        return 0;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.page_insights_sheet_opened_full;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.page_insights_sheet_closed;
    }

    private void onBackButtonPressed(){
        showFeedPage();
    }

    void showLoadingIndicator() {
        setVisibilityById(mSheetContentView, R.id.page_insights_loading_indicator, View.VISIBLE);
        setVisibilityById(mToolbarView, R.id.page_insights_feed_header, View.VISIBLE);
        setVisibilityById(mSheetContentView, R.id.page_insights_feed_content, View.GONE);
        setVisibilityById(mToolbarView, R.id.page_insights_child_page_header, View.GONE);
        setVisibilityById(mSheetContentView, R.id.page_insights_child_content, View.GONE);
    }

    void showFeedPage() {
        setVisibilityById(mSheetContentView, R.id.page_insights_loading_indicator, View.GONE);
        setVisibilityById(mToolbarView, R.id.page_insights_feed_header, View.VISIBLE);
        setVisibilityById(mToolbarView, R.id.page_insights_child_page_header, View.GONE);
        setVisibilityById(mSheetContentView, R.id.page_insights_feed_content, View.VISIBLE);
        setVisibilityById(mSheetContentView, R.id.page_insights_child_content, View.GONE);
        if (mShouldPrivacyNoticeBeShown) {
            setVisibilityById(mSheetContentView, R.id.page_insights_privacy_notice, View.VISIBLE);
        } else {
            setVisibilityById(mSheetContentView, R.id.page_insights_privacy_notice, View.GONE);
        }
    }

    void initContent(View feedPageView) {
        initPrivacyNotice();
        ViewGroup feedContentView = mSheetContentView.findViewById(R.id.page_insights_feed_content);
        feedContentView.removeAllViews();
        feedContentView.addView(feedPageView);
    }

    @VisibleForTesting
    void showChildPage(View childPageView, String childPageTitle) {
        TextView childTitleView = mToolbarView.findViewById(R.id.page_insights_child_title);
        childTitleView.setText(childPageTitle);
        ViewGroup childContentView =
                mSheetContentView.findViewById(R.id.page_insights_child_content);
        childContentView.removeAllViews();
        childContentView.addView(childPageView);
        setVisibilityById(mToolbarView, R.id.page_insights_feed_header, View.GONE);
        setVisibilityById(mToolbarView, R.id.page_insights_child_page_header, View.VISIBLE);
        setVisibilityById(mSheetContentView, R.id.page_insights_feed_content, View.GONE);
        setVisibilityById(mSheetContentView, R.id.page_insights_child_content, View.VISIBLE);
    }

    private void setVisibilityById(ViewGroup mViewGroup, int id, int visibility) {
        mViewGroup.findViewById(id).setVisibility(visibility);
    }

    private void initPrivacyNotice() {
        /*
         * The function checks if the privacy notice should be shown and inflates the privacy notice
         * UI if it has to be shown. The privacy notice should appear in Page Insights Hub (PIH) the
         * first time PIH is opened each day, until either the user dismisses it (by tapping X), or
         * the privacy notice has been shown 3 times.
         */
        mShouldPrivacyNoticeBeShown = false;
        if (sSharedPreferencesManager.readBoolean(
                ChromePreferenceKeys.PIH_PRIVACY_NOTICE_CLOSED, false)) {
            return;
        }
        int numberOfTimesPrivacyNoticeShown =
                sSharedPreferencesManager.readInt(
                        ChromePreferenceKeys.PIH_PRIVACY_NOTICE_SHOWN_TOTAL_COUNT, 0);
        if (numberOfTimesPrivacyNoticeShown >= 3) {
            return;
        }
        long currentTimestamp = System.currentTimeMillis();
        if (!hasPrivacyNoticeBeenShownToday(currentTimestamp)) {
            updatePrivacyNoticePreferences(currentTimestamp, numberOfTimesPrivacyNoticeShown);
            preparePrivacyNoticeView();
            mShouldPrivacyNoticeBeShown = true;
        }
    }

    private static void updatePrivacyNoticePreferences(
            long currentTimestamp, int numberOfTimesPrivacyNoticeShown) {
        sSharedPreferencesManager.writeLong(
                ChromePreferenceKeys.PIH_PRIVACY_NOTICE_LAST_SHOWN_TIMESTAMP, currentTimestamp);
        sSharedPreferencesManager.writeInt(
                ChromePreferenceKeys.PIH_PRIVACY_NOTICE_SHOWN_TOTAL_COUNT,
                numberOfTimesPrivacyNoticeShown + 1);
    }

    private static boolean hasPrivacyNoticeBeenShownToday(long currentTimestamp) {
        DateFormat dateFormat = DateFormat.getDateInstance();
        String currentDate = dateFormat.format(new Date(currentTimestamp));
        long lastPihTimestamp =
                sSharedPreferencesManager.readLong(
                        ChromePreferenceKeys.PIH_PRIVACY_NOTICE_LAST_SHOWN_TIMESTAMP, -1);
        return (lastPihTimestamp != INVALID_TIMESTAMP)
                && (currentDate.compareTo(dateFormat.format(new Date(lastPihTimestamp))) == 0);
    }

    private void onPrivacyNoticeClosed() {
        sSharedPreferencesManager.writeBoolean(
                ChromePreferenceKeys.PIH_PRIVACY_NOTICE_CLOSED, true);
        setVisibilityById(mSheetContentView, R.id.page_insights_privacy_notice, View.GONE);
    }

    private void preparePrivacyNoticeView() {
        mSheetContentView.findViewById(R.id.page_insights_privacy_notice_close_button)
                .setOnClickListener((view) -> onPrivacyNoticeClosed());
        TextView privacyNoticeMessage =
                mSheetContentView.findViewById(R.id.page_insights_privacy_notice_message);
        privacyNoticeMessage.setMovementMethod(LinkMovementMethod.getInstance());
        privacyNoticeMessage.setText(
                SpanApplier.applySpans(
                        mContext.getString(R.string.page_insights_hub_privacy_notice),
                        new SpanApplier.SpanInfo(
                                "<link>",
                                "</link>",
                                new NoUnderlineClickableSpan(
                                        mContext,
                                        R.color.default_bg_color_blue,
                                        mOnPrivacyNoticeLinkClickCallback))));
    }
}
