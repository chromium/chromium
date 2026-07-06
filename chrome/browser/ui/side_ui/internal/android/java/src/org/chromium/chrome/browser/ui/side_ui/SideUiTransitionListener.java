// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.side_ui;

import android.transition.Transition;
import android.transition.TransitionListenerAdapter;
import android.transition.TransitionManager;
import android.view.ViewGroup;

import androidx.annotation.IntDef;

import com.google.errorprone.annotations.DoNotMock;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.ViewUtils;

import java.lang.annotation.ElementType;
import java.lang.annotation.Target;

/** Tracks a {@link Transition} that updates the UI. */
@DoNotMock
@NullMarked
final class SideUiTransitionListener extends TransitionListenerAdapter {

    @IntDef({TransitionState.NOT_STARTED, TransitionState.STARTED, TransitionState.ENDED})
    @Target(ElementType.TYPE_USE)
    private @interface TransitionState {
        int NOT_STARTED = 0;
        int STARTED = 1;
        int ENDED = 2;
    }

    private @Nullable Callback<SideUiUpdateSpecs> mOnTransitionEndCallback;
    private @Nullable SideUiUpdateSpecs mSideUiUpdateSpecs;
    private @Nullable ViewGroup mSceneRoot;
    private @TransitionState int mTransitionState;

    @Override
    public void onTransitionStart(Transition transition) {
        mTransitionState = TransitionState.STARTED;
    }

    @Override
    public void onTransitionEnd(Transition transition) {
        onTransitionEndInternal();
    }

    /**
     * Starts tracking the {@link Transition} that will apply the given {@link SideUiUpdateSpecs}.
     *
     * @param sceneRoot The root of the View hierarchy to run the {@link Transition} on.
     * @param sideUiUpdateSpecs See {@link SideUiUpdateSpecs}.
     * @param onTransitionEndCallback Callback to be invoked when the {@link Transition} ends.
     */
    void startListening(
            ViewGroup sceneRoot,
            SideUiUpdateSpecs sideUiUpdateSpecs,
            Callback<SideUiUpdateSpecs> onTransitionEndCallback) {
        mSceneRoot = sceneRoot;
        mSideUiUpdateSpecs = sideUiUpdateSpecs;
        mOnTransitionEndCallback = onTransitionEndCallback;

        mTransitionState = TransitionState.NOT_STARTED;
    }

    /**
     * Ends the {@link Transition} tracked by {@link #startListening}.
     *
     * <p>If the {@link Transition} hasn't ended, this method will guarantee the following:
     *
     * <ul>
     *   <li>The layout properties are at the end state of the {@link Transition}.
     *   <li>{@code onTransitionEndCallback} is invoked.
     * </ul>
     */
    void endTransitions() {
        if (mSceneRoot == null) {
            return;
        }

        switch (mTransitionState) {
            case TransitionState.NOT_STARTED:
                // TransitionManager.beginDelayedTransition() schedules the transition to start on
                // the next layout/pre-draw pass, it's possible that a caller wants to end the
                // transition before it starts running.
                //
                // In the case above, TransitionManager.endTransitions() can only remove the pending
                // transition, and _no_ transition listener will be invoked.
                //
                // To ensure SideUiContainers and SideUiObservers are notified of the correct
                // end state, we trigger a synchronous measure and layout pass to apply the end
                // state, then explicitly invoke mOnTransitionEndCallback.
                //
                // If we rely on onTransitionStart() to manage states in the future, we may need to
                // explicitly invoke that as well, before the synchronous measure and layout pass.
                TransitionManager.endTransitions(mSceneRoot);
                ViewUtils.triggerSynchronousMeasureAndLayout(mSceneRoot);
                onTransitionEndInternal();
                break;
            case TransitionState.STARTED:
                TransitionManager.endTransitions(mSceneRoot);
                break;
            case TransitionState.ENDED:
                // Nothing to do.
                break;
        }
    }

    private void onTransitionEndInternal() {
        assert mSceneRoot != null;
        assert mSideUiUpdateSpecs != null;
        assert mOnTransitionEndCallback != null;

        mOnTransitionEndCallback.onResult(mSideUiUpdateSpecs);

        mOnTransitionEndCallback = null;
        mSideUiUpdateSpecs = null;
        mSceneRoot = null;
        mTransitionState = TransitionState.ENDED;
    }
}
