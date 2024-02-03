// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.editurl;

import android.content.Context;
import android.view.Gravity;
import android.view.View;
import android.widget.FrameLayout;

import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionView;

/** Container view for Search-Ready Omnibox suggestions. Decorates the suggestion with a divider. */
public class EditUrlSuggestionView extends FrameLayout {
    private final BaseSuggestionView<View> mContent;
    private final View mDivider;

    public EditUrlSuggestionView(Context context) {
        super(context, null);
        mContent = new BaseSuggestionView<>(context, R.layout.omnibox_basic_suggestion);
        LayoutParams contentLayoutParams = generateDefaultLayoutParams();
        contentLayoutParams.width = LayoutParams.MATCH_PARENT;
        contentLayoutParams.height = LayoutParams.WRAP_CONTENT;
        addView(mContent, contentLayoutParams);

        setFocusable(true);

        mDivider = new View(context, null, 0, R.style.HorizontalDivider);
        LayoutParams dividerLayoutParams = generateDefaultLayoutParams();
        dividerLayoutParams.gravity = Gravity.BOTTOM;
        dividerLayoutParams.width = LayoutParams.MATCH_PARENT;
        dividerLayoutParams.height = getResources().getDimensionPixelSize(R.dimen.divider_height);
        addView(mDivider, dividerLayoutParams);
    }

    /**
     * @return The base suggestion view for this edit URL suggestion.
     */
    BaseSuggestionView<View> getBaseSuggestionView() {
        return mContent;
    }

    /**
     * @return The divider of this edit URL suggestion.
     */
    View getDivider() {
        return mDivider;
    }

    @Override
    public void setSelected(boolean selected) {
        mContent.setSelected(selected);
    }
}
