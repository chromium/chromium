// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.infobars;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.content.res.TypedArray;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.text.SpannableString;
import android.text.SpannableStringBuilder;
import android.text.Spanned;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.ColorRes;
import androidx.annotation.Nullable;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.widget.ImageViewCompat;

import org.chromium.components.browser_ui.widget.DualControlLayout;
import org.chromium.components.browser_ui.widget.DualControlLayout.ButtonType;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.ChromeImageButton;
import org.chromium.ui.widget.ChromeImageView;

import java.util.ArrayList;
import java.util.List;

/**
 * Layout that arranges an infobar's views.
 *
 * An InfoBarLayout consists of:
 * - A message describing why the infobar is being displayed.
 * - A close button in the top right corner.
 * - (optional) An icon representing the infobar's purpose in the top left corner.
 * - (optional) Additional {@link InfoBarControlLayouts} for specialized controls (e.g. spinners).
 * - (optional) One or two buttons with text at the bottom, or a button paired with an ImageView.
 *
 * When adding custom views, widths and heights defined in the LayoutParams will be ignored.
 * Setting a minimum width using {@link View#setMinimumWidth()} will be obeyed.
 *
 * Logic for what happens when things are clicked should be implemented by the
 * InfoBarInteractionHandler.
 */
public final class InfoBarLayout extends ViewGroup implements View.OnClickListener {
    /** Parameters used for laying out children. */
    private static class LayoutParams extends ViewGroup.LayoutParams {
        public int startMargin;
        public int endMargin;
        public int topMargin;
        public int bottomMargin;

        // Where this view will be laid out. Calculated in onMeasure() and used in onLayout().
        public int start;
        public int top;

        LayoutParams(int startMargin, int topMargin, int endMargin, int bottomMargin) {
            super(LayoutParams.WRAP_CONTENT, LayoutParams.WRAP_CONTENT);
            this.startMargin = startMargin;
            this.topMargin = topMargin;
            this.endMargin = endMargin;
            this.bottomMargin = bottomMargin;
        }
    }

    private final int mSmallIconSize;
    private final int mSmallIconMargin;
    private final int mMarginAboveButtonGroup;
    private final int mMarginAboveControlGroups;
    private final int mPadding;
    private final int mMinWidth;

    private final InfoBarInteractionHandler mInfoBar;
    private final ImageButton mCloseButton;
    private final InfoBarControlLayout mMessageLayout;
    private final List<InfoBarControlLayout> mControlLayouts;
    private ViewGroup mFooterViewGroup;

    private TextView mMessageTextView;
    private ImageView mIconView;
    private DualControlLayout mButtonRowLayout;

    private CharSequence mMessageMainText;
    private String mMessageLinkText;
    private int mMessageInlineLinkRangeStart;
    private int mMessageInlineLinkRangeEnd;

    /**
     * Constructs a layout for the specified infobar. After calling this, be sure to set the
     * message, the buttons, and/or the custom content using setMessage(), setButtons(), and
     * setCustomContent().
     * @param context The context used to render.
     * @param infoBar InfoBarInteractionHandler that listens to events.
     * @param iconResourceId ID of the icon to use for the infobar.
     * @param iconTintId The {@link ColorRes} used as tint for {@code iconResourceId}.
     * @param iconBitmap Bitmap for the icon to use, if the resource ID wasn't passed through.
     * @param message The message to show in the infobar.
     */
    public InfoBarLayout(
            Context context,
            InfoBarInteractionHandler infoBar,
            int iconResourceId,
            @ColorRes int iconTintId,
            Bitmap iconBitmap,
            CharSequence message) {
        super(context);
        mControlLayouts = new ArrayList<InfoBarControlLayout>();

        mInfoBar = infoBar;

        // Cache resource values.
        Resources res = getResources();
        mSmallIconSize = res.getDimensionPixelSize(R.dimen.infobar_small_icon_size);
        mSmallIconMargin = res.getDimensionPixelSize(R.dimen.infobar_small_icon_margin);
        mMarginAboveButtonGroup =
                res.getDimensionPixelSize(R.dimen.infobar_margin_above_button_row);
        mMarginAboveControlGroups =
                res.getDimensionPixelSize(R.dimen.infobar_margin_above_control_groups);
        mPadding = res.getDimensionPixelOffset(R.dimen.infobar_padding);
        mMinWidth = res.getDimensionPixelSize(R.dimen.infobar_min_width);

        // Set up the close button. Apply padding so it has a big touch target.
        mCloseButton = createCloseButton(context);
        mCloseButton.setOnClickListener(this);
        mCloseButton.setPadding(mPadding, mPadding, mPadding, mPadding);
        mCloseButton.setLayoutParams(new LayoutParams(0, -mPadding, -mPadding, -mPadding));

        // Set up the icon, if necessary.
        mIconView = createIconView(context, iconResourceId, iconTintId, iconBitmap);
        if (mIconView != null) {
            mIconView.setLayoutParams(new LayoutParams(0, 0, mSmallIconMargin, 0));
            mIconView.getLayoutParams().width = mSmallIconSize;
            mIconView.getLayoutParams().height = mSmallIconSize;
        }

        // Set up the message view.
        mMessageMainText = message;
        mMessageLayout = new InfoBarControlLayout(context);
        mMessageTextView = mMessageLayout.addMainMessage(prepareMainMessageString());
    }

