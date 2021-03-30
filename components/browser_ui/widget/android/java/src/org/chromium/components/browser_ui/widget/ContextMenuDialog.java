// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.provider.Settings;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnLayoutChangeListener;
import android.view.ViewGroup.LayoutParams;
import android.view.Window;
import android.view.animation.Animation;
import android.view.animation.Animation.AnimationListener;
import android.view.animation.ScaleAnimation;
import android.widget.FrameLayout;

import org.chromium.base.ContextUtils;
import org.chromium.components.browser_ui.widget.animation.Interpolators;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.RectProvider;

/**
 * ContextMenuDialog is a subclass of AlwaysDismissedDialog that ensures that the proper scale
 * animation is played upon calling {@link #show()} and {@link #dismiss()}.
 */
public class ContextMenuDialog extends AlwaysDismissedDialog {
    public static final int NO_CUSTOM_MARGIN = -1;

    private static final long ENTER_ANIMATION_DURATION_MS = 250;
    // Exit animation duration should be set to 60% of the enter animation duration.
    private static final long EXIT_ANIMATION_DURATION_MS = 150;
    private final Activity mActivity;
    private final View mContentView;
    private final float mTouchPointXPx;
    private final float mTouchPointYPx;
    private final float mTopContentOffsetPx;
    private final boolean mIsPopup;

    private float mContextMenuSourceXPx;
    private float mContextMenuSourceYPx;
    private int mContextMenuFirstLocationYPx;
    private AnchoredPopupWindow mPopupWindow;
    private View mLayout;

    private int mTopMarginPx;
    private int mBottomMarginPx;

    /**
     * Creates an instance of the ContextMenuDialog.
     * @param ownerActivity The activity in which the dialog should run
     * @param theme A style resource describing the theme to use for the window, or {@code 0} to use
     *              the default dialog theme
     * @param touchPointXPx The x-coordinate of the touch that triggered the context menu.
     * @param touchPointYPx The y-coordinate of the touch that triggered the context menu.
     * @param topContentOffsetPx The offset of the content from the top.
     * @param topMarginPx An explicit top margin for the dialog, or -1 to use default
     *                    defined in XML.
     * @param bottomMarginPx An explicit bottom margin for the dialog, or -1 to use default
     *                       defined in XML.
     * @param layout The context menu layout that will house the menu.
     * @param contentView The context menu view to display on the dialog.
     * @param isPopup Whether the context menu is being shown in a {@link AnchoredPopupWindow}.
     */
    public ContextMenuDialog(Activity ownerActivity, int theme, float touchPointXPx,
            float touchPointYPx, float topContentOffsetPx, int topMarginPx, int bottomMarginPx,
            View layout, View contentView, boolean isPopup) {
        super(ownerActivity, theme);
        mActivity = ownerActivity;
        mTouchPointXPx = touchPointXPx;
        mTouchPointYPx = touchPointYPx;
        mTopContentOffsetPx = topContentOffsetPx;
        mTopMarginPx = topMarginPx;
        mBottomMarginPx = bottomMarginPx;
        mContentView = contentView;
        mLayout = layout;
        mIsPopup = isPopup;
    }

    @Override
    public void onStart() {
        super.onStart();
        Window dialogWindow = getWindow();
        dialogWindow.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        dialogWindow.setLayout(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);

        // Both bottom margin and top margin must be set together to ensure default
        // values are not relied upon for custom behavior.
        if (mTopMarginPx != NO_CUSTOM_MARGIN && mBottomMarginPx != NO_CUSTOM_MARGIN) {
            // TODO(benwgold): Update to relative layout to avoid have to set fixed margin.
            FrameLayout.LayoutParams layoutParams =
                    (FrameLayout.LayoutParams) mContentView.getLayoutParams();
            if (layoutParams == null) return;
            layoutParams.bottomMargin = mBottomMarginPx;
            layoutParams.topMargin = mTopMarginPx;
        }

        (mIsPopup ? mLayout : mContentView).addOnLayoutChangeListener(new OnLayoutChangeListener() {
            @Override
            public void onLayoutChange(View v, int left, int top, int right, int bottom,
                    int oldLeft, int oldTop, int oldRight, int oldBottom) {
                if (mIsPopup) {
                    // If the menu is a popup, wait for the layout to be measured, then proceed with
                    // showing the popup window.
                    if (v.getMeasuredHeight() == 0) return;

                    final int posX = (int) mTouchPointXPx;
                    final int posY = (int) (mTouchPointYPx + mTopContentOffsetPx);
                    final Rect rect = new Rect(posX, posY, posX, posY);
                    mPopupWindow = new AnchoredPopupWindow(mActivity, mLayout,
                            new ColorDrawable(Color.TRANSPARENT), mContentView,
                            new RectProvider(rect));
                    mPopupWindow.setOutsideTouchable(false);
                    mPopupWindow.show();
                } else {
                    // Otherwise, the menu will already be in the hierarchy, and we need to make
                    // sure the menu itself is measured before starting the animation.
                    if (v.getMeasuredHeight() == 0) return;

                    startEnterAnimation();
                }
                v.removeOnLayoutChangeListener(this);
            }
        });
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
        if (mIsPopup) {
            mPopupWindow.dismiss();
            super.dismiss();

            return;
        }

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
        float durationScale =
                Settings.Global.getFloat(ContextUtils.getApplicationContext().getContentResolver(),
                        Settings.Global.ANIMATOR_DURATION_SCALE, 1f);

        animation.setDuration((long) (duration * durationScale));
        animation.setInterpolator(Interpolators.LINEAR_OUT_SLOW_IN_INTERPOLATOR);
        return animation;
    }
}
