// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.content.res.Configuration;
import android.graphics.drawable.Animatable;
import android.graphics.drawable.Drawable;
import android.os.Handler;
import android.util.AttributeSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.TextView;

import androidx.vectordrawable.graphics.drawable.Animatable2Compat;
import androidx.vectordrawable.graphics.drawable.AnimatedVectorDrawableCompat;

/**
 * A common base dialog view for IPH. This dialog view is composed of 3 elements from top to bottom:
 *
 * <ol>
 *   <li>An animatable drawable of educating users on how to use the feature.
 *   <li>The title.
 *   <li>The description of this feature.
 * </ol>
 *
 * <p>These elements must be set before the animation is triggered.
 *
 * TODO(https://crbug.com/352614216): Merge into PromoDialog
 */
public class IphDialogView extends LinearLayout {
    private final int mDialogHeight;
    private final int mDialogTopMargin;
    private final int mDialogTextSideMargin;
    private final int mDialogTextTopMarginPortrait;
    private final int mDialogTextTopMarginLandscape;
    private final Context mContext;
    private View mRootView;
    private long mIntervalMs = 1500; // Delay before repeating the animation.
    private ImageView mIphImageView;
    private Drawable mIphDrawable;
    private Animatable mIphAnimation;
    private Animatable2Compat.AnimationCallback mAnimationCallback;
    private ViewGroup.MarginLayoutParams mTitleTextMarginParams;
    private ViewGroup.MarginLayoutParams mDescriptionTextMarginParams;
    private int mParentViewHeight;

    public IphDialogView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
        mDialogHeight = (int) mContext.getResources().getDimension(R.dimen.iph_dialog_height);
        mDialogTopMargin =
                (int) mContext.getResources().getDimension(R.dimen.iph_dialog_top_margin);
        mDialogTextSideMargin =
                (int) mContext.getResources().getDimension(R.dimen.iph_dialog_text_side_margin);
        mDialogTextTopMarginPortrait =
                (int)
                        mContext.getResources()
                                .getDimension(R.dimen.iph_dialog_text_top_margin_portrait);
        mDialogTextTopMarginLandscape =
                (int)
                        mContext.getResources()
                                .getDimension(R.dimen.iph_dialog_text_top_margin_landscape);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mAnimationCallback =
                new Animatable2Compat.AnimationCallback() {
                    @Override
                    public void onAnimationEnd(Drawable drawable) {
                        Handler handler = new Handler();
                        handler.postDelayed(mIphAnimation::start, mIntervalMs);
                    }
                };
    }

    /**
     * @param drawable The promo drawable.
     * @param contentDescription The content description of the drawable.
     */
    public void setDrawable(Drawable drawable, String contentDescription) {
        mIphImageView = ((ImageView) findViewById(R.id.animation_drawable));
        mIphImageView.setImageDrawable(drawable);
        mIphDrawable = drawable;
        mIphAnimation = (Animatable) mIphDrawable;
    }

    /**
     * @param title The title shown in the dialog.
     */
    public void setTitle(String title) {
        TextView iphDialogTitleText = findViewById(R.id.title);
        iphDialogTitleText.setText(title);
        mTitleTextMarginParams =
                (ViewGroup.MarginLayoutParams) iphDialogTitleText.getLayoutParams();
    }

    /**
     * @param description The description shown below the title.
     */
    public void setDescription(String description) {
        TextView iphDialogDescriptionText = findViewById(R.id.description);
        iphDialogDescriptionText.setText(description);
        mDescriptionTextMarginParams =
                (ViewGroup.MarginLayoutParams) iphDialogDescriptionText.getLayoutParams();
    }

    /**
     * Setup the root view of the dialog.
     *
     * @param rootView The root view of the IPH dialog. Will be used to update the IPH view layout.
     */
    public void setRootView(View rootView) {
        mRootView = rootView;
    }

    /** Stops the IPH animation. This is called when the IPH dialog hides. */
    public void stopIPHAnimation() {
        AnimatedVectorDrawableCompat.unregisterAnimationCallback(mIphDrawable, mAnimationCallback);
        mIphAnimation.stop();
    }

    /**
     * Update the IPH view layout and start playing IPH animation. This is called when the IPH
     * dialog shows.
     */
    public void startIPHAnimation() {
        updateLayout();
        AnimatedVectorDrawableCompat.registerAnimationCallback(mIphDrawable, mAnimationCallback);
        mIphAnimation.start();
    }

    /** Update the IPH view layout based on the current size of the root view. */
    public void updateLayout() {
        if (mParentViewHeight == mRootView.getHeight()) return;
        mParentViewHeight = mRootView.getHeight();
        int orientation = mContext.getResources().getConfiguration().orientation;
        int textTopMargin;
        if (orientation == Configuration.ORIENTATION_PORTRAIT) {
            textTopMargin = mDialogTextTopMarginPortrait;
        } else {
            textTopMargin = mDialogTextTopMarginLandscape;
        }
        mTitleTextMarginParams.setMargins(
                mDialogTextSideMargin, textTopMargin, mDialogTextSideMargin, textTopMargin);
        mDescriptionTextMarginParams.setMargins(
                mDialogTextSideMargin, 0, mDialogTextSideMargin, textTopMargin);

        // The IPH view height should be at most (root view height - 2 * top margin).
        int dialogHeight = Math.min(mDialogHeight, mParentViewHeight - 2 * mDialogTopMargin);
        setMinimumHeight(dialogHeight);
    }

    /**
     * Set the interval between repeating animations.
     *
     * @param intervalMs Interval in milliseconds.
     */
    public void setIntervalMs(long intervalMs) {
        mIntervalMs = intervalMs;
    }
}
