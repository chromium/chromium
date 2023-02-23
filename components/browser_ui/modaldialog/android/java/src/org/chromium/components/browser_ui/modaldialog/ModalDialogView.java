// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.modaldialog;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.text.method.LinkMovementMethod;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.IntDef;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.BoundedLinearLayout;
import org.chromium.components.browser_ui.widget.FadingEdgeScrollView;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modaldialog.ModalDialogProperties;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.reflect.Field;

/**
 * Generic dialog view for app modal or tab modal alert dialogs.
 */
public class ModalDialogView extends BoundedLinearLayout implements View.OnClickListener {
    private static final String TAG = "ModalDialogView";
    private static final String UMA_SECURITY_FILTERED_TOUCH_RESULT =
            "Android.ModalDialog.SecurityFilteredTouchResult";

    // Intdef with constants for recording the result of filtering touch events on security
    // sensitive dialogs. Should stay in sync with the SecurityFilteredTouchResult enum defined in
    // tools/metrics/histograms/enums.xml.
    @IntDef({SecurityFilteredTouchResult.BLOCKED, SecurityFilteredTouchResult.HANDLED})
    @Retention(RetentionPolicy.SOURCE)
    public @interface SecurityFilteredTouchResult {
        int HANDLED = 0;
        int BLOCKED = 1;
        int NUM_ENTRIES = 2;
    }

    private FadingEdgeScrollView mScrollView;
    private ViewGroup mTitleContainer;
    private TextView mTitleView;
    private ImageView mTitleIcon;
    private TextView mMessageParagraph1;
    private TextView mMessageParagraph2;
    private ViewGroup mCustomViewContainer;
    private ViewGroup mCustomButtonBarViewContainer;
    private View mButtonBar;
    private Button mPositiveButton;
    private Button mNegativeButton;
    private Callback<Integer> mOnButtonClickedCallback;
    private boolean mTitleScrollable;
    private boolean mFilterTouchForSecurity;
    private boolean mFilteredTouchResultRecorded;
    private Runnable mOnTouchFilteredCallback;
    private ViewGroup mFooterContainer;
    private TextView mFooterMessageView;

    /**
     * Constructor for inflating from XML.
     */
    public ModalDialogView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mScrollView = findViewById(R.id.modal_dialog_scroll_view);
        mTitleContainer = findViewById(R.id.title_container);
        mTitleView = mTitleContainer.findViewById(R.id.title);
        mTitleIcon = mTitleContainer.findViewById(R.id.title_icon);
        mMessageParagraph1 = findViewById(R.id.message_paragraph_1);
        mMessageParagraph2 = findViewById(R.id.message_paragraph_2);
        mCustomViewContainer = findViewById(R.id.custom);
        mCustomButtonBarViewContainer = findViewById(R.id.custom_button_bar);
        mButtonBar = findViewById(R.id.button_bar);
        mPositiveButton = findViewById(R.id.positive_button);
        mNegativeButton = findViewById(R.id.negative_button);
        mFooterContainer = findViewById(R.id.footer);
        mFooterMessageView = findViewById(R.id.footer_message);

        mPositiveButton.setOnClickListener(this);
        mNegativeButton.setOnClickListener(this);
        mMessageParagraph1.setMovementMethod(LinkMovementMethod.getInstance());
        mFooterMessageView.setMovementMethod(LinkMovementMethod.getInstance());
        mFooterContainer.setBackgroundColor(
                ChromeColors.getSurfaceColor(getContext(), R.dimen.default_elevation_1));
        updateContentVisibility();
        updateButtonVisibility();

