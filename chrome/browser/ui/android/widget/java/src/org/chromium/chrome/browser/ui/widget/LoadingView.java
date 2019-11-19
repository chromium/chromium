// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.widget;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.content.Context;
import android.os.SystemClock;
import android.util.AttributeSet;
import android.view.View;
import android.widget.ProgressBar;

import org.chromium.ui.interpolators.BakedBezierInterpolator;

/**
 * A {@link ProgressBar} that understands the hiding/showing policy defined in Material Design.
 */
public class LoadingView extends ProgressBar {
    private static final int LOADING_ANIMATION_DELAY_MS = 500;
    private static final int MINIMUM_ANIMATION_SHOW_TIME_MS = 500;

    private long mStartTime = -1;

    private final Runnable mDelayedShow = new Runnable() {
        @Override
        public void run() {
            if (!mShouldShow) return;
            mStartTime = SystemClock.elapsedRealtime();
            setVisibility(View.VISIBLE);
            setAlpha(1.0f);
        }
    };

    /**
     * Tracks whether the View should be displayed when {@link #mDelayedShow} is run.  Android
     * doesn't always cancel a Runnable when requested, meaning that the View could be hidden before
     * it even has a chance to be shown.
     */
    private boolean mShouldShow;

    // Material loading design spec requires us to show progress spinner at least 500ms, so we need
    // this delayed runnable to implement that.
    private final Runnable mDelayedHide = new Runnable() {
        @Override
        public void run() {
            animate()
                    .alpha(0.0f)
                    .setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE)
                    .setListener(new AnimatorListenerAdapter() {
                        @Override
                        public void onAnimationEnd(Animator animation) {
                            setVisibility(GONE);
                        }
                    });
        }
    };

    /**
     * Constructor for creating the view programatically.
     */
    public LoadingView(Context context) {
        super(context);
    }

    /**
     * Constructor for inflating from XML.
     */
    public LoadingView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Show loading UI. It shows the loading animation 500ms after.
     */
    public void showLoadingUI() {
        removeCallbacks(mDelayedShow);
        removeCallbacks(mDelayedHide);
        mShouldShow = true;

        setVisibility(GONE);
        postDelayed(mDelayedShow, LOADING_ANIMATION_DELAY_MS);
    }

    /**
     * Hide loading UI. If progress bar is not shown, it disappears immediately. If so, it smoothly
     * fades out.
     */
    public void hideLoadingUI() {
        removeCallbacks(mDelayedShow);
        removeCallbacks(mDelayedHide);
        mShouldShow = false;

        if (getVisibility() == VISIBLE) {
            postDelayed(mDelayedHide,
                    Math.max(0,
                            mStartTime + MINIMUM_ANIMATION_SHOW_TIME_MS
                                    - SystemClock.elapsedRealtime()));
        }
    }
}
