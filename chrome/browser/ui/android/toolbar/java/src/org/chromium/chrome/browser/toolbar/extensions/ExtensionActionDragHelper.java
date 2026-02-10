// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar.extensions;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewConfiguration;

import androidx.recyclerview.widget.ItemTouchHelper;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.util.MotionEventUtils;

/** Helper class to handle the custom drag and drop interaction logic for toolbar actions. */
@NullMarked
public class ExtensionActionDragHelper implements View.OnAttachStateChangeListener {

    private final ItemTouchHelper mItemTouchHelper;
    private final RecyclerView.ViewHolder mViewHolder;
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final int mTouchSlop;
    private float mHandleDownX;
    private float mHandleDownY;

    // Distinguishes between a click and a long-press.
    private boolean mIsLongPressTriggered;

    private final Runnable mLongPressRunnable = this::onLongPress;

    /**
     * @param context Context for resources and touch slop.
     * @param itemTouchHelper Helper to start the drag interaction.
     * @param recyclerView The RecyclerView containing the actions.
     * @param viewHolder The specific {@code ViewHolder} object that wraps and manages the action
     *     button view instance.
     */
    public ExtensionActionDragHelper(
            Context context, ItemTouchHelper itemTouchHelper, RecyclerView.ViewHolder viewHolder) {
        assert itemTouchHelper != null;
        mItemTouchHelper = itemTouchHelper;
        mViewHolder = viewHolder;

        mTouchSlop = ViewConfiguration.get(context).getScaledTouchSlop();

        // Listen for detach events to clean up timers.
        mViewHolder.itemView.addOnAttachStateChangeListener(this);
    }

    /**
     * Handles touch events for the extension action icon.
     *
     * @param v The view the touch event has been dispatched.
     * @param event The MotionEvent object containing full information about the event.
     * @return True if the listener has consumed the event, false otherwise.
     */
    public boolean onTouch(View v, MotionEvent event) {
        if (MotionEventUtils.isSecondaryClick(event.getButtonState())) {
            return false;
        }

        int action = event.getActionMasked();

        if (action == MotionEvent.ACTION_DOWN) {
            v.setPressed(true);

            mHandleDownX = event.getRawX();
            mHandleDownY = event.getRawY();

            mIsLongPressTriggered = false;

            mHandler.postDelayed(mLongPressRunnable, ViewConfiguration.getLongPressTimeout());

            return true;
        } else if (action == MotionEvent.ACTION_MOVE) {
            if (mIsLongPressTriggered) {
                return false;
            }

            float deltaX = Math.abs(event.getRawX() - mHandleDownX);
            float deltaY = Math.abs(event.getRawY() - mHandleDownY);

            if (deltaX > mTouchSlop || deltaY > mTouchSlop) {
                cleanupTimer();
                v.setPressed(false);
                mItemTouchHelper.startDrag(mViewHolder);
            }

            return true;
        } else if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_CANCEL) {
            v.setPressed(false);
            cleanupTimer();

            if (!mIsLongPressTriggered && action == MotionEvent.ACTION_UP) {
                v.performClick();
            }

            mIsLongPressTriggered = false;

            return true;
        }
        return false;
    }

    @Override
    public void onViewAttachedToWindow(View v) {
        // No-op.
    }

    @Override
    public void onViewDetachedFromWindow(View v) {
        cleanupTimer();
        v.setPressed(false);
        v.removeOnAttachStateChangeListener(this);
    }

    private void cleanupTimer() {
        mHandler.removeCallbacks(mLongPressRunnable);
    }

    private void onLongPress() {
        mIsLongPressTriggered = true;
        mViewHolder.itemView.performLongClick();
    }
}
