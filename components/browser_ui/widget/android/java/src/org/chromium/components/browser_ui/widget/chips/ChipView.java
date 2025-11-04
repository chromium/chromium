// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.browser_ui.widget.chips;

import static org.chromium.build.NullUtil.assumeNonNull;

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
import androidx.appcompat.content.res.AppCompatResources;
import androidx.appcompat.widget.AppCompatTextView;
import androidx.core.graphics.drawable.DrawableCompat;
import androidx.core.widget.ImageViewCompat;

import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.widget.ChromeImageView;
import org.chromium.ui.widget.LoadingView;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.RippleBackgroundHelper;
import org.chromium.ui.widget.RippleBackgroundHelper.BorderType;
import org.chromium.ui.widget.ViewRectProvider;

/**
 * The view responsible for displaying a material chip. The chip has the following components:
 *
 * <ul>
 *   <li>A primary text to be shown.
 *   <li>An optional start icon that can be rounded as well.
 *   <li>An optional boolean (twoLineChip) that puts the secondary text view below the primary text
 *       view.
 *   <li>An optional secondary text view that is shown next to the primary text view.
 *   <li>An optional remove icon at the end, intended for use with input chips.
 *   <li>An optional boolean (solidColorChip) to remove the default chip border.
 *   <li>An optional boolean (allowMultipleLines) to avoid longer text strings to wrap to a second
 *       line.
 *   <li>An optional boolean (showLoadingView) to show a loading view in place of the start icon.
 * </ul>
 */
@NullMarked
public class ChipView extends LinearLayout {
    /** An id to use for {@link #setIcon(int, boolean)} when there is no icon on the chip. */
    public static final int INVALID_ICON_ID = -1;

    private static final int MAX_LINES = 2;

    private static final int HORIZONTAL_TEXT_ARANGEMENT = 0;
    private static final int VERTICAL_TEXT_ARANGEMENT = 1;

    private final RippleBackgroundHelper mRippleBackgroundHelper;
    private final AppCompatTextView mPrimaryText;
    private final ChromeImageView mStartIcon;
    private final boolean mUseRoundedStartIcon;
    private final LoadingView mLoadingView;
    private final @Px int mTextStartPadding;
    private final @StyleRes int mSecondaryTextAppearanceId;
    private final boolean mTextAlignStart;
    private final int mEndIconWidth;
    private final int mEndIconHeight;
    private final int mEndIconMarginStart;
    private final int mEndIconMarginEnd;
    private final int mCornerRadius;

