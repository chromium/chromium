// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.modaldialog;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.text.method.LinkMovementMethod;
import android.util.AttributeSet;
import android.util.DisplayMetrics;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.TimeUtils;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.BoundedLinearLayout;
import org.chromium.components.browser_ui.widget.FadingEdgeScrollView;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modaldialog.ModalDialogProperties.ButtonType;
import org.chromium.ui.widget.ButtonCompat;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/** Generic dialog view for app modal or tab modal alert dialogs. */
public class ModalDialogView extends BoundedLinearLayout implements View.OnClickListener {
    private static final String TAG_PREFIX = "ModalDialogViewButton";
    static final int NOT_SPECIFIED = -1;

    private static boolean sDisableButtonTapProtectionForTesting;

    private FadingEdgeScrollView mTitleScrollView;

    private FadingEdgeScrollView mModalDialogScrollView;
    private ViewGroup mTitleContainer;
    private TextView mTitleView;
    private ImageView mTitleIcon;
    private TextView mMessageParagraph1;
    private TextView mMessageParagraph2;
    private ViewGroup mCustomViewContainer;
    private ViewGroup mCustomButtonBarViewContainer;
    private View mButtonBar;
    private LinearLayout mButtonGroup;
    private Button mPositiveButton;
    private Button mNegativeButton;
    private Callback<Integer> mOnButtonClickedCallback;
    private Runnable mOnEscapeCallback;
    private boolean mTitleScrollable;
    private boolean mShouldWrapCustomViewScrollable;
    private boolean mFilterTouchForSecurity;
    private Runnable mOnTouchFilteredCallback;
    private final Set<View> mTouchFilterableViews = new HashSet<>();
    private ViewGroup mFooterContainer;
    private TextView mFooterMessageView;
    private long mStartProtectingButtonTimestamp = -1;
    // The duration for which dialog buttons should not react to any tap event after this view is
    // displayed to prevent potentially unintentional user interactions. A value of zero turns off
    // this kind of tap-jacking protection.
    private long mButtonTapProtectionDurationMs;

    private int mHorizontalMargin = NOT_SPECIFIED;
    private int mVerticalMargin = NOT_SPECIFIED;

