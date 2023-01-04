// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.os.Parcelable;
import android.util.AttributeSet;
import android.util.SparseArray;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.LinearLayout;

import androidx.annotation.CallSuper;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.MarginLayoutParamsCompat;
import androidx.core.widget.ImageViewCompat;

import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.omnibox.status.StatusView;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.components.browser_ui.widget.CompositeTouchDelegate;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.ArrayList;
import java.util.List;

/**
 * This class represents the location bar where the user types in URLs and
 * search terms.
 */
public class LocationBarLayout extends FrameLayout {
    protected ImageButton mDeleteButton;
    protected ImageButton mMicButton;
    protected ImageButton mLensButton;
    protected UrlBar mUrlBar;
    protected View mStatusViewLeftSpace;
    protected View mStatusViewRightSpace;

    protected UrlBarCoordinator mUrlCoordinator;
    protected AutocompleteCoordinator mAutocompleteCoordinator;

    protected LocationBarDataProvider mLocationBarDataProvider;

    protected StatusCoordinator mStatusCoordinator;

    protected boolean mNativeInitialized;

    protected LinearLayout mUrlActionContainer;

    protected CompositeTouchDelegate mCompositeTouchDelegate;
    protected SearchEngineLogoUtils mSearchEngineLogoUtils;

    public LocationBarLayout(Context context, AttributeSet attrs) {
        this(context, attrs, R.layout.location_bar);

        mCompositeTouchDelegate = new CompositeTouchDelegate(this);
        setTouchDelegate(mCompositeTouchDelegate);
    }

    public LocationBarLayout(Context context, AttributeSet attrs, int layoutId) {
        super(context, attrs);

        LayoutInflater.from(context).inflate(layoutId, this, true);

        mDeleteButton = findViewById(R.id.delete_button);
        mUrlBar = findViewById(R.id.url_bar);
        mMicButton = findViewById(R.id.mic_button);
        mLensButton = findViewById(R.id.lens_camera_button);
        mUrlActionContainer = (LinearLayout) findViewById(R.id.url_action_container);
        mStatusViewLeftSpace = findViewById(R.id.location_bar_status_view_left_space);
        mStatusViewRightSpace = findViewById(R.id.location_bar_status_view_right_space);
    }

