// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.page_insights;

import android.content.Context;
import android.content.res.ColorStateList;
import android.text.method.LinkMovementMethod;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewTreeObserver;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.base.Callback;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;

import java.text.DateFormat;
import java.util.Date;

public class PageInsightsSheetContent implements BottomSheetContent, View.OnLayoutChangeListener {

    @VisibleForTesting
    static final String PAGE_INSIGHTS_FULL_HEIGHT_RATIO_PARAM = "page_insights_full_height_ratio";

    @VisibleForTesting
    static final String PAGE_INSIGHTS_PEEK_HEIGHT_RATIO_PARAM = "page_insights_peek_height_ratio";

    @VisibleForTesting
    static final String PAGE_INSIGHTS_PEEK_WITH_PRIVACY_HEIGHT_RATIO_PARAM =
            "page_insights_peek_with_privacy_height_ratio";

    interface OnBottomSheetTapHandler {
        /** Returns true if the tap has been handled. */
        boolean handle();
    }

    interface OnBackPressHandler {
        /** Returns true if the back press has been handled. */
        boolean handle();
    }

    /** Ratio of the height when in full mode. */
    static final float DEFAULT_FULL_HEIGHT_RATIO = 0.9f;

    /** Ratio of the height when in peek mode and privacy notice is not showing. */
    @VisibleForTesting static final float DEFAULT_PEEK_HEIGHT_RATIO = 0.201f;

    /** Ratio of the height when in peek mode and privacy notice is showing. */
    @VisibleForTesting static final float DEFAULT_PEEK_WITH_PRIVACY_HEIGHT_RATIO = 0.263f;

    private static final SharedPreferencesManager sSharedPreferencesManager =
            ChromeSharedPreferences.getInstance();
    private static final long INVALID_TIMESTAMP = -1;

    private final OnBackPressHandler mOnBackPressHandler;
    private final ObservableSupplierImpl<Boolean> mWillHandleBackPressSupplier;
    private final float mFullHeightRatio;
    private final float mPeekHeightRatio;
    private final float mPeekWithPrivacyHeightRatio;

    private Context mContext;
    private View mLayoutView;
    private ViewGroup mToolbarView;
    private ViewGroup mSheetContentView;
    private boolean mShouldPrivacyNoticeBeShown;
    private int mFullScreenHeight;
    private Callback<View> mOnPrivacyNoticeLinkClickCallback;
    private boolean mShouldHavePeekState;
    @Nullable private RecyclerView mCurrentRecyclerView;

    /**
     * Constructor.
     *
     * @param context An Android context.
     * @param layoutView the top-level view for the Window
     * @param onPrivacyNoticeLinkClickCallback callback for use on privacy notice
     * @param onBottomSheetTapHandler handler for taps on bottom sheet
     */
    public PageInsightsSheetContent(
            Context context,
            View layoutView,
            Callback<View> onPrivacyNoticeLinkClickCallback,
            OnBackPressHandler onBackPressHandler,
            ObservableSupplierImpl<Boolean> willHandleBackPressSupplier,
            OnBottomSheetTapHandler onBottomSheetTapHandler) {
        mFullHeightRatio =
                (float)
                        ChromeFeatureList.getFieldTrialParamByFeatureAsDouble(
                                ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB,
                                PAGE_INSIGHTS_FULL_HEIGHT_RATIO_PARAM,
                                DEFAULT_FULL_HEIGHT_RATIO);
        mPeekHeightRatio =
                (float)
                        ChromeFeatureList.getFieldTrialParamByFeatureAsDouble(
                                ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB,
                                PAGE_INSIGHTS_PEEK_HEIGHT_RATIO_PARAM,
                                DEFAULT_PEEK_HEIGHT_RATIO);
        mPeekWithPrivacyHeightRatio =
                (float)
                        ChromeFeatureList.getFieldTrialParamByFeatureAsDouble(
                                ChromeFeatureList.CCT_PAGE_INSIGHTS_HUB,
                                PAGE_INSIGHTS_PEEK_WITH_PRIVACY_HEIGHT_RATIO_PARAM,
                                DEFAULT_PEEK_WITH_PRIVACY_HEIGHT_RATIO);
        mLayoutView = layoutView;
        mToolbarView = (ViewGroup) LayoutInflater.from(context).inflate(
            R.layout.page_insights_sheet_toolbar, null);
        mToolbarView
                .findViewById(R.id.page_insights_back_button)
                .setOnClickListener((view) -> onBackPressHandler.handle());
        mSheetContentView = (ViewGroup) LayoutInflater.from(context).inflate(
                R.layout.page_insights_sheet_content, null);

        // TODO(b/306377148): Remove this once a solution is built into bottom sheet infra.
        TapInterceptingLinearLayout contentContainer =
                (TapInterceptingLinearLayout)
                        mSheetContentView.findViewById(R.id.page_insights_content_container);
        contentContainer.setOnTapHandler(onBottomSheetTapHandler);
        contentContainer.setOnClickListener((view) -> onBottomSheetTapHandler.handle());
        mToolbarView.setOnClickListener((view) -> onBottomSheetTapHandler.handle());

        mContext = context;
        mOnPrivacyNoticeLinkClickCallback = onPrivacyNoticeLinkClickCallback;
        mOnBackPressHandler = onBackPressHandler;
        mWillHandleBackPressSupplier = willHandleBackPressSupplier;
        mFullScreenHeight = context.getResources().getDisplayMetrics().heightPixels;
        updateContentDimensions();
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
        if (mCurrentRecyclerView == null) {
            // If we don't have the RecyclerView, hardcode 1 to ensure that scrolling up is never
            // interpreted as trying to drag the bottom sheet down.
            return 1;
        }
        return mCurrentRecyclerView.computeVerticalScrollOffset();
    }