        // If the scroll view can not be scrolled, make the scroll view not focusable so that the
        // focusing behavior for hardware keyboard is less confusing.
        // See https://codereview.chromium.org/2939883002.
        mScrollView.addOnLayoutChangeListener(
                (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                    boolean isScrollable = v.canScrollVertically(-1) || v.canScrollVertically(1);
                    v.setFocusable(isScrollable);
                });
    }

    // View.OnClickListener implementation.

    @Override
    public void onClick(View view) {
        if (view == mPositiveButton) {
            mOnButtonClickedCallback.onResult(ModalDialogProperties.ButtonType.POSITIVE);
        } else if (view == mNegativeButton) {
            mOnButtonClickedCallback.onResult(ModalDialogProperties.ButtonType.NEGATIVE);
        } else if (view == mTitleIcon) {
            mOnButtonClickedCallback.onResult(ModalDialogProperties.ButtonType.TITLE_ICON);
        }
    }

    /**
     * @param callback The {@link Callback<Integer>} when a button on the dialog button bar is
     *                 clicked. The {@link Integer} indicates the button type.
     */
    void setOnButtonClickedCallback(Callback<Integer> callback) {
        mOnButtonClickedCallback = callback;
    }

    /** @param title The title of the dialog. */
    public void setTitle(CharSequence title) {
        mTitleView.setText(title);
        updateContentVisibility();
    }

    /** @param maxLines The maximum number of title lines. */
    public void setTitleMaxLines(int maxLines) {
        mTitleView.setMaxLines(maxLines);
    }

    /**
     * @param drawable The icon drawable on the title.
     */
    public void setTitleIcon(Drawable drawable) {
        mTitleIcon.setImageDrawable(drawable);
        updateContentVisibility();
        if (drawable != null) mTitleIcon.setOnClickListener(this);
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

        mTitleContainer = findViewById(
                titleScrollable ? R.id.scrollable_title_container : R.id.title_container);
        mTitleView = mTitleContainer.findViewById(R.id.title);
        mTitleIcon = mTitleContainer.findViewById(R.id.title_icon);
        setTitle(title);
        setTitleIcon(icon);

        LayoutParams layoutParams = (LayoutParams) mCustomViewContainer.getLayoutParams();
        if (titleScrollable) {
            layoutParams.height = LayoutParams.WRAP_CONTENT;
            layoutParams.weight = 0;
            mScrollView.setEdgeVisibility(
                    FadingEdgeScrollView.EdgeType.FADING, FadingEdgeScrollView.EdgeType.FADING);
        } else {
            layoutParams.height = 0;
            layoutParams.weight = 1;
            mScrollView.setEdgeVisibility(
                    FadingEdgeScrollView.EdgeType.NONE, FadingEdgeScrollView.EdgeType.NONE);
        }
        mCustomViewContainer.setLayoutParams(layoutParams);
    }

    /**
     * @param filterTouchForSecurity Whether button touch events should be filtered when buttons are
     *                               obscured by another visible window.
     */
    void setFilterTouchForSecurity(boolean filterTouchForSecurity) {
        if (mFilterTouchForSecurity == filterTouchForSecurity) return;

        mFilterTouchForSecurity = filterTouchForSecurity;
        if (filterTouchForSecurity) {
            setupFilterTouchForSecurity();
        } else {
            assert false : "Shouldn't remove touch filter after setting it up";
        }
    }

    /** Setup touch filters to block events when buttons are obscured by another window. */
    private void setupFilterTouchForSecurity() {
        Button positiveButton = getButton(ModalDialogProperties.ButtonType.POSITIVE);
        Button negativeButton = getButton(ModalDialogProperties.ButtonType.NEGATIVE);
        View.OnTouchListener onTouchListener = (View v, MotionEvent ev) -> {
            boolean shouldBlockTouchEvent = false;

            try {
                Field field = MotionEvent.class.getField("FLAG_WINDOW_IS_PARTIALLY_OBSCURED");
                if ((ev.getFlags() & field.getInt(null)) != 0) {
                    shouldBlockTouchEvent = true;
                }
                if (ev.getAction() == MotionEvent.ACTION_DOWN && !mFilteredTouchResultRecorded) {
                    mFilteredTouchResultRecorded = true;
                    RecordHistogram.recordEnumeratedHistogram(UMA_SECURITY_FILTERED_TOUCH_RESULT,
                            shouldBlockTouchEvent ? SecurityFilteredTouchResult.BLOCKED
                                                  : SecurityFilteredTouchResult.HANDLED,
                            SecurityFilteredTouchResult.NUM_ENTRIES);
                }
                if (shouldBlockTouchEvent && mOnTouchFilteredCallback != null
                        && ev.getAction() == MotionEvent.ACTION_DOWN) {
                    mOnTouchFilteredCallback.run();
                }
            } catch (NoSuchFieldException | IllegalAccessException e) {
                Log.e(TAG, "Reflection failure: " + e);
            }
            return shouldBlockTouchEvent;
        };

        positiveButton.setFilterTouchesWhenObscured(true);
        positiveButton.setOnTouchListener(onTouchListener);
        negativeButton.setFilterTouchesWhenObscured(true);
        negativeButton.setOnTouchListener(onTouchListener);
    }

    /**
     * @param callback The callback is called when touch event is filtered because of an overlay
     *                 window.
     */
    void setOnTouchFilteredCallback(Runnable callback) {
        mOnTouchFilteredCallback = callback;
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
    private Button getButton(@ModalDialogProperties.ButtonType int buttonType) {
        switch (buttonType) {
            case ModalDialogProperties.ButtonType.POSITIVE:
                return mPositiveButton;
            case ModalDialogProperties.ButtonType.NEGATIVE:
                return mNegativeButton;
            default:
                assert false;
                return null;
        }
    }

    /**
     * Sets button text for the specified button. If {@code buttonText} is empty or null, the
     * specified button will not be visible.
     * @param buttonType The {@link ModalDialogProperties.ButtonType} of the button.
     * @param buttonText The text to be set on the specified button.
     */
    void setButtonText(@ModalDialogProperties.ButtonType int buttonType, String buttonText) {
        getButton(buttonType).setText(buttonText);
        updateButtonVisibility();
    }

    /**
     * @param drawable The icon drawable on the positive button.
     */
    void setPositiveButtonIcon(Drawable drawable) {
        Button button = getButton(ModalDialogProperties.ButtonType.POSITIVE);
        button.setCompoundDrawablesRelativeWithIntrinsicBounds(drawable, null, null, null);
        button.setCompoundDrawablePadding(getResources().getDimensionPixelSize(
                R.dimen.modal_dialog_button_with_icon_text_padding));
        button.setPaddingRelative(getResources().getDimensionPixelSize(
                                          R.dimen.modal_dialog_button_with_icon_start_padding),
                button.getPaddingTop(), button.getPaddingEnd(), button.getPaddingBottom());
        updateButtonVisibility();
    }

    /**
     * Sets content description for the specified button.
     * @param buttonType The {@link ModalDialogProperties.ButtonType} of the button.
     * @param contentDescription The content description to be set for the specified button.
     */
    void setButtonContentDescription(
            @ModalDialogProperties.ButtonType int buttonType, String contentDescription) {
        getButton(buttonType).setContentDescription(contentDescription);
    }

    /**
     * @param buttonType The {@link ModalDialogProperties.ButtonType} of the button.
     * @param enabled Whether the specified button should be enabled.
     */
    void setButtonEnabled(@ModalDialogProperties.ButtonType int buttonType, boolean enabled) {
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
        boolean scrollViewVisible = (mTitleScrollable && titleContainerVisible)
                || messageParagraph1Visibile || messageParagraph2Visible;
        boolean footerMessageVisible = !TextUtils.isEmpty(mFooterMessageView.getText());

        mTitleView.setVisibility(titleVisible ? View.VISIBLE : View.GONE);
        mTitleIcon.setVisibility(titleIconVisible ? View.VISIBLE : View.GONE);
        mTitleContainer.setVisibility(titleContainerVisible ? View.VISIBLE : View.GONE);
        mMessageParagraph1.setVisibility(messageParagraph1Visibile ? View.VISIBLE : View.GONE);
        mScrollView.setVisibility(scrollViewVisible ? View.VISIBLE : View.GONE);
        mMessageParagraph2.setVisibility(messageParagraph2Visible ? View.VISIBLE : View.GONE);
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
}
