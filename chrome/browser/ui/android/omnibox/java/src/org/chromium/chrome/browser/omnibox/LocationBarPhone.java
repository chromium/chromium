// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.View;
import android.view.ViewStub;
import android.widget.FrameLayout;
import android.widget.ImageButton;

import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.core.widget.ImageViewCompat;

import org.chromium.base.TraceEvent;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxState;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.toolbar.ToolbarVariationUtils;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;

/** A location bar implementation specific for smaller/phone screens. */
@NullMarked
class LocationBarPhone extends LocationBarLayout {
    protected ImageButton mBackButton;
    protected @Nullable FrameLayout mOptionalButton;
    private final int mCenteringSpacePx;
    private final int mUrlCenteringSafetyMarginPx;
    private final int mMinUrlWidthPx;
    private final int mNonCenteredUrlBarEndPaddingPx;
    private LocationBarDataProvider.@Nullable Observer mUrlObserver;

    private @FuseboxState int mFuseboxState = FuseboxState.DISABLED;

    /** Constructor used to inflate from XML. */
    public LocationBarPhone(Context context, AttributeSet attrs) {
        super(context, attrs);

        mBackButton = findViewById(R.id.omnibox_back_button);
        ViewStub optionalButtonStub = findViewById(R.id.optional_button_location_bar_stub);

        if (optionalButtonStub != null
                && ToolbarVariationUtils.isToolbarUiRefactorEnabled(context)) {
            mOptionalButton = (FrameLayout) optionalButtonStub.inflate();
            mOptionalButton.setVisibility(View.GONE);
            ConstraintLayout.LayoutParams params =
                    (ConstraintLayout.LayoutParams) mOptionalButton.getLayoutParams();
            params.endToEnd = ConstraintLayout.LayoutParams.PARENT_ID;
            mOptionalButton.setLayoutParams(params);
        } else {
            mOptionalButton = null;
        }

        Resources res = getResources();
        mCenteringSpacePx =
                res.getDimensionPixelSize(R.dimen.location_bar_url_centering_edge_space);
        mUrlCenteringSafetyMarginPx =
                res.getDimensionPixelSize(R.dimen.location_bar_url_centering_safety_margin);
        mMinUrlWidthPx = res.getDimensionPixelSize(R.dimen.location_bar_min_url_width);
        mNonCenteredUrlBarEndPaddingPx = res.getDimensionPixelSize(R.dimen.url_bar_end_padding);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        if (isToolbarUiRefactorEnabled()) {
            setClickable(true);
            setLongClickable(true);
        }
    }

    @Override
    public void initialize(
            AutocompleteCoordinator autocompleteCoordinator,
            UrlBarCoordinator urlCoordinator,
            StatusCoordinator statusCoordinator,
            LocationBarDataProvider locationBarDataProvider,
            WindowAndroid windowAndroid) {
        super.initialize(
                autocompleteCoordinator,
                urlCoordinator,
                statusCoordinator,
                locationBarDataProvider,
                windowAndroid);

        if (isToolbarUiRefactorEnabled()) {
            mUrlObserver =
                    new LocationBarDataProvider.Observer() {
                        @Override
                        public void onUrlChanged(boolean isTabChanging) {
                            ViewUtils.requestLayout(
                                    LocationBarPhone.this,
                                    "LocationBarPhone.LocationBarDataProvider.onUrlChanged");
                        }
                    };
            mLocationBarDataProvider.addObserver(mUrlObserver);
        }
    }

    @Override
    protected boolean isBackButtonVisible() {
        return mBackButton.getVisibility() == View.VISIBLE;
    }

    @Override
    /* package */ void setBackButtonVisibility(boolean shouldShow) {
        mBackButton.setVisibility(shouldShow ? VISIBLE : GONE);
        updateStartPadding();
    }

    @Override
    /* package */ void setBackButtonEnabled(boolean enabled) {
        mBackButton.setEnabled(enabled);
    }

