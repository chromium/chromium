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
import android.widget.ImageButton;
import android.widget.LinearLayout;

import androidx.annotation.CallSuper;
import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;
import androidx.constraintlayout.widget.ConstraintLayout;
import androidx.core.widget.ImageViewCompat;

import org.chromium.base.MathUtils;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.lens.LensEntryPoint;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.omnibox.status.StatusView;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.omnibox.voice.VoiceRecognitionHandler;
import org.chromium.components.browser_ui.widget.CompositeTouchDelegate;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.ui.base.DeviceFormFactor;

/** This class represents the location bar where the user types in URLs and search terms. */
@NullMarked
public class LocationBarLayout extends ConstraintLayout {
    protected ImageButton mDeleteButton;
    protected ImageButton mMicButton;
    protected ImageButton mLensButton;
    protected ImageButton mZoomButton;
    protected ImageButton mInstallButton;
    protected ImageButton mComposeplateButton;
    protected UrlBar mUrlBar;

    protected UrlBarCoordinator mUrlCoordinator;
    protected AutocompleteCoordinator mAutocompleteCoordinator;

    protected LocationBarDataProvider mLocationBarDataProvider;

    protected StatusCoordinator mStatusCoordinator;

    protected boolean mNativeInitialized;
    protected boolean mHidingActionContainerForNarrowWindow;
    private final int mMinimumActionContainerWidthPx;

    protected LinearLayout mUrlActionContainer;
    private final View mMarginSpacer;

