// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.browser_ui.widget.chips;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.TypedArray;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.ContextThemeWrapper;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.AttrRes;
import androidx.annotation.ColorInt;
import androidx.annotation.DrawableRes;
import androidx.annotation.Px;
import androidx.annotation.StyleRes;
import androidx.appcompat.widget.AppCompatTextView;
import androidx.core.widget.ImageViewCompat;

import org.chromium.components.browser_ui.widget.R;
import org.chromium.ui.widget.ChromeImageView;
import org.chromium.ui.widget.LoadingView;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.RippleBackgroundHelper;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * The view responsible for displaying a material chip. The chip has the following components:
 *
 * <ul>
 *   <li>A primary text to be shown.
 *   <li>An optional start icon that can be rounded as well.
 *   <li>An optional secondary text view that is shown to the right of the primary text view.
 *   <li>An optional remove icon at the end, intended for use with input chips.
 *   <li>An optional boolean (solidColorChip) to remove the default chip border.
 *   <li>An optional boolean (allowMultipleLines) to avoid longer text strings to wrap to a second
 *       line.
 *   <li>An optional boolean (showLoadingView) to show a loading view in place of the start icon.
 * </ul>
 */
public class ChipView extends LinearLayout {
    /** An id to use for {@link #setIcon(int, boolean)} when there is no icon on the chip. */
    public static final int INVALID_ICON_ID = -1;

    private static final int MAX_LINES = 2;

    private final RippleBackgroundHelper mRippleBackgroundHelper;
    private final AppCompatTextView mPrimaryText;
    private final ChromeImageView mStartIcon;
    private final boolean mUseRoundedStartIcon;
    private final LoadingView mLoadingView;
    private final @StyleRes int mSecondaryTextAppearanceId;
    private final int mEndIconWidth;
    private final int mEndIconHeight;
    private final int mEndIconStartPadding;
    private final int mEndIconEndPadding;
    private final int mCornerRadius;

    private ViewGroup mEndIconWrapper;
    private AppCompatTextView mSecondaryText;
    private int mMaxWidth = Integer.MAX_VALUE;

    /** Constructor for applying a theme overlay. */
    public ChipView(Context context, @StyleRes int themeOverlay) {
        this(new ContextThemeWrapper(context, themeOverlay), null, R.attr.chipStyle, 0);
    }

    /** Constructor for inflating from XML. */
    public ChipView(Context context, AttributeSet attrs) {
        this(
                new ContextThemeWrapper(context, R.style.SuggestionChipThemeOverlay),
                attrs,
                R.attr.chipStyle,
                0);
    }

    /** Constructor for base classes and programmatic creation. */
    public ChipView(
            Context context,
            AttributeSet attrs,
            @AttrRes int defStyleAttr,
            @StyleRes int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);

        TypedArray a =
                getContext()
                        .obtainStyledAttributes(
                                attrs, R.styleable.ChipView, defStyleAttr, defStyleRes);

        boolean extendLateralPadding =
                a.getBoolean(R.styleable.ChipView_extendLateralPadding, false);
        boolean reduceEndPadding = a.getBoolean(R.styleable.ChipView_reduceEndPadding, false);

        @Px
        int leadingElementPadding =
                extendLateralPadding
                        ? getResources()
                                .getDimensionPixelSize(
                                        R.dimen.chip_element_extended_leading_padding)
                        : getResources()
                                .getDimensionPixelSize(R.dimen.chip_element_leading_padding);

        // End padding is already longer so no need to adjust in the 'extendLateralPadding' case.
        @Px
        int endPadding =
                reduceEndPadding
                        ? getResources().getDimensionPixelSize(R.dimen.chip_reduced_end_padding)
                        : getResources().getDimensionPixelSize(R.dimen.chip_end_padding);

        mEndIconStartPadding =
                extendLateralPadding
                        ? getResources()
                                .getDimensionPixelSize(R.dimen.chip_end_icon_extended_margin_start)
                        : getResources().getDimensionPixelSize(R.dimen.chip_end_icon_margin_start);

        mEndIconEndPadding =
                extendLateralPadding
                        ? getResources()
                                .getDimensionPixelSize(
                                        R.dimen.chip_extended_end_padding_with_end_icon)
                        : getResources()
                                .getDimensionPixelSize(R.dimen.chip_end_padding_with_end_icon);

