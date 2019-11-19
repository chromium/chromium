// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.ui.widget.textbubble;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.StringRes;

import org.chromium.chrome.browser.ui.widget.R;
import org.chromium.ui.widget.RectProvider;

/**
 * UI component that handles showing a text callout bubble with a preceding image.
 */
public class ImageTextBubble extends TextBubble {
    /**
     * Constructs a {@link ImageTextBubble} instance.
     * @param context  Context to draw resources from.
     * @param rootView The {@link View} to use for size calculations and for display.
     * @param stringId The id of the string resource for the text that should be shown.
     * @param accessibilityStringId The id of the string resource of the accessibility text.
     * @param showArrow Whether the bubble should have an arrow.
     * @param anchorRectProvider The {@link RectProvider} used to anchor the text bubble.
     * @param imageDrawableId The resource id of the image to show at the start of the text bubble.
     */
    public ImageTextBubble(Context context, View rootView, @StringRes int stringId,
            @StringRes int accessibilityStringId, boolean showArrow,
            RectProvider anchorRectProvider, int imageDrawableId) {
        super(context, rootView, stringId, accessibilityStringId, showArrow, anchorRectProvider);

        ((ImageView) mContentView.findViewById(R.id.image)).setImageResource(imageDrawableId);
    }

    @Override
    protected View createContentView() {
        View view =
                LayoutInflater.from(mContext).inflate(R.layout.textbubble_text_with_image, null);
        setText((TextView) view.findViewById(R.id.message));
        return view;
    }
}