    private @MonotonicNonNull ViewGroup mEndIconWrapper;
    private @MonotonicNonNull LinearLayout mTextViewsWrapper;
    private @MonotonicNonNull AppCompatTextView mSecondaryText;
    private @Px int mMaxWidth = Integer.MAX_VALUE;

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
            @Nullable AttributeSet attrs,
            @AttrRes int defStyleAttr,
            @StyleRes int defStyleRes) {
        super(context, attrs, defStyleAttr, defStyleRes);

        TypedArray a =
                getContext()
                        .obtainStyledAttributes(
                                attrs, R.styleable.ChipView, defStyleAttr, defStyleRes);

        @Px
        int chipStartPadding =
                a.getDimensionPixelSize(
                        R.styleable.ChipView_chipStartPadding,
                        getResources().getDimensionPixelSize(R.dimen.chip_view_start_padding));

        @Px
        int chipEndPadding =
                a.getDimensionPixelSize(
                        R.styleable.ChipView_chipEndPadding,
                        getResources().getDimensionPixelSize(R.dimen.chip_view_end_padding));

        mEndIconMarginStart =
                a.getDimensionPixelSize(
                        R.styleable.ChipView_endIconMarginStart,
                        getResources().getDimensionPixelSize(R.dimen.chip_end_icon_margin_start));

        mEndIconMarginEnd =
                a.getDimensionPixelSize(
                        R.styleable.ChipView_endIconMarginEnd,
                        getResources().getDimensionPixelSize(R.dimen.chip_end_icon_margin_end));

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
        final boolean alignTextVertically =
                a.getInteger(R.styleable.ChipView_textArrangement, HORIZONTAL_TEXT_ARANGEMENT)
                        == VERTICAL_TEXT_ARANGEMENT;
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
        mTextAlignStart = a.getBoolean(R.styleable.ChipView_textAlignStart, false);
        mTextStartPadding =
                a.getDimensionPixelSize(
                        R.styleable.ChipView_primaryTextStartPadding,
                        getResources()
                                .getDimensionPixelSize(R.dimen.chip_primary_text_start_padding));
        a.recycle();

        mStartIcon = new ChromeImageView(getContext());
        mStartIcon.setId(R.id.chip_view_start_icon);
        mStartIcon.setLayoutParams(new LinearLayout.LayoutParams(iconWidth, iconHeight));
        addView(mStartIcon);

        if (mUseRoundedStartIcon) {
            int chipHeight = getResources().getDimensionPixelOffset(R.dimen.chip_default_height);
            chipStartPadding = (chipHeight - iconHeight) / 2;
        }

        int loadingViewSize = getResources().getDimensionPixelSize(R.dimen.chip_loading_view_size);
        int loadingViewHeightPadding = (iconHeight - loadingViewSize) / 2;
        int loadingViewWidthPadding = (iconWidth - loadingViewSize) / 2;
        mLoadingView = new LoadingView(getContext());
        mLoadingView.setId(R.id.chip_view_loading_view);
        mLoadingView.setVisibility(GONE);
        mLoadingView.setIndeterminateTintList(
                ColorStateList.valueOf(
                        getContext().getColor(R.color.default_icon_color_accent1_baseline)));
        mLoadingView.setPaddingRelative(
                loadingViewWidthPadding,
                loadingViewHeightPadding,
                loadingViewWidthPadding,
                loadingViewHeightPadding);
        addView(mLoadingView, new LinearLayout.LayoutParams(iconWidth, iconHeight));

        // Setting this enforces 16dp padding at the end and 8dp at the start (unless overridden).
        // For text, the start padding needs to be 16dp which is why a ChipTextView contributes the
        // remaining 8dp.
        this.setPaddingRelative(chipStartPadding, 0, chipEndPadding, 0);

        mPrimaryText =
                new AppCompatTextView(new ContextThemeWrapper(getContext(), R.style.ChipTextView));
        mPrimaryText.setId(R.id.chip_view_primary_text);
        mPrimaryText.setTextAppearance(primaryTextAppearance);
        // Reduce font padding if the text is aligned vertically.
        mPrimaryText.setIncludeFontPadding(!alignTextVertically);
        // Default layout parameters used for vertically oriented linear layout are (MATCH_PARENT,
        // WRAP_CONTENT). Chip view isn't measured correctly with these layout parameters. For more
        // information, see crbug.com/450830784.
        mPrimaryText.setLayoutParams(
                new LinearLayout.LayoutParams(
                        LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));

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
        if (mTextAlignStart) {
            // Default of 'center' is defined in the ChipTextView style.
            mPrimaryText.setTextAlignment(View.TEXT_ALIGNMENT_VIEW_START);
        }
        mPrimaryText.setPaddingRelative(
                mTextStartPadding,
                mPrimaryText.getPaddingTop(),
                mPrimaryText.getPaddingEnd(),
                mPrimaryText.getPaddingBottom());

        if (alignTextVertically) {
            mTextViewsWrapper = createTextViewsWrapper();
            mTextViewsWrapper.addView(mPrimaryText);
            addView(mTextViewsWrapper);
        } else {
            addView(mPrimaryText);
        }

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
        setIconWithTint(INVALID_ICON_ID, /* tintWithTextColor= */ false);

        updateLayoutDirection();
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
     * Sets the icon at the start of the chip view. If the {@code tintWithTextColor} is set to
     * {@code true}, applies the primary text's tint to the icon. TODO: crbug.com/454608496 - Rename
     * to setIcon once the other method is removed.
     *
     * @param icon The resource id pointing to the icon.
     * @param tintWithTextColor Whether to change the icon's tint to match the primary text's tint.
     */
    public void setIconWithTint(@DrawableRes int iconId, boolean tintWithTextColor) {
        if (iconId == INVALID_ICON_ID) {
            mStartIcon.setVisibility(ViewGroup.GONE);
            return;
        }
        Drawable icon = AppCompatResources.getDrawable(getContext(), iconId);
        setIconWithTint(icon, tintWithTextColor);
    }

    /**
     * Sets the icon at the start of the chip view. If the {@code tintWithTextColor} is set to
     * {@code true}, applies the primary text's tint to the icon. TODO: crbug.com/454608496 - Rename
     * to setIcon once the other method is removed.
     *
     * @param drawable Drawable to display.
     * @param tintWithTextColor Whether to change the icon's tint to match the primary text's tint.
     */
    public void setIconWithTint(@Nullable Drawable drawable, boolean tintWithTextColor) {
        if (drawable == null) {
            mStartIcon.setVisibility(ViewGroup.GONE);
            return;
        }

        mStartIcon.setVisibility(ViewGroup.VISIBLE);
        if (tintWithTextColor) {
            // Do not set tint on the `mStartIcon` because the tint in the `ImageView` cannot be
            // fully reset: `ImageView::setImageTintList(null)` will always reset the original tint
            // of the `Drawable` the `ImageView` is displaying.
            DrawableCompat.setTintList(drawable, mPrimaryText.getTextColors());
        }
        mStartIcon.setImageDrawable(drawable);
    }

    /**
     * Sets the icon at the start of the chip view. TODO: crbug.com/454608496 - Remove once this
     * method is no longer used.
     *
     * @param icon The resource id pointing to the icon.
     */
    @Deprecated(since = "Use setIconWithTint(int, boolean) instead", forRemoval = true)
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
     * Sets the icon at the start of the chip view. TODO: crbug.com/454608496 - Remove once this
     * method is no longer used.
     *
     * @param drawable Drawable to display.
     */
    @Deprecated(since = "Use setIconWithTint(Drawable, boolean) instead", forRemoval = true)
    public void setIcon(@Nullable Drawable drawable, boolean tintWithTextColor) {
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
                    public void onShowLoadingUiComplete() {
                        mStartIcon.setVisibility(GONE);
                    }

                    @Override
                    public void onHideLoadingUiComplete() {
                        mStartIcon.setVisibility(VISIBLE);
                    }
                });
        mLoadingView.addObserver(loadingViewObserver);
        mLoadingView.showLoadingUi();
    }

    /**
     * Hides the {@link LoadingView} at the start of the chip view.
     *
     * @param loadingViewObserver A {@link LoadingView.Observer} to add to the LoadingView.
     */
    public void hideLoadingView(LoadingView.Observer loadingViewObserver) {
        mLoadingView.addObserver(loadingViewObserver);
        mLoadingView.hideLoadingUi();
    }

    /** Adds a remove icon (X button) at the trailing end of the chip next to the primary text. */
    public void addRemoveIcon() {
        if (mEndIconWrapper != null) return;

        ChromeImageView endIcon = new ChromeImageView(getContext());
        endIcon.setId(R.id.chip_view_end_icon);
        endIcon.setImageResource(R.drawable.btn_close);
        ImageViewCompat.setImageTintList(endIcon, mPrimaryText.getTextColors());

        // Adding a wrapper view around the X icon to make the touch target larger, which would
        // cover the start and end margin for the X icon, and full height of the chip.
        mEndIconWrapper = new FrameLayout(getContext());
        mEndIconWrapper.setId(R.id.chip_cancel_btn);

        FrameLayout.LayoutParams layoutParams =
                new FrameLayout.LayoutParams(mEndIconWidth, mEndIconHeight);
        layoutParams.setMarginStart(mEndIconMarginStart);
        layoutParams.setMarginEnd(mEndIconMarginEnd);
        layoutParams.gravity = Gravity.CENTER_VERTICAL;
        mEndIconWrapper.addView(endIcon, layoutParams);
        addView(
                mEndIconWrapper,
                new LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.MATCH_PARENT));

        // Remove the end padding from the chip to make X icon touch target extend till the end of
        // the chip.
        this.setPaddingRelative(getPaddingStart(), getPaddingTop(), 0, getPaddingBottom());
        updateLayoutDirection();
    }

    /** Adds a dropdown icon at the trailing end of the chip next to the primary text. */
    public void addDropdownIcon() {
        if (mEndIconWrapper != null) return;

        ChromeImageView endIcon = new ChromeImageView(getContext());
        endIcon.setId(R.id.chip_view_end_icon);
        endIcon.setImageResource(R.drawable.mtrl_dropdown_arrow);
        ImageViewCompat.setImageTintList(endIcon, mPrimaryText.getTextColors());

        mEndIconWrapper = new FrameLayout(getContext());

        FrameLayout.LayoutParams layoutParams =
                new FrameLayout.LayoutParams(mEndIconWidth, mEndIconHeight);
        layoutParams.setMarginStart(mEndIconMarginStart);
        layoutParams.setMarginEnd(mEndIconMarginEnd);
        layoutParams.gravity = Gravity.CENTER_VERTICAL;
        mEndIconWrapper.addView(endIcon, layoutParams);
        addView(
                mEndIconWrapper,
                new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.MATCH_PARENT));

        // Remove the end padding from the chip to make X icon touch target extend till the end of
        // the chip.
        this.setPaddingRelative(getPaddingStart(), getPaddingTop(), 0, getPaddingBottom());
        updateLayoutDirection();
    }

    /**
     * Sets a {@link android.view.View.OnClickListener} for the remove icon. {@link
     * ChipView#addRemoveIcon()} must be called prior to this method.
     *
     * @param listener The listener to be invoked on click events.
     */
    public void setRemoveIconClickListener(OnClickListener listener) {
        assumeNonNull(mEndIconWrapper);
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
            mSecondaryText.setId(R.id.chip_view_secondary_text);
            mSecondaryText.setTextAppearance(mSecondaryTextAppearanceId);
            // Default layout parameters used for vertically oriented linear layout are
            // (MATCH_PARENT, WRAP_CONTENT). Chip view isn't measured correctly with these layout
            // parameters. For more information, see crbug.com/450830784.
            mSecondaryText.setLayoutParams(
                    new LinearLayout.LayoutParams(
                            LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));
            // Reduce font padding if the text is aligned vertically.
            mSecondaryText.setIncludeFontPadding(isSingleLineChip());
            // Ensure that basic state changes are aligned with the ChipView. They update
            // automatically once the view is part of the hierarchy.
            mSecondaryText.setSelected(isSelected());
            mSecondaryText.setEnabled(isEnabled());
            if (isTwoLineChip()) {
                if (mTextAlignStart) {
                    mSecondaryText.setTextAlignment(View.TEXT_ALIGNMENT_VIEW_START);
                }
                // Align secondary text view with the primary text view if they are stacked
                // vertically.
                mSecondaryText.setPaddingRelative(
                        mTextStartPadding,
                        mSecondaryText.getPaddingTop(),
                        mSecondaryText.getPaddingEnd(),
                        mSecondaryText.getPaddingBottom());
                mTextViewsWrapper.addView(mSecondaryText);
            } else {
                addView(mSecondaryText);
            }
            updateLayoutDirection();
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
    public void setBorder(int width, @Nullable ColorStateList color) {
        mRippleBackgroundHelper.setBorder(width, color, BorderType.SOLID);
    }

    @Override
    public void setBackgroundColor(@ColorInt int color) {
        mRippleBackgroundHelper.setBackgroundColor(color);
    }

    @Override
    public void setBackgroundTintList(@Nullable ColorStateList color) {
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
    public void setMaxWidth(@Px int maxWidth) {
        // Remove any existing width constraints. The new maximum width will take effect after the
        // next view measurement.
        mPrimaryText.setMaxWidth(Integer.MAX_VALUE);
        mPrimaryText.setEllipsize(null);
        if (isTwoLineChip() && mSecondaryText != null) {
            mSecondaryText.setMaxWidth(Integer.MAX_VALUE);
            mSecondaryText.setEllipsize(null);
        }
        mMaxWidth = maxWidth;
    }

    /**
     * Returns the max width of this {@link ChipView}. Returns Integer.MAX_VALUE if this {@link
     * ChipView} doesn't have width constraints.
     *
     * @return the max width set to this {@link ChipView}.
     */
    public @Px int getMaxWidth() {
        return mMaxWidth;
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
            final int textWidth =
                    isSingleLineChip()
                            ? mPrimaryText.getMeasuredWidth()
                            : mTextViewsWrapper.getMeasuredWidth();
            final int excessWidth = getMeasuredWidth() - mMaxWidth;
            // The text width should be reduced by the difference between the actual width and the
            // width constraint imposed on this ChipView.
            final int newTextWidth = textWidth - excessWidth;

            // TODO (crbug.com/1376691): The primary text must be at least a few pixels wide,
            // else only the ellipses will be visible. If there is space for displaying the
            // {@link mPrimaryText}, adjust it's size, and add trailing ellipses. If not, check
            // if the secondary text exists. If it does, remove the primary text, else do not
            // width constrain the chip. The chip should ALWAYS display some text.
            if (newTextWidth > 0) {
                mPrimaryText.setMaxWidth(newTextWidth);
                mPrimaryText.setEllipsize(TextUtils.TruncateAt.END);
                if (isTwoLineChip() && mSecondaryText != null) {
                    mSecondaryText.setMaxWidth(newTextWidth);
                    mSecondaryText.setEllipsize(TextUtils.TruncateAt.END);
                }
                super.onMeasure(
                        MeasureSpec.makeMeasureSpec(mMaxWidth, MeasureSpec.EXACTLY),
                        heightMeasureSpec);
            } else if (isSingleLineChip()
                    && mSecondaryText != null
                    && mSecondaryText.getVisibility() != GONE) {
                // If the text views are stacked horizontally and the second text view is displayed,
                // hide the primary text view.
                mPrimaryText.setVisibility(GONE);
                super.onMeasure(
                        MeasureSpec.makeMeasureSpec(
                                getMeasuredWidth() - textWidth, MeasureSpec.EXACTLY),
                        heightMeasureSpec);
            }
        }
    }

    private LinearLayout createTextViewsWrapper() {
        // The wrapper layout around the text views is created only if the text views are
        // stacked vertically. Otherwise, they can be added to the parent layout directly to
        // avoid crearing nested linear layouts with the same orientation.
        LinearLayout textViewsWrapper = new LinearLayout(getContext());
        textViewsWrapper.setId(R.id.chip_view_text_wrapper);
        textViewsWrapper.setOrientation(LinearLayout.VERTICAL);
        textViewsWrapper.setLayoutParams(
                new LinearLayout.LayoutParams(
                        LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT));
        return textViewsWrapper;
    }

    @EnsuresNonNullIf("mTextViewsWrapper")
    private boolean isTwoLineChip() {
        return mTextViewsWrapper != null;
    }

    @EnsuresNonNullIf(value = "mTextViewsWrapper", result = false)
    private boolean isSingleLineChip() {
        return mTextViewsWrapper == null;
    }

    private void updateLayoutDirection() {
        // Apply RTL layout changes, this is mostly relevant for render tests.
        int layoutDirection =
                LocalizationUtils.isLayoutRtl()
                        ? View.LAYOUT_DIRECTION_RTL
                        : View.LAYOUT_DIRECTION_LTR;

        setLayoutDirection(layoutDirection);
    }
}
