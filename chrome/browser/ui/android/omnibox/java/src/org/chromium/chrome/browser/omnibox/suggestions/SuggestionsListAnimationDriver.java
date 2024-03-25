// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import androidx.annotation.NonNull;
import androidx.core.view.WindowInsetsAnimationCompat;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.components.browser_ui.widget.InsetObserver;
import org.chromium.components.browser_ui.widget.InsetObserver.WindowInsetsAnimationListener;
import org.chromium.components.browser_ui.widget.InsetObserverSupplier;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * "Driver" class that synchronizes the omnibox suggestions list appearance animation with the IME
 * appearance animation.
 */
public class SuggestionsListAnimationDriver implements WindowInsetsAnimationListener {

    private final WindowAndroid mWindowAndroid;
    private final PropertyModel mListPropertyModel;
    private WindowInsetsAnimationCompat mAnimation;

    public SuggestionsListAnimationDriver(
            WindowAndroid windowAndroid, PropertyModel listPropertyModel) {
        mWindowAndroid = windowAndroid;
        mListPropertyModel = listPropertyModel;
    }

    void onShowAnimationAboutToStart() {
        InsetObserver insetObserver = InsetObserverSupplier.getValueOrNullFrom(mWindowAndroid);
        insetObserver.addWindowInsetsAnimationListener(this);
        mListPropertyModel.set(SuggestionListProperties.ALPHA, 0.0f);
    }

    private void removeInsetListener() {
        InsetObserver insetObserver = InsetObserverSupplier.getValueOrNullFrom(mWindowAndroid);
        insetObserver.removeWindowInsetsAnimationListener(this);
    }

    @Override
    public void onPrepare(@NonNull WindowInsetsAnimationCompat animation) {
        if ((animation.getTypeMask() & WindowInsetsCompat.Type.ime()) == 0) {
            return;
        }

        mAnimation = animation;
    }

    @Override
    public void onStart(
            @NonNull WindowInsetsAnimationCompat animation,
            @NonNull WindowInsetsAnimationCompat.BoundsCompat bounds) {}

    @Override
    public void onEnd(@NonNull WindowInsetsAnimationCompat animation) {
        if (animation != mAnimation) return;
        removeInsetListener();
        mAnimation = null;
        mListPropertyModel.set(SuggestionListProperties.ALPHA, 1.0f);
    }

    @NonNull
    @Override
    public void onProgress(
            @NonNull WindowInsetsCompat windowInsetsCompat,
            @NonNull List<WindowInsetsAnimationCompat> runningAnimations) {
        if (mAnimation == null || !runningAnimations.contains(mAnimation)) return;

        mListPropertyModel.set(
                SuggestionListProperties.ALPHA, mAnimation.getInterpolatedFraction());
    }
}
