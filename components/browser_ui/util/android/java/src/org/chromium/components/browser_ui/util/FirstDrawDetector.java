// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.util;

import android.view.Choreographer;
import android.view.View;
import android.view.ViewTreeObserver;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.ref.WeakReference;

/** A utility for observing when a view gets drawn for the first time. */
@NullMarked
public class FirstDrawDetector {
    WeakReference<View> mView;
    @Nullable Runnable mCallback;
    private boolean mHasRunBefore;

    private final ViewTreeObserver.OnDrawListener mFirstDrawListener =
            new ViewTreeObserver.OnDrawListener() {
                @Override
                public void onDraw() {
                    if (mHasRunBefore) return;
                    mHasRunBefore = true;
                    // This callback will be run in the normal case (e.g., screen is on).
                    onFirstDraw();
                    // The draw listener can't be removed from within the callback, so remove it
                    // asynchronously.
                    PostTask.postTask(
                            TaskTraits.UI_BEST_EFFORT,
                            () -> {
                                View view = mView.get();
                                if (view == null) return;
                                view.getViewTreeObserver().removeOnDrawListener(this);
                            });
                }
            };
    private final ViewTreeObserver.OnPreDrawListener mFirstPredrawListener =
            new ViewTreeObserver.OnPreDrawListener() {
                @Override
                public boolean onPreDraw() {
                    // The pre-draw listener will run both when the screen is on or off, but the
                    // view might not have been drawn yet at this point. Trigger the first paint
                    // at the next frame.
                    Choreographer.getInstance()
                            .postFrameCallback(
                                    (long frameTimeNanos) -> {
                                        onFirstDraw();
                                    });
                    View view = mView.get();
                    if (view != null) {
                        view.getViewTreeObserver().removeOnPreDrawListener(this);
                    }
                    return true;
                }
            };

    private FirstDrawDetector(View view, Runnable callback) {
        mView = new WeakReference<>(view);
        mCallback = callback;
    }

    /**
     * Waits for a view to be drawn on the screen for the first time.
     *
     * @param view View whose drawing to observe.
     * @param callback Callback to trigger on first draw. Will be called on the UI thread.
     */
    public static void waitForFirstDraw(View view, Runnable callback) {
        new FirstDrawDetector(view, callback).startWaiting(view, /* strict= */ false);
    }

    /**
     * Waits for a view to be drawn on the screen for the first time. Unlike |#waitForFirstDraw()|,
     * which can trigger the callback in |#onPreDraw()|, this method waits for |#onDraw()|. This is
     * useful when the caller knows that the draw may be delayed because some {@link
     * OnPreDrawListener}s return false.
     *
     * @param view View whose drawing to observe.
     * @param callback Callback to trigger on first draw. Will be called on the UI thread.
     */
    public static void waitForFirstDrawStrict(View view, Runnable callback) {
        new FirstDrawDetector(view, callback).startWaiting(view, /* strict= */ true);
    }

    /**
     * Starts waiting for a draw to trigger the callback.
     *
     * @param strict Whether to wait for an |#onDraw| strictly. See |#waitForFirstDrawStrict()|.
     */
    private void startWaiting(View view, boolean strict) {
        view.getViewTreeObserver().addOnDrawListener(mFirstDrawListener);
        if (strict) return;
        // We use a draw listener to detect when a view is first drawn. However, if the view
        // doesn't get drawn for some reason (e.g. the screen is off), our listener will never
        // get called. To work around this, we also schedule a callback for the next frame from
        // a pre-draw listener (which will always get called). Whichever callback runs first
        // will declare the view to have been drawn.
        //
        // Note that we cannot just use a pre-draw listener here, because it does not guarantee
        // that the view has actually been drawn.
        view.getViewTreeObserver().addOnPreDrawListener(mFirstPredrawListener);
    }

    private void onFirstDraw() {
        if (mCallback != null) {
            mCallback.run();
            mCallback = null;
        }
    }
}
