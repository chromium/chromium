// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.pedal;

import android.content.Context;
import android.view.KeyEvent;
import android.view.View;

import androidx.annotation.LayoutRes;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionView;
import org.chromium.chrome.browser.omnibox.suggestions.base.SimpleVerticalLayoutView;

/**
 * Base layout for pedals suggestion types. This is a {@link BaseSuggestionView} with a list of
 * Action chips.
 *
 * @param <T> The type of View being wrapped by BaseSuggestionView.
 */
public class PedalSuggestionView<T extends View> extends SimpleVerticalLayoutView {
    private final @NonNull BaseSuggestionView<T> mBaseSuggestionView;
    private final @NonNull PedalView mPedalView;

    /**
     * Constructs a new suggestion view and inflates supplied layout as the contents view.
     *
     * @param context The context used to construct the suggestion view.
     * @param layoutId Layout ID to be inflated as the BaseSuggestionView.
     */
    public PedalSuggestionView(Context context, @LayoutRes int layoutId) {
        super(context);
        setFocusable(true);
        mBaseSuggestionView = new BaseSuggestionView<T>(context, layoutId);
        mPedalView = new PedalView(context);
        addView(mBaseSuggestionView);
        addView(mPedalView);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        return mPedalView.onKeyDown(keyCode, event) || super.onKeyDown(keyCode, event);
    }

    /** @return base suggestion view. */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public BaseSuggestionView<T> getBaseSuggestionView() {
        return mBaseSuggestionView;
    }

    /** @return The {@link PedalView} in this view. */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public PedalView getPedalView() {
        return mPedalView;
    }

    @Override
    public boolean isFocused() {
        return super.isFocused() || (isSelected() && !isInTouchMode());
    }
}