    /** Constructor for inflating from XML. */
    public ModalDialogView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext())) {
            // On tablets, we set the android:windowMinWidth* attrs in the modal dialog style to
            // 280dp, so a measure mode of AT_MOST applies this value if it is smaller than the
            // measured width (which would typically be the case), causing the dialog to be shorter
            // width-wise than expected. Use MeasureSpec.EXACTLY to ensure that the measured width
            // is used, as long as it doesn't violate other width constraints.
            // TODO (crbug/369842880): Remove the check when this attr is added for phones.
            widthMeasureSpec = MeasureSpec.makeMeasureSpec(widthMeasureSpec, MeasureSpec.EXACTLY);
        }
        if (!ModalDialogFeatureMap.isEnabled(
                        ModalDialogFeatureList.MODAL_DIALOG_LAYOUT_WITH_SYSTEM_INSETS)
                || (mHorizontalMargin <= 0 && mVerticalMargin <= 0)) {
            super.onMeasure(widthMeasureSpec, heightMeasureSpec);
            return;
        }

        DisplayMetrics metrics = getResources().getDisplayMetrics();
        if (mHorizontalMargin > 0) {
            int dialogWidth = MeasureSpec.getSize(widthMeasureSpec);
            int maxWidth = metrics.widthPixels - 2 * mHorizontalMargin;
            int width = Math.min(dialogWidth, maxWidth);
            widthMeasureSpec = MeasureSpec.makeMeasureSpec(width, MeasureSpec.EXACTLY);
        }

        if (mVerticalMargin > 0) {
            int dialogHeight = MeasureSpec.getSize(heightMeasureSpec);
            int maxHeight = metrics.heightPixels - 2 * mVerticalMargin;
            int height = Math.min(dialogHeight, maxHeight);
            heightMeasureSpec = MeasureSpec.makeMeasureSpec(height, MeasureSpec.AT_MOST);
        }

        super.onMeasure(widthMeasureSpec, heightMeasureSpec);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mTitleScrollView = findViewById(R.id.modal_dialog_title_scroll_view);
        mModalDialogScrollView = findViewById(R.id.modal_dialog_scroll_view);
        mTitleContainer = findViewById(R.id.title_container);
        mTitleView = mTitleContainer.findViewById(R.id.title);
        mTitleIcon = mTitleContainer.findViewById(R.id.title_icon);
        mMessageParagraph1 = findViewById(R.id.message_paragraph_1);
        mMessageParagraph2 = findViewById(R.id.message_paragraph_2);
        mCustomViewContainer = findViewById(R.id.custom_view_not_in_scrollable);
        mCustomButtonBarViewContainer = findViewById(R.id.custom_button_bar);
        mButtonBar = findViewById(R.id.button_bar);
        mPositiveButton = findViewById(R.id.positive_button);
        mNegativeButton = findViewById(R.id.negative_button);
        setupClickableView(mPositiveButton, ButtonType.POSITIVE);
        setupClickableView(mNegativeButton, ButtonType.NEGATIVE);

        mFooterContainer = findViewById(R.id.footer);
        mFooterMessageView = findViewById(R.id.footer_message);
        mButtonGroup = findViewById(R.id.button_group);
        mMessageParagraph1.setMovementMethod(LinkMovementMethod.getInstance());
        mFooterMessageView.setMovementMethod(LinkMovementMethod.getInstance());
        mFooterContainer.setBackgroundColor(
                ChromeColors.getSurfaceColor(getContext(), R.dimen.default_elevation_1));
        updateContentVisibility();
        updateButtonVisibility();

        // If the scroll view can not be scrolled, make the scroll view not focusable so that the
        // focusing behavior for hardware keyboard is less confusing.
        // See https://codereview.chromium.org/2939883002.
        mTitleScrollView.addOnLayoutChangeListener(
                (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                    boolean isScrollable = v.canScrollVertically(-1) || v.canScrollVertically(1);
                    v.setFocusable(isScrollable);
                });
    }

    @VisibleForTesting
    public static String getTagForButtonType(@ButtonType int buttonType) {
        return TAG_PREFIX + buttonType;
    }

    private @ButtonType int getButtonTypeForTag(Object tag) {
        assert tag instanceof String;
        String tagString = (String) tag;
        Integer buttonType = Integer.parseInt(tagString.substring(TAG_PREFIX.length()));
        assert buttonType != null;
        return buttonType;
    }

    private void setupClickableView(View view, @ButtonType int buttonType) {
        setFilterTouchForSecurityIfNecessary(view);
        view.setTag(getTagForButtonType(buttonType));
        view.setOnClickListener(this);
    }

    // View.OnClickListener implementation.
    @Override
    public void onClick(View v) {
        if (isWithinButtonTapProtectionPeriod()) return;
        mOnButtonClickedCallback.onResult(getButtonTypeForTag(v.getTag()));
    }

    // Dialog buttons will not react to any tap event for a short period after this view is
    // displayed. This is to prevent potentially unintentional user interactions.
    private boolean isWithinButtonTapProtectionPeriod() {
        if (sDisableButtonTapProtectionForTesting) return false;

        // Not set by feature clients.
        if (mButtonTapProtectionDurationMs == 0) return false;

        // The view has not even started animating yet.
        if (mStartProtectingButtonTimestamp < 0) return true;

        // Calculate whether we are still within the button protection period and reset the timer to
        // prevent further tapjacking vectors.
        long timestamp = TimeUtils.elapsedRealtimeMillis();
        boolean shortEventAfterLastEvent =
                timestamp <= mStartProtectingButtonTimestamp + mButtonTapProtectionDurationMs;
        mStartProtectingButtonTimestamp = timestamp;

        // True if not showing for sufficient time.
        return shortEventAfterLastEvent;
    }

    /**
     * Callback when view is starting to appear on screen.
     * @param animationDuration Duration of enter animation.
     */
    void onEnterAnimationStarted(long animationDuration) {
        // Start button protection as soon as dialog is presented, but timer is kicked off in the
        // middle of the animation.
        mStartProtectingButtonTimestamp = TimeUtils.elapsedRealtimeMillis() + animationDuration / 2;
    }

    /**
     * @param callback The {@link Callback<Integer>} when a button on the dialog button bar is
     *                 clicked. The {@link Integer} indicates the button type.
     */
    void setOnButtonClickedCallback(Callback<Integer> callback) {
        mOnButtonClickedCallback = callback;
    }

    /**
     * @param callback The {@link Runnable} to invoke when the keyboard escape key is pressed.
     */
    void setOnEscapeCallback(Runnable callback) {
        mOnEscapeCallback = callback;
    }

    /**
     * @param title The title of the dialog.
     */
    public void setTitle(CharSequence title) {
        mTitleView.setText(title);
        updateContentVisibility();
    }

    /** @param maxLines The maximum number of title lines. */
    public void setTitleMaxLines(int maxLines) {
        mTitleView.setMaxLines(maxLines);
    }

    /** @param drawable The icon drawable on the title. */
    public void setTitleIcon(Drawable drawable) {
        mTitleIcon.setImageDrawable(drawable);
        updateContentVisibility();
        if (drawable != null) {
            setupClickableView(mTitleIcon, ButtonType.TITLE_ICON);
        }
    }

    /** @param titleScrollable Whether the title is scrollable with the message. */
    void setTitleScrollable(boolean titleScrollable) {
        if (mTitleScrollable == titleScrollable) return;

        mTitleScrollable = titleScrollable;
        CharSequence title = mTitleView.getText();
        Drawable icon = mTitleIcon.getDrawable();

        // Hide the previous title container since the scrollable and non-scrollable title container
        // should not be shown at the same time.
        mTitleContainer.setVisibility(View.GONE);

        mTitleContainer =
                findViewById(
                        titleScrollable ? R.id.scrollable_title_container : R.id.title_container);
        mTitleView = mTitleContainer.findViewById(R.id.title);
        mTitleIcon = mTitleContainer.findViewById(R.id.title_icon);
        setTitle(title);
        setTitleIcon(icon);

        LayoutParams layoutParams = (LayoutParams) mCustomViewContainer.getLayoutParams();
        if (titleScrollable) {
            layoutParams.height = LayoutParams.WRAP_CONTENT;
            layoutParams.weight = 0;
            mTitleScrollView.setEdgeVisibility(
                    FadingEdgeScrollView.EdgeType.FADING, FadingEdgeScrollView.EdgeType.FADING);
        } else {
            layoutParams.height = 0;
            layoutParams.weight = 1;
            mTitleScrollView.setEdgeVisibility(
                    FadingEdgeScrollView.EdgeType.NONE, FadingEdgeScrollView.EdgeType.NONE);
        }
        mCustomViewContainer.setLayoutParams(layoutParams);
    }

    void setWrapCustomViewInScrollable(boolean shouldWrapCustomViewInScrollable) {
        if (mShouldWrapCustomViewScrollable == shouldWrapCustomViewInScrollable) return;
        mShouldWrapCustomViewScrollable = shouldWrapCustomViewInScrollable;

        List<View> storedChildViews = new ArrayList<>();
        int wasVisible = mCustomViewContainer.getVisibility();
        for (int i = 0; i < mCustomViewContainer.getChildCount(); i++) {
            storedChildViews.add(mCustomViewContainer.getChildAt(0));
        }

        mCustomViewContainer.setVisibility(View.GONE);
        mCustomViewContainer =
                findViewById(
                        mShouldWrapCustomViewScrollable
                                ? R.id.custom_view_in_scrollable
                                : R.id.custom_view_not_in_scrollable);
        mCustomViewContainer.removeAllViews();
        for (View view : storedChildViews) {
            UiUtils.removeViewFromParent(view);
            mCustomViewContainer.addView(view);
        }
        mCustomViewContainer.setVisibility(wasVisible);
    }

    /**
     * @param filterTouchForSecurity Whether button touch events should be filtered when buttons are
     *                               obscured by another visible window.
     */
    void setFilterTouchForSecurity(boolean filterTouchForSecurity) {
        if (mFilterTouchForSecurity == filterTouchForSecurity) return;

        mFilterTouchForSecurity = filterTouchForSecurity;
        if (filterTouchForSecurity) {
            mTouchFilterableViews.forEach(this::setupFilterTouchForView);
        } else {
            assert false : "Shouldn't remove touch filter after setting it up";
        }
    }

    /**
     * @param duration The duration for which dialog buttons should not react to any tap event after
     *         this view is displayed to prevent potentially unintentional user interactions.
     */
    void setButtonTapProtectionDurationMs(long duration) {
        mButtonTapProtectionDurationMs = duration;
    }

    private void setFilterTouchForSecurityIfNecessary(View view) {
        if (mFilterTouchForSecurity) {
            setupFilterTouchForView(view);
        } else {
            mTouchFilterableViews.add(view);
        }
    }

    /** Setup touch filters to block events when buttons are obscured by another window. */
    private void setupFilterTouchForView(View view) {
        view.setFilterTouchesWhenObscured(true);
        view.setOnTouchListener(
                (View v, MotionEvent ev) -> {
                    boolean shouldBlockTouchEvent =
                            (ev.getFlags() & MotionEvent.FLAG_WINDOW_IS_PARTIALLY_OBSCURED) != 0;
                    if (shouldBlockTouchEvent
                            && mOnTouchFilteredCallback != null
                            && ev.getAction() == MotionEvent.ACTION_DOWN) {
                        mOnTouchFilteredCallback.run();
                    }
                    return shouldBlockTouchEvent;
                });
    }


    /**
     * @param callback The callback is called when touch event is filtered because of an overlay
     *                 window.
     */
    void setOnTouchFilteredCallback(Runnable callback) {
        mOnTouchFilteredCallback = callback;
    }

    void setupButtonGroup(ModalDialogProperties.ModalDialogButtonSpec[] buttonSpecList) {
        mButtonGroup.setVisibility(View.VISIBLE);
        int numButtons = buttonSpecList.length;

        for (int i = 0; i < buttonSpecList.length; i++) {
            ModalDialogProperties.ModalDialogButtonSpec spec = buttonSpecList[i];
            int style = 0;
            if (numButtons == 1) {
                style = R.style.FilledButton_Tonal_SingleButton;
            } else {
                if (i == 0) {
                    style = R.style.FilledButton_Tonal_TopButton;
                } else if (i == numButtons - 1) {
                    style = R.style.FilledButton_Tonal_BottomButton;
                } else {
                    style = R.style.FilledButton_Tonal_MiddleButton;
                }
            }

            Button button = new ButtonCompat(mButtonGroup.getContext(), style);
            button.setText(spec.getText());
            button.setContentDescription(spec.getContentDescription());

            setupClickableView(button, spec.getButtonType());
            setFilterTouchForSecurityIfNecessary(button);
            mButtonGroup.addView(button);
        }
        updateContentVisibility();
    }

    /** @param message The message in the dialog content. */
    void setMessageParagraph1(CharSequence message) {
        mMessageParagraph1.setText(message);
        updateContentVisibility();
    }

    /**
     * @param message The message shown below the text set via
     *         {@link #setMessageParagraph1(CharSequence)} when both are set.
     */
    void setMessageParagraph2(CharSequence message) {
        mMessageParagraph2.setText(message);
        updateContentVisibility();
    }

    /** @param view The customized view in the dialog content. */
    void setCustomView(View view) {
        if (mCustomViewContainer.getChildCount() > 0) mCustomViewContainer.removeAllViews();

        if (view != null) {
            UiUtils.removeViewFromParent(view);
            mCustomViewContainer.addView(view);
            mCustomViewContainer.setVisibility(View.VISIBLE);
        } else {
            mCustomViewContainer.setVisibility(View.GONE);
        }
    }

    /** @param view The customized button bar for the dialog. */
    void setCustomButtonBar(View view) {
        if (mCustomButtonBarViewContainer.getChildCount() > 0) {
            mCustomButtonBarViewContainer.removeAllViews();
        }

        if (view != null) {
            UiUtils.removeViewFromParent(view);
            mCustomButtonBarViewContainer.addView(view);
            mCustomButtonBarViewContainer.setVisibility(View.VISIBLE);
            assert mCustomButtonBarViewContainer.getChildCount() > 0
                    : "The CustomButtonBar cannot be empty.";

        } else {
            mCustomButtonBarViewContainer.setVisibility(View.GONE);
        }
        updateButtonVisibility();
    }

    /**
     * @param buttonType Indicates which button should be returned.
     */
    private Button getButton(@ButtonType int buttonType) {
        Button button = findViewWithTag(getTagForButtonType(buttonType));
        assert button != null : "Tried to retrieve a button that doesn't exist.";
        return button;
    }

    /**
     * Sets button text for the specified button. If {@code buttonText} is empty or null, the
     * specified button will not be visible.
     *
     * @param buttonType The {@link ButtonType} of the button.
     * @param buttonText The text to be set on the specified button.
     */
    void setButtonText(@ButtonType int buttonType, String buttonText) {
        getButton(buttonType).setText(buttonText);
        updateButtonVisibility();
    }

    /**
     * Sets the minimum horizontal margin relative to the window that this view can assume. This
     * method does not trigger a measure pass, and it is expected that this value is set before the
     * view is measured.
     *
     * @param margin The horizontal margin (in px) that the dialog should use.
     */
    void setHorizontalMargin(int margin) {
        mHorizontalMargin = margin;
    }

    int getHorizontalMarginForTesting() {
        return mHorizontalMargin;
    }

    /**
     * Sets the minimum vertical margin relative to the window that this view can assume. This
     * method does not trigger a measure pass, and it is expected that this value is set before the
     * view is measured.
     *
     * @param margin The vertical margin (in px) that the dialog should use.
     */
    void setVerticalMargin(int margin) {
        mVerticalMargin = margin;
    }

    int getVerticalMarginForTesting() {
        return mVerticalMargin;
    }

    /**
     * Sets content description for the specified button.
     *
     * @param buttonType The {@link ButtonType} of the button.
     * @param contentDescription The content description to be set for the specified button.
     */
    void setButtonContentDescription(@ButtonType int buttonType, String contentDescription) {
        getButton(buttonType).setContentDescription(contentDescription);
    }

    /**
     * @param buttonType The {@link ButtonType} of the button.
     * @param enabled Whether the specified button should be enabled.
     */
    void setButtonEnabled(@ButtonType int buttonType, boolean enabled) {
        getButton(buttonType).setEnabled(enabled);
    }

    /** @param message The message in the dialog footer. */
    void setFooterMessage(CharSequence message) {
        mFooterMessageView.setText(message);
        updateContentVisibility();
    }

    private void updateContentVisibility() {
        boolean titleVisible = !TextUtils.isEmpty(mTitleView.getText());
        boolean titleIconVisible = mTitleIcon.getDrawable() != null;
        boolean titleContainerVisible = titleVisible || titleIconVisible;
        boolean messageParagraph1Visibile = !TextUtils.isEmpty(mMessageParagraph1.getText());
        boolean messageParagraph2Visible = !TextUtils.isEmpty(mMessageParagraph2.getText());
        boolean scrollViewVisible =
                (mTitleScrollable && titleContainerVisible)
                        || messageParagraph1Visibile
                        || messageParagraph2Visible;
        boolean footerMessageVisible = !TextUtils.isEmpty(mFooterMessageView.getText());
        boolean modalDialogScrollViewVisible =
                mShouldWrapCustomViewScrollable || mButtonGroup.getVisibility() == View.VISIBLE;

        mTitleView.setVisibility(titleVisible ? View.VISIBLE : View.GONE);
        mTitleIcon.setVisibility(titleIconVisible ? View.VISIBLE : View.GONE);
        mTitleContainer.setVisibility(titleContainerVisible ? View.VISIBLE : View.GONE);
        mMessageParagraph1.setVisibility(messageParagraph1Visibile ? View.VISIBLE : View.GONE);
        mTitleScrollView.setVisibility(scrollViewVisible ? View.VISIBLE : View.GONE);
        mMessageParagraph2.setVisibility(messageParagraph2Visible ? View.VISIBLE : View.GONE);
        mModalDialogScrollView.setVisibility(
                modalDialogScrollViewVisible ? View.VISIBLE : View.GONE);
        mFooterContainer.setVisibility(footerMessageVisible ? View.VISIBLE : View.GONE);
    }

    private void updateButtonVisibility() {
        boolean positiveButtonVisible = !TextUtils.isEmpty(mPositiveButton.getText());
        boolean negativeButtonVisible = !TextUtils.isEmpty(mNegativeButton.getText());
        boolean customButtonBarViewVisible =
                mCustomButtonBarViewContainer.getVisibility() == View.VISIBLE;
        boolean defaultButtonBarVisible =
                (positiveButtonVisible || negativeButtonVisible) && !customButtonBarViewVisible;

        mPositiveButton.setVisibility(positiveButtonVisible ? View.VISIBLE : View.GONE);
        mNegativeButton.setVisibility(negativeButtonVisible ? View.VISIBLE : View.GONE);
        mButtonBar.setVisibility(defaultButtonBarVisible ? View.VISIBLE : View.GONE);
    }

    public static void disableButtonTapProtectionForTesting() {
        sDisableButtonTapProtectionForTesting = true;
        ResettersForTesting.register(() -> sDisableButtonTapProtectionForTesting = false);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        if (mOnEscapeCallback != null
                && event.getKeyCode() == KeyEvent.KEYCODE_ESCAPE
                && event.getAction() == KeyEvent.ACTION_DOWN
                && event.getRepeatCount() == 0) {
            mOnEscapeCallback.run();
            return true;
        }
        return super.dispatchKeyEvent(event);
    }
}
