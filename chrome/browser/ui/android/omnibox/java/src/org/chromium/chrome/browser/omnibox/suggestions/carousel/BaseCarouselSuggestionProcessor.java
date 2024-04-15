// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.carousel;

import android.content.Context;

import androidx.annotation.CallSuper;
import androidx.annotation.NonNull;

import org.chromium.chrome.browser.omnibox.R;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionProcessor;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.ui.modelutil.PropertyModel;

/** The base processor implementation for the Carousel suggestions. */
public abstract class BaseCarouselSuggestionProcessor implements SuggestionProcessor {
    protected final @NonNull Context mContext;
    private final int mCarouselViewDecorationHeightPx;

    /**
     * @param context Current context.
     */
    public BaseCarouselSuggestionProcessor(Context context) {
        mContext = context;
        mCarouselViewDecorationHeightPx =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.omnibox_suggestion_header_height);
    }

    /** Returns the height of the Carousel view for use when computing view minimum height. */
    @Override
    public final int getMinimumViewHeight() {
        return mCarouselViewDecorationHeightPx + getCarouselItemViewHeight();
    }

    /** Returns the height of an element hosted by the carousel. */
    public abstract int getCarouselItemViewHeight();

    @CallSuper
    @Override
    public void populateModel(AutocompleteMatch suggestion, PropertyModel model, int matchIndex) {}
}
