// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import android.content.Context;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;

import org.chromium.chrome.browser.omnibox.R;
import org.chromium.components.browser_ui.widget.RoundedCornerOutlineProvider;

/**
 * Container view for omnibox suggestions supplying icon decoration.
 */
class DecoratedSuggestionView<T extends View> extends SimpleHorizontalLayoutView {
    private final ImageView mSuggestionIcon;
    private T mContentView;

    /**
     * Constructs a new suggestion view.
     *
     * @param context The context used to construct the suggestion view.
     */
    DecoratedSuggestionView(Context context) {
        super(context);

        setClickable(true);
        setFocusable(true);

        mSuggestionIcon = new ImageView(getContext());
        mSuggestionIcon.setOutlineProvider(new RoundedCornerOutlineProvider(
                getResources().getDimensionPixelSize(R.dimen.default_rounded_corner_radius)));
        mSuggestionIcon.setScaleType(ImageView.ScaleType.FIT_CENTER);

        mSuggestionIcon.setLayoutParams(new LayoutParams(
                getResources().getDimensionPixelSize(R.dimen.omnibox_suggestion_icon_area_size),
                ViewGroup.LayoutParams.WRAP_CONTENT));
        addView(mSuggestionIcon);
    }

    /** Specify content view (suggestion body).  */
    void setContentView(T view) {
        if (mContentView != null) removeView(view);
        mContentView = view;
        mContentView.setLayoutParams(LayoutParams.forDynamicView());
        addView(mContentView);
    }

    /** @return Embedded suggestion content view.  */
    T getContentView() {
        return mContentView;
    }

    /** @return Widget holding suggestion decoration icon.  */
    ImageView getImageView() {
        return mSuggestionIcon;
    }

    @Override
    public boolean isFocused() {
        return super.isFocused() || (isSelected() && !isInTouchMode());
    }
}