        boolean solidColorChip = a.getBoolean(R.styleable.ChipView_solidColorChip, false);
        int chipBorderWidthId =
                solidColorChip ? R.dimen.chip_solid_border_width : R.dimen.chip_border_width;
        int chipColorId =
                a.getResourceId(R.styleable.ChipView_chipColor, R.color.chip_background_color);
        int chipStateLayerColorId =
                a.getResourceId(
                        R.styleable.ChipView_chipStateLayerColor, R.color.chip_state_layer_color);
        int rippleColorId =
                a.getResourceId(R.styleable.ChipView_rippleColor, R.color.chip_ripple_color);
        int chipStrokeColorId =
                a.getResourceId(R.styleable.ChipView_chipStrokeColor, R.color.chip_stroke_color);
        mCornerRadius =
                a.getDimensionPixelSize(
                        R.styleable.ChipView_cornerRadius,
                        getContext()
                                .getResources()
                                .getDimensionPixelSize(R.dimen.chip_corner_radius));
        int iconWidth =
                a.getDimensionPixelSize(
                        R.styleable.ChipView_iconWidth,
                        getResources().getDimensionPixelSize(R.dimen.chip_icon_size));
        int iconHeight =
                a.getDimensionPixelSize(
                        R.styleable.ChipView_iconHeight,
                        getResources().getDimensionPixelSize(R.dimen.chip_icon_size));
        mUseRoundedStartIcon = a.getBoolean(R.styleable.ChipView_useRoundedIcon, false);
        int primaryTextAppearance =
                a.getResourceId(
                        R.styleable.ChipView_primaryTextAppearance,
                        R.style.TextAppearance_ChipText);

        mEndIconWidth =
                a.getDimensionPixelSize(
                        R.styleable.ChipView_endIconWidth,
                        getResources().getDimensionPixelSize(R.dimen.chip_icon_size));
        mEndIconHeight =
                a.getDimensionPixelSize(
                        R.styleable.ChipView_endIconHeight,
                        getResources().getDimensionPixelSize(R.dimen.chip_icon_size));
        mSecondaryTextAppearanceId =
                a.getResourceId(
                        R.styleable.ChipView_secondaryTextAppearance,
                        R.style.TextAppearance_ChipText);
        int verticalInset =
                a.getDimensionPixelSize(
                        R.styleable.ChipView_verticalInset,
                        getResources().getDimensionPixelSize(R.dimen.chip_bg_vertical_inset));
        boolean allowMultipleLines = a.getBoolean(R.styleable.ChipView_allowMultipleLines, false);
        int minMultilineVerticalTextPadding =
                a.getDimensionPixelSize(
                        R.styleable.ChipView_multiLineVerticalPadding,
                        getResources()
                                .getDimensionPixelSize(
                                        R.dimen.chip_text_multiline_vertical_padding));
        boolean textAlignStart = a.getBoolean(R.styleable.ChipView_textAlignStart, false);
        boolean reduceTextStartPadding =
                a.getBoolean(R.styleable.ChipView_reduceTextStartPadding, false);
        a.recycle();

        mStartIcon = new ChromeImageView(getContext());
        mStartIcon.setLayoutParams(new LayoutParams(iconWidth, iconHeight));
        addView(mStartIcon);

        if (mUseRoundedStartIcon) {
            int chipHeight = getResources().getDimensionPixelOffset(R.dimen.chip_default_height);
            leadingElementPadding = (chipHeight - iconHeight) / 2;
        }

        int loadingViewSize = getResources().getDimensionPixelSize(R.dimen.chip_loading_view_size);
        int loadingViewHeightPadding = (iconHeight - loadingViewSize) / 2;
        int loadingViewWidthPadding = (iconWidth - loadingViewSize) / 2;
        mLoadingView = new LoadingView(getContext());
        mLoadingView.setVisibility(GONE);
        mLoadingView.setIndeterminateTintList(
                ColorStateList.valueOf(
                        getContext().getColor(R.color.default_icon_color_accent1_baseline)));
        mLoadingView.setPaddingRelative(
                loadingViewWidthPadding,
                loadingViewHeightPadding,
                loadingViewWidthPadding,
                loadingViewHeightPadding);
        addView(mLoadingView, new LayoutParams(iconWidth, iconHeight));

