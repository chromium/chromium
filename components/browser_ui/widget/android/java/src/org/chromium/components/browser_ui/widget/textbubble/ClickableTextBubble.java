// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.components.browser_ui.widget.textbubble;

import android.content.Context;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;

import org.chromium.ui.widget.RectProvider;

/**
 * UI component that handles showing a clickable text callout bubble.
 *
 * <p>This has special styling specific to clickable text bubbles:
 * <ul>
 *     <li>No arrow
 *     <li>Rounder corners
 *     <li>Smaller padding
 *     //TODO(sophey): Implement shadow once 9-patches are available.
 *     <li>Shadow
 * </ul>
 */
public class ClickableTextBubble extends TextBubble {
    /**
     * Constructs a {@link ClickableTextBubble} instance.
     *
     * @param context Context to draw resources from.
     * @param rootView The {@link View} to use for size calculations and for display.
     * @param stringId The id of the string resource for the text that should be shown.
     * @param accessibilityStringId The id of the string resource of the accessibility text.
     * @param anchorRectProvider The {@link RectProvider} used to anchor the text bubble.
     * @param imageDrawableId The resource id of the image to show at the start of the text bubble.
     * @param isAccessibilityEnabled Whether accessibility mode is enabled. Used to determine bubble
     * text and dismiss UX.
     * @param onTouchListener The callback for all touch events being dispatched to the bubble.
     */
    public ClickableTextBubble(Context context, View rootView, @StringRes int stringId,
            @StringRes int accessibilityStringId, RectProvider anchorRectProvider,
            @DrawableRes int imageDrawableId, boolean isAccessibilityEnabled,
            View.OnTouchListener onTouchListener) {
        super(context, rootView, stringId, accessibilityStringId, /*showArrow=*/false,
                anchorRectProvider, imageDrawableId, /*isRoundBubble=*/true,
                isAccessibilityEnabled);
        setTouchInterceptor(onTouchListener);
    }
}
