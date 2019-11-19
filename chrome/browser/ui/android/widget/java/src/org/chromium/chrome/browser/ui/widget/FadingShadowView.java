// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.widget;

import android.content.Context;
import android.graphics.Canvas;
import android.util.AttributeSet;
import android.view.View;

/**
 * A class that encapsulates {@link FadingShadow} as a self-contained view.
 */
public class FadingShadowView extends View {
    private FadingShadow mFadingShadow;
    private int mPosition;
    private float mStrength = 1f;

    /**
     * Constructor for inflating from XML.
     */
    public FadingShadowView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Sets shadow color and position, please refer to the below function. Note that this function
     * must be called before using this class.
     * @see FadingShadow#drawShadow(View, Canvas, int, float, float)
     */
    public void init(int shadowColor, int position) {
        mFadingShadow = new FadingShadow(shadowColor);
        mPosition = position;
        postInvalidateOnAnimation();
    }

    /**
     * Sets the shadow strength. Please refer to the below function's shadowStrength argument.
     * @see FadingShadow#drawShadow(View, Canvas, int, float, float)
     */
    public void setStrength(float strength) {
        if (mStrength != strength) {
            mStrength = strength;
            postInvalidateOnAnimation();
        }
    }

    /**
     * Gets the shadow strength. Please refer to the below function's shadowStrength argument.
     * @see FadingShadow#drawShadow(View, Canvas, int, float, float)
     */
    public float getStrength() {
        return mStrength;
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        if (mFadingShadow != null) {
            mFadingShadow.drawShadow(this, canvas, mPosition, getHeight(), mStrength);
        }
    }
}