        // Setting this enforces 16dp padding at the end and 8dp at the start (unless overridden).
        // For text, the start padding needs to be 16dp which is why a ChipTextView contributes the
        // remaining 8dp.
        this.setPaddingRelative(leadingElementPadding, 0, endPadding, 0);

        mPrimaryText =
                new AppCompatTextView(new ContextThemeWrapper(getContext(), R.style.ChipTextView));
        mPrimaryText.setTextAppearance(primaryTextAppearance);

        // If false fall back to single line defined in XML styles.
        if (allowMultipleLines) {
            mPrimaryText.setMaxLines(MAX_LINES);
            // TODO(benwgold): Test for non multiline chips to see if 4dp vertical padding can be
            // safely applied to all chips without affecting styling.
            mPrimaryText.setPaddingRelative(
                    mPrimaryText.getPaddingStart(),
                    minMultilineVerticalTextPadding,
                    mPrimaryText.getPaddingEnd(),
                    minMultilineVerticalTextPadding);
        }
        if (textAlignStart) {
            // Default of 'center' is defined in the ChipTextView style.
            mPrimaryText.setTextAlignment(View.TEXT_ALIGNMENT_VIEW_START);
        }
        if (reduceTextStartPadding) {
            mPrimaryText.setPaddingRelative(
                    getResources().getDimensionPixelSize(R.dimen.chip_text_reduced_leading_padding),
                    mPrimaryText.getPaddingTop(),
                    mPrimaryText.getPaddingEnd(),
                    mPrimaryText.getPaddingBottom());
        }
        addView(mPrimaryText);

