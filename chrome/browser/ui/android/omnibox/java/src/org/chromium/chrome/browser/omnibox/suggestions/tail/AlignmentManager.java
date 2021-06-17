// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.tail;

import android.view.View;

import java.util.ArrayList;
import java.util.List;

/**
 * Coordinates horizontal alignment of the tail suggestions.
 * Tail suggestions are aligned to
 * - the user input in the Omnibox, when possible,
 * - to each other (left edge) when longest tail suggestion makes it impossible to align it to
 *   user input.
 *
 * Examples:
 * 1. Aligned to User input:
 *    ( User Query In Omni             )
 *    [           ... Omnibox          ]
 *    [           ... Omnibox Android  ]
 *
 * 2. Aligned to longest suggestion:
 *    ( Longer User Query In The Omni  )
 *    [             ... Omnibox        ]
 *    [             ... Omnibox Android]
 */
class AlignmentManager {
    private final List<View> mVisibleTailSuggestions = new ArrayList<>();
    private int mLongestFullTextWidth;
    private int mLongestQueryWidth;

    /**
     * Compute additional suggestion text offset for tail suggestions.
     * Ensures that tail suggestion is
     * - aligned with user query, if adjusted suggestion can still fit in the available space, or
     *   right edge otherwise,
     * - all tail suggestions are left aligned with each other.
     *
     * @param requestor View requesting the pad.
     * @param queryTextWidth Length of the text that is displayed in the suggestion.
     * @param fullTextWidth Total length of the query (user input + tail).
     * @param textAreaWidth Maximum area size available for tail suggestion.
     * @return additional padding required to properly adjust tail suggestion.
     */
    int requestStartPadding(
            View requestor, int queryTextWidth, int fullTextWidth, int textAreaWidth) {
        final int lastLongestFullTextWidth = mLongestFullTextWidth;
        mLongestFullTextWidth = Math.max(mLongestFullTextWidth, fullTextWidth);
        mLongestQueryWidth = Math.max(mLongestQueryWidth, queryTextWidth);

        // If there is enough space to render entire user text, padding is equal to everything not
        // covered by query text.
        if (textAreaWidth >= mLongestFullTextWidth) {
            return (fullTextWidth - queryTextWidth);
        }

        // Only re-layout all children, if we found a new longest text.
        if (mLongestFullTextWidth != lastLongestFullTextWidth) {
            relayoutAllViewsExcept(requestor);
        }

        return Math.max(textAreaWidth - mLongestQueryWidth, 0);
    }

    /**
     * Register view to receive layout requests in cases where it may be misaligned
     * with other views.
     *
     * @param View view to register.
     */
    void registerView(View view) {
        mVisibleTailSuggestions.add(view);
    }

    /**
     * Re-layout all views except supplied one.
     * This call is needed to re-align all existing suggestion views, if the newly added view causes
     * lack of horizontal alignment.
     *
     * @param excluded View to be excluded from re-layout process.
     */
    private void relayoutAllViewsExcept(View excluded) {
        final int count = mVisibleTailSuggestions.size();
        for (int index = 0; index < count; ++index) {
            final View view = mVisibleTailSuggestions.get(index);
            if (view == excluded) continue;
            view.requestLayout();
        }
    }
}