    @Override
    /* package */ void setBackButtonTint(ColorStateList colorStateList) {
        ImageViewCompat.setImageTintList(mBackButton, colorStateList);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        try (TraceEvent e = TraceEvent.scoped("LocationBarPhone.onMeasure")) {
            boolean wasCentering = mIsCenteringApplied;
            boolean shouldCenter = shouldCenterUrlAndStatus();
            if (shouldCenter != mIsCenteringApplied) {
                mIsCenteringApplied = shouldCenter;
                setCenteringUrlAndStatusConstraints(mIsCenteringApplied);
            }

            if (isToolbarUiRefactorEnabled() || wasCentering) {
                updateUrlBarWidth(widthMeasureSpec, heightMeasureSpec);
            }

            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        }
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        try (TraceEvent e = TraceEvent.scoped("LocationBarPhone.onLayout")) {
            super.onLayout(changed, left, top, right, bottom);
        }
    }

    @Override
    public boolean performClick() {
        if (mIsCenteringApplied) {
            // performClick() only triggers click listeners and doesn't request focus
            // when called programmatically (bypassing onTouchEvent). We need to
            // explicitly request focus to bring up the keyboard.
            mUrlBar.requestFocus();
            return mUrlBar.performClick();
        }
        return super.performClick();
    }

    @Override
    public boolean performLongClick() {
        if (mIsCenteringApplied) {
            return mUrlBar.performLongClick();
        }
        return super.performLongClick();
    }

    @Override
    void onFuseboxStateChanged(@FuseboxState int state) {
        mFuseboxState = state;
    }

    @Override
    protected void updateCenteringUrlAndStatusState() {
        boolean shouldCenter = shouldCenterUrlAndStatus();
        if (shouldCenter != mIsCenteringApplied) {
            mIsCenteringApplied = shouldCenter;
            setCenteringUrlAndStatusConstraints(mIsCenteringApplied);
        }
    }

    /** Applies constraints to center or fill space for StatusView and UrlBar. */
    private void setCenteringUrlAndStatusConstraints(boolean shouldCenter) {
        ConstraintLayout.LayoutParams statusParams =
                (ConstraintLayout.LayoutParams) mLocationBarStatusView.getLayoutParams();
        ConstraintLayout.LayoutParams urlBarParams =
                (ConstraintLayout.LayoutParams) mUrlBar.getLayoutParams();

        ConstraintLayout.LayoutParams backButtonParams =
                (ConstraintLayout.LayoutParams) mBackButton.getLayoutParams();

        if (shouldCenter) {
            // Constrain the chain to the parent edges to center it globally.
            // We use CHAIN_PACKED to clump StatusView and UrlBar together.
            statusParams.startToStart = ConstraintLayout.LayoutParams.PARENT_ID;
            statusParams.startToEnd = ConstraintLayout.LayoutParams.UNSET;
            statusParams.horizontalChainStyle = ConstraintLayout.LayoutParams.CHAIN_PACKED;

            urlBarParams.endToEnd = ConstraintLayout.LayoutParams.PARENT_ID;
            urlBarParams.endToStart = ConstraintLayout.LayoutParams.UNSET;
            urlBarParams.horizontalWeight = 0;

            // Break the constraint from back button to status view to prevent the back button
            // from moving when the centered clump shifts due to width changes.
            backButtonParams.endToStart = ConstraintLayout.LayoutParams.UNSET;

            // Remove end padding to let text extend to the edge of the view.
            mUrlBar.setPaddingRelative(
                    mUrlBar.getPaddingStart(),
                    mUrlBar.getPaddingTop(),
                    0,
                    mUrlBar.getPaddingBottom());
        } else {
            // Restore original constraints to fill space between barriers.
            statusParams.startToStart = ConstraintLayout.LayoutParams.UNSET;
            statusParams.startToEnd = R.id.back_button_barrier;
            statusParams.horizontalChainStyle = ConstraintLayout.LayoutParams.CHAIN_SPREAD_INSIDE;

            urlBarParams.endToEnd = ConstraintLayout.LayoutParams.UNSET;
            urlBarParams.endToStart =
                    mFuseboxState == FuseboxState.EXPANDED
                            ? R.id.delete_button
                            : R.id.action_buttons_segment;
            urlBarParams.width = ConstraintLayout.LayoutParams.MATCH_CONSTRAINT;
            urlBarParams.horizontalWeight = 1;

            // Restore back button link to status view.
            backButtonParams.endToStart = R.id.location_bar_status;

            // Restore original end padding.
            mUrlBar.setPaddingRelative(
                    mUrlBar.getPaddingStart(),
                    mUrlBar.getPaddingTop(),
                    mNonCenteredUrlBarEndPaddingPx,
                    mUrlBar.getPaddingBottom());
        }

        mLocationBarStatusView.setLayoutParams(statusParams);
        mUrlBar.setLayoutParams(urlBarParams);
        mBackButton.setLayoutParams(backButtonParams);
    }