    @Override
    public void destroy() {
        mLayoutView.removeOnLayoutChangeListener(this);
        mCurrentRecyclerView = null;
        mShouldHavePeekState = false;
    }

    @Override
    public int getPeekHeight() {
        if (!mShouldHavePeekState) {
            return HeightMode.DISABLED;
        } else if (mShouldPrivacyNoticeBeShown) {
            // TODO(b/282739536): Find the right peeking height value from the feed view dimension.
            return (int) (mPeekWithPrivacyHeightRatio * mFullScreenHeight);
        } else {
            // TODO(b/282739536): Find the right peeking height value from the feed view dimension.
            return (int) (mPeekHeightRatio * mFullScreenHeight);
        }
    }

    @Override
    public float getHalfHeightRatio() {
        return HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
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

    @Override
    public void onLayoutChange(
            View view,
            int left,
            int top,
            int right,
            int bottom,
            int oldLeft,
            int oldTop,
            int oldRight,
            int oldBottom) {
        int fullScreenHeight = mContext.getResources().getDisplayMetrics().heightPixels;
        if (fullScreenHeight == mFullScreenHeight) {
            return;
        }
        mFullScreenHeight = fullScreenHeight;
        updateContentDimensions();
    }

    @Override
    public boolean handleBackPress() {
        return mOnBackPressHandler.handle();
    }

    @Override
    public ObservableSupplierImpl<Boolean> getBackPressStateChangedSupplier() {
        return mWillHandleBackPressSupplier;
    }

    @Override
    public void onBackPressed() {
        mOnBackPressHandler.handle();
    }

    /**
     * Returns the actual height of the fully expanded bottom sheet, as a ratio of the screen
     * height.
     */
    public float getActualFullHeightRatio() {
        return mFullHeightRatio;
    }

    void showLoadingIndicator() {
        setVisibilityById(mSheetContentView, R.id.page_insights_loading_indicator, View.VISIBLE);
        setVisibilityById(mToolbarView, R.id.page_insights_feed_header, View.VISIBLE);
        setVisibilityById(mSheetContentView, R.id.page_insights_feed_content, View.GONE);
        setVisibilityById(mToolbarView, R.id.page_insights_back_button, View.GONE);
        setVisibilityById(mToolbarView, R.id.page_insights_child_title, View.GONE);
        setVisibilityById(mSheetContentView, R.id.page_insights_child_content, View.GONE);
        setVisibilityById(mSheetContentView, R.id.page_insights_privacy_notice, View.GONE);
    }

    void showFeedPage() {
        setVisibilityById(mSheetContentView, R.id.page_insights_loading_indicator, View.GONE);
        setVisibilityById(mToolbarView, R.id.page_insights_feed_header, View.VISIBLE);
        setVisibilityById(mToolbarView, R.id.page_insights_back_button, View.GONE);
        setVisibilityById(mToolbarView, R.id.page_insights_child_title, View.GONE);
        setVisibilityById(mSheetContentView, R.id.page_insights_feed_content, View.VISIBLE);
        setVisibilityById(mSheetContentView, R.id.page_insights_child_content, View.GONE);
        if (mShouldPrivacyNoticeBeShown) {
            setVisibilityById(mSheetContentView, R.id.page_insights_privacy_notice, View.VISIBLE);
        } else {
            setVisibilityById(mSheetContentView, R.id.page_insights_privacy_notice, View.GONE);
        }
        updateCurrentRecyclerView(mSheetContentView.findViewById(R.id.page_insights_feed_content));
    }

    void initContent(
            View feedPageView, boolean isPrivacyNoticeRequired, boolean shouldHavePeekState) {
        mShouldHavePeekState = shouldHavePeekState;
        initPrivacyNotice(isPrivacyNoticeRequired);
        if (mShouldPrivacyNoticeBeShown) {
            ViewGroup privacyNoticeView =
                    mSheetContentView.findViewById(R.id.page_insights_privacy_notice);
            final ViewTreeObserver observer = privacyNoticeView.getViewTreeObserver();
            observer.addOnPreDrawListener(
                    new ViewTreeObserver.OnPreDrawListener() {
                        @Override
                        public boolean onPreDraw() {
                            privacyNoticeView.getViewTreeObserver().removeOnPreDrawListener(this);
                            PageInsightsSheetContent.this.updateContentDimensions();
                            return true;
                        }
                    });
        } else {
            updateContentDimensions();
        }
        mLayoutView.addOnLayoutChangeListener(this);

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
        setVisibilityById(mToolbarView, R.id.page_insights_back_button, View.VISIBLE);
        setVisibilityById(mToolbarView, R.id.page_insights_child_title, View.VISIBLE);
        setVisibilityById(mSheetContentView, R.id.page_insights_feed_content, View.GONE);
        setVisibilityById(mSheetContentView, R.id.page_insights_child_content, View.VISIBLE);
        updateCurrentRecyclerView(childPageView);
    }

    void setPrivacyCardColor(int color) {
        if (!mShouldPrivacyNoticeBeShown) {
            return;
        }
        View privacyCard = mSheetContentView.findViewById(R.id.page_insights_privacy_notice_card);
        if (privacyCard != null) {
            privacyCard.setBackgroundTintList(ColorStateList.valueOf(color));
        }
    }

    private void updateContentDimensions() {
        // Glide (used in our XSurface views) throws an error when one of its images is assigned a
        // width or height of zero; this happens briefly if we use a parent LinearLayout or
        // ConstraintLayout to dynamically size our content. So instead we have to do the dynamic
        // sizing ourselves, here in Java. :(
        // TODO(b/306894418): See if this issue can be fixed or avoided.
        float contentHeight =
                (mFullScreenHeight * mFullHeightRatio)
                        - mContext.getResources()
                                .getDimensionPixelSize(R.dimen.page_insights_toolbar_height);
        int contentWidth =
                Math.min(
                        mLayoutView.getWidth(),
                        mContext.getResources()
                                .getDimensionPixelSize(R.dimen.page_insights_max_width));
        mSheetContentView
                .findViewById(R.id.page_insights_content_container)
                .setLayoutParams(
                        new FrameLayout.LayoutParams(
                                contentWidth, (int) contentHeight, Gravity.CENTER_HORIZONTAL));

        int heightTakenByPrivacyNotice =
                mShouldPrivacyNoticeBeShown
                        ? mSheetContentView
                                .findViewById(R.id.page_insights_privacy_notice)
                                .getHeight()
                        : 0;
        LinearLayout.LayoutParams remainingContentLayoutParams =
                new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT,
                        (int) contentHeight - heightTakenByPrivacyNotice);
        mSheetContentView
                .findViewById(R.id.page_insights_feed_content)
                .setLayoutParams(remainingContentLayoutParams);
        mSheetContentView
                .findViewById(R.id.page_insights_child_content)
                .setLayoutParams(remainingContentLayoutParams);
    }

