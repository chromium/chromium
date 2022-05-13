// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.user_data;

import android.content.Context;
import android.graphics.PorterDuff.Mode;
import android.graphics.PorterDuffColorFilter;
import android.util.AttributeSet;

import androidx.appcompat.widget.AppCompatImageView;
import androidx.swiperefreshlayout.widget.CircularProgressDrawable;

import org.chromium.components.autofill_assistant.R;

/** View that shows a Material themed spinner. */
public class AssistantLoadingSpinner extends AppCompatImageView {
    private final CircularProgressDrawable mDrawable;

    public AssistantLoadingSpinner(Context context, AttributeSet attrs) {
        super(context, attrs);

        mDrawable = new CircularProgressDrawable(context);
        mDrawable.setStyle(CircularProgressDrawable.DEFAULT);
        mDrawable.setColorFilter(new PorterDuffColorFilter(
                context.getResources().getColor(R.color.default_text_color_link_disabled_baseline),
                Mode.MULTIPLY));
        setImageDrawable(mDrawable);
    }

    public void setAnimationState(boolean animate) {
        if (animate) {
            mDrawable.start();
        } else {
            mDrawable.stop();
        }
    }
}