    /**
     * Returns the {@link TextView} corresponding to the main infobar message.
     * The returned view is a part of internal layout strucutre and shouldn't be accessed by InfoBar
     * implementations.
     */
    public TextView getMessageTextView() {
        return mMessageTextView;
    }

    /**
     * Returns the {@link InfoBarControlLayout} containing the TextView showing the main infobar
     * message and associated controls, which is sandwiched between its icon and close button.
     * The returned view is a part of internal layout strucutre and shouldn't be accessed by InfoBar
     * implementations.
     */
    public InfoBarControlLayout getMessageLayout() {
        return mMessageLayout;
    }

    /**
     * Sets the message to show on the infobar.
     * TODO(dfalcantara): Do some magic here to determine if TextViews need to have line spacing
     *                    manually added.  Android changed when these values were applied between
     *                    KK and L: https://crbug.com/543205
     */
    public void setMessage(CharSequence message) {
        mMessageMainText = message;
        mMessageTextView.setText(prepareMainMessageString());
    }

    /** Appends a link to the message, if an infobar requires one (e.g. "Learn more"). */
    public void appendMessageLinkText(String linkText) {
        mMessageLinkText = linkText;
        mMessageTextView.setText(prepareMainMessageString());
    }

    /**
     * Sets up the message to have an inline link, assuming an inclusive range.
     * @param rangeStart Where the link starts.
     * @param rangeEnd   Where the link ends.
     */
    public void setInlineMessageLink(int rangeStart, int rangeEnd) {
        mMessageInlineLinkRangeStart = rangeStart;
        mMessageInlineLinkRangeEnd = rangeEnd;
        mMessageTextView.setText(prepareMainMessageString());
    }

    /**
     * Adds an {@link InfoBarControlLayout} to house additional infobar controls, like toggles and
     * spinners.
     */
    public InfoBarControlLayout addControlLayout() {
        InfoBarControlLayout controlLayout = new InfoBarControlLayout(getContext());
        mControlLayouts.add(controlLayout);
        return controlLayout;
    }

    /**
     * Adds a footer at the bottom of the InfoBar which spans the InfoBar's whole width.
     *
     * @param footerView footer to be added.
     */
    public ViewGroup addFooterView(ViewGroup footerView) {
        mFooterViewGroup = footerView;
        return footerView;
    }

    /**
     * Adds one or two buttons to the layout.
     *
     * @param primaryText Text for the primary button.  If empty, no buttons are added at all.
     * @param secondaryText Text for the secondary button, or null if there isn't a second button.
     */
    public void setButtons(String primaryText, String secondaryText) {
        if (TextUtils.isEmpty(primaryText)) {
            assert TextUtils.isEmpty(secondaryText);
            return;
        }

        Button secondaryButton = null;
        if (!TextUtils.isEmpty(secondaryText)) {
            secondaryButton =
                    DualControlLayout.createButtonForLayout(
                            getContext(), ButtonType.SECONDARY_TEXT, secondaryText, this);
        }

        setBottomViews(
                primaryText, secondaryButton, DualControlLayout.DualControlLayoutAlignment.END);
    }

