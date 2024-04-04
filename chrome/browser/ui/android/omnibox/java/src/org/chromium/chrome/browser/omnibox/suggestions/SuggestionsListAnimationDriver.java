// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import androidx.annotation.NonNull;
import androidx.core.view.WindowInsetsAnimationCompat;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.supplier.Supplier;
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

    @NonNull private final WindowAndroid mWindowAndroid;
    @NonNull private final PropertyModel mListPropertyModel;
    private WindowInsetsAnimationCompat mAnimation;
    @NonNull private final Supplier<Float> mOmniboxVerticalTranslationSupplier;
    private final float mAdditionalVerticalOffset;

    /**
     * Construct a new SuggestionsListAnimationDriver
     *
     * @param windowAndroid WindowAndroid in which the driver is operating.
     * @param listPropertyModel Property model for the suggestions list view being animated.
     * @param omniboxVerticalTranslationSupplier Supplier of the current translation of the omnibox,
     *     used to synchronize the position of the suggestions list to match.
     * @param additionalVerticalOffset Vertical compensation that should be added to vertical
     *     translation of the suggestions list during animation. The magnitude of the compensation
     *     will scale inversely with animation progress, from 100% at start to 0% at end.
     */
    public SuggestionsListAnimationDriver(
            @NonNull WindowAndroid windowAndroid,
            @NonNull PropertyModel listPropertyModel,
            @NonNull Supplier<Float> omniboxVerticalTranslationSupplier,
            int additionalVerticalOffset) {
        mWindowAndroid = windowAndroid;
        mListPropertyModel = listPropertyModel;
        mOmniboxVerticalTranslationSupplier = omniboxVerticalTranslationSupplier;
        mAdditionalVerticalOffset = additionalVerticalOffset;
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
        mListPropertyModel.set(SuggestionListProperties.CHILD_TRANSLATION_Y, 0.0f);
    }

    @NonNull
    @Override
    public void onProgress(
            @NonNull WindowInsetsCompat windowInsetsCompat,
            @NonNull List<WindowInsetsAnimationCompat> runningAnimations) {
        if (mAnimation == null || !runningAnimations.contains(mAnimation)) return;

        float interpolatedFraction = mAnimation.getInterpolatedFraction();
        mListPropertyModel.set(SuggestionListProperties.ALPHA, interpolatedFraction);
        float verticalTranslationOfOmnibox = mOmniboxVerticalTranslationSupplier.get();
        if (verticalTranslationOfOmnibox > 0.0f
                || mListPropertyModel.get(SuggestionListProperties.CHILD_TRANSLATION_Y) > 0.0f) {
            mListPropertyModel.set(
                    SuggestionListProperties.CHILD_TRANSLATION_Y,
                    verticalTranslationOfOmnibox
                            + mAdditionalVerticalOffset * (1.0f - interpolatedFraction));
        }
    }
}
