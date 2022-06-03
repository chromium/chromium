// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * A more button that will automatically turn to progress spinner and run an associated action when
 * clicked.
 *
 * To use, add {@link R.layout.more_progress_button} to your view hierarchy and set a click listener
 * via {@link #setOnClickRunnable()}. Initially the MoreProgressButton starts in
 * {@link State#Hidden}.
 *
 * Call {@link #setState(int)} to transition between the loading spinner, button, or hidden states.
 */
public class MoreProgressButton extends FrameLayout implements View.OnClickListener {
    /**
     * State for the button, reflects the visibility for the button and spinner
     */
    @IntDef({State.INVALID, State.HIDDEN, State.BUTTON, State.LOADING})
    @Retention(RetentionPolicy.SOURCE)
    public @interface State {
        /**
         * Internal state used before the button finished inflating.
         */
        int INVALID = -1;

        /**
         * Both the button and spinner are GONE.
         */
        int HIDDEN = 0;

        /**
         * The button is visible and the loading spinner is hidden.
         */
        int BUTTON = 1;

        /**
         * The spinner is visible and the button is hidden.
         */
        int LOADING = 2;
    }

    protected View mProgressSpinner;
    protected View mButton;
    protected Runnable mOnClickRunnable;

    protected @State int mState = State.INVALID;

    public MoreProgressButton(@NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();

        mButton = findViewById(R.id.action_button);
        mButton.setOnClickListener(this);

        mProgressSpinner = findViewById(R.id.progress_spinner);

        setState(State.HIDDEN);
    }

    @Override
    public void onClick(View view) {
        assert view == mButton;

        setState(State.LOADING);
        if (mOnClickRunnable != null) mOnClickRunnable.run();
    }

    /**
     * Set the runnable that the button will execute when the button is clicked.
     * @param onClickRunnable Runnable that will run in {@link #onClick(View)}
     */
    public void setOnClickRunnable(Runnable onClickRunnable) {
        mOnClickRunnable = onClickRunnable;
    }

    /**
     * Set the state for the more button.
     * @param state New state for the button. Must be one of {@link State#HIDDEN},
     *              {@link State#BUTTON}, or {@link State#LOADING}.
     */
    public void setState(@State int state) {
        if (state == mState) return;

        assert state != State.INVALID;

        mState = state;
        this.mButton.setVisibility(State.BUTTON == state ? View.VISIBLE : View.GONE);
        this.mProgressSpinner.setVisibility(State.LOADING == state ? View.VISIBLE : View.GONE);
    }

    @VisibleForTesting
    public @State int getStateForTest() {
        return mState;
    }
}