    /**
     * Sets up the bottom-most part of the infobar with a primary button (e.g. OK) and a secondary
     * View of your choice.  Subclasses should be calling {@link #setButtons(String, String)}
     * instead of this function in nearly all cases (that function calls this one).
     *
     * @param primaryText Text to display on the primary button.  If empty, the bottom layout is not
     *                    created.
     * @param secondaryView View that is aligned with the primary button.  May be null.
     * @param alignment One of ALIGN_START, ALIGN_APART, or ALIGN_END from
     *                  {@link DualControlLayout}.
     */
    public void setBottomViews(String primaryText, View secondaryView, int alignment) {
        assert !TextUtils.isEmpty(primaryText);
        Button primaryButton =
                DualControlLayout.createButtonForLayout(
                        getContext(), ButtonType.PRIMARY_FILLED, primaryText, this);

        assert mButtonRowLayout == null;
        mButtonRowLayout = new DualControlLayout(getContext(), null);
        mButtonRowLayout.setAlignment(alignment);
        mButtonRowLayout.setStackedMargin(
                getResources()
                        .getDimensionPixelSize(R.dimen.infobar_margin_between_stacked_buttons));

        mButtonRowLayout.addView(primaryButton);
        if (secondaryView != null) {
            mButtonRowLayout.addView(secondaryView);
        }
    }

    /** Returns the primary button, or null if it doesn't exist. */
    public ButtonCompat getPrimaryButton() {
        return mButtonRowLayout == null
                ? null
                : (ButtonCompat) mButtonRowLayout.findViewById(R.id.button_primary);
    }

    /** Returns whether or not InfoBar has a footer. */
    public boolean hasFooter() {
        return mFooterViewGroup != null;
    }

    /** Returns the icon, or null if it doesn't exist. */
    public ImageView getIcon() {
        return mIconView;
    }

    /**
     * Must be called after the message, buttons, and custom content have been set, and before the
     * first call to onMeasure().
     */
    // TODO(crbug.com/40120294): onContentCreated is made public to allow access from InfoBar. Once
    // InfoBar is modularized, restore access to package private.
    public void onContentCreated() {
        // Add the child views in the desired focus order.
        if (mIconView != null) addView(mIconView);
        addView(mMessageLayout);
        for (View v : mControlLayouts) addView(v);
        if (mButtonRowLayout != null) addView(mButtonRowLayout);
        if (mFooterViewGroup != null) addView(mFooterViewGroup);
        addView(mCloseButton);
    }

    @Override
    protected LayoutParams generateDefaultLayoutParams() {
        return new LayoutParams(0, 0, 0, 0);
    }

    @Override
    protected void onLayout(boolean changed, int left, int top, int right, int bottom) {
        // Place all the views in the positions already determined during onMeasure().
        int width = right - left;
        boolean isRtl = getLayoutDirection() == LAYOUT_DIRECTION_RTL;

        for (int i = 0; i < getChildCount(); i++) {
            View child = getChildAt(i);
            LayoutParams lp = (LayoutParams) child.getLayoutParams();
            int childLeft = lp.start;
            int childRight = lp.start + child.getMeasuredWidth();

            if (isRtl) {
                int tmp = width - childRight;
                childRight = width - childLeft;
                childLeft = tmp;
            }

            child.layout(childLeft, lp.top, childRight, lp.top + child.getMeasuredHeight());
        }
    }

    /**
     * Measures and determines where children should go.
     *
     * For current specs, see https://goto.google.com/infobar-spec
     *
     * All controls are padded from the infobar boundary by the same amount, but different types of
     * control groups are bound by different widths and have different margins:
     * --------------------------------------------------------------------------------
     * |  PADDING                                                                     |
     * |  --------------------------------------------------------------------------  |
     * |  | ICON | MESSAGE LAYOUT                                              | X |  |
     * |  |------+                                                             +---|  |
     * |  |      |                                                             |   |  |
     * |  |      ------------------------------------------------------------------|  |
     * |  |      | CONTROL LAYOUT #1                                               |  |
     * |  |      ------------------------------------------------------------------|  |
     * |  |      | CONTROL LAYOUT #X                                               |  |
     * |  |------------------------------------------------------------------------|  |
     * |  | BOTTOM ROW LAYOUT                                                      |  |
     * |  -------------------------------------------------------------------------|  |
     * |                                                                              |
     * --------------------------------------------------------------------------------
     * |  FOOTER                                                                      |
     * --------------------------------------------------------------------------------
     */
    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        assert getLayoutParams().height == LayoutParams.WRAP_CONTENT
                : "InfoBar heights cannot be constrained.";

        // Apply the padding that surrounds all the infobar controls.
        final int layoutWidth = Math.max(MeasureSpec.getSize(widthMeasureSpec), mMinWidth);
        final int paddedStart = mPadding;
        final int paddedEnd = layoutWidth - mPadding;
        int layoutBottom = mPadding;

