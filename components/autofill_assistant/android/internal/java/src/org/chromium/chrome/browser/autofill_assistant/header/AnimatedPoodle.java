// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.header;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.autofill_assistant.poodle.R;

/**
 * Represents a poodle that can be animated. This default implementation is a static poodle, the
 * actual animation is implemented downstream and replaces this one. The correct version will be
 * determined at compile time via build rules.
 *
 * <p>Warning: do not rename this class or change the signature of the non-private methods
 * (including constructor) without adapting the associated downstream code.
 */
/* package */ class AnimatedPoodle {
    private final ImageView mView;

    /**
     * Create an AnimatedPoodle whose View will have a width and height of {@code viewSizePx}
     * pixels that displays a centered poodle with a width and height of {@code poodleSizePx}
     * pixels.
     */
    /* package */ AnimatedPoodle(Context context, int viewSizePx, int poodleSizePx) {
        mView = new ImageView(context);
        mView.setImageDrawable(
                AppCompatResources.getDrawable(context, R.drawable.ic_autofill_assistant_24dp));
        mView.setLayoutParams(new ViewGroup.LayoutParams(viewSizePx, viewSizePx));

        int padding = (viewSizePx - poodleSizePx) / 2;
        mView.setPadding(padding, padding, padding, padding);
    }

    /** Get the view associated to this animated poodle. */
    /* package */ View getView() {
        return mView;
    }

    /** Enable or disable the spin animation. */
    /* package */ void setSpinEnabled(boolean enabled) {
        // Do nothing.
    }
}