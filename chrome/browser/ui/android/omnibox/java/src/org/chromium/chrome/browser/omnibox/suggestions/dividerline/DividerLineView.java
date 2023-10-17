// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.dividerline;

import android.content.Context;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;

import androidx.core.view.ViewCompat;

import org.chromium.chrome.browser.omnibox.R;

/** View for divider line. */
public class DividerLineView extends FrameLayout {
    private View mDivider;

    /**
     * Constructs a new divider line view.
     *
     * @param context Current context.
     */
    public DividerLineView(Context context) {
        super(context, null);

        setClickable(false);
        setFocusable(false);

        mDivider = new View(context, null, 0, R.style.HorizontalDivider);
        LayoutParams dividerLayoutParams = generateDefaultLayoutParams();
        dividerLayoutParams.gravity = Gravity.TOP;
        dividerLayoutParams.width = LayoutParams.MATCH_PARENT;
        dividerLayoutParams.height = getResources().getDimensionPixelSize(R.dimen.divider_height);
        addView(mDivider, dividerLayoutParams);

        int paddingBottom =
                context.getResources()
                        .getDimensionPixelOffset(
                                R.dimen.omnibox_suggestion_list_divider_line_padding);
        ViewCompat.setPaddingRelative(this, 0, 0, 0, paddingBottom);
    }

    /**
     * @return The divider of this view.
     */
    View getDivider() {
        return mDivider;
    }
}