        // Measure and place the icon in the top-left corner.
        int unspecifiedSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        if (mIconView != null) {
            LayoutParams iconParams = getChildLayoutParams(mIconView);
            measureChild(mIconView, unspecifiedSpec, unspecifiedSpec);
            iconParams.start = paddedStart + iconParams.startMargin;
            iconParams.top = layoutBottom + iconParams.topMargin;
        }
        final int iconWidth = getChildWidthWithMargins(mIconView);

        // Measure and place the close button in the top-right corner of the layout.
        LayoutParams closeParams = getChildLayoutParams(mCloseButton);
        measureChild(mCloseButton, unspecifiedSpec, unspecifiedSpec);
        closeParams.start = paddedEnd - closeParams.endMargin - mCloseButton.getMeasuredWidth();
        closeParams.top = layoutBottom + closeParams.topMargin;

        // Determine how much width is available for all the different control layouts; see the
        // function JavaDoc above for details.
        final int paddedWidth = paddedEnd - paddedStart;
        final int controlLayoutWidth = paddedWidth - iconWidth;
        final int messageWidth = controlLayoutWidth - getChildWidthWithMargins(mCloseButton);

        // The message layout is sandwiched between the icon and the close button.
        LayoutParams messageParams = getChildLayoutParams(mMessageLayout);
        measureChildWithFixedWidth(mMessageLayout, messageWidth);
        messageParams.start = paddedStart + iconWidth;
        messageParams.top = layoutBottom;

        // Control layouts are placed below the message layout and the close button.  The icon is
        // ignored for this particular calculation because the icon enforces a left margin on all of
        // the control layouts and won't be overlapped.
        layoutBottom +=
                Math.max(
                        getChildHeightWithMargins(mMessageLayout),
                        getChildHeightWithMargins(mCloseButton));

        // The other control layouts are constrained only by the icon's width.
        final int controlPaddedStart = paddedStart + iconWidth;
        for (int i = 0; i < mControlLayouts.size(); i++) {
            View child = mControlLayouts.get(i);
            measureChildWithFixedWidth(child, controlLayoutWidth);

            layoutBottom += mMarginAboveControlGroups;
            getChildLayoutParams(child).start = controlPaddedStart;
            getChildLayoutParams(child).top = layoutBottom;
            layoutBottom += child.getMeasuredHeight();
        }

        // The button layout takes up the full width of the infobar and sits below everything else,
        // including the icon.
        layoutBottom = Math.max(layoutBottom, getChildHeightWithMargins(mIconView));
        if (mButtonRowLayout != null) {
            measureChildWithFixedWidth(mButtonRowLayout, paddedWidth);

            layoutBottom += mMarginAboveButtonGroup;
            getChildLayoutParams(mButtonRowLayout).start = paddedStart;
            getChildLayoutParams(mButtonRowLayout).top = layoutBottom;
            layoutBottom += mButtonRowLayout.getMeasuredHeight();
        }

        // Apply padding to the bottom of the infobar.
        layoutBottom += mPadding;

        if (mFooterViewGroup != null) {
            LayoutParams footerParams = getChildLayoutParams(mFooterViewGroup);
            measureChildWithFixedWidth(mFooterViewGroup, layoutWidth);
            footerParams.start = 0;
            footerParams.top = layoutBottom;
            layoutBottom += mFooterViewGroup.getMeasuredHeight();
        }

        setMeasuredDimension(
                resolveSize(layoutWidth, widthMeasureSpec),
                resolveSize(layoutBottom, heightMeasureSpec));
    }

    private static int getChildWidthWithMargins(View view) {
        if (view == null) return 0;
        return view.getMeasuredWidth()
                + getChildLayoutParams(view).startMargin
                + getChildLayoutParams(view).endMargin;
    }

    private static int getChildHeightWithMargins(View view) {
        if (view == null) return 0;
        return view.getMeasuredHeight()
                + getChildLayoutParams(view).topMargin
                + getChildLayoutParams(view).bottomMargin;
    }

    private static LayoutParams getChildLayoutParams(View view) {
        return (LayoutParams) view.getLayoutParams();
    }

    /** Measures a child for the given space, taking into account its margins. */
    private void measureChildWithFixedWidth(View child, int width) {
        LayoutParams lp = getChildLayoutParams(child);
        int availableWidth = width - lp.startMargin - lp.endMargin;
        int widthSpec = MeasureSpec.makeMeasureSpec(availableWidth, MeasureSpec.EXACTLY);
        int heightSpec = MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED);
        child.measure(widthSpec, heightSpec);
    }

    /**
     * Listens for View clicks.
     * Classes that override this function MUST call this one.
     * @param view View that was clicked on.
     */
    @Override
    public void onClick(View view) {
        mInfoBar.onClick();

        if (view.getId() == R.id.infobar_close_button) {
            mInfoBar.onCloseButtonClicked();
        } else if (view.getId() == R.id.button_primary) {
            mInfoBar.onButtonClicked(true);
        } else if (view.getId() == R.id.button_secondary) {
            mInfoBar.onButtonClicked(false);
        }
    }

    /**
     * Prepares text to be displayed as the infobar's main message, including setting up a
     * clickable link if the infobar requires it.
     */
    private CharSequence prepareMainMessageString() {
        SpannableStringBuilder fullString = new SpannableStringBuilder();

        if (!TextUtils.isEmpty(mMessageMainText)) {
            SpannableString spannedMessage = new SpannableString(mMessageMainText);

            // If there's an inline link, apply the necessary span for it.
            if (mMessageInlineLinkRangeEnd != 0) {
                assert mMessageInlineLinkRangeStart < mMessageInlineLinkRangeEnd;
                assert mMessageInlineLinkRangeEnd < mMessageMainText.length();

                spannedMessage.setSpan(
                        createClickableSpan(),
                        mMessageInlineLinkRangeStart,
                        mMessageInlineLinkRangeEnd,
                        Spanned.SPAN_INCLUSIVE_INCLUSIVE);
            }

            fullString.append(spannedMessage);
        }

        // Concatenate the text to display for the link and make it clickable.
        if (!TextUtils.isEmpty(mMessageLinkText)) {
            if (fullString.length() > 0) fullString.append(" ");
            int spanStart = fullString.length();

            fullString.append(mMessageLinkText);
            fullString.setSpan(
                    createClickableSpan(),
                    spanStart,
                    fullString.length(),
                    Spanned.SPAN_EXCLUSIVE_EXCLUSIVE);
        }

        return fullString;
    }

    private NoUnderlineClickableSpan createClickableSpan() {
        return new NoUnderlineClickableSpan(getContext(), (view) -> mInfoBar.onLinkClicked());
    }

    /**
     * Creates a View that holds an icon representing an infobar.
     * @param context Context to grab resources from.
     * @param iconResourceId ID of the icon to use for the infobar.
     * @param iconTintId The {@link ColorRes} used as tint for {@code iconResourceId}.
     * @param iconBitmap Bitmap for the icon to use, if the resource ID wasn't passed through.
     * @return {@link ImageButton} that represents the icon.
     */
    @Nullable
    public static ImageView createIconView(
            Context context, int iconResourceId, @ColorRes int iconTintId, Bitmap iconBitmap) {
        if (iconResourceId == 0 && iconBitmap == null) return null;

        final ChromeImageView iconView = new ChromeImageView(context);
        if (iconResourceId != 0) {
            iconView.setImageDrawable(AppCompatResources.getDrawable(context, iconResourceId));
            if (iconTintId != 0) {
                ImageViewCompat.setImageTintList(
                        iconView, AppCompatResources.getColorStateList(context, iconTintId));
            }
        } else {
            iconView.setImageBitmap(iconBitmap);
        }

        iconView.setFocusable(false);
        iconView.setId(R.id.infobar_icon);
        iconView.setScaleType(ImageView.ScaleType.CENTER_INSIDE);
        return iconView;
    }

    /**
     * Creates a close button that can be inserted into an infobar.
     * @param context Context to grab resources from.
     * @return {@link ImageButton} that represents a close button.
     */
    public static ImageButton createCloseButton(Context context) {
        final ColorStateList tint =
                AppCompatResources.getColorStateList(context, R.color.default_icon_color_tint_list);
        TypedArray a =
                context.obtainStyledAttributes(new int[] {android.R.attr.selectableItemBackground});
        Drawable closeButtonBackground = a.getDrawable(0);
        a.recycle();

        ChromeImageButton closeButton = new ChromeImageButton(context);
        closeButton.setId(R.id.infobar_close_button);
        closeButton.setImageResource(R.drawable.btn_close);
        ImageViewCompat.setImageTintList(closeButton, tint);
        closeButton.setBackground(closeButtonBackground);
        closeButton.setContentDescription(context.getString(R.string.close));
        closeButton.setScaleType(ImageView.ScaleType.CENTER_INSIDE);

        return closeButton;
    }
}