    private void updateUrlBarWidth(int widthMeasureSpec, int heightMeasureSpec) {
        int totalWidth = MeasureSpec.getSize(widthMeasureSpec);
        ConstraintLayout.LayoutParams urlBarLayoutParams =
                (ConstraintLayout.LayoutParams) mUrlBar.getLayoutParams();
        if (mIsCenteringApplied) {
            int maxComponentWidth = totalWidth - 2 * mCenteringSpacePx;
            int maxUrlWidth =
                    getMaxUrlWidth(mLocationBarStatusView, maxComponentWidth, heightMeasureSpec);
            maxUrlWidth = Math.max(mMinUrlWidthPx, maxUrlWidth);

            CharSequence text = mUrlBar.getText();
            if (TextUtils.isEmpty(text)) {
                text = mUrlBar.getHint();
            }
            // TODO(crbug.com/522366005): Clean up layout loop risk in custom spans and restore
            // native CharSequence measurement.
            // Ensure we measure plain text to strip any spans (like BoundsEllipsisSpan)
            // that could trigger layout requests during measurement, causing layout loops.
            String plainText = text != null ? text.toString() : "";

            int desiredUrlWidth =
                    getUrlTextWidth(plainText)
                            + mUrlBar.getPaddingStart()
                            + mUrlBar.getPaddingEnd()
                            + mUrlCenteringSafetyMarginPx;

            int finalUrlWidth;
            boolean fitsInCenteringSpace = desiredUrlWidth <= maxUrlWidth;
            if (fitsInCenteringSpace) {
                finalUrlWidth = desiredUrlWidth;
            } else {
                finalUrlWidth = Math.min(maxUrlWidth, Math.max(mMinUrlWidthPx, desiredUrlWidth));
            }

            updateUrlBarCenteringProperties(
                    /* centeringApplied= */ true,
                    fitsInCenteringSpace,
                    finalUrlWidth,
                    urlBarLayoutParams);
        } else {
            updateUrlBarCenteringProperties(
                    /* centeringApplied= */ false,
                    /* fitsInCenteringSpace= */ false,
                    /* finalUrlWidth= */ 0,
                    urlBarLayoutParams);
        }
    }

    /**
     * Updates the UrlBar's gravity, alignment, scrollability, and layout parameters based on
     * whether the URL fits in the centering space.
     *
     * <p>If the URL fits, horizontal scrolling is disabled to prevent visual misalignment, and its
     * scroll is reset to (0, 0).
     *
     * <p>If the URL does not fit, it is start-aligned, horizontal scrolling is enabled to allow
     * navigating the full text, and its layout width is capped.
     *
     * @param fitsInCenteringSpace True if the URL fits within the maximum allowed centering width.
     * @param finalUrlWidth The target width to set on the UrlBar's layout parameters.
     * @param urlBarLayoutParams The ConstraintLayout LayoutParams for the UrlBar.
     */
    private void updateUrlBarCenteringProperties(
            boolean centeringApplied,
            boolean fitsInCenteringSpace,
            int finalUrlWidth,
            ConstraintLayout.LayoutParams urlBarLayoutParams) {
        updateUrlBarAlignmentAndGravity(centeringApplied, fitsInCenteringSpace);

        // Horizontal scrolling must be disabled when centering to ensure layout-gravity centering
        // behaves correctly and doesn't get offset by residual scroll ranges. It is restored to
        // true when not centering to allow horizontal scrolling/editing of long text.
        mUrlBar.setHorizontallyScrolling(!centeringApplied || !fitsInCenteringSpace);

        // Reset scroll position to 0 when centering. Otherwise, if the previous URL was long
        // and scrolled, the short centered URL might remain scrolled out of view.
        if (centeringApplied && fitsInCenteringSpace && mUrlBar.getScrollX() != 0) {
            mUrlBar.scrollTo(0, 0);
        }

        if (centeringApplied) {
            if (urlBarLayoutParams.width != finalUrlWidth
                    || urlBarLayoutParams.matchConstraintMinWidth != 0) {
                urlBarLayoutParams.width = finalUrlWidth;
                urlBarLayoutParams.matchConstraintMinWidth = 0;
                mUrlBar.setLayoutParams(urlBarLayoutParams);
            }
        } else {
            if (mUrlBar.getMaxWidth() != Integer.MAX_VALUE
                    || urlBarLayoutParams.width != ConstraintLayout.LayoutParams.MATCH_CONSTRAINT
                    || urlBarLayoutParams.matchConstraintMinWidth != mMinUrlWidthPx) {
                mUrlBar.setMaxWidth(Integer.MAX_VALUE);
                urlBarLayoutParams.width = ConstraintLayout.LayoutParams.MATCH_CONSTRAINT;
                urlBarLayoutParams.matchConstraintMinWidth = mMinUrlWidthPx;
                mUrlBar.setLayoutParams(urlBarLayoutParams);
            }
        }
    }

