// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.page_info;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.app.Dialog;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.LinearLayout;

import androidx.annotation.NonNull;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.components.browser_ui.widget.FadingEdgeScrollView;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Represents the dialog containing the page info view. */
public class PageInfoDialog {
    private static final int ENTER_START_DELAY_MS = 100;
    private static final int ENTER_EXIT_DURATION_MS = 200;
    private static final int CLOSE_CLEANUP_DELAY_MS = 10;

    @NonNull private final PageInfoContainer mPageInfoContainer;
    @NonNull private final ViewGroup mScrollView;

    private final boolean mIsSheet;
    // The dialog implementation.
    // mSheetDialog is set if the dialog appears as a sheet. Otherwise, mModalDialog is set.
    private final Dialog mSheetDialog;
    private final PropertyModel mModalDialogModel;
    @NonNull private final ModalDialogManager mManager;
    @NonNull private final ModalDialogProperties.Controller mController;

    // Animation which is currently running, if there is one.
    private Animator mCurrentAnimation;

    private boolean mDismissWithoutAnimation;

    /**
     * Creates a new page info dialog. The dialog can appear as a sheet (using Android dialogs) or a
     * standard dialog (using modal dialogs).
     *
     * @param context The context used for creating the dialog.
     * @param containerView The pageInfoContainer the dialog is shown in.
     * @param isSheet Whether the dialog should appear as a sheet.
     * @param manager The dialog's manager used for modal dialogs.
     * @param controller The dialog's controller.
     *
     */
    public PageInfoDialog(
            Context context,
            @NonNull PageInfoContainer pageInfoContainer,
            View containerView,
            boolean isSheet,
            @NonNull ModalDialogManager manager,
            @NonNull ModalDialogProperties.Controller controller) {
        mPageInfoContainer = pageInfoContainer;
        mIsSheet = isSheet;
        mManager = manager;
        mController = controller;

        if (isSheet) {
            // On smaller screens, make the dialog fill the width of the screen.
            mScrollView = createSheetContainer(context, containerView);
        } else {
            // On larger screens, modal dialog already has an maximum width set.
            mScrollView = new FadingEdgeScrollView(context, null);
        }

        mScrollView.setVisibility(View.INVISIBLE);
        mScrollView.addOnLayoutChangeListener(
                new View.OnLayoutChangeListener() {
                    @Override
                    public void onLayoutChange(
                            View v, int l, int t, int r, int b, int ol, int ot, int or, int ob) {
                        // Trigger the entrance animations once the main container has been laid out
                        // and has a height.
                        mScrollView.removeOnLayoutChangeListener(this);
                        mScrollView.setVisibility(View.VISIBLE);
                        createDialogSlideAnimaton(true, null).start();
                    }
                });

        mScrollView.addView(pageInfoContainer);

        if (isSheet) {
            mSheetDialog = createSheetDialog(context, mScrollView);
            mModalDialogModel = null;
        } else {
            mModalDialogModel = createModalDialog(mScrollView);
            mSheetDialog = null;
        }
    }

    public void destroy() {
        dismiss(false);
    }

    /** Shows the dialogs. */
    public void show() {
        if (mIsSheet) {
            mSheetDialog.show();
        } else {
            mManager.showDialog(mModalDialogModel, ModalDialogManager.ModalDialogType.APP);
        }
    }

    /**
     * Hides the dialog.
     *
     * @param animated Whether to animate the transition to hidden.
     */
    public void dismiss(boolean animated) {
        mDismissWithoutAnimation = !animated;
        if (mIsSheet) {
            mSheetDialog.dismiss();
        } else {
            mManager.dismissDialog(mModalDialogModel, DialogDismissalCause.UNKNOWN);
        }
    }

