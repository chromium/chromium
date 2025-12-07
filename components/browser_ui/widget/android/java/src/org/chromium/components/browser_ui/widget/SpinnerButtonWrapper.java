// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.ProgressBar;

import androidx.annotation.ColorInt;
import androidx.annotation.DimenRes;
import androidx.annotation.StringRes;

import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * A layout that will automatically play a progress spinner inside the button and run an associated
 * action when clicked. The button will not reset afterwards and the spinner will keep running.
 */
@NullMarked
public class SpinnerButtonWrapper extends FrameLayout implements View.OnClickListener {
    private @ColorInt int mSpinnerColor;
    private @DimenRes int mSpinnerSize;
    private Button mButton;
    private @StringRes int mOnButtonClickContentDescription;
    private ProgressBar mSpinner;
    private Runnable mOnButtonClickRunnable;

    /**
     * Creates a spinner button wrapper and adds a custom button view to its hierarchy.
     *
     * @param context The current context.
     * @param button The button view to add.
     * @param onButtonClickContentDescription The content description read for the spinner state.
     * @param spinnerSize The width and height that the spinner should be set to.
     * @param spinnerColor The color for the spinner.
     * @param onButtonClickRunnable The runnable to execute when the spinner button is clicked.
     * @return The SpinnerButtonWrapper to be added to an XML hierarchy after inflation.
     */
    public static SpinnerButtonWrapper createSpinnerButtonWrapper(
            Context context,
            Button button,
            @StringRes int onButtonClickContentDescription,
            @DimenRes int spinnerSize,
            @ColorInt int spinnerColor,
            Runnable onButtonClickRunnable) {
        SpinnerButtonWrapper spinnerButtonWrapper =
                (SpinnerButtonWrapper)
                        LayoutInflater.from(context).inflate(R.layout.spinner_button_wrapper, null);
        spinnerButtonWrapper.setupButton(
                button,
                onButtonClickContentDescription,
                spinnerSize,
                spinnerColor,
                onButtonClickRunnable);
        return spinnerButtonWrapper;
    }

    public SpinnerButtonWrapper(Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();
        mSpinner = findViewById(R.id.progress_bar);
    }

    /** Helper for setting up the custom button. */
    @Initializer
    private void setupButton(
            Button button,
            @StringRes int onButtonClickContentDescription,
            @DimenRes int spinnerSize,
            @ColorInt int spinnerColor,
            Runnable onButtonClickRunnable) {
        mButton = button;
        mOnButtonClickContentDescription = onButtonClickContentDescription;
        mSpinnerSize = spinnerSize;
        mSpinnerColor = spinnerColor;
        mOnButtonClickRunnable = onButtonClickRunnable;
        mButton.setOnClickListener(this);

        if (mButton.getParent() != null) {
            ((ViewGroup) mButton.getParent()).removeView(mButton);
        }
        addView(mButton);
        bringChildToFront(mSpinner);
    }

    /**
     * OnClickListener implementation. When the spinner button is clicked, the text will be replaced
     * with an indeterminate spinner animation and a runnable will be executed for any client that
     * wishes to run an action as result of a button click. The button will not be reset afterwards.
     * This overrides any previously implemented onClickListener on the button being passed in.
     */
    @Override
    public void onClick(View view) {
        Resources resources = getContext().getResources();
        int spinnerSizePx = resources.getDimensionPixelSize(mSpinnerSize);
        var spinnerLayoutParams = mSpinner.getLayoutParams();
        spinnerLayoutParams.width = spinnerSizePx;
        spinnerLayoutParams.height = spinnerSizePx;
        mSpinner.setLayoutParams(spinnerLayoutParams);

        // Preserve the button width when hiding the text.
        int buttonWidth = mButton.getWidth();
        var buttonLayoutParams = mButton.getLayoutParams();
        buttonLayoutParams.width = buttonWidth;
        mButton.setLayoutParams(buttonLayoutParams);
        mButton.setTextScaleX(0f);

        mButton.setClickable(false);
        mButton.setContentDescription(resources.getString(mOnButtonClickContentDescription));
        mSpinner.setIndeterminateTintList(ColorStateList.valueOf(mSpinnerColor));
        mSpinner.setVisibility(View.VISIBLE);
        mOnButtonClickRunnable.run();
    }
}
