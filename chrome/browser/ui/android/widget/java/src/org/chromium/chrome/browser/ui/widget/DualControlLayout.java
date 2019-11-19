// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.widget;

import android.content.Context;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;

import androidx.annotation.IntDef;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.ui.widget.ButtonCompat;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Automatically lays out one or two Views, placing them on the same row if possible and stacking
 * them otherwise.
 *
 * Use cases of this Layout include placement of infobar buttons and placement of TextViews inside
 * of spinner controls (http://goto.google.com/infobar-spec).
 *
 * Layout parameters (i.e. margins) are ignored to enforce consistency.  Alignment defines where the
 * controls are placed (for RTL, flip everything):
 *
 * ALIGN_START                      ALIGN_APART                      ALIGN_END
 * -----------------------------    -----------------------------    -----------------------------
 * | PRIMARY SECONDARY         |    | SECONDARY         PRIMARY |    |         SECONDARY PRIMARY |
 * -----------------------------    -----------------------------    -----------------------------
 *
 * Controls are stacked automatically when they don't fit on the same row, with each control taking
 * up the full available width and with the primary control sitting on top of the secondary.
 * -----------------------------
 * | PRIMARY------------------ |
 * | SECONDARY---------------- |
 * -----------------------------
 */
public final class DualControlLayout extends ViewGroup {
    // When changing these values, you need to update ui/android/java/res/values/attrs.xml
    @IntDef({DualControlLayoutAlignment.START, DualControlLayoutAlignment.END,
            DualControlLayoutAlignment.APART})
    @Retention(RetentionPolicy.SOURCE)
    public @interface DualControlLayoutAlignment {
        int START = 0;
        int END = 1;
        int APART = 2;
    }

    /**
     * Creates a standardized Button that can be used for DualControlLayouts showing buttons.
     *
     * @param isPrimary Whether or not the button is meant to act as a "Confirm" button.
     * @param text      Text to display on the button.
     * @param listener  Listener to alert when the button has been clicked.
     * @return Button that can be used in the view.
     */
    public static Button createButtonForLayout(
            Context context, boolean isPrimary, String text, OnClickListener listener) {
        if (isPrimary) {
            ButtonCompat primaryButton =
                    new ButtonCompat(context, R.style.FilledButtonThemeOverlay_Flat);
            primaryButton.setId(R.id.button_primary);
            primaryButton.setOnClickListener(listener);
            primaryButton.setText(text);
            return primaryButton;
        } else {
            Button secondaryButton = new ButtonCompat(context, R.style.TextButtonThemeOverlay);
            secondaryButton.setId(R.id.button_secondary);
            secondaryButton.setOnClickListener(listener);
            secondaryButton.setText(text);
            ApiCompatibilityUtils.setTextAppearance(
                    secondaryButton, R.style.TextAppearance_BlueButtonText2);
            return secondaryButton;
        }
    }

    private final int mHorizontalMarginBetweenViews;

    /** Define how the controls will be laid out. */
    @DualControlLayoutAlignment
    private int mAlignment = DualControlLayoutAlignment.START;

    /** Margin between the controls when they're stacked.  By default, there is no margin. */
    private int mStackedMargin;

    private boolean mIsStacked;
    private View mPrimaryView;
    private View mSecondaryView;

    /**
     * Construct a new DualControlLayout.
     *
     * See {@link ViewGroup} for parameter details.  attrs may be null if constructed dynamically.
     */
    public DualControlLayout(Context context, AttributeSet attrs) {
        super(context, attrs);

        // Cache dimensions.
        Resources resources = getContext().getResources();
        mHorizontalMarginBetweenViews =
                resources.getDimensionPixelSize(R.dimen.dual_control_margin_between_items);

        if (attrs != null) parseAttributes(attrs);
    }

    /**
     * Define how the controls will be laid out.
     *
     * @param alignment One of ALIGN_START, ALIGN_APART, ALIGN_END.
     */
    public void setAlignment(@DualControlLayoutAlignment int alignment) {
        mAlignment = alignment;
    }

    /** See {@link #mAlignment}. */
    @DualControlLayoutAlignment
    public int getAlignment() {
        return mAlignment;
    }

    /** See {@link #mStackedMargin}. */
    public void setStackedMargin(int stackedMargin) {
        mStackedMargin = stackedMargin;
    }

    /** See {@link #mStackedMargin}. */
    public int getStackedMargin() {
        return mStackedMargin;
    }

    @Override
    public void onViewAdded(View child) {
        super.onViewAdded(child);

        if (mPrimaryView == null) {
            mPrimaryView = child;
        } else if (mSecondaryView == null) {
            mSecondaryView = child;
        } else {
            throw new IllegalStateException("Too many children added to DualControlLayout");
        }
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        mIsStacked = false;
        int sidePadding = getPaddingLeft() + getPaddingRight();
        int verticalPadding = getPaddingTop() + getPaddingBottom();

        // Measure the primary View, allowing it to be as wide as the Layout.
        int maxWidth = MeasureSpec.getMode(widthMeasureSpec) == MeasureSpec.UNSPECIFIED
                ? Integer.MAX_VALUE
                : (MeasureSpec.getSize(widthMeasureSpec) - sidePadding);
        int unspecifiedSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        measureChild(mPrimaryView, unspecifiedSpec, unspecifiedSpec);

        int layoutWidth = mPrimaryView.getMeasuredWidth();
        int layoutHeight = mPrimaryView.getMeasuredHeight();

        if (mSecondaryView != null) {
            // Measure the secondary View, allowing it to be as wide as the layout.
            measureChild(mSecondaryView, unspecifiedSpec, unspecifiedSpec);
            int combinedWidth = mPrimaryView.getMeasuredWidth() + mSecondaryView.getMeasuredWidth();
            if (mPrimaryView.getMeasuredWidth() > 0 && mSecondaryView.getMeasuredWidth() > 0) {
                combinedWidth += mHorizontalMarginBetweenViews;
            }

            if (combinedWidth > maxWidth) {
                // Stack the Views on top of each other.
                mIsStacked = true;

                int widthSpec = MeasureSpec.makeMeasureSpec(maxWidth, MeasureSpec.EXACTLY);
                mPrimaryView.measure(widthSpec, unspecifiedSpec);
                mSecondaryView.measure(widthSpec, unspecifiedSpec);

                layoutWidth = maxWidth;
                layoutHeight = mPrimaryView.getMeasuredHeight() + mStackedMargin
                        + mSecondaryView.getMeasuredHeight();
            } else {
                // The Views fit side by side.  Check which is taller to find the layout height.
                layoutWidth = combinedWidth;
                layoutHeight = Math.max(layoutHeight, mSecondaryView.getMeasuredHeight());
            }
        }
        layoutWidth += sidePadding;
        layoutHeight += verticalPadding;

        setMeasuredDimension(resolveSize(layoutWidth, widthMeasureSpec),
                resolveSize(layoutHeight, heightMeasureSpec));
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        int leftPadding = getPaddingLeft();
        int rightPadding = getPaddingRight();

        int width = right - left;
        boolean isRtl = getLayoutDirection() == LAYOUT_DIRECTION_RTL;
        boolean isPrimaryOnRight = (isRtl && mAlignment == DualControlLayoutAlignment.START)
                || (!isRtl
                        && (mAlignment == DualControlLayoutAlignment.APART
                                || mAlignment == DualControlLayoutAlignment.END));

        int primaryRight = isPrimaryOnRight ? (width - rightPadding)
                                            : (mPrimaryView.getMeasuredWidth() + leftPadding);
        int primaryLeft = primaryRight - mPrimaryView.getMeasuredWidth();
        int primaryTop = getPaddingTop();
        int primaryBottom = primaryTop + mPrimaryView.getMeasuredHeight();
        mPrimaryView.layout(primaryLeft, primaryTop, primaryRight, primaryBottom);

        if (mIsStacked) {
            // Fill out the row.  onMeasure() should have already applied the correct width.
            int secondaryTop = primaryBottom + mStackedMargin;
            int secondaryBottom = secondaryTop + mSecondaryView.getMeasuredHeight();
            mSecondaryView.layout(leftPadding, secondaryTop,
                    leftPadding + mSecondaryView.getMeasuredWidth(), secondaryBottom);
        } else if (mSecondaryView != null) {
            // Center the secondary View vertically with the primary View.
            int secondaryHeight = mSecondaryView.getMeasuredHeight();
            int primaryCenter = (primaryTop + primaryBottom) / 2;
            int secondaryTop = primaryCenter - (secondaryHeight / 2);
            int secondaryBottom = secondaryTop + secondaryHeight;

            // Determine where to place the secondary View.
            int secondaryLeft;
            int secondaryRight;
            if (mAlignment == DualControlLayoutAlignment.APART) {
                // Put the second View on the other side of the Layout from the primary View.
                secondaryLeft = isPrimaryOnRight
                        ? leftPadding
                        : width - rightPadding - mSecondaryView.getMeasuredWidth();
                secondaryRight = secondaryLeft + mSecondaryView.getMeasuredWidth();
            } else if (isPrimaryOnRight) {
                // Sit to the left of the primary View.
                secondaryRight = primaryLeft;
                if (mPrimaryView.getMeasuredWidth() > 0) {
                    secondaryRight -= mHorizontalMarginBetweenViews;
                }
                secondaryLeft = secondaryRight - mSecondaryView.getMeasuredWidth();
            } else {
                // Sit to the right of the primary View.
                secondaryLeft = primaryRight;
                if (mPrimaryView.getMeasuredWidth() > 0) {
                    secondaryLeft += mHorizontalMarginBetweenViews;
                }
                secondaryRight = secondaryLeft + mSecondaryView.getMeasuredWidth();
            }

            mSecondaryView.layout(secondaryLeft, secondaryTop, secondaryRight, secondaryBottom);
        }
    }

    private void parseAttributes(AttributeSet attrs) {
        TypedArray a =
                getContext().obtainStyledAttributes(attrs, R.styleable.DualControlLayout, 0, 0);

        // Set the stacked margin.
        if (a.hasValue(R.styleable.DualControlLayout_stackedMargin)) {
            setStackedMargin(
                    a.getDimensionPixelSize(R.styleable.DualControlLayout_stackedMargin, 0));
        }

        // Create the primary button, if necessary.
        String primaryButtonText = null;
        if (a.hasValue(R.styleable.DualControlLayout_primaryButtonText)) {
            primaryButtonText = a.getString(R.styleable.DualControlLayout_primaryButtonText);
        }
        if (!TextUtils.isEmpty(primaryButtonText)) {
            addView(createButtonForLayout(getContext(), true, primaryButtonText, null));
        }

        // Build the secondary button, but only if there's a primary button set.
        String secondaryButtonText = null;
        if (a.hasValue(R.styleable.DualControlLayout_secondaryButtonText)) {
            secondaryButtonText = a.getString(R.styleable.DualControlLayout_secondaryButtonText);
        }
        if (!TextUtils.isEmpty(primaryButtonText) && !TextUtils.isEmpty(secondaryButtonText)) {
            addView(createButtonForLayout(getContext(), false, secondaryButtonText, null));
        }

        // Set the alignment.
        if (a.hasValue(R.styleable.DualControlLayout_buttonAlignment)) {
            setAlignment(a.getInt(R.styleable.DualControlLayout_buttonAlignment,
                    DualControlLayoutAlignment.START));
        }

        a.recycle();
    }
}