    private Dialog createSheetDialog(Context context, View container) {
        Dialog sheetDialog =
                new Dialog(context) {
                    private void superDismiss() {
                        super.dismiss();
                    }

                    // Cancels any animation or queued callbacks for dismissing the dialog.
                    private void cancelAnimatedDismiss() {
                        if (mCurrentAnimation != null && mCurrentAnimation.isRunning()) {
                            mCurrentAnimation.cancel();
                        }
                        mScrollView.removeCallbacks(null);
                    }

                    @Override
                    public void dismiss() {
                        if (mDismissWithoutAnimation) {
                            cancelAnimatedDismiss();
                            // Dismiss the modal dialogs without any custom animations.
                            super.dismiss();
                        } else {
                            createDialogSlideAnimaton(
                                            false,
                                            () -> {
                                                // onAnimationEnd is called during the final frame
                                                // of the animation.
                                                // Delay the cleanup by a tiny amount to give this
                                                // frame a chance to be displayed before we
                                                // destroy the dialog.
                                                mScrollView.postDelayed(
                                                        this::superDismiss, CLOSE_CLEANUP_DELAY_MS);
                                            })
                                    .start();
                        }
                    }
                };
        sheetDialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        sheetDialog.setCanceledOnTouchOutside(true);

        Window window = sheetDialog.getWindow();
        window.setGravity(Gravity.TOP);
        window.setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));

        sheetDialog.setOnDismissListener(
                dialog -> mController.onDismiss(null, DialogDismissalCause.UNKNOWN));

        sheetDialog.addContentView(
                container,
                new LinearLayout.LayoutParams(
                        LinearLayout.LayoutParams.MATCH_PARENT,
                        LinearLayout.LayoutParams.MATCH_PARENT));

        // This must be called after addContentView, or it won't fully fill to the edge.
        window.setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);

        return sheetDialog;
    }

    private PropertyModel createModalDialog(View container) {
        return new PropertyModel.Builder(ModalDialogProperties.ALL_KEYS)
                .with(ModalDialogProperties.CONTROLLER, mController)
                .with(ModalDialogProperties.CUSTOM_VIEW, container)
                .with(ModalDialogProperties.CANCEL_ON_TOUCH_OUTSIDE, true)
                .build();
    }

    /** Create a container for PageInfo when it is shown as a top-sheet. */
    private ViewGroup createSheetContainer(Context context, View containerView) {
        return new FadingEdgeScrollView(context, null) {
            {
                if (mPageInfoContainer != null) {
                    int padding =
                            (int)
                                    context.getResources()
                                            .getDimension(R.dimen.page_info_popup_corners_radius);
                    Drawable background =
                            AppCompatResources.getDrawable(getContext(), R.drawable.page_info_bg);
                    setBackground(background);
                    setPadding(0, 0, 0, padding);
                }
            }

            @Override
            protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
                heightMeasureSpec =
                        MeasureSpec.makeMeasureSpec(
                                containerView != null ? containerView.getHeight() * 90 / 100 : 0,
                                MeasureSpec.AT_MOST);
                super.onMeasure(widthMeasureSpec, heightMeasureSpec);
            }
        };
    }

    /**
     * Create an animator to show/hide the entire dialog as a slide animation.
     * On phones the dialog is slid in as a sheet. Otherwise, the default fade-in is used.
     */
    private Animator createDialogSlideAnimaton(boolean isEnter, Runnable onAnimationEnd) {
        Animator dialogAnimation;
        if (mIsSheet) {
            final float animHeight = -mScrollView.getHeight();
            ObjectAnimator translateAnim;
            if (isEnter) {
                mScrollView.setTranslationY(animHeight);
                translateAnim = ObjectAnimator.ofFloat(mScrollView, View.TRANSLATION_Y, 0f);
                translateAnim.setInterpolator(Interpolators.LINEAR_OUT_SLOW_IN_INTERPOLATOR);
            } else {
                translateAnim = ObjectAnimator.ofFloat(mScrollView, View.TRANSLATION_Y, animHeight);
                translateAnim.setInterpolator(Interpolators.FAST_OUT_LINEAR_IN_INTERPOLATOR);
            }
            translateAnim.setDuration(ENTER_EXIT_DURATION_MS);
            dialogAnimation = translateAnim;
        } else {
            dialogAnimation = new AnimatorSet();
        }

        if (isEnter) dialogAnimation.setStartDelay(ENTER_START_DELAY_MS);
        dialogAnimation.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mCurrentAnimation = null;
                        if (onAnimationEnd == null) return;
                        onAnimationEnd.run();
                    }
                });
        if (mCurrentAnimation != null) mCurrentAnimation.cancel();
        mCurrentAnimation = dialogAnimation;
        return dialogAnimation;
    }
}
