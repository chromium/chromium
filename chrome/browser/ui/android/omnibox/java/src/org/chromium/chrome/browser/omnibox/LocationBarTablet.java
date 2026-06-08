// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Rect;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.LayerDrawable;
import android.os.Handler;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnLongClickListener;
import android.view.ViewGroup;
import android.view.ViewOutlineProvider;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxLayoutMode;
import org.chromium.chrome.browser.omnibox.fusebox.FuseboxCoordinator.FuseboxState;
import org.chromium.chrome.browser.omnibox.status.StatusCoordinator;
import org.chromium.chrome.browser.omnibox.styles.OmniboxResourceProvider;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteCoordinator;
import org.chromium.chrome.browser.ui.theme.BrandedColorScheme;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.widget.Toast;

/** Location bar for tablet form factors. */
@NullMarked
class LocationBarTablet extends LocationBarLayout implements OnLongClickListener {
    // The number of toolbar buttons that can be hidden at small widths (reload, back, forward).
    private static final int HIDEABLE_BUTTON_COUNT = 3;
    private static final float OVERLAY_Z_TRANSLATION = 1.0f;
    private static final float NEUTRAL_Z_TRANSLATION = 0.0f;
    private final LayerDrawable mFocusedPopupDrawable;
    private final GradientDrawable mOuterRect;
    private final GradientDrawable mInnerRect;
    private LayerDrawable mUnfocusedDrawable;
    private final LayerDrawable mHoverDrawable;

    private View mLocationBarIcon;
    private View mBookmarkButton;
    private View[] mTargets;
    private final Rect mCachedTargetBounds = new Rect();
    private final GlifStrokeDrawable mGlifBorderDrawable;
    private final Handler mHandler;

    // Variables needed for animating the location bar and toolbar buttons hiding/showing.
    private final int mToolbarButtonsWidth;
    private final int mMicButtonWidth;
    private final int mLensButtonWidth;
    private boolean mAnimatingWidthChange;
    private float mWidthChangeFraction;
    private float mLayoutLeft;
    private float mLayoutRight;
    private int mToolbarStartPaddingDifference;
    private final int[] mPositionArray = new int[2];

    @SuppressWarnings("HidingField")
    private UrlBar mUrlBar;

    private View mStatusView;

    private WindowAndroid mWindowAndroid;
    private @FuseboxState int mFuseboxState;
    private boolean mHasSuggestions;
    private int mSuggestionsListScrollOffset;
    private int mScreenWidthDp;
    private @Nullable ViewOutlineProvider mOutlineProvider;
    private final int mLocationBarTabletFuseboxPopupInset;
    private final float mOmniboxSuggestionDropdownRoundCornerRadius;
    private final int mModernToolbarBackgroundVerticalOffset;
    private final float mModernToolbarBackgroundCornerRadius;
    private final float mModernToolbarBackgroundInnerCornerRadius;
    // The holder view dictates our height and width but is otherwise logic-less. It exists to allow
    // us to reparent the LocationBar without needing to explicitly reposition other elements of the
    // toolbar.
    private View mHolder;
    private @FuseboxLayoutMode int mLayoutMode;
    private boolean mIsReparentedToPopover;
    private @BrandedColorScheme int mBrandedColorScheme = BrandedColorScheme.APP_DEFAULT;

