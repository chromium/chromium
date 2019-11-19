// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.widget;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnLayoutChangeListener;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.Window;
import android.view.animation.Animation;
import android.view.animation.Animation.AnimationListener;
import android.view.animation.ScaleAnimation;

import org.chromium.chrome.browser.ui.widget.animation.Interpolators;

/**
 * ContextMenuDialog is a subclass of AlwaysDismissedDialog that ensures that the proper scale
 * animation is played upon calling {@link #show()} and {@link #dismiss()}.
 */
public class ContextMenuDialog extends AlwaysDismissedDialog {
    private static final long ENTER_ANIMATION_DURATION_MS = 250;
    // Exit animation duration should be set to 60% of the enter animation duration.
    private static final long EXIT_ANIMATION_DURATION_MS = 150;

    private final Activity mActivity;
    private final View mContentView;
    private final float mTouchPointXPx;
    private final float mTouchPointYPx;
    private final float mTopContentOffsetPx;

    private float mContextMenuSourceXPx;
    private float mContextMenuSourceYPx;
    private int mContextMenuFirstLocationYPx;

    /**
     * Creates an instance of the ContextMenuDialog.
     * @param ownerActivity The activity in which the dialog should run
     * @param theme A style resource describing the theme to use for the window, or {@code 0} to use
     *              the default dialog theme
     * @param touchPointXPx The x-coordinate of the touch that triggered the context menu.
     * @param touchPointYPx The y-coordinate of the touch that triggered the context menu.
     * @param topContentOffsetPx The offset of the content from the top.
     * @param contentView The context menu view to display on the dialog.
     */
    public ContextMenuDialog(Activity ownerActivity, int theme, float touchPointXPx,
            float touchPointYPx, float topContentOffsetPx, View contentView) {
        super(ownerActivity, theme);
        mActivity = ownerActivity;
        mTouchPointXPx = touchPointXPx;
        mTouchPointYPx = touchPointYPx;
        mTopContentOffsetPx = topContentOffsetPx;
        mContentView = contentView;
    }

    @Override
    public void show() {
        Window dialogWindow = getWindow();
        dialogWindow.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        dialogWindow.setLayout(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);

        mContentView.setVisibility(View.INVISIBLE);
        mContentView.addOnLayoutChangeListener(new OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                if (v instanceof ViewGroup) {
                    ViewGroup group = (ViewGroup) v;
                    for (int i = 0; i < group.getChildCount(); i++) {
                        if (group.getChildAt(i).getMeasuredHeight() == 0
                                && group.getChildAt(i).getVisibility() == View.VISIBLE) {
                            // Return early because not all the views have been measured, so
                            // animation pivots will be off.
                            return;
                        }
                    }
                }
                mContentView.setVisibility(View.VISIBLE);
                startEnterAnimation();
                mContentView.removeOnLayoutChangeListener(this);
            }
        });
        super.show();
    }

    private void startEnterAnimation() {
        Rect rectangle = new Rect();
        Window window = mActivity.getWindow();
        window.getDecorView().getWindowVisibleDisplayFrame(rectangle);

        float xOffsetPx = rectangle.left;
        float yOffsetPx = rectangle.top + mTopContentOffsetPx;

        int[] currentLocationOnScreenPx = new int[2];
        mContentView.getLocationOnScreen(currentLocationOnScreenPx);

        mContextMenuFirstLocationYPx = currentLocationOnScreenPx[1];

        mContextMenuSourceXPx = mTouchPointXPx - currentLocationOnScreenPx[0] + xOffsetPx;
        mContextMenuSourceYPx = mTouchPointYPx - currentLocationOnScreenPx[1] + yOffsetPx;

        Animation animation = getScaleAnimation(true, mContextMenuSourceXPx, mContextMenuSourceYPx);
        mContentView.startAnimation(animation);
    }

    @Override
    public void dismiss() {
        int[] contextMenuFinalLocationPx = new int[2];
        mContentView.getLocationOnScreen(contextMenuFinalLocationPx);
        // Recalculate mContextMenuDestinationY because the context menu's final location may not be
        // the same as its first location if it changed in height.
        float contextMenuDestinationYPx = mContextMenuSourceYPx
                + (mContextMenuFirstLocationYPx - contextMenuFinalLocationPx[1]);

        Animation exitAnimation =
                getScaleAnimation(false, mContextMenuSourceXPx, contextMenuDestinationYPx);
        exitAnimation.setAnimationListener(new AnimationListener() {
            @Override
            public void onAnimationStart(Animation animation) {}

            @Override
            public void onAnimationRepeat(Animation animation) {}

            @Override
            public void onAnimationEnd(Animation animation) {
                ContextMenuDialog.super.dismiss();
            }
        });
        mContentView.startAnimation(exitAnimation);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        if (event.getAction() == MotionEvent.ACTION_DOWN) {
            dismiss();
            return true;
        }
        return false;
    }

    /**
     * @param isEnterAnimation Whether the animation to be returned is for showing the context menu
     *                         as opposed to hiding it.
     * @param pivotX The X coordinate of the point about which the object is being scaled, specified
     *               as an absolute number where 0 is the left edge.
     * @param pivotY The Y coordinate of the point about which the object is being scaled, specified
     *               as an absolute number where 0 is the top edge.
     * @return Returns the scale animation for the context menu.
     */
    public Animation getScaleAnimation(boolean isEnterAnimation, float pivotX, float pivotY) {
        float fromX = isEnterAnimation ? 0f : 1f;
        float toX = isEnterAnimation ? 1f : 0f;
        float fromY = fromX;
        float toY = toX;

        ScaleAnimation animation = new ScaleAnimation(
                fromX, toX, fromY, toY, Animation.ABSOLUTE, pivotX, Animation.ABSOLUTE, pivotY);

        long duration = isEnterAnimation ? ENTER_ANIMATION_DURATION_MS : EXIT_ANIMATION_DURATION_MS;

        animation.setDuration(duration);
        animation.setInterpolator(Interpolators.LINEAR_OUT_SLOW_IN_INTERPOLATOR);
        return animation;
    }
}