    /**
     * Updates the UrlBar's text alignment and gravity based on whether centering is applied and
     * whether the URL fits within the centering space.
     *
     * @param centeringApplied True if the URL and status centering is currently applied.
     * @param fitsInCenteringSpace True if the URL fits in the available centering space.
     */
    private void updateUrlBarAlignmentAndGravity(
            boolean centeringApplied, boolean fitsInCenteringSpace) {
        int targetGravity;
        int targetAlignment;

        if (centeringApplied) {
            targetGravity =
                    fitsInCenteringSpace
                            ? Gravity.CENTER_HORIZONTAL | Gravity.CENTER_VERTICAL
                            : Gravity.START | Gravity.CENTER_VERTICAL;
            targetAlignment = View.TEXT_ALIGNMENT_GRAVITY;
        } else {
            targetGravity = Gravity.START | Gravity.CENTER_VERTICAL;
            targetAlignment = View.TEXT_ALIGNMENT_VIEW_START;
        }

        if (mUrlBar.getGravity() != targetGravity
                || mUrlBar.getTextAlignment() != targetAlignment) {
            mUrlBar.setGravity(targetGravity);
            mUrlBar.setTextAlignment(targetAlignment);
        }
    }

    /** Returns the measured width of the given text. */
    private int getUrlTextWidth(CharSequence text) {
        if (TextUtils.isEmpty(text)) return 0;
        return (int) Math.ceil(mUrlBar.getPaint().measureText(text, 0, text.length()));
    }

    /** Calculates the maximum allowed width for the UrlBar. */
    private int getMaxUrlWidth(View statusView, int maxComponentWidth, int heightMeasureSpec) {
        statusView.measure(
                MeasureSpec.makeMeasureSpec(maxComponentWidth, MeasureSpec.AT_MOST),
                heightMeasureSpec);
        return maxComponentWidth - statusView.getMeasuredWidth();
    }

    /** Returns whether the URL and status should be centered. */
    private boolean shouldCenterUrlAndStatus() {
        // Only apply centering if the experimental flag is enabled.
        if (!isToolbarUiRefactorEnabled()) {
            return false;
        }

        // Do not center when the URL bar is focused or in transition.
        if (mUrlBar == null || mUrlBarLaidOutAtFocusedWidth) {
            return false;
        }
        if (mLocationBarDataProvider == null) {
            return false;
        }

        // Do not center in Android Hub or Custom Tabs.
        int pageClassification = mLocationBarDataProvider.getPageClassification(false);
        if (!OmniboxViewUtil.isRegularTabContext(pageClassification)) {
            return false;
        }

        return !UrlUtilities.isNtpUrl(mLocationBarDataProvider.getCurrentGurl());
    }

    private boolean isToolbarUiRefactorEnabled() {
        return ToolbarVariationUtils.isToolbarUiRefactorEnabled(getContext());
    }

    /**
     * Returns {@link MarginLayoutParams} of the LocationBar view.
     *
     * <p>TODO(crbug.com/40151029): Hide this View interaction if possible.
     *
     * @see View#getLayoutParams()
     */
    public MarginLayoutParams getMarginLayoutParams() {
        return (MarginLayoutParams) getLayoutParams();
    }

    int getOffsetOfFirstVisibleFocusedView() {
        return 0;
    }

    @Override
    public void destroy() {
        if (mUrlObserver != null) {
            mLocationBarDataProvider.removeObserver(mUrlObserver);
            mUrlObserver = null;
        }
        super.destroy();
    }
}