    private void setVisibilityById(ViewGroup mViewGroup, int id, int visibility) {
        mViewGroup.findViewById(id).setVisibility(visibility);
    }

    private void initPrivacyNotice(boolean isPrivacyNoticeRequired) {
        /*
         * The function checks if the privacy notice should be shown and inflates the privacy notice
         * UI if it has to be shown. The privacy notice should appear in Page Insights Hub (PIH) the
         * first time PIH is opened each day, until either the user dismisses it (by tapping X), or
         * the privacy notice has been shown 3 times.
         */
        mShouldPrivacyNoticeBeShown = false;
        if (!isPrivacyNoticeRequired) {
            return;
        }
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
        mShouldPrivacyNoticeBeShown = false;
        updateContentDimensions();
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

    private void updateCurrentRecyclerView(View currentPageView) {
        mCurrentRecyclerView = findRecyclerView(currentPageView);
        final ViewTreeObserver observer = currentPageView.getViewTreeObserver();
        observer.addOnPreDrawListener(
                new ViewTreeObserver.OnPreDrawListener() {
                    @Override
                    public boolean onPreDraw() {
                        currentPageView.getViewTreeObserver().removeOnPreDrawListener(this);
                        mCurrentRecyclerView = findRecyclerView(currentPageView);
                        return true;
                    }
                });
    }

    @Nullable
    private static RecyclerView findRecyclerView(View view) {
        // The  content view is implemented internally, and we do not currently have an API exposed
        // to us to obtain the scroll position inside it. So instead we search the view for the
        // outermost RecyclerView within it (only going at most 10 levels deep, and only looking at
        // first children).
        // TODO(b/305194266): Expose scroll position API, and remove this rather horrible hack.
        for (int i = 0; i < 10; i++) {
            if (view instanceof RecyclerView) {
                return (RecyclerView) view;
            } else if (view instanceof ViewGroup) {
                view = ((ViewGroup) view).getChildAt(0);
            } else {
                return null;
            }
        }
        return null;
    }
}