    /**
     * Called when activity is being destroyed.
     */
    void destroy() {
        if (mAutocompleteCoordinator != null) {
            // Don't call destroy() on mAutocompleteCoordinator since we don't own it.
            mAutocompleteCoordinator = null;
        }

        mUrlCoordinator = null;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        setLayoutTransition(null);

        StatusView statusView = findViewById(R.id.location_bar_status);
        statusView.setCompositeTouchDelegate(mCompositeTouchDelegate);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        updateLayoutParams();
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    /**
     * Initializes LocationBarLayout with dependencies that aren't immediately available at
     * construction time.
     *
     * @param autocompleteCoordinator The coordinator for interacting with the autocomplete
     *         subsystem.
     * @param urlCoordinator The coordinator for interacting with the url bar.
     * @param statusCoordinator The coordinator for interacting with the status icon.
     * @param locationBarDataProvider Provider of LocationBar data, e.g. url and title.
     * @param searchEngineLogoUtils Allows querying the state of the search engine logo feature.
     */
    @CallSuper
    public void initialize(@NonNull AutocompleteCoordinator autocompleteCoordinator,
            @NonNull UrlBarCoordinator urlCoordinator, @NonNull StatusCoordinator statusCoordinator,
            @NonNull LocationBarDataProvider locationBarDataProvider,
            @NonNull SearchEngineLogoUtils searchEngineLogoUtils) {
        mAutocompleteCoordinator = autocompleteCoordinator;
        mUrlCoordinator = urlCoordinator;
        mStatusCoordinator = statusCoordinator;
        mLocationBarDataProvider = locationBarDataProvider;
        mSearchEngineLogoUtils = searchEngineLogoUtils;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public AutocompleteCoordinator getAutocompleteCoordinator() {
        return mAutocompleteCoordinator;
    }

    /**
     *  Signals to LocationBarLayout that's it safe to call code that requires native to be loaded.
     */
    public void onFinishNativeInitialization() {
        mNativeInitialized = true;
    }

    /* package */ void setMicButtonDrawable(Drawable drawable) {
        mMicButton.setImageDrawable(drawable);
    }

    /* package */ void setMicButtonTint(ColorStateList colorStateList) {
        ImageViewCompat.setImageTintList(mMicButton, colorStateList);
    }

    /* package */ void setDeleteButtonTint(ColorStateList colorStateList) {
        ImageViewCompat.setImageTintList(mDeleteButton, colorStateList);
    }

    /* package */ void setLensButtonTint(ColorStateList colorStateList) {
        ImageViewCompat.setImageTintList(mLensButton, colorStateList);
    }

    @Override
    protected void dispatchRestoreInstanceState(SparseArray<Parcelable> container) {
        // Don't restore the state of the location bar, it can lead to all kind of bad states with
        // the popup.
        // When we restore tabs, we focus the selected tab so the URL of the page shows.
    }

    protected void onNtpStartedLoading() {}

    public View getSecurityIconView() {
        return mStatusCoordinator.getSecurityIconView();
    }

    /**
     * @return The margin to be applied to the URL bar based on the buttons currently visible next
     *         to it, used to avoid text overlapping the buttons and vice versa.
     */
    private int getUrlContainerMarginEnd() {
        int urlContainerMarginEnd = 0;
        for (View childView : getUrlContainerViewsForMargin()) {
            ViewGroup.MarginLayoutParams childLayoutParams =
                    (ViewGroup.MarginLayoutParams) childView.getLayoutParams();
            urlContainerMarginEnd += childLayoutParams.width
                    + MarginLayoutParamsCompat.getMarginStart(childLayoutParams)
                    + MarginLayoutParamsCompat.getMarginEnd(childLayoutParams);
        }
        if (mUrlActionContainer != null && mUrlActionContainer.getVisibility() == View.VISIBLE) {
            ViewGroup.MarginLayoutParams urlActionContainerLayoutParams =
                    (ViewGroup.MarginLayoutParams) mUrlActionContainer.getLayoutParams();
            urlContainerMarginEnd +=
                    MarginLayoutParamsCompat.getMarginStart(urlActionContainerLayoutParams)
                    + MarginLayoutParamsCompat.getMarginEnd(urlActionContainerLayoutParams);
        }
        urlContainerMarginEnd += mStatusCoordinator.isSearchEngineStatusIconVisible()
                        && mStatusCoordinator.shouldDisplaySearchEngineIcon()
                ? getEndPaddingPixelSizeOnFocusDelta()
                : 0;
        // Account for the URL action container end padding on tablets.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext())) {
            urlContainerMarginEnd +=
                    getResources().getDimensionPixelSize(R.dimen.location_bar_url_action_padding);
        }
        return urlContainerMarginEnd;
    }

    /**
     * Updates the layout params for the location bar start aligned views.
     */
    @VisibleForTesting
    void updateLayoutParams() {
        int startMargin = 0;
        for (int i = 0; i < getChildCount(); i++) {
            View childView = getChildAt(i);
            if (childView.getVisibility() != GONE) {
                LayoutParams childLayoutParams = (LayoutParams) childView.getLayoutParams();
                if (MarginLayoutParamsCompat.getMarginStart(childLayoutParams) != startMargin) {
                    MarginLayoutParamsCompat.setMarginStart(childLayoutParams, startMargin);
                    childView.setLayoutParams(childLayoutParams);
                }
                if (childView == mUrlBar) break;

                int widthMeasureSpec;
                int heightMeasureSpec;
                if (childLayoutParams.width == LayoutParams.WRAP_CONTENT) {
                    widthMeasureSpec =
                            MeasureSpec.makeMeasureSpec(getMeasuredWidth(), MeasureSpec.AT_MOST);
                } else if (childLayoutParams.width == LayoutParams.MATCH_PARENT) {
                    widthMeasureSpec =
                            MeasureSpec.makeMeasureSpec(getMeasuredWidth(), MeasureSpec.EXACTLY);
                } else {
                    widthMeasureSpec = MeasureSpec.makeMeasureSpec(
                            childLayoutParams.width, MeasureSpec.EXACTLY);
                }
                if (childLayoutParams.height == LayoutParams.WRAP_CONTENT) {
                    heightMeasureSpec =
                            MeasureSpec.makeMeasureSpec(getMeasuredHeight(), MeasureSpec.AT_MOST);
                } else if (childLayoutParams.height == LayoutParams.MATCH_PARENT) {
                    heightMeasureSpec =
                            MeasureSpec.makeMeasureSpec(getMeasuredHeight(), MeasureSpec.EXACTLY);
                } else {
                    heightMeasureSpec = MeasureSpec.makeMeasureSpec(
                            childLayoutParams.height, MeasureSpec.EXACTLY);
                }
                childView.measure(widthMeasureSpec, heightMeasureSpec);
                startMargin += childView.getMeasuredWidth();
            }
        }

        int urlContainerMarginEnd = getUrlContainerMarginEnd();
        LayoutParams urlLayoutParams = (LayoutParams) mUrlBar.getLayoutParams();
        if (MarginLayoutParamsCompat.getMarginEnd(urlLayoutParams) != urlContainerMarginEnd) {
            MarginLayoutParamsCompat.setMarginEnd(urlLayoutParams, urlContainerMarginEnd);
            mUrlBar.setLayoutParams(urlLayoutParams);
        }
    }

    /**
     * Gets the list of views that need to be taken into account for adding margin to the end of the
     * URL bar.
     *
     * @return A {@link List} of the views to be taken into account for URL bar margin to avoid
     *         overlapping text and buttons.
     */
    protected List<View> getUrlContainerViewsForMargin() {
        List<View> outList = new ArrayList<View>();
        if (mUrlActionContainer == null) return outList;

        for (int i = 0; i < mUrlActionContainer.getChildCount(); i++) {
            View childView = mUrlActionContainer.getChildAt(i);
            if (childView.getVisibility() != GONE) outList.add(childView);
        }
        return outList;
    }

    /** Sets the visibility of the delete URL content button. */
    /* package */ void setDeleteButtonVisibility(boolean shouldShow) {
        mDeleteButton.setVisibility(shouldShow ? VISIBLE : GONE);
    }

    /** Sets the visibility of the mic button. */
    /* package */ void setMicButtonVisibility(boolean shouldShow) {
        mMicButton.setVisibility(shouldShow ? VISIBLE : GONE);
    }

    /** Sets the visibility of the mic button. */
    /* package */ void setLensButtonVisibility(boolean shouldShow) {
        mLensButton.setVisibility(shouldShow ? VISIBLE : GONE);
    }

    protected void setUnfocusedWidth(int unfocusedWidth) {
        mStatusCoordinator.setUnfocusedLocationBarWidth(unfocusedWidth);
    }

    @VisibleForTesting
    public StatusCoordinator getStatusCoordinatorForTesting() {
        return mStatusCoordinator;
    }

    @VisibleForTesting
    public void setStatusCoordinatorForTesting(StatusCoordinator statusCoordinator) {
        mStatusCoordinator = statusCoordinator;
    }

    /* package */ void setUrlActionContainerVisibility(int visibility) {
        mUrlActionContainer.setVisibility(visibility);
    }

    /** Returns the increase in StatusView end padding, when the Url bar is focused. */
    public int getEndPaddingPixelSizeOnFocusDelta() {
        return getResources().getDimensionPixelSize(R.dimen.location_bar_icon_end_padding_focused)
                - getResources().getDimensionPixelSize(R.dimen.location_bar_icon_end_padding);
    }

    /**
     * Expand the left and right space besides the status view, and increase the location bar
     * vertical padding based on current animation progress percent.
     *
     * @param percent The current animation progress percent.
     */
    protected void setUrlFocusChangePercent(float percent) {
        setStatusViewLeftSpacePercent(percent);
        setStatusViewRightSpacePercent(percent);
    }

    /**
     * Set the status view's left space's width based on current animation progress percent.
     *
     * @param percent The animation progress percent.
     */
    protected void setStatusViewLeftSpacePercent(float percent) {
        if (!OmniboxFeatures.shouldShowModernizeVisualUpdate(getContext())) {
            return;
        }

        // Set the left space expansion width.
        ViewGroup.LayoutParams leftSpacingParams = mStatusViewLeftSpace.getLayoutParams();
        leftSpacingParams.width = (int) (getResources().getDimensionPixelSize(
                                                 R.dimen.location_bar_status_view_left_space_width)
                * percent);
        mStatusViewLeftSpace.setLayoutParams(leftSpacingParams);
    }

    /**
     * Set the status view's right space's width based on current animation progress percent.
     *
     * @param percent The animation progress percent.
     */
    protected void setStatusViewRightSpacePercent(float percent) {
        // Status view's right space does not need to expand for tablets.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext())) {
            return;
        }

        // Set the right space expansion width.
        ViewGroup.LayoutParams rightSpacingParams = mStatusViewRightSpace.getLayoutParams();
        rightSpacingParams.width = (int) (getEndPaddingPixelSizeOnFocusDelta() * percent);
        mStatusViewRightSpace.setLayoutParams(rightSpacingParams);
    }

    public void notifyVoiceRecognitionCanceled() {}
}