    protected @Nullable CompositeTouchDelegate mCompositeTouchDelegate;
    protected @Nullable SearchEngineUtils mSearchEngineUtils;
    private boolean mUrlBarLaidOutAtFocusedWidth;
    private final int mStatusIconAndUrlBarOffset;
    private int mUrlActionContainerEndMargin;

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
        mZoomButton = findViewById(R.id.zoom_button);
        mInstallButton = findViewById(R.id.install_button);
        mComposeplateButton = findViewById(R.id.composeplate_button);
        mUrlActionContainer = findViewById(R.id.url_action_container);
        mMarginSpacer = findViewById(R.id.margin_spacer);
        mMinimumActionContainerWidthPx =
                context.getResources().getDimensionPixelSize(R.dimen.min_touch_target_size)
                        - context.getResources()
                                .getDimensionPixelSize(R.dimen.location_bar_url_action_offset);
        mStatusIconAndUrlBarOffset =
                OmniboxResourceProvider.getToolbarSidePaddingForNtp(context)
                        - OmniboxResourceProvider.getToolbarSidePadding(context);
        mUrlActionContainerEndMargin =
                getResources().getDimensionPixelOffset(R.dimen.location_bar_url_action_offset);
    }

    /** Called when activity is being destroyed. */
    @SuppressWarnings("NullAway")
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
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        checkUrlContainerWidth();
    }

    /**
     * Initializes LocationBarLayout with dependencies that aren't immediately available at
     * construction time.
     *
     * @param autocompleteCoordinator The coordinator for interacting with the autocomplete
     *     subsystem.
     * @param urlCoordinator The coordinator for interacting with the url bar.
     * @param statusCoordinator The coordinator for interacting with the status icon.
     * @param locationBarDataProvider Provider of LocationBar data, e.g. url and title.
     */
    @Initializer
    @CallSuper
    public void initialize(
            AutocompleteCoordinator autocompleteCoordinator,
            UrlBarCoordinator urlCoordinator,
            StatusCoordinator statusCoordinator,
            LocationBarDataProvider locationBarDataProvider) {
        mAutocompleteCoordinator = autocompleteCoordinator;
        mUrlCoordinator = urlCoordinator;
        mStatusCoordinator = statusCoordinator;
        mLocationBarDataProvider = locationBarDataProvider;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public @Nullable AutocompleteCoordinator getAutocompleteCoordinator() {
        return mAutocompleteCoordinator;
    }

    /**
     * Signals to LocationBarLayout that's it safe to call code that requires native to be loaded.
     */
    public void onFinishNativeInitialization() {
        mNativeInitialized = true;
    }

    /* package */ void setMicButtonDrawable(Drawable drawable) {
        mMicButton.setImageDrawable(drawable);
    }

    /* package */ void setComposeplateButtonDrawable(Drawable drawable) {
        mComposeplateButton.setImageDrawable(drawable);
    }

    /* package */ void setMicButtonTint(ColorStateList colorStateList) {
        ImageViewCompat.setImageTintList(mMicButton, colorStateList);
    }

    /* package */ void setDeleteButtonTint(ColorStateList colorStateList) {
        ImageViewCompat.setImageTintList(mDeleteButton, colorStateList);
    }

    /* package */ void setDeleteButtonBackground(@DrawableRes int resourceId) {
        mDeleteButton.setBackgroundResource(resourceId);
    }

    /* package */ void setLensButtonTint(ColorStateList colorStateList) {
        ImageViewCompat.setImageTintList(mLensButton, colorStateList);
    }

    /* package */ void setComposeplateButtonTint(ColorStateList colorStateList) {
        ImageViewCompat.setImageTintList(mComposeplateButton, colorStateList);
    }

    /* package */ void setInstallButtonTint(ColorStateList colorStateList) {
        ImageViewCompat.setImageTintList(mInstallButton, colorStateList);
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
     * Hides the url action container if the window is too narrow to show it alongside the url bar,
     * or shows it if the window is now wide enough.
     */
    void checkUrlContainerWidth() {
        int actionContainerWidth = mUrlActionContainer.getMeasuredWidth();

        if (!mHidingActionContainerForNarrowWindow
                && actionContainerWidth > 0
                && actionContainerWidth < mMinimumActionContainerWidthPx) {
            mHidingActionContainerForNarrowWindow = true;
            mUrlActionContainer.setVisibility(INVISIBLE);
            ConstraintLayout.LayoutParams layoutParams =
                    (LayoutParams) mUrlActionContainer.getLayoutParams();
            layoutParams.startToEnd = LayoutParams.PARENT_ID;
            layoutParams.endToEnd = LayoutParams.UNSET;
            mUrlActionContainer.setLayoutParams(layoutParams);
        } else if (mHidingActionContainerForNarrowWindow
                && mUrlActionContainer.getVisibility() != VISIBLE
                && actionContainerWidth >= mMinimumActionContainerWidthPx) {
            mHidingActionContainerForNarrowWindow = false;
            mUrlActionContainer.setVisibility(VISIBLE);
            ConstraintLayout.LayoutParams layoutParams =
                    (LayoutParams) mUrlActionContainer.getLayoutParams();
            layoutParams.startToEnd = mUrlBar.getId();
            layoutParams.endToEnd = mMarginSpacer.getId();
            mUrlActionContainer.setLayoutParams(layoutParams);
        }
    }

    /** Sets the visibility of the delete URL content button. */
    /* package */ void setDeleteButtonVisibility(boolean shouldShow) {
        mDeleteButton.setVisibility(shouldShow ? VISIBLE : GONE);
    }

    /** Sets the visibility of the mic button. */
    /* package */ void setMicButtonVisibility(boolean shouldShow) {
        mMicButton.setVisibility(shouldShow ? VISIBLE : GONE);
    }

    /** Sets the visibility of the lens button. */
    /* package */ void setLensButtonVisibility(boolean shouldShow) {
        mLensButton.setVisibility(shouldShow ? VISIBLE : GONE);
    }

    /** Sets the visibility of the zoom button. */
    /* package */ void setZoomButtonVisibility(boolean shouldShow) {
        mZoomButton.setVisibility(shouldShow ? VISIBLE : GONE);
    }

    /** Sets the visibility of the install button. */
    /* package */ void setInstallButtonVisibility(boolean shouldShow) {
        mInstallButton.setVisibility(shouldShow ? VISIBLE : GONE);
    }

    /** Sets the visibility of the composeplate button. */
    /* package */ void setComposeplateButtonVisibility(boolean shouldShow) {
        mComposeplateButton.setVisibility(shouldShow ? VISIBLE : GONE);
    }

    protected void setUnfocusedWidth(int unfocusedWidth) {
        mStatusCoordinator.setUnfocusedLocationBarWidth(unfocusedWidth);
    }

    public StatusCoordinator getStatusCoordinatorForTesting() {
        return mStatusCoordinator;
    }

    public void setStatusCoordinatorForTesting(StatusCoordinator statusCoordinator) {
        mStatusCoordinator = statusCoordinator;
    }

    /* package */ void setUrlActionContainerVisibility(int visibility) {
        if (mHidingActionContainerForNarrowWindow && visibility == VISIBLE) return;
        mUrlActionContainer.setVisibility(visibility);
    }

    /** Returns the increase in StatusView end padding, when the Url bar is focused. */
    public int getEndPaddingPixelSizeOnFocusDelta() {
        return getResources()
                .getDimensionPixelSize(
                        mLocationBarDataProvider.isIncognitoBranded()
                                ? R.dimen.location_bar_icon_end_padding_focused_incognito
                                : R.dimen.location_bar_icon_end_padding_focused);
    }

    /**
     * Expand the left and right margins besides the status view, and increase the location bar
     * vertical padding based on current animation progress percent.
     *
     * @param ntpSearchBoxScrollFraction The degree to which the omnibox has expanded to full width
     *     in NTP due to the NTP search box is being scrolled up.
     * @param urlFocusChangeFraction The degree to which the omnibox has expanded due to it is
     *     getting focused.
     * @param isUrlFocusChangeInProgress True if the url focus change is in progress.
     */
    protected void setUrlFocusChangePercent(
            float ntpSearchBoxScrollFraction,
            float urlFocusChangeFraction,
            boolean isUrlFocusChangeInProgress) {
        float urlFocusPercentage = Math.max(ntpSearchBoxScrollFraction, urlFocusChangeFraction);
        mUrlBarLaidOutAtFocusedWidth = urlFocusPercentage > 0.0f || mUrlBar.hasFocus();

        setStatusViewLeftMarginPercent(
                ntpSearchBoxScrollFraction, urlFocusChangeFraction, isUrlFocusChangeInProgress);
        setStatusViewRightMarginPercent(
                ntpSearchBoxScrollFraction, urlFocusChangeFraction, isUrlFocusChangeInProgress);

        int urlBarStartMargin =
                mUrlBarLaidOutAtFocusedWidth ? getFocusedStatusViewSpacingDelta() : 0;
        MarginLayoutParams layoutParams = (MarginLayoutParams) mUrlBar.getLayoutParams();
        if (layoutParams.getMarginStart() != urlBarStartMargin) {
            layoutParams.setMarginStart(urlBarStartMargin);
            mUrlBar.setLayoutParams(layoutParams);
        }
    }

    /**
     * Set the "left margin width" based on current animation progress percent. This uses
     * translation to avoid triggering a relayout.
     *
     * @param ntpSearchBoxScrollFraction The degree to which the omnibox has expanded to full width
     *     in NTP due to the NTP search box is being scrolled up.
     * @param urlFocusChangeFraction The degree to which the omnibox has expanded due to it is
     *     getting focused.
     * @param isUrlFocusChangeInProgress True if the url focus change is in progress.
     */
    protected void setStatusViewLeftMarginPercent(
            float ntpSearchBoxScrollFraction,
            float urlFocusChangeFraction,
            boolean isUrlFocusChangeInProgress) {
        float maxPercent = Math.max(ntpSearchBoxScrollFraction, urlFocusChangeFraction);
        boolean isOnTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext());
        float translationX;
        if (!isOnTablet && isUrlFocusChangeInProgress && ntpSearchBoxScrollFraction == 1) {
            translationX =
                    OmniboxResourceProvider.getFocusedStatusViewLeftSpacing(getContext())
                            + mStatusIconAndUrlBarOffset * (1 - urlFocusChangeFraction);
        } else {
            translationX =
                    OmniboxResourceProvider.getFocusedStatusViewLeftSpacing(getContext())
                            * maxPercent;
        }
        mStatusCoordinator.setTranslationX(
                MathUtils.flipSignIf(translationX, getLayoutDirection() == LAYOUT_DIRECTION_RTL));
    }

    /**
     * Set the "right margin width" based on current animation progress percent. This uses
     * translation to avoid triggering a relayout.
     *
     * @param ntpSearchBoxScrollFraction The degree to which the omnibox has expanded to full width
     *     in NTP due to the NTP search box is being scrolled up.
     * @param urlFocusChangeFraction The degree to which the omnibox has expanded due to it is
     *     getting focused.
     * @param isUrlFocusChangeInProgress True if the url focus change is in progress.
     */
    protected void setStatusViewRightMarginPercent(
            float ntpSearchBoxScrollFraction,
            float urlFocusChangeFraction,
            boolean isUrlFocusChangeInProgress) {
        float translationX;
        if (mUrlBarLaidOutAtFocusedWidth) {
            translationX =
                    getUrlBarTranslationXForFocusAndScrollAnimationOnNtp(
                            ntpSearchBoxScrollFraction,
                            urlFocusChangeFraction,
                            isUrlFocusChangeInProgress,
                            DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext()));
        } else {
            // No compensation is needed at 0% because the margin is reset to normal.
            translationX = 0.0f;
        }

        mUrlBar.setTranslationX(
                MathUtils.flipSignIf(translationX, getLayoutDirection() == LAYOUT_DIRECTION_RTL));
    }

    /**
     * Get the translation for the status view based on the current animation progress percent.
     *
     * @param ntpSearchBoxScrollFraction The degree to which the omnibox has expanded to full width
     *     in NTP due to the NTP search box is being scrolled up.
     * @param urlFocusChangeFraction The degree to which the omnibox has expanded due to it is
     *     getting focused.
     * @param isUrlFocusChangeInProgress True if the url focus change is in progress.
     * @param isOnTablet True if the current page is on the tablet.
     */
    float getUrlBarTranslationXForFocusAndScrollAnimationOnNtp(
            float ntpSearchBoxScrollFraction,
            float urlFocusChangeFraction,
            boolean isUrlFocusChangeInProgress,
            boolean isOnTablet) {

        if (!isOnTablet && isUrlFocusChangeInProgress && ntpSearchBoxScrollFraction == 1) {
            // For the focus and un-focus animation when the real search box is visible
            // on NTP.
            return mStatusIconAndUrlBarOffset * (1 - urlFocusChangeFraction);
        }

        float translationX = -getFocusedStatusViewSpacingDelta();

        boolean isNtpOnPhone =
                mStatusCoordinator.isSearchEngineStatusIconVisible()
                        && UrlUtilities.isNtpUrl(mLocationBarDataProvider.getCurrentGurl())
                        && !isOnTablet;
        boolean isScrollingOnNtpOnPhone = !mUrlBar.hasFocus() && isNtpOnPhone;

        if (isScrollingOnNtpOnPhone) {
            translationX -=
                    mStatusCoordinator.getStatusIconWidth()
                            - getResources()
                                    .getDimensionPixelSize(
                                            R.dimen.location_bar_status_icon_holding_space_size);
        }

        boolean isInSingleUrlBarMode =
                isNtpOnPhone
                        && mSearchEngineUtils != null
                        && mSearchEngineUtils.doesDefaultSearchEngineHaveLogo();
        if (isInSingleUrlBarMode) {
            translationX +=
                    (getResources().getDimensionPixelSize(R.dimen.fake_search_box_start_padding)
                            - getResources()
                                    .getDimensionPixelSize(
                                            R.dimen.location_bar_status_icon_holding_space_size));
        }

        // If the url bar is laid out at its smaller, focused width, translate back towards
        // start to compensate for the increased start margin set in #updateLayoutParams. The
        // magnitude of the compensation decreases as % increases and is 0 at full focus %.
        float percent = Math.max(ntpSearchBoxScrollFraction, urlFocusChangeFraction);
        return translationX * (1.0f - percent);
    }

    /**
     * The delta between the total status view spacing (left + right) when unfocused vs focused. The
     * status view has additional spacing applied when focused to visually align it and the UrlBar
     * with omnibox suggestions. See below diagram; the additional spacing is denoted with _
     * Unfocused: [ (i) www.example.com] Focused: [ _(G)_ Search or type web address] [ 🔍 Foobar ↖
     * ] [ 🔍 Barbaz ↖ ]
     */
    @VisibleForTesting
    int getFocusedStatusViewSpacingDelta() {
        return getEndPaddingPixelSizeOnFocusDelta()
                + OmniboxResourceProvider.getFocusedStatusViewLeftSpacing(getContext());
    }

    /** Applies the new SearchEngineUtils. */
    void setSearchEngineUtils(SearchEngineUtils searchEngineUtils) {
        mSearchEngineUtils = searchEngineUtils;
    }

    /** Returns the source of Voice Recognition interactions. */
    public int getVoiceRecognitionSource() {
        return VoiceRecognitionHandler.VoiceInteractionSource.OMNIBOX;
    }

    /** Returns the entrypoint used to launch Lens. */
    public int getLensEntryPoint() {
        return LensEntryPoint.OMNIBOX;
    }

    /** Returns whether the Omnibox text should be cleared on focus. */
    public boolean shouldClearTextOnFocus() {
        return true;
    }

    /**
     * Updates the value for the end margin of the url action container in the search box.
     *
     * @param useDefaultUrlActionContainerEndMargin Whether to use the default end margin for the
     *     url action container in the search box. If not we will use the specific end margin value
     *     for NTP's un-focus state.
     */
    public void updateUrlActionContainerEndMargin(boolean useDefaultUrlActionContainerEndMargin) {
        // ConstraintLayout doesn't trivially support negative margins. We emulate one here by
        // positioning a spacer view past the end of the layout and constraining the url action
        // container to end at the end of this view.
        mUrlActionContainerEndMargin =
                useDefaultUrlActionContainerEndMargin
                        ? getResources()
                                .getDimensionPixelSize(R.dimen.location_bar_url_action_offset)
                        : getResources()
                                .getDimensionPixelSize(R.dimen.location_bar_url_action_offset_ntp);
        MarginLayoutParams spacerParams = (MarginLayoutParams) mMarginSpacer.getLayoutParams();
        if (spacerParams.getMarginEnd() != mUrlActionContainerEndMargin) {
            spacerParams.setMarginEnd(mUrlActionContainerEndMargin);
            mMarginSpacer.setLayoutParams(spacerParams);
        }
    }

    int getUrlActionContainerEndMarginForTesting() {
        return mUrlActionContainerEndMargin;
    }
}