    /** Constructor used to inflate from XML. */
    public LocationBarTablet(Context context, AttributeSet attrs) {
        super(context, attrs);

        Resources resources = context.getResources();
        mToolbarButtonsWidth =
                resources.getDimensionPixelOffset(R.dimen.toolbar_button_width)
                        * HIDEABLE_BUTTON_COUNT;
        int locationBarIconWidth =
                resources.getDimensionPixelOffset(R.dimen.location_bar_icon_width);
        mMicButtonWidth = locationBarIconWidth;
        mLensButtonWidth = locationBarIconWidth;
        mFocusedPopupDrawable =
                (LayerDrawable)
                        assumeNonNull(
                                context.getDrawable(
                                        R.drawable
                                                .modern_toolbar_tablet_text_box_background_focused_popup));
        mFocusedPopupDrawable.mutate();
        mOuterRect =
                (GradientDrawable)
                        mFocusedPopupDrawable.findDrawableByLayerId(R.id.focused_popup_bg);
        mInnerRect =
                (GradientDrawable)
                        mFocusedPopupDrawable.findDrawableByLayerId(R.id.focused_popup_inner_bg);
        mGlifBorderDrawable = new GlifStrokeDrawable(context);
        mLocationBarTabletFuseboxPopupInset =
                resources.getDimensionPixelSize(R.dimen.location_bar_tablet_fusebox_popup_inset);
        mOmniboxSuggestionDropdownRoundCornerRadius =
                resources.getDimension(R.dimen.omnibox_suggestion_dropdown_round_corner_radius);
        mModernToolbarBackgroundVerticalOffset =
                resources.getDimensionPixelSize(R.dimen.modern_toolbar_background_vertical_offset);
        mModernToolbarBackgroundCornerRadius =
                resources.getDimension(R.dimen.modern_toolbar_background_corner_radius);
        mModernToolbarBackgroundInnerCornerRadius =
                resources.getDimension(R.dimen.modern_toolbar_background_inner_corner_radius);
        mHoverDrawable =
                (LayerDrawable)
                        assumeNonNull(
                                AppCompatResources.getDrawable(
                                        getContext(),
                                        R.drawable.modern_toolbar_text_box_background_highlight));
        mHoverDrawable.mutate();
        mHandler = new Handler();
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mUnfocusedDrawable = (LayerDrawable) getBackground();
        mUnfocusedDrawable.mutate();

        mLocationBarIcon = findViewById(R.id.location_bar_status_icon);
        mBookmarkButton = findViewById(R.id.bookmark_button);
        mUrlBar = findViewById(R.id.url_bar);
        mStatusView = findViewById(R.id.location_bar_status);

        mUrlBar.setOnHoverListener(
                new View.OnHoverListener() {
                    @Override
                    public boolean onHover(View v, MotionEvent event) {
                        switch (event.getAction()) {
                            case MotionEvent.ACTION_HOVER_ENTER:
                                setForeground(mHoverDrawable);
                                return true;
                            case MotionEvent.ACTION_HOVER_EXIT:
                                setForeground(null);
                                return true;
                            default:
                                return false;
                        }
                    }
                });

        setOnLongClickListener(this);

        mTargets = new View[] {mUrlBar, mDeleteButton};
        mScreenWidthDp = getResources().getConfiguration().screenWidthDp;
    }

    @Initializer
    public void setHolder(ViewGroup holder) {
        mHolder = holder;
    }

    @SuppressLint("ClickableViewAccessibility")
    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (mTargets == null) return true;

        View selectedTarget = null;
        float selectedDistance = 0;
        // newX and newY are in the coordinates of the selectedTarget.
        float newX = 0;
        float newY = 0;
        for (View target : mTargets) {
            if (!target.isShown()) continue;

            mCachedTargetBounds.set(0, 0, target.getWidth(), target.getHeight());
            offsetDescendantRectToMyCoords(target, mCachedTargetBounds);
            float x = event.getX();
            float y = event.getY();
            float dx = distanceToRange(mCachedTargetBounds.left, mCachedTargetBounds.right, x);
            float dy = distanceToRange(mCachedTargetBounds.top, mCachedTargetBounds.bottom, y);
            float distance = Math.abs(dx) + Math.abs(dy);
            if (selectedTarget == null || distance < selectedDistance) {
                selectedTarget = target;
                selectedDistance = distance;
                newX = x + dx;
                newY = y + dy;
            }
        }

        if (selectedTarget == null) return false;

        event.setLocation(newX, newY);
        return selectedTarget.onTouchEvent(event);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int measuredWidth = getMeasuredWidth();

        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        if (getMeasuredWidth() != measuredWidth) {
            setUnfocusedWidth(getMeasuredWidth());
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
        }
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        super.onLayout(changed, left, top, right, bottom);
        mLayoutLeft = left;
        mLayoutRight = right;

        if (mAnimatingWidthChange) {
            setWidthChangeAnimationFraction(mWidthChangeFraction);
        }

