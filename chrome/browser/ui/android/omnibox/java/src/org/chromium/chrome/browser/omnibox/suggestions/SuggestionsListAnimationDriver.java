// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.os.Handler;
import android.view.View;
import android.view.View.OnAttachStateChangeListener;
import android.view.Window;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.view.WindowInsetsAnimationCompat;
import androidx.core.view.WindowInsetsCompat;
import androidx.core.view.WindowInsetsControllerCompat;
import androidx.core.view.WindowInsetsControllerCompat.OnControllableInsetsChangedListener;

import org.chromium.base.metrics.TimingMetric;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.omnibox.OmniboxMetrics;
import org.chromium.ui.InsetObserver;
import org.chromium.ui.InsetObserver.WindowInsetsAnimationListener;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/**
 * "Driver" class that synchronizes the omnibox suggestions list appearance animation with the IME
 * appearance animation.
 */
public class SuggestionsListAnimationDriver
        implements WindowInsetsAnimationListener,
                OnControllableInsetsChangedListener,
                OnAttachStateChangeListener {

    private static final long MAX_ANIMATION_DELAY_MS = 450;
    @NonNull private final InsetObserver mInsetObserver;
    @NonNull private final PropertyModel mListPropertyModel;
    @NonNull private final Handler mHandler;
    @NonNull private final Supplier<Float> mOmniboxVerticalTranslationSupplier;
    @NonNull private final Runnable mShowSuggestionsListCallback;
    private final float mAdditionalVerticalOffset;

    @NonNull final Window mWindow;

    private WindowInsetsAnimationCompat mAnimation;
    @Nullable private WindowInsetsControllerCompat mInsetsController;
    private TimingMetric mFocusToImeAnimationTimingMetric;
    private boolean mImeAnimationEnabled;
    private boolean mCancelled;

    /**
     * Construct a new SuggestionsListAnimationDriver
     *
     * @param insetObserver InsetObserver for observing changes to window insets in the relevant
     *     window.
     * @param listPropertyModel Property model for the suggestions list view being animated.
     * @param omniboxVerticalTranslationSupplier Supplier of the current translation of the omnibox,
     *     used to synchronize the position of the suggestions list to match.
     * @param showSuggestionsListCallback Callback that shows the suggestions list when invoked.
     * @param additionalVerticalOffset Vertical compensation that should be added to vertical
     *     translation of the suggestions list during animation. The magnitude of the compensation
     *     will scale inversely with animation progress, from 100% at start to 0% at end.
     * @param handler Handler on which to post tasks.
     * @param window Window in which we're operating. Used to access WindowInsetsController* APIS.
     */
    public SuggestionsListAnimationDriver(
            @NonNull InsetObserver insetObserver,
            @NonNull PropertyModel listPropertyModel,
            @NonNull Supplier<Float> omniboxVerticalTranslationSupplier,
            @NonNull Runnable showSuggestionsListCallback,
            int additionalVerticalOffset,
            @NonNull Handler handler,
            @NonNull Window window) {
        mInsetObserver = insetObserver;
        mListPropertyModel = listPropertyModel;
        mOmniboxVerticalTranslationSupplier = omniboxVerticalTranslationSupplier;
        mShowSuggestionsListCallback = showSuggestionsListCallback;
        mAdditionalVerticalOffset = additionalVerticalOffset;
        mHandler = handler;
        mWindow = window;
        mWindow.getDecorView().addOnAttachStateChangeListener(this);
    }

    void onOmniboxSessionStateChange(boolean active) {
        if (active && mImeAnimationEnabled) {
            mCancelled = false;
            mFocusToImeAnimationTimingMetric = OmniboxMetrics.recordTimeFromFocusToImeAnimation();
            mInsetObserver.addWindowInsetsAnimationListener(this);
            mHandler.postDelayed(this::cancelAnimation, MAX_ANIMATION_DELAY_MS);
        } else {
            removeInsetListener();
        }
    }

    boolean isImeAnimationEnabled() {
        return mImeAnimationEnabled;
    }

    private void removeInsetListener() {
        mInsetObserver.removeWindowInsetsAnimationListener(this);
    }

    private void cancelAnimation() {
        mShowSuggestionsListCallback.run();
        // Signal cancellation. We will keep waiting for the onPrepare event, but only for the
        // purpose of measuring the duration from focus to onPrepare.
        mCancelled = true;
    }

    // OnAttachStateChangeListener impl.
    @Override
    public void onViewAttachedToWindow(View view) {
        mInsetsController = new WindowInsetsControllerCompat(mWindow, mWindow.getDecorView());
        mInsetsController.addOnControllableInsetsChangedListener(this);
        mWindow.getDecorView().removeOnAttachStateChangeListener(this);
    }

    @Override
    public void onViewDetachedFromWindow(View view) {}

    // OnControllableInsetsChangedListener impl.
    @Override
    public void onControllableInsetsChanged(
            @NonNull WindowInsetsControllerCompat windowInsetsControllerCompat,
            int controllableTypesMask) {
        // Only enable animations if IME insets are controllable; they can't be animated if they
        // can't be controlled.
        // controllableTypesMask is a bitmask of every currently controllable type; '&
        // WindowInsetsCompat.Type.ime()' masks out IME specifically.
        mImeAnimationEnabled = (controllableTypesMask & WindowInsetsCompat.Type.ime()) != 0;
    }

    // WindowInsetsAnimationListener impl.
    @Override
    public void onPrepare(@NonNull WindowInsetsAnimationCompat animation) {
        // This method is called for any inset animation, including for types we don't care about.
        // Ignore any animation that doesn't affect IME insets.
        if ((animation.getTypeMask() & WindowInsetsCompat.Type.ime()) == 0
                || !mImeAnimationEnabled) {
            return;
        }

        mFocusToImeAnimationTimingMetric.close();
        if (mCancelled) {
            // When cancelling after a timeout, we keep our listener active until onPrepare solely
            // for the purposes of measuring the true focus->onPrepare latency. Otherwise we would
            // ignore all values higher than our timeout. We remove the listener now to avoid
            // triggering animation behavior that would not look congruent after cancellation.
            removeInsetListener();
            return;
        }

        mHandler.removeCallbacksAndMessages(null);
        mAnimation = animation;
        mListPropertyModel.set(SuggestionListProperties.ALPHA, 0.0f);
        mShowSuggestionsListCallback.run();
    }

    @Override
    public void onStart(
            @NonNull WindowInsetsAnimationCompat animation,
            @NonNull WindowInsetsAnimationCompat.BoundsCompat bounds) {}

    @Override
    public void onEnd(@NonNull WindowInsetsAnimationCompat animation) {
        if (mCancelled || animation != mAnimation) return;
        removeInsetListener();
        mAnimation = null;
        mListPropertyModel.set(SuggestionListProperties.ALPHA, 1.0f);
        mListPropertyModel.set(SuggestionListProperties.CHILD_TRANSLATION_Y, 0.0f);
    }

    @Override
    public void onProgress(
            @NonNull WindowInsetsCompat windowInsetsCompat,
            @NonNull List<WindowInsetsAnimationCompat> runningAnimations) {
        if (mCancelled || mAnimation == null || !runningAnimations.contains(mAnimation)) return;

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
