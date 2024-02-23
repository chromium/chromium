// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.os.Build;
import android.provider.Settings;
import android.view.MotionEvent;
import android.view.View;
import android.view.View.OnDragListener;
import android.view.View.OnLayoutChangeListener;
import android.view.ViewGroup.LayoutParams;
import android.view.Window;
import android.view.WindowManager;
import android.view.animation.Animation;
import android.view.animation.ScaleAnimation;
import android.widget.FrameLayout;

import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.ui.UiUtils;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.animation.EmptyAnimationListener;
import org.chromium.ui.dragdrop.DragEventDispatchHelper;
import org.chromium.ui.dragdrop.DragEventDispatchHelper.DragEventDispatchDestination;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.util.ColorUtils;
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
    private final boolean mIsPopup;
    private final boolean mShouldRemoveScrim;
    private final boolean mShouldSysUiMatchActivity;

    private float mContextMenuSourceXPx;
    private float mContextMenuSourceYPx;
    private int mContextMenuFirstLocationYPx;
    private @Nullable AnchoredPopupWindow mPopupWindow;
    private View mLayout;
    private OnLayoutChangeListener mOnLayoutChangeListener;
    private DragEventDispatchHelper mDragEventDispatchHelper;
    private Rect mRect;

    private int mTopMarginPx;
    private int mBottomMarginPx;

    private Integer mPopupMargin;
    private Integer mDesiredPopupContentWidth;

    /**
     * View that is showing behind the context menu. If menu is shown as a popup without scrim, this
     * view will be used to dispatch touch events other than ACTION_DOWN.
     */
    private @Nullable View mTouchEventDelegateView;

    /**
     * Creates an instance of the ContextMenuDialog.
     *
     * @param ownerActivity The activity in which the dialog should run
     * @param theme A style resource describing the theme to use for the window, or {@code 0} to use
     *     the default dialog theme
     * @param topMarginPx An explicit top margin for the dialog, or -1 to use default defined in
     *     XML.
     * @param bottomMarginPx An explicit bottom margin for the dialog, or -1 to use default defined
     *     in XML.
     * @param layout The context menu layout that will house the menu.
     * @param contentView The context menu view to display on the dialog.
     * @param isPopup Whether the context menu is being shown in a {@link AnchoredPopupWindow}.
     * @param shouldRemoveScrim Whether the context menu should removes the scrim behind the dialog
     *     visually.
     * @param shouldSysUiMatchActivity Whether the status bar and navigation bar for the dialog
     *     window should be styled to match the {@code ownerActivity}.
     * @param popupMargin The margin for the context menu.
     * @param desiredPopupContentWidth The desired width for the content of the context menu.
     * @param touchEventDelegateView View View that is showing behind the context menu. If menu is
     *     shown as a popup without scrim, and this view is provided, the context menu will dispatch
     *     touch events other than ACTION_DOWN.
     * @param rect Rect location where context menu is triggered. If this menu is a popup, the
     *     coordinates are expected to be screen coordinates.
     */
    public ContextMenuDialog(
            Activity ownerActivity,
            int theme,
            int topMarginPx,
            int bottomMarginPx,
            View layout,
            View contentView,
            boolean isPopup,
            boolean shouldRemoveScrim,
            boolean shouldSysUiMatchActivity,
            @Nullable Integer popupMargin,
            @Nullable Integer desiredPopupContentWidth,
            @Nullable View touchEventDelegateView,
            Rect rect) {
        super(ownerActivity, theme);
        mActivity = ownerActivity;
        mTopMarginPx = topMarginPx;
        mBottomMarginPx = bottomMarginPx;
        mContentView = contentView;
        mLayout = layout;
        mIsPopup = isPopup;
        mShouldRemoveScrim = shouldRemoveScrim;
        mShouldSysUiMatchActivity = shouldSysUiMatchActivity;
        mPopupMargin = popupMargin;
        mDesiredPopupContentWidth = desiredPopupContentWidth;
        mTouchEventDelegateView = touchEventDelegateView;
        mRect = rect;
    }

    @Override
    public void onStart() {
        super.onStart();
        Window dialogWindow = getWindow();
        dialogWindow.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        dialogWindow.setLayout(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
        if (mShouldRemoveScrim) {
            dialogWindow.clearFlags(WindowManager.LayoutParams.FLAG_DIM_BEHIND);
            dialogWindow.addFlags(WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL);
        }
        if (mShouldRemoveScrim || mShouldSysUiMatchActivity) {
            dialogWindow.addFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
            // Set the navigation bar when API level >= 27 to match android:navigationBarColor
            // reference in styles.xml.
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O_MR1) {
                dialogWindow.setNavigationBarColor(mActivity.getWindow().getNavigationBarColor());
                UiUtils.setNavigationBarIconColor(
                        dialogWindow.getDecorView(),
                        mActivity.getResources().getBoolean(R.bool.window_light_navigation_bar));
            }
            // Apply the status bar color in case the website had override them.
            UiUtils.setStatusBarColor(dialogWindow, mActivity.getWindow().getStatusBarColor());
            UiUtils.setStatusBarIconColor(
                    dialogWindow.getDecorView().getRootView(),
                    !ColorUtils.shouldUseLightForegroundOnBackground(
                            mActivity.getWindow().getStatusBarColor()));
        }

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

        mOnLayoutChangeListener =
                new OnLayoutChangeListener() {
                    @Override
                    public void onLayoutChange(
                            View v,
                            int left,
                            int top,
                            int right,
                            int bottom,
                            int oldLeft,
                            int oldTop,
                            int oldRight,
                            int oldBottom) {
                        // // If the layout size does not change (e.g. call due to #forceLayout), do
                        // nothing // because we don't want to dismiss the context menu.
                        if (left == oldLeft
                                && right == oldRight
                                && top == oldTop
                                && bottom == oldBottom) {
                            return;
                        }

                        if (mIsPopup) {
                            // If the menu is a popup, wait for the layout to be measured, then
                            // proceed with showing the popup window.
                            if (v.getMeasuredHeight() == 0) return;

                            // If dialog is showing and the layout changes, we might lost the anchor
                            // point.
                            // We'll dismiss the context menu and remove the listener.
                            if (mPopupWindow != null && mPopupWindow.isShowing()) {
                                dismiss();
                                return;
                            }
                            mPopupWindow =
                                    new AnchoredPopupWindow(
                                            mActivity,
                                            mLayout,
                                            new ColorDrawable(Color.TRANSPARENT),
                                            mContentView,
                                            new RectProvider(mRect));
                            if (mPopupMargin != null) {
                                mPopupWindow.setMargin(mPopupMargin);
                            }
                            if (mDesiredPopupContentWidth != null) {
                                mPopupWindow.setDesiredContentWidth(mDesiredPopupContentWidth);
                            }
                            mPopupWindow.setSmartAnchorWithMaxWidth(true);
                            mPopupWindow.setVerticalOverlapAnchor(true);
                            mPopupWindow.setOutsideTouchable(false);
                            mPopupWindow.setAnimateFromAnchor(true);
                            // Set popup focusable so the screen reader can announce the popup
                            // properly.
                            if (AccessibilityState.isScreenReaderEnabled()) {
                                mPopupWindow.setFocusable(true);
                            }
                            // If the popup is dismissed, dismiss this dialog as well. This is
                            // required when the popup is dismissed through backpress / hardware
                            // accessiries where the #dismiss is not triggered by #onTouchEvent.
                            mPopupWindow.addOnDismissListener(ContextMenuDialog.this::dismiss);
                            mPopupWindow.show();
                        } else {
                            // Otherwise, the menu will already be in the hierarchy, and we need to
                            // make sure the menu itself is measured before starting the
                            // animation.
                            if (v.getMeasuredHeight() == 0) return;

                            startEnterAnimation();
                            v.removeOnLayoutChangeListener(this);
                            mOnLayoutChangeListener = null;
                        }
                    }
                };
        (mIsPopup ? mLayout : mContentView).addOnLayoutChangeListener(mOnLayoutChangeListener);

        // Forward the drag events to delegate view if it is an DragEventDispatchDestination.
        if (isDialogNonModal()) {
            DragEventDispatchDestination dest =
                    DragEventDispatchDestination.from(mTouchEventDelegateView);
            if (dest != null) {
                mDragEventDispatchHelper = new DragEventDispatchHelper(mLayout, dest);
            }
        }
    }

    /**
     * Start the entering animation for context menu dialog. Only used when dialog is presenting
     * as a full screen dialog.
     */
    private void startEnterAnimation() {
        Rect windowRect = new Rect();
        Window window = mActivity.getWindow();
        window.getDecorView().getWindowVisibleDisplayFrame(windowRect);

        float xOffsetPx = windowRect.left;
        float yOffsetPx = windowRect.top;

        int[] currentLocationOnScreenPx = new int[2];
        mContentView.getLocationOnScreen(currentLocationOnScreenPx);

        mContextMenuFirstLocationYPx = currentLocationOnScreenPx[1];

        // Start entering animation from the center of where ContextMenu is triggered on screen.
        // Noting that the Rect already considered the top content offset of the content view that
        // context menu is hosted.
        mContextMenuSourceXPx = mRect.centerX() - currentLocationOnScreenPx[0] + xOffsetPx;
        mContextMenuSourceYPx = mRect.centerY() - currentLocationOnScreenPx[1] + yOffsetPx;

        Animation animation = getScaleAnimation(true, mContextMenuSourceXPx, mContextMenuSourceYPx);
        mContentView.startAnimation(animation);
    }

    @Override
    public void dismiss() {
        if (mIsPopup) {
            if (mPopupWindow != null) {
                mPopupWindow.dismiss();
                mPopupWindow = null;
            }
            if (mOnLayoutChangeListener != null) {
                mLayout.removeOnLayoutChangeListener(mOnLayoutChangeListener);
                mOnLayoutChangeListener = null;
            }
            if (mDragEventDispatchHelper != null) {
                mDragEventDispatchHelper.stop();
                mDragEventDispatchHelper = null;
            }
            super.dismiss();

            return;
        }

        if (mOnLayoutChangeListener != null) {
            mContentView.removeOnLayoutChangeListener(mOnLayoutChangeListener);
            mOnLayoutChangeListener = null;
        }
        int[] contextMenuFinalLocationPx = new int[2];
        mContentView.getLocationOnScreen(contextMenuFinalLocationPx);
        // Recalculate mContextMenuDestinationY because the context menu's final location may not be
        // the same as its first location if it changed in height.
        float contextMenuDestinationYPx =
                mContextMenuSourceYPx
                        + (mContextMenuFirstLocationYPx - contextMenuFinalLocationPx[1]);

        Animation exitAnimation =
                getScaleAnimation(false, mContextMenuSourceXPx, contextMenuDestinationYPx);
        exitAnimation.setAnimationListener(
                new EmptyAnimationListener() {
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
        if (isDialogNonModal() && mTouchEventDelegateView.isAttachedToWindow()) {
            return mTouchEventDelegateView.dispatchTouchEvent(event);
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
    private Animation getScaleAnimation(boolean isEnterAnimation, float pivotX, float pivotY) {
        float fromX = isEnterAnimation ? 0f : 1f;
        float toX = isEnterAnimation ? 1f : 0f;
        float fromY = fromX;
        float toY = toX;

        ScaleAnimation animation =
                new ScaleAnimation(
                        fromX,
                        toX,
                        fromY,
                        toY,
                        Animation.ABSOLUTE,
                        pivotX,
                        Animation.ABSOLUTE,
                        pivotY);

        long duration = isEnterAnimation ? ENTER_ANIMATION_DURATION_MS : EXIT_ANIMATION_DURATION_MS;
        float durationScale =
                Settings.Global.getFloat(
                        ContextUtils.getApplicationContext().getContentResolver(),
                        Settings.Global.ANIMATOR_DURATION_SCALE,
                        1f);

        animation.setDuration((long) (duration * durationScale));
        animation.setInterpolator(Interpolators.LINEAR_OUT_SLOW_IN_INTERPOLATOR);
        return animation;
    }

    private boolean isDialogNonModal() {
        return mIsPopup && mShouldRemoveScrim && mTouchEventDelegateView != null;
    }

    OnDragListener getOnDragListenerForTesting() {
        return mDragEventDispatchHelper;
    }
}
