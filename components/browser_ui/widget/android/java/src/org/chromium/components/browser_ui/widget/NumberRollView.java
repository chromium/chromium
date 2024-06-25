// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.animation.Animator;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.content.res.ColorStateList;
import android.util.AttributeSet;
import android.util.FloatProperty;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.chromium.ui.interpolators.Interpolators;

import java.text.NumberFormat;

/**
 * View that shows an integer number. It provides a smooth roll animation on changing the
 * number.
 */
public class NumberRollView extends FrameLayout {
    private TextView mUpNumber;
    private TextView mDownNumber;
    private float mNumber;
    private Animator mLastRollAnimator;
    private int mStringId;
    private String mStringForZero;

    /**
     * A Property wrapper around the <code>number</code> functionality handled by the {@link
     * NumberRollView#setNumberRoll(float)} and {@link NumberRollView#getNumberRoll()} methods.
     */
    public static final FloatProperty<NumberRollView> NUMBER_PROPERTY =
            new FloatProperty<NumberRollView>("") {
                @Override
                public void setValue(NumberRollView view, float value) {
                    view.setNumberRoll(value);
                }

                @Override
                public Float get(NumberRollView view) {
                    return view.getNumberRoll();
                }
            };

    /** Constructor for inflating from XML. */
    public NumberRollView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mUpNumber = findViewById(R.id.up);
        mDownNumber = findViewById(R.id.down);

        assert mUpNumber != null;
        assert mDownNumber != null;

        setNumberRoll(mNumber);
    }

    /**
     * Sets a number to display.
     * @param animate Whether it should smoothly animate to the number.
     */
    public void setNumber(int number, boolean animate) {
        if (mLastRollAnimator != null) mLastRollAnimator.cancel();

        if (animate) {
            Animator rollAnimator = ObjectAnimator.ofFloat(this, NUMBER_PROPERTY, number);
            rollAnimator.setInterpolator(Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR);
            rollAnimator.start();
            mLastRollAnimator = rollAnimator;
        } else {
            setNumberRoll(number);
        }
    }

    /**
     * @param stringId The id of the string to use for the description. The string must be a plural
     *                 that has one placeholder for a quantity.
     */
    public void setString(int stringId) {
        mStringId = stringId;
    }

    /**
     * @param stringIdForZero The id of the string to use for the description when the number is
     *     zero.
     */
    public void setStringForZero(int stringIdForZero) {
        mStringForZero = getResources().getString(stringIdForZero);
    }

    /**
     * @param string The string to use for the description when the number is 0.
     */
    public void setStringForZero(String stringForZero) {
        mStringForZero = stringForZero;
        int number = (int) mNumber;
        if (number == 0) {
            setNumberRoll(mNumber);
        }
    }

    /** Gets the current number roll position. */
    private float getNumberRoll() {
        return mNumber;
    }

    /** Sets the number roll position. */
    private void setNumberRoll(float number) {
        mNumber = number;
        int downNumber = (int) number;
        int upNumber = downNumber + 1;

        NumberFormat numberFormatter = NumberFormat.getIntegerInstance();
        String newString;
        if (mStringId != 0) {
            newString =
                    (upNumber == 0 && mStringForZero != null)
                            ? mStringForZero
                            : getResources().getQuantityString(mStringId, upNumber, upNumber);
        } else {
            newString = numberFormatter.format(upNumber);
        }
        if (!newString.equals(mUpNumber.getText().toString())) {
            mUpNumber.setText(newString);
        }

        if (mStringId != 0) {
            newString =
                    (downNumber == 0 && mStringForZero != null)
                            ? mStringForZero
                            : getResources().getQuantityString(mStringId, downNumber, downNumber);
        } else {
            newString = numberFormatter.format(downNumber);
        }
        if (!newString.equals(mDownNumber.getText().toString())) {
            mDownNumber.setText(newString);
        }

        float offset = number % 1.0f;

        mUpNumber.setTranslationY(mUpNumber.getHeight() * (offset - 1.0f));
        mDownNumber.setTranslationY(mDownNumber.getHeight() * offset);

        mUpNumber.setAlpha(offset);
        mDownNumber.setAlpha(1.0f - offset);
    }

    /** Ends any in-progress animations. */
    public void endAnimationsForTesting() {
        if (mLastRollAnimator != null) mLastRollAnimator.end();
    }

    /**
     * Update the text color with {@link ColorStateList} for both {@link TextView}.
     * @param resId The new text {@link ColorStateList} to use.
     */
    public void setTextColorStateList(ColorStateList colorStateList) {
        mUpNumber.setTextColor(colorStateList);
        mDownNumber.setTextColor(colorStateList);
    }
}
