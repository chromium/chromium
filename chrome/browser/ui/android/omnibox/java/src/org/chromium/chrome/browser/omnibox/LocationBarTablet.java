// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Rect;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.ui.base.LocalizationUtils;

/** Location bar for tablet form factors. */
class LocationBarTablet extends LocationBarLayout {
    // The number of toolbar buttons that can be hidden at small widths (reload, back, forward).
    private static final int HIDEABLE_BUTTON_COUNT = 3;

    private View mLocationBarIcon;
    private View mBookmarkButton;
    private View mSaveOfflineButton;
    private View[] mTargets;
    private final Rect mCachedTargetBounds = new Rect();

    // Variables needed for animating the location bar and toolbar buttons hiding/showing.
    private final int mToolbarButtonsWidth;
    private final int mMicButtonWidth;
    private final int mLensButtonWidth;
    private boolean mAnimatingWidthChange;
    private float mWidthChangeFraction;
    private float mLayoutLeft;
    private float mLayoutRight;
    private int mToolbarStartPaddingDifference;
    private UrlBar mUrlBar;

    /** Constructor used to inflate from XML. */
    public LocationBarTablet(Context context, AttributeSet attrs) {
        super(context, attrs);

        mToolbarButtonsWidth =
                getResources().getDimensionPixelOffset(R.dimen.toolbar_button_width)
                        * HIDEABLE_BUTTON_COUNT;
        int locationBarIconWidth =
                getResources().getDimensionPixelOffset(R.dimen.location_bar_icon_width);
        mMicButtonWidth = locationBarIconWidth;
        mLensButtonWidth = locationBarIconWidth;
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mLocationBarIcon = findViewById(R.id.location_bar_status_icon);
        mBookmarkButton = findViewById(R.id.bookmark_button);
        mSaveOfflineButton = findViewById(R.id.save_offline_button);
        mUrlBar = findViewById(R.id.url_bar);

        mUrlBar.setOnHoverListener(
                new View.OnHoverListener() {
                    @Override
                    public boolean onHover(View v, MotionEvent event) {
                        switch (event.getAction()) {
                            case MotionEvent.ACTION_HOVER_ENTER:
                                setForeground(
                                        AppCompatResources.getDrawable(
                                                getContext(),
                                                R.drawable
                                                        .modern_toolbar_text_box_background_highlight));
                                return true;
                            case MotionEvent.ACTION_HOVER_EXIT:
                                setForeground(null);
                                return true;
                            default:
                                return false;
                        }
                    }
                });

        mTargets = new View[] {mUrlBar, mDeleteButton};
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
        mMicButton.setTranslationX(0);
        mLensButton.setTranslationX(0);
        mDeleteButton.setTranslationX(0);
        mBookmarkButton.setTranslationX(0);
        mSaveOfflineButton.setTranslationX(0);
        mLocationBarIcon.setTranslationX(0);
        mUrlBar.setTranslationX(0);

        mMicButton.setAlpha(1.f);
        mLensButton.setAlpha(1.f);
        mDeleteButton.setAlpha(1.f);
        mBookmarkButton.setAlpha(1.f);
        mSaveOfflineButton.setAlpha(1.f);
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
            if (mSaveOfflineButton.getVisibility() == View.VISIBLE) {
                mSaveOfflineButton.setTranslationX(offset);
            } else {
                mMicButton.setTranslationX(offset);
            }

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
        mBookmarkButton.setVisibility(showBookmarkButton ? View.VISIBLE : View.GONE);
    }

    /* package */ void setSaveOfflineButtonVisibility(
            boolean showSaveOfflineButton, boolean isSaveOfflineButtonEnabled) {
        mSaveOfflineButton.setVisibility(showSaveOfflineButton ? View.VISIBLE : View.GONE);
        if (showSaveOfflineButton) mSaveOfflineButton.setEnabled(isSaveOfflineButtonEnabled);
    }

    /* package */ boolean isSaveOfflineButtonVisible() {
        return mSaveOfflineButton.getVisibility() == VISIBLE;
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
     * Gets the save offline button view for the purposes of creating an animator that targets it.
     * Don't use this for any other reason, e.g. to access or modify the view's properties directly.
     */
    @Deprecated
    /* package */ View getSaveOfflineButtonForAnimation() {
        return mSaveOfflineButton;
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
}