        // Reset icon and background:
        mRippleBackgroundHelper =
                new RippleBackgroundHelper(
                        this,
                        chipColorId,
                        chipStateLayerColorId,
                        rippleColorId,
                        mCornerRadius,
                        chipStrokeColorId,
                        chipBorderWidthId,
                        verticalInset);
        setIcon(INVALID_ICON_ID, false);
    }

    /**
     * Unlike setSelected, setEnabled doesn't properly propagate the new state to its subcomponents.
     * Enforce this so ColorStateLists used for the text appearance apply as intended.
     *
     * @param enabled The new enabled state for the chip view and the TextViews owned by it.
     */
    @Override
    public void setEnabled(boolean enabled) {
        super.setEnabled(enabled);
        getPrimaryTextView().setEnabled(enabled);
        mStartIcon.setEnabled(enabled);
        if (mSecondaryText != null) mSecondaryText.setEnabled(enabled);
    }

    /**
     * Sets the icon at the start of the chip view.
     *
     * @param icon The resource id pointing to the icon.
     */
    public void setIcon(@DrawableRes int icon, boolean tintWithTextColor) {
        if (icon == INVALID_ICON_ID) {
            mStartIcon.setVisibility(ViewGroup.GONE);
            return;
        }

        mStartIcon.setVisibility(ViewGroup.VISIBLE);
        mStartIcon.setImageResource(icon);
        setTint(tintWithTextColor);
    }

    /**
     * Sets the icon at the start of the chip view.
     *
     * @param drawable Drawable to display.
     */
    public void setIcon(Drawable drawable, boolean tintWithTextColor) {
        if (drawable == null) {
            mStartIcon.setVisibility(ViewGroup.GONE);
            return;
        }

        mStartIcon.setVisibility(ViewGroup.VISIBLE);
        mStartIcon.setImageDrawable(drawable);
        setTint(tintWithTextColor);
    }

    /**
     * Shows a {@link LoadingView} at the start of the chip view. This replaces the start icon.
     *
     * @param loadingViewObserver A {@link LoadingView.Observer} to add to the LoadingView.
     */
    public void showLoadingView(LoadingView.Observer loadingViewObserver) {
        mLoadingView.addObserver(
                new LoadingView.Observer() {
                    @Override
                    public void onShowLoadingUIComplete() {
                        mStartIcon.setVisibility(GONE);
                    }

                    @Override
                    public void onHideLoadingUIComplete() {
                        mStartIcon.setVisibility(VISIBLE);
                    }
                });
        mLoadingView.addObserver(loadingViewObserver);
        mLoadingView.showLoadingUI();
    }

    /**
     * Hides the {@link LoadingView} at the start of the chip view.
     *
     * @param loadingViewObserver A {@link LoadingView.Observer} to add to the LoadingView.
     */
    public void hideLoadingView(LoadingView.Observer loadingViewObserver) {
        mLoadingView.addObserver(loadingViewObserver);
        mLoadingView.hideLoadingUI();
    }

    /** Adds a remove icon (X button) at the trailing end of the chip next to the primary text. */
    public void addRemoveIcon() {
        if (mEndIconWrapper != null) return;

        ChromeImageView endIcon = new ChromeImageView(getContext());
        endIcon.setImageResource(R.drawable.btn_close);
        ImageViewCompat.setImageTintList(endIcon, mPrimaryText.getTextColors());

        // Adding a wrapper view around the X icon to make the touch target larger, which would
        // cover the start and end margin for the X icon, and full height of the chip.
        mEndIconWrapper = new FrameLayout(getContext());
        mEndIconWrapper.setId(R.id.chip_cancel_btn);

        FrameLayout.LayoutParams layoutParams =
                new FrameLayout.LayoutParams(mEndIconWidth, mEndIconHeight);
        layoutParams.setMarginStart(mEndIconStartPadding);
        layoutParams.setMarginEnd(mEndIconEndPadding);
        layoutParams.gravity = Gravity.CENTER_VERTICAL;
        mEndIconWrapper.addView(endIcon, layoutParams);
        addView(
                mEndIconWrapper,
                new LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.MATCH_PARENT));

        // Remove the end padding from the chip to make X icon touch target extend till the end of
        // the chip.
        this.setPaddingRelative(getPaddingStart(), getPaddingTop(), 0, getPaddingBottom());
    }

    /** Adds a dropdown icon at the trailing end of the chip next to the primary text. */
    public void addDropdownIcon() {
        if (mEndIconWrapper != null) return;

        ChromeImageView endIcon = new ChromeImageView(getContext());
        endIcon.setImageResource(R.drawable.mtrl_dropdown_arrow);
        ImageViewCompat.setImageTintList(endIcon, mPrimaryText.getTextColors());

        mEndIconWrapper = new FrameLayout(getContext());

        FrameLayout.LayoutParams layoutParams =
                new FrameLayout.LayoutParams(mEndIconWidth, mEndIconHeight);
        layoutParams.setMarginStart(mEndIconStartPadding);
        layoutParams.setMarginEnd(mEndIconEndPadding);
        layoutParams.gravity = Gravity.CENTER_VERTICAL;
        mEndIconWrapper.addView(endIcon, layoutParams);
        addView(
                mEndIconWrapper,
                new LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.MATCH_PARENT));

        // Remove the end padding from the chip to make X icon touch target extend till the end of
        // the chip.
        this.setPaddingRelative(getPaddingStart(), getPaddingTop(), 0, getPaddingBottom());
    }

    /**
     * Sets a {@link android.view.View.OnClickListener} for the remove icon. {@link
     * ChipView#addRemoveIcon()} must be called prior to this method.
     *
     * @param listener The listener to be invoked on click events.
     */
    public void setRemoveIconClickListener(OnClickListener listener) {
        mEndIconWrapper.setOnClickListener(listener);
        String chipText = mPrimaryText.getText().toString();
        assert !TextUtils.isEmpty(chipText);
        mEndIconWrapper.setContentDescription(
                mPrimaryText
                        .getContext()
                        .getString(R.string.chip_remove_icon_content_description, chipText));
    }

    /**
     * Returns the {@link TextView} that contains the label of the chip.
     *
     * @return A {@link TextView}.
     */
    public TextView getPrimaryTextView() {
        return mPrimaryText;
    }

    /**
     * Returns the {@link TextView} that contains the secondary label of the chip. If it wasn't used
     * until now, this creates the view.
     *
     * @return A {@link TextView}.
     */
    public TextView getSecondaryTextView() {
        if (mSecondaryText == null) {
            mSecondaryText =
                    new AppCompatTextView(
                            new ContextThemeWrapper(getContext(), R.style.ChipTextView));
            mSecondaryText.setTextAppearance(mSecondaryTextAppearanceId);
            // Ensure that basic state changes are aligned with the ChipView. They update
            // automatically once the view is part of the hierarchy.
            mSecondaryText.setSelected(isSelected());
            mSecondaryText.setEnabled(isEnabled());
            addView(mSecondaryText);
        }
        return mSecondaryText;
    }

    /**
     * Returns the {@link RectProvider} that contains the start icon for the chip view.
     *
     * @return A {@link RectProvider}
     */
    public RectProvider getStartIconViewRect() {
        return new ViewRectProvider(mStartIcon);
    }

    /**
     * Sets the correct tinting on the Chip's image view.
     *
     * @param tintWithTextColor If true then the image view will be tinted with the primary text
     *     color. If not, the tint will be cleared.
     */
    private void setTint(boolean tintWithTextColor) {
        if (mPrimaryText.getTextColors() != null && tintWithTextColor) {
            ImageViewCompat.setImageTintList(mStartIcon, mPrimaryText.getTextColors());
        } else {
            ImageViewCompat.setImageTintList(mStartIcon, null);
        }
    }

    /**
     * Sets border around the chip. If width is zero, then no border is drawn.
     *
     * @param width of the border in pixels.
     * @param color of the border.
     */
    public void setBorder(int width, @ColorInt int color) {
        mRippleBackgroundHelper.setBorder(width, color);
    }

    @Override
    public void setBackgroundColor(@ColorInt int color) {
        mRippleBackgroundHelper.setBackgroundColor(color);
    }

    @Override
    public void setBackgroundTintList(ColorStateList color) {
        mRippleBackgroundHelper.setBackgroundColor(color);
    }

    /** @return The corner radius in pixels of this ChipView. */
    public @Px int getCornerRadius() {
        return mCornerRadius;
    }

    /**
     * TODO (crbug.com/1376691): Set a constant minimum width for the chips. The chips must always
     * display some text. Sets the maximum width of the chip. This is achieved by resizing the
     * primary text view. The primary text is either truncated or completely removed depending on
     * the space available after all other chip contents are accounted for. After the primary text
     * gets removed, the secondary text is truncated. Note: This method can cause additional
     * measure/layout passes and could impact performance.
     *
     * @param maxWidth of the chip in px.
     */
    public void setMaxWidth(int maxWidth) {
        mMaxWidth = maxWidth;
    }

    /**
     * Another approach is to override the {@link LinearLayout#onLayout()} which doesn't require an
     * additional measure pass at the end. Performance wise they are comparable.
     */
    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        super.onMeasure(widthMeasureSpec, heightMeasureSpec);

        // If the chip width exceeds the maximum allowed size, resize the contents to respect the
        // width constraint.
        if (getMeasuredWidth() > mMaxWidth) {
            int newPrimaryTextWidth =
                    mMaxWidth
                            - getPaddingLeft()
                            - getPaddingRight()
                            - ((mStartIcon != null && mStartIcon.getVisibility() != GONE)
                                    ? mStartIcon.getMeasuredWidth()
                                    : 0)
                            - ((mSecondaryText != null && mSecondaryText.getVisibility() != GONE)
                                    ? mSecondaryText.getMeasuredWidth()
                                    : 0);
            // TODO (crbug.com/1376691): The primary text must be at least a few pixels wide, else
            // only the ellipses will be visible.
            // If there is space for displaying the {@link mPrimaryText}, adjust it's size, and add
            // trailing ellipses. If not, check if the secondary text exists. If it does, remove the
            // primary text, else do not width constrain the chip. The chip should ALWAYS display
            // some text.
            if (newPrimaryTextWidth > 0) {
                mPrimaryText.setMaxWidth(newPrimaryTextWidth);
                mPrimaryText.setEllipsize(TextUtils.TruncateAt.END);
            } else if (mSecondaryText != null && mSecondaryText.getVisibility() != GONE) {
                mPrimaryText.setVisibility(GONE);
            } else {
                return;
            }
            super.onMeasure(
                    MeasureSpec.makeMeasureSpec(mMaxWidth, MeasureSpec.EXACTLY), heightMeasureSpec);
        }
    }

    @Override
    public boolean isFocused() {
        // When the selection does not follow focus, we still want to properly reflect the user
        // selection by highlighting the chip.
        // An example where this happens is: the user interacts with the Omnibox, and the typed
        // query triggers an Action chip to be shown.
        // These chips can be navigated to using physical keyboard (arrow keys to select
        // corresponding suggestion, tab to activate the chip).
        // At this time the Omnibox continues to retain focus, but Chip should be highlighted, as
        // pressing <Enter> on the keyboard will activate the Chip.
        // Make sure the highlight is properly reflected.
        return super.isFocused() || isSelected();
    }
}