        int screenWidthDp = getResources().getConfiguration().screenWidthDp;
        boolean widthChangedSinceLastLayout = screenWidthDp != mScreenWidthDp;
        if (widthChangedSinceLastLayout) {
            // Our fusebox-specific margins become wrong when the window width changes, since they
            // depend on the window width. When we detect that the window width changes, recalculate
            // margins for the current state + new width using a post(), whose delay allows the full
            // layout pass to finish.
            mScreenWidthDp = screenWidthDp;
            mHandler.post(() -> onFuseboxStateChanged(mFuseboxState));
        }
    }

    @Override
    public void setOutlineProvider(ViewOutlineProvider provider) {
        mOutlineProvider = provider;
        super.setOutlineProvider(provider);
    }

    /** Returns amount by which to adjust to move value inside the given range. */
    private static float distanceToRange(float min, float max, float value) {
        return value < min ? (min - value) : value > max ? (max - value) : 0;
    }

    /**
     * Resets the alpha and translation X for all views affected by the animations for showing or
     * hiding buttons.
     */
    /* package */ void resetValuesAfterAnimation() {
        setLocationBarButtonTranslationForNtpAnimation(0.f);
        mUrlBar.setTranslationX(0);

        mMicButton.setAlpha(1.f);
        mLensButton.setAlpha(1.f);
        mDeleteButton.setAlpha(1.f);
        mBookmarkButton.setAlpha(1.f);
    }

    /**
     * Updates completion progress for the location bar width change animation.
     *
     * @param fraction How complete the animation is, where 0 represents the normal width (toolbar
     *     buttons fully visible) and 1.f represents the expanded width (toolbar buttons fully
     *     hidden).
     */
    /* package */ void setWidthChangeAnimationFraction(float fraction) {
        mWidthChangeFraction = fraction;

        float offset = (mToolbarButtonsWidth + mToolbarStartPaddingDifference) * fraction;

        if (LocalizationUtils.isLayoutRtl()) {
            // The location bar's right edge is its regular layout position when toolbar buttons are
            // completely visible and its layout position + mToolbarButtonsWidth when toolbar
            // buttons are completely hidden.
            setRight((int) (mLayoutRight + offset));
        } else {
            // The location bar's left edge is it's regular layout position when toolbar buttons are
            // completely visible and its layout position - mToolbarButtonsWidth when they are
            // completely hidden.
            setLeft((int) (mLayoutLeft - offset));
        }

        // As the location bar's right edge moves right (increases) or left edge moves left
        // (decreases), the child views' translation X increases, keeping them visually in the same
        // location for the duration of the animation.
        int deleteOffset = (int) (mMicButtonWidth * fraction);
        if (isLensButtonVisible()) {
            deleteOffset += (int) (mLensButtonWidth * fraction);
        }
        setChildTranslationsForWidthChangeAnimation((int) offset, deleteOffset);
    }

    /* package */ float getWidthChangeFraction() {
        return mWidthChangeFraction;
    }

    /**
     * Sets the translation X values for child views during the width change animation. This
     * compensates for the change to the left/right position of the location bar and ensures child
     * views stay in the same spot visually during the animation.
     *
     * <p>The delete button is special because if it's visible during the animation its start and
     * end location are not the same. When buttons are shown in the unfocused location bar, the
     * delete button is left of the microphone. When buttons are not shown in the unfocused location
     * bar, the delete button is aligned with the left edge of the location bar.
     *
     * @param offset The offset to use for the child views.
     * @param deleteOffset The additional offset to use for the delete button.
     */
    private void setChildTranslationsForWidthChangeAnimation(int offset, int deleteOffset) {
        if (getLayoutDirection() != LAYOUT_DIRECTION_RTL) {
            // When the location bar layout direction is LTR, the buttons at the end (left side)
            // of the location bar need to stick to the left edge.
            mMicButton.setTranslationX(offset);

            if (mDeleteButton.getVisibility() == View.VISIBLE) {
                mDeleteButton.setTranslationX(offset + deleteOffset);
            } else {
                mBookmarkButton.setTranslationX(offset);
            }
        } else {
            // When the location bar layout direction is RTL, the location bar icon and url
            // container at the start (right side) of the location bar need to stick to the right
            // edge.
            mLocationBarIcon.setTranslationX(offset);
            mUrlBar.setTranslationX(offset);

            if (mDeleteButton.getVisibility() == View.VISIBLE) {
                mDeleteButton.setTranslationX(-deleteOffset);
            }
        }
    }

    /* package */ void setBookmarkButtonVisibility(boolean showBookmarkButton) {
        // The button may be null if this method is called before initialization is finished.
        if (mBookmarkButton == null) return;
        mBookmarkButton.setVisibility(showBookmarkButton ? View.VISIBLE : View.GONE);
    }

    /* package */ boolean isDeleteButtonVisible() {
        return mDeleteButton.getVisibility() == VISIBLE;
    }

    /* package */ boolean isMicButtonVisible() {
        return mMicButton.getVisibility() == VISIBLE;
    }

    /* package */ float getMicButtonAlpha() {
        return mMicButton.getAlpha();
    }

    /* package */ boolean isLensButtonVisible() {
        return mLensButton.getVisibility() == VISIBLE;
    }

    /* package */ float getLensButtonAlpha() {
        return mLensButton.getAlpha();
    }

    /**
     * Gets the bookmark button view for the purposes of creating an animator that targets it. Don't
     * use this for any other reason, e.g. to access or modify the view's properties directly.
     */
    @Deprecated
    /* package */ View getBookmarkButtonForAnimation() {
        return mBookmarkButton;
    }

    /**
     * Gets the mic button view for the purposes of creating an animator that targets it. Don't use
     * this for any other reason, e.g. to access or modify the view's properties directly.
     */
    @Deprecated
    /* package */ View getMicButtonForAnimation() {
        return mMicButton;
    }

    /**
     * Gets the Lens button view for the purposes of creating an animator that targets it. Don't use
     * this for any other reason, e.g. to access or modify the view's properties directly.
     */
    @Deprecated
    /* package */ View getLensButtonForAnimation() {
        return mLensButton;
    }

    /* package */ void startAnimatingWidthChange(int toolbarStartPaddingDifference) {
        mAnimatingWidthChange = true;
        mToolbarStartPaddingDifference = toolbarStartPaddingDifference;
    }

    /* package */ void finishAnimatingWidthChange() {
        mAnimatingWidthChange = false;
        mToolbarStartPaddingDifference = 0;
    }

    @Override
    public boolean onLongClick(View v) {
        String description = null;
        Context context = getContext();
        Resources resources = context.getResources();

        if (v == mBookmarkButton) {
            description = resources.getString(R.string.menu_bookmark);
        }
        return Toast.showAnchoredToast(context, v, description);
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
        mWindowAndroid = windowAndroid;
    }

    @Override
    /* package */ void updateVisualsForState(@BrandedColorScheme int brandedColorScheme) {
        super.updateVisualsForState(brandedColorScheme);

        mBrandedColorScheme = brandedColorScheme;
        Context context = getContext();
        mOuterRect.setColor(
                mLayoutMode == FuseboxLayoutMode.SUGGESTIONS_POPOVER
                        ? OmniboxResourceProvider.getStandardSuggestionBackgroundColor(
                                context, mBrandedColorScheme)
                        : OmniboxResourceProvider.getSuggestionsDropdownBackgroundColor(
                                context, mBrandedColorScheme));
        mInnerRect.setColor(
                OmniboxResourceProvider.getStandardSuggestionBackgroundColor(
                        context, mBrandedColorScheme));

        GradientDrawable unfocusedRect =
                (GradientDrawable) mUnfocusedDrawable.findDrawableByLayerId(R.id.unfocused_bg);
        if (unfocusedRect != null) {
            unfocusedRect.setColor(
                    OmniboxResourceProvider.getTabletToolbarTextBoxBackgroundColor(
                            context, mBrandedColorScheme));
        }
    }

    @Override
    /* package */ void setLocationBarButtonTranslationForNtpAnimation(float translationX) {
        super.setLocationBarButtonTranslationForNtpAnimation(translationX);
        mBookmarkButton.setTranslationX(translationX);
    }

    @Override
    /* package */ View getAlignmentView() {
        return mHolder;
    }

    @Override
    void onSuggestionsChanged(boolean hasSuggestions) {
        mHasSuggestions = hasSuggestions;
        if (getBackground() != mFocusedPopupDrawable) {
            return;
        }

        adjustBackgroundForSuggestions();
    }

    @Override
    void onSuggestionsListScrollOffsetChanged(int verticalScrollOffset) {
        mSuggestionsListScrollOffset = verticalScrollOffset;
        if (getBackground() != mFocusedPopupDrawable) {
            return;
        }

        adjustBackgroundForSuggestions();
    }

    @Override
    public void onSpecializedFuseboxModeActivated(boolean isSpecializedRequestType) {
        if (isSpecializedRequestType) {
            mFocusedPopupDrawable.setDrawableByLayerId(R.id.glif_border_layer, mGlifBorderDrawable);
            mGlifBorderDrawable.start();
        } else {
            mFocusedPopupDrawable.setDrawableByLayerId(R.id.glif_border_layer, null);
        }
    }

    @Override
    void setFuseboxLayoutMode(@FuseboxLayoutMode int layoutMode) {
        super.setFuseboxLayoutMode(layoutMode);
        mLayoutMode = layoutMode;
        // We don't expect that the layout mode will ever change back after becoming
        // SUGGESTIONS_POPOVER (it depends only on flags set at build time and startup) and thus
        // don't handle that case.
        if (layoutMode == FuseboxLayoutMode.SUGGESTIONS_POPOVER) {
            mOuterRect.setColor(
                    OmniboxResourceProvider.getStandardSuggestionBackgroundColor(
                            getContext(), mBrandedColorScheme));
        }
    }

    @Override
    public void onFuseboxStateChanged(@FuseboxState int state) {
        super.onFuseboxStateChanged(state);
        mFuseboxState = state;
        updateLayoutAndBackground();
    }

    @Override
    /* package */ void setReparentedToPopover(boolean isReparented) {
        mIsReparentedToPopover = isReparented;
        updateLayoutAndBackground();
    }

    private void updateLayoutAndBackground() {
        adjustVerticalTranslationForFuseboxState(mFuseboxState);
        FrameLayout.LayoutParams layoutParams = (FrameLayout.LayoutParams) getLayoutParams();
        Resources resources = getResources();
        LinearLayout.LayoutParams parentParams =
                (LinearLayout.LayoutParams) mHolder.getLayoutParams();
        if (mFuseboxState == FuseboxState.COMPACT
                || mFuseboxState == FuseboxState.EXPANDED
                || (mLayoutMode == FuseboxLayoutMode.SUGGESTIONS_POPOVER
                        && mIsReparentedToPopover)) {
            parentParams.height = ViewGroup.LayoutParams.WRAP_CONTENT;
            int expansionPx =
                    resources.getDimensionPixelSize(
                            R.dimen.location_bar_tablet_fusebox_popup_inset);
            parentParams.topMargin = -expansionPx;
            setMarginsForWindowWidth(parentParams, expansionPx);
            parentParams.gravity = Gravity.TOP;
            setPadding(expansionPx, expansionPx, expansionPx, getPaddingBottom());
            mHolder.setTranslationZ(OVERLAY_Z_TRANSLATION);
            // Call super to avoid overwriting our locally saved reference to our OutlineProvider.
            // Null out the outline provider to avoid casting a shadow on views with translationZ
            // lower than ours.
            super.setOutlineProvider(null);
            ViewUtils.setAncestorsShouldClipToPadding(this, false, View.NO_ID);
            ViewUtils.setAncestorsShouldClipChildren(this, false, View.NO_ID);
            setBackground(mFocusedPopupDrawable);
        } else {
            parentParams.leftMargin = 0;
            parentParams.rightMargin = 0;
            parentParams.topMargin = 0;
            parentParams.height =
                    resources.getDimensionPixelSize(R.dimen.modern_toolbar_tablet_background_size);
            parentParams.gravity = Gravity.CENTER_VERTICAL;
            setPadding(0, 0, 0, getPaddingBottom());
            mHolder.setTranslationZ(NEUTRAL_Z_TRANSLATION);
            super.setOutlineProvider(mOutlineProvider);
            ViewUtils.setAncestorsShouldClipToPadding(this, true, View.NO_ID);
            ViewUtils.setAncestorsShouldClipChildren(this, true, View.NO_ID);
            // Put the focused background back into its starting state before swapping it out;
            // without this, it may still display the GLIF animation when we refocus.
            mGlifBorderDrawable.reset();
            mFocusedPopupDrawable.setDrawableByLayerId(R.id.glif_border_layer, null);
            // Reset our background to reflect non-zero suggestion count, which is the typical
            // state. Not setting this risks visual glitches when returning to the fusebox.
            setBackground(mUnfocusedDrawable);
        }
        adjustBackgroundForSuggestions();
        setLayoutParams(layoutParams);
        mHolder.setLayoutParams(parentParams);
    }

    private void adjustVerticalTranslationForFuseboxState(@FuseboxState int state) {
        MarginLayoutParams statusViewLayoutParams =
                (MarginLayoutParams) mStatusView.getLayoutParams();
        Resources resources = getResources();
        if (state == FuseboxState.COMPACT) {
            // In the compact fusebox state, the location bar is taller than its inner background,
            // creating the appearance of vertical misalignment. We resolve this by translating
            // constituent views to be centered withing the 56 dp inner background, shifting them
            // either 4dp up or down.
            int translationY =
                    resources.getDimensionPixelSize(R.dimen.fusebox_url_bar_translation_y);
            // Url bar and delete button are positioned too high relative to inner background.
            mUrlBar.setTranslationY(translationY);
            mDeleteButton.setTranslationY(translationY);
            // Bottom stacked buttons are positioned too low relative to inner background; use a
            // negative translation.
            setTranslationYOfBottomStackedUrlActionButtons(-translationY);

            // For the LocationBar to have the correct height in COMPACT mode for the two-tone
            // background to look correct (exactly 68dp), we inflate the allocated height of the
            // StatusView by 8dp using top margin. The translation compensates to keep the
            // StatusView vertically centered. This is not very clean and should be resolved by the
            // unified popover.
            statusViewLayoutParams.topMargin =
                    resources
                            .getDimensionPixelSize(R.dimen.fusebox_compact_status_view_top_margin);
            mStatusView.setTranslationY(-translationY);
        } else {
            mUrlBar.setTranslationY(0);
            mDeleteButton.setTranslationY(0);
            statusViewLayoutParams.topMargin = 0;
            mStatusView.setTranslationY(0);
            setTranslationYOfBottomStackedUrlActionButtons(0);
        }
        mStatusView.setLayoutParams(statusViewLayoutParams);
    }

    private void setMarginsForWindowWidth(
            MarginLayoutParams layoutParams, int minHorizontalExpansionPx) {
        Resources resources = getResources();
        int screenWidthDp = resources.getConfiguration().screenWidthDp;
        int windowWidthPx = DisplayUtil.dpToPx(mWindowAndroid.getDisplay(), screenWidthDp);
        int measuredWidthWithoutExpansion =
                getMeasuredWidth()
                        + Math.min(0, layoutParams.leftMargin)
                        + Math.min(0, layoutParams.rightMargin);
        int minTabletWidthPx = resources.getDimensionPixelSize(R.dimen.fusebox_min_tablet_width);
        boolean isPhoneWidthScreen = screenWidthDp < DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP;
        int targetWidthPx =
                isPhoneWidthScreen
                        ? windowWidthPx
                        : Math.max(
                                minTabletWidthPx,
                                measuredWidthWithoutExpansion + 2 * minHorizontalExpansionPx);

        ViewUtils.getRelativeLayoutPosition(getRootView(), this, mPositionArray);
        int currentLeft = mPositionArray[0] - layoutParams.leftMargin;
        // Our view is relatively centered already; make it exactly centered when expanded.
        boolean isViewApproximatelyCentered =
                windowWidthPx - 2 * currentLeft <= minTabletWidthPx || isPhoneWidthScreen;
        if (isViewApproximatelyCentered) {
            int targetLeft = (windowWidthPx - targetWidthPx) / 2;
            int targetRight = targetLeft + targetWidthPx;

            int currentRight = currentLeft + measuredWidthWithoutExpansion;
            int shiftLeft = targetLeft - currentLeft;
            int shiftRight = targetRight - currentRight;

            layoutParams.leftMargin = shiftLeft;
            layoutParams.rightMargin = -shiftRight;
        } else {
            // Our view is relatively off-center. Leave it that way, expanding symmetrically from
            // our current position.
            int expansionPx = (targetWidthPx - measuredWidthWithoutExpansion) / 2;
            layoutParams.leftMargin = -expansionPx;
            layoutParams.rightMargin = -expansionPx;
        }
    }

    private void adjustBackgroundForSuggestions() {
        // In popover mode suggestions and the UrlBar live in the same parent container, so we can
        // use ordinary padding to keep the vertical separation between the two consistent instead
        // of manipulating the background like we do below.
        if (mLayoutMode == FuseboxLayoutMode.SUGGESTIONS_POPOVER) return;

        FrameLayout.LayoutParams layoutParams = (FrameLayout.LayoutParams) getLayoutParams();

        boolean suggestionsListScrolledDown =
                mSuggestionsListScrollOffset > mLocationBarTabletFuseboxPopupInset;
        boolean bleedIntoDropdown =
                mFuseboxState == FuseboxState.DISABLED
                        || (mHasSuggestions && !suggestionsListScrolledDown);

        int bottomInset = bleedIntoDropdown ? 0 : mLocationBarTabletFuseboxPopupInset;
        boolean roundBottomCorners = !bleedIntoDropdown && !suggestionsListScrolledDown;
        float bottomRadius = roundBottomCorners ? mOmniboxSuggestionDropdownRoundCornerRadius : 0f;

        layoutParams.bottomMargin = -bottomInset;
        mOuterRect.setCornerRadii(
                new float[] {
                    mOmniboxSuggestionDropdownRoundCornerRadius,
                            mOmniboxSuggestionDropdownRoundCornerRadius,
                            mOmniboxSuggestionDropdownRoundCornerRadius,
                            mOmniboxSuggestionDropdownRoundCornerRadius, // Top corners
                    bottomRadius, bottomRadius, bottomRadius, bottomRadius // Bottom corners
                });
        mFocusedPopupDrawable.setLayerInsetRelative(
                1,
                mLocationBarTabletFuseboxPopupInset,
                mLocationBarTabletFuseboxPopupInset,
                mLocationBarTabletFuseboxPopupInset,
                bottomInset);
        mFocusedPopupDrawable.setLayerInsetRelative(
                mFocusedPopupDrawable.findIndexByLayerId(R.id.glif_border_layer),
                mLocationBarTabletFuseboxPopupInset,
                mLocationBarTabletFuseboxPopupInset,
                mLocationBarTabletFuseboxPopupInset,
                bottomInset);
        mFocusedPopupDrawable.invalidateSelf();
        setPadding(getPaddingLeft(), getPaddingTop(), getPaddingRight(), bottomInset);
        setLayoutParams(layoutParams);

        GradientDrawable hoverRect = (GradientDrawable) mHoverDrawable.getDrawable(0);
        if (mFuseboxState == FuseboxState.DISABLED) {
            mHoverDrawable.setLayerInsetRelative(
                    0,
                    0,
                    mModernToolbarBackgroundVerticalOffset,
                    0,
                    mModernToolbarBackgroundVerticalOffset);
            hoverRect.setCornerRadius(mModernToolbarBackgroundCornerRadius);
        } else {
            mHoverDrawable.setLayerInsetRelative(
                    0,
                    mLocationBarTabletFuseboxPopupInset,
                    mLocationBarTabletFuseboxPopupInset,
                    mLocationBarTabletFuseboxPopupInset,
                    bottomInset);
            hoverRect.setCornerRadius(mModernToolbarBackgroundInnerCornerRadius);
        }
        mHoverDrawable.invalidateSelf();
    }

    private void setTranslationYOfBottomStackedUrlActionButtons(float translationY) {
        mMicButton.setTranslationY(translationY);
        mLensButton.setTranslationY(translationY);
    }

    LayerDrawable getHoverDrawableForTesting() {
        return mHoverDrawable;
    }
}
