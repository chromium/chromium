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

/**
 * Base layout for pedals suggestion types. This is a {@link BaseSuggestionView} with a list of
 * Action chips.
 *
 * @param <T> The type of the Content view.
 */
public class PedalSuggestionView<T extends View> extends BaseSuggestionView<T> {
    private final @NonNull PedalView mPedalView;

    public PedalSuggestionView(Context context, @LayoutRes int layoutId) {
        super(context, layoutId);
        mPedalView = new PedalView(context);
        addView(mPedalView, LayoutParams.forViewType(LayoutParams.SuggestionViewType.FOOTER));
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        return mPedalView.onKeyDown(keyCode, event) || super.onKeyDown(keyCode, event);
    }

    /** @return The {@link PedalView} in this view. */
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public PedalView getPedalView() {
        return mPedalView;
    }
}
