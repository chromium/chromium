// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.messages.snackbar;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ObjectAnimator;
import android.app.Activity;
import android.graphics.Rect;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.view.Gravity;
import android.view.LayoutInflater;
import android.view.SurfaceView;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnLayoutChangeListener;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout.LayoutParams;
import android.widget.TextView;

import androidx.annotation.ColorInt;
import androidx.core.view.ViewCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeControllerFactory;
import org.chromium.chrome.ui.messages.R;
import org.chromium.components.browser_ui.edge_to_edge.EdgeToEdgePadAdjuster;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.text.TemplatePreservingTextView;
import org.chromium.ui.InsetObserver;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.interpolators.Interpolators;

/**
 * Visual representation of a snackbar. On phone it matches the width of the activity; on tablet it
 * has a fixed width and is anchored at the start-bottom corner of the current window.
 */
// TODO (jianli): Change this class and its methods back to package protected after the offline
// indicator experiment is done.
@NullMarked
public class SnackbarView implements InsetObserver.WindowInsetObserver {
    private static final int MAX_LINES = 5;

    private final @Nullable WindowAndroid mWindowAndroid;
    protected final ViewGroup mContainerView;
    protected final ViewGroup mSnackbarView;
    protected final TemplatePreservingTextView mMessageView;
    private final TextView mActionButtonView;
    private final ImageView mProfileImageView;
    private final int mAnimationDuration;
    private final boolean mIsTablet;
    private final @Nullable EdgeToEdgeController mEdgeToEdgeSupplier;
    private final @Nullable EdgeToEdgePadAdjuster mEdgeToEdgePadAdjuster;
    private final ViewGroup mOriginalParent;
    protected ViewGroup mParent;
    protected Snackbar mSnackbar;
    private final View mRootContentView;
    private @ColorInt int mBackgroundColor;

    // Variables used to adjust view position and size when visible frame is changed.
    private final Rect mCurrentVisibleRect = new Rect();
    private final Rect mPreviousVisibleRect = new Rect();

    private final OnLayoutChangeListener mLayoutListener =
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
                    adjustViewPosition();
                }
            };

    /**
     * Creates an instance of the {@link SnackbarView}.
     *
     * @param activity The activity that displays the snackbar.
     * @param listener An {@link OnClickListener} that will be called when the action button is
     *     clicked.
     * @param snackbar The snackbar to be displayed.
     * @param parentView The ViewGroup used to display this snackbar.
     * @param windowAndroid The WindowAndroid used for starting animation. If it is null,
     *     Animator#start is called instead.
     */
    public SnackbarView(
            Activity activity,
            OnClickListener listener,
            Snackbar snackbar,
            ViewGroup parentView,
            @Nullable WindowAndroid windowAndroid,
            boolean isTablet) {
        this(activity, listener, snackbar, parentView, windowAndroid, null, isTablet);
    }

    /**
     * Creates an instance of the {@link SnackbarView}.
     *
     * @param activity The activity that displays the snackbar.
     * @param listener An {@link OnClickListener} that will be called when the action button is
     *     clicked.
     * @param snackbar The snackbar to be displayed.
     * @param parentView The ViewGroup used to display this snackbar.
     * @param windowAndroid The WindowAndroid used for starting animation. If it is null,
     *     Animator#start is called instead.
     * @param edgeToEdgeSupplier The supplier publishes the changes of the edge-to-edge state and
     *     the expected bottom paddings when edge-to-edge is on.
     */
    public SnackbarView(
            Activity activity,
            OnClickListener listener,
            Snackbar snackbar,
            ViewGroup parentView,
            @Nullable WindowAndroid windowAndroid,
            @Nullable EdgeToEdgeController edgeToEdgeSupplier,
            boolean isTablet) {
        mIsTablet = isTablet;
        mOriginalParent = parentView;
        mWindowAndroid = windowAndroid;

        mRootContentView = activity.findViewById(android.R.id.content);
        mParent = mOriginalParent;

        int snackbarLayout =
                SnackbarManager.isFloatingSnackbarEnabled()
                        ? R.layout.floating_snackbar
                        : R.layout.snackbar;

        mContainerView =
                (ViewGroup) LayoutInflater.from(activity).inflate(snackbarLayout, mParent, false);

        // Make sure clicks are not consumed by content beneath the container view.
        mContainerView.setClickable(true);

        mSnackbarView = mContainerView.findViewById(R.id.snackbar);
        mAnimationDuration =
                mContainerView.getResources().getInteger(android.R.integer.config_mediumAnimTime);
        mMessageView =
                (TemplatePreservingTextView) mContainerView.findViewById(R.id.snackbar_message);
        mActionButtonView = (TextView) mContainerView.findViewById(R.id.snackbar_button);
        mActionButtonView.setOnClickListener(listener);
        mProfileImageView = (ImageView) mContainerView.findViewById(R.id.snackbar_profile_image);
        mEdgeToEdgeSupplier = edgeToEdgeSupplier;
        if (SnackbarManager.isFloatingSnackbarEnabled()) {
            // Add bottom margin to extend the snackbar view into the bottom window inset. This
            // margin has to be applied to the snackbar view itself to avoid weird visual clipping
            // in its dismissal animation.
            FrameLayout.LayoutParams lp = getLayoutParams();
            int bottomInsetPx =
                    edgeToEdgeSupplier != null ? edgeToEdgeSupplier.getBottomInsetPx() : 0;
            lp.bottomMargin = lp.bottomMargin + bottomInsetPx;
            mContainerView.setLayoutParams(lp);

            mEdgeToEdgePadAdjuster = null;
        } else {
            mEdgeToEdgePadAdjuster =
                    edgeToEdgeSupplier != null
                            ? EdgeToEdgeControllerFactory.createForView(mSnackbarView)
                            : null;
        }
        updateInternal(snackbar, false);
    }

    public void show() {
        addToParent();
        mContainerView.addOnLayoutChangeListener(
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
                        mContainerView.removeOnLayoutChangeListener(this);
                        mContainerView.setTranslationY(getYPositionForMoveAnimation());
                        Animator animator =
                                ObjectAnimator.ofFloat(mContainerView, View.TRANSLATION_Y, 0);
                        animator.setInterpolator(Interpolators.STANDARD_INTERPOLATOR);
                        animator.setDuration(mAnimationDuration);
                        startAnimatorOnSurfaceView(animator);
                    }
                });

        if (!SnackbarManager.isFloatingSnackbarEnabled()) {
            // We do not use mEdgeToEdgePadAdjuster if FloatingSnackbar is enabled.
            if (mEdgeToEdgeSupplier != null) {
                assumeNonNull(mEdgeToEdgePadAdjuster);
                mEdgeToEdgeSupplier.registerAdjuster(mEdgeToEdgePadAdjuster);
            }
        }
    }

    public void dismiss() {
        // Prevent clicks during dismissal animations. Intentionally not using setEnabled(false) to
        // avoid unnecessary text color changes in this transitory state.
        mActionButtonView.setOnClickListener(null);
        Animator moveAnimator =
                ObjectAnimator.ofFloat(
                        mContainerView, View.TRANSLATION_Y, getYPositionForMoveAnimation());
        moveAnimator.setInterpolator(Interpolators.DECELERATE_INTERPOLATOR);
        moveAnimator.setDuration(mAnimationDuration);
        moveAnimator.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mRootContentView.removeOnLayoutChangeListener(mLayoutListener);
                        mParent.removeView(mContainerView);

                        // Remove the pad adjuster after the animation to avoid the view
                        // changes its size before animation ends.
                        if (mEdgeToEdgeSupplier != null && mEdgeToEdgePadAdjuster != null) {
                            mEdgeToEdgePadAdjuster.destroy();
                            mEdgeToEdgeSupplier.unregisterAdjuster(mEdgeToEdgePadAdjuster);
                        }
                    }
                });
        startAnimatorOnSurfaceView(moveAnimator);
    }

    /**
     * Adjusts the position when visible area is updated, such as resizing the window, in order to
     * ensure its maximum width.
     */
    void adjustViewPosition() {
        mParent.getWindowVisibleDisplayFrame(mCurrentVisibleRect);
        // Only update if the visible frame has changed, otherwise there will be a layout loop.
        if (!mCurrentVisibleRect.equals(mPreviousVisibleRect)) {
            mPreviousVisibleRect.set(mCurrentVisibleRect);
            FrameLayout.LayoutParams lp = getLayoutParams();

            int prevWidth = lp.width;
            int prevGravity = lp.gravity;

            if (SnackbarManager.isFloatingSnackbarEnabled()) {
                // If floating snackbar is enabled, set a max width of 600dp for both mobile and
                // tablet.
                int maxWidth =
                        mParent.getResources().getDimensionPixelSize(R.dimen.snackbar_width_max);
                int snackbarMargin =
                        mParent.getResources()
                                .getDimensionPixelSize(R.dimen.snackbar_floating_margin);
                lp.width = Math.min(maxWidth, mParent.getWidth() - 2 * snackbarMargin);
                lp.gravity = Gravity.CENTER_HORIZONTAL | Gravity.BOTTOM;
            } else if (mIsTablet) {
                // Floating Snackbar disabled && mIsTablet.
                int margin =
                        mParent.getResources()
                                .getDimensionPixelSize(R.dimen.snackbar_margin_tablet);
                int width =
                        mParent.getResources().getDimensionPixelSize(R.dimen.snackbar_width_tablet);
                lp.width = Math.min(width, mParent.getWidth() - 2 * margin);
                lp.gravity = Gravity.CENTER_HORIZONTAL | Gravity.BOTTOM;
            }
            if (prevWidth != lp.width || prevGravity != lp.gravity) {
                mContainerView.setLayoutParams(lp);
            }
        }
    }

    protected int getYPositionForMoveAnimation() {
        return mContainerView.getHeight() + getLayoutParams().bottomMargin;
    }

    /**
     * @see SnackbarManager#overrideParent(ViewGroup)
     */
    void overrideParent(ViewGroup overridingParent) {
        mRootContentView.removeOnLayoutChangeListener(mLayoutListener);
        mParent = overridingParent == null ? mOriginalParent : overridingParent;
        if (mContainerView.getParent() != null) {
            ((ViewGroup) mContainerView.getParent()).removeView(mContainerView);
        }
        addToParent();
    }

    boolean isShowing() {
        return mContainerView.isShown();
    }

    void bringToFront() {
        mContainerView.bringToFront();
    }

    /**
     * Updates the accessibility pane title for mMessageView which will be read aloud if a screen
     * reader is enabled.
     */
    public void updateAccessibilityPaneTitle() {
        StringBuilder accessibilityText = new StringBuilder(mMessageView.getContentDescription());
        if (mActionButtonView.getContentDescription() != null) {
            accessibilityText
                    .append(". ")
                    .append(mActionButtonView.getContentDescription())
                    .append(". ")
                    .append(
                            mContainerView
                                    .getResources()
                                    .getString(R.string.bottom_bar_screen_position));
        }

        // This post call is required to ensure the pane title change results in a
        // reliable announcement to the user. See https://crbug.com/395925721
        mMessageView.post(
                () -> ViewCompat.setAccessibilityPaneTitle(mMessageView, accessibilityText));
    }

    /**
     * Updates the view to display data from the given snackbar. No-op if the view is already
     * showing the given snackbar.
     *
     * @param snackbar The snackbar to display
     * @return Whether update has actually been executed.
     */
    boolean update(Snackbar snackbar) {
        return updateInternal(snackbar, true);
    }

    private void addToParent() {
        mParent.addView(mContainerView);

        // Why setting listener on parent? It turns out that if we force a relayout in the layout
        // change listener of the view itself, the force layout flag will be reset to 0 when
        // layout() returns. Therefore we have to do request layout on one level above the requested
        // view.
        mRootContentView.addOnLayoutChangeListener(mLayoutListener);
    }

    // TODO(fgorski): Start using color ID, to remove the view from arguments.
    private static int calculateBackgroundColor(View view, Snackbar snackbar) {
        // Themes are used first.
        if (snackbar.getTheme() == Snackbar.Theme.GOOGLE) {
            // TODO(crbug.com/40798080): Revisit once we know whether to make this dynamic.
            return view.getContext().getColor(R.color.default_control_color_active_baseline);
        }

        assert snackbar.getTheme() == Snackbar.Theme.BASIC;
        if (snackbar.getBackgroundColor() != 0) {
            return snackbar.getBackgroundColor();
        }

        if (SnackbarManager.isFloatingSnackbarEnabled()) {
            return SemanticColorUtils.getFloatingSnackbarBackgroundColor(view.getContext());
        }

        return SemanticColorUtils.getSnackbarBackgroundColor(view.getContext());
    }

    public @ColorInt int getBackgroundColor() {
        return mBackgroundColor;
    }

    private static int getTextAppearance(Snackbar snackbar) {
        if (snackbar.getTheme() == Snackbar.Theme.GOOGLE) {
            return R.style.TextAppearance_TextMedium_Primary_OnAccent1;
        }

        assert snackbar.getTheme() == Snackbar.Theme.BASIC;
        if (snackbar.getTextAppearance() != 0) {
            return snackbar.getTextAppearance();
        }

        return R.style.TextAppearance_TextMedium_Primary;
    }

    private static int getButtonTextAppearance(Snackbar snackbar) {
        if (snackbar.getTheme() == Snackbar.Theme.GOOGLE) {
            return R.style.TextAppearance_Button_Text_Filled;
        }

        assert snackbar.getTheme() == Snackbar.Theme.BASIC;
        return R.style.TextButton;
    }

    private boolean updateInternal(Snackbar snackbar, boolean animate) {
        if (mSnackbar == snackbar) return false;
        mSnackbar = snackbar;
        mMessageView.setMaxLines(snackbar.getSingleLine() ? 1 : MAX_LINES);
        mMessageView.setTemplate(snackbar.getTemplateText());
        setViewText(mMessageView, snackbar.getText(), animate);

        mMessageView.setTextAppearance(getTextAppearance(snackbar));
        mActionButtonView.setTextAppearance(getButtonTextAppearance(snackbar));

        mBackgroundColor = calculateBackgroundColor(mContainerView, snackbar);

        if (SnackbarManager.isFloatingSnackbarEnabled()) {
            // Round the corners for snackbars in both tablets and non-tablets.
            mSnackbarView.setBackgroundResource(R.drawable.snackbar_background);
            GradientDrawable backgroundDrawable =
                    (GradientDrawable) mSnackbarView.getBackground().mutate();
            backgroundDrawable.setColor(mBackgroundColor);
        } else if (mIsTablet) {
            // isFloatingSnackbarEnabled == false, mIsTablet == true
            // On tablet, snackbars have rounded corners.
            mSnackbarView.setBackgroundResource(R.drawable.snackbar_background_tablet);
            GradientDrawable backgroundDrawable =
                    (GradientDrawable) mSnackbarView.getBackground().mutate();
            backgroundDrawable.setColor(mBackgroundColor);

            mContainerView.findViewById(R.id.snackbar_shadow_left).setVisibility(View.VISIBLE);
            mContainerView.findViewById(R.id.snackbar_shadow_right).setVisibility(View.VISIBLE);
        } else {
            // isFloatingSnackbarEnabled == false, mIsTablet == false
            mSnackbarView.setBackgroundColor(mBackgroundColor);
        }

        if (snackbar.getActionText() != null) {
            mActionButtonView.setVisibility(View.VISIBLE);
            mActionButtonView.setContentDescription(snackbar.getActionText());
            setViewText(mActionButtonView, snackbar.getActionText(), animate);
            // Set the end margin on the message view to 0 when there is action text.
            if (mMessageView.getLayoutParams() instanceof LayoutParams) {
                LayoutParams lp = (LayoutParams) mMessageView.getLayoutParams();
                lp.setMarginEnd(0);
                mMessageView.setLayoutParams(lp);
            }
        } else {
            mActionButtonView.setVisibility(View.GONE);
            // Set a non-zero end margin on the message view when there is no action text.
            if (mMessageView.getLayoutParams() instanceof LayoutParams) {
                LayoutParams lp = (LayoutParams) mMessageView.getLayoutParams();
                lp.setMarginEnd(
                        mParent.getResources()
                                .getDimensionPixelSize(R.dimen.snackbar_text_view_margin));
                mMessageView.setLayoutParams(lp);
            }
        }
        Drawable profileImage = snackbar.getProfileImage();
        if (profileImage != null) {
            mProfileImageView.setVisibility(View.VISIBLE);
            mProfileImageView.setImageDrawable(profileImage);
        } else {
            mProfileImageView.setVisibility(View.GONE);
        }
        return true;
    }

    /**
     * Starts the {@link Animator} with {@link SurfaceView} optimization disabled. If a
     * {@link SurfaceView} is not present (mWindowAndroid is null), start the {@link Animator}
     * in the normal way.
     */
    private void startAnimatorOnSurfaceView(Animator animator) {
        if (mWindowAndroid != null) {
            mWindowAndroid.startAnimationOverContent(animator);
        } else {
            animator.start();
        }
    }

    private FrameLayout.LayoutParams getLayoutParams() {
        return (FrameLayout.LayoutParams) mContainerView.getLayoutParams();
    }

    private void setViewText(TextView view, CharSequence text, boolean animate) {
        if (view.getText().toString().equals(text)) return;
        view.animate().cancel();
        if (animate) {
            view.setAlpha(0.0f);
            view.setText(text);
            view.animate().alpha(1.f).setDuration(mAnimationDuration).setListener(null);
        } else {
            view.setText(text);
        }
    }

    public ViewGroup getViewForTesting() {
        return mSnackbarView;
    }

    public @Nullable EdgeToEdgePadAdjuster getEdgeToEdgePadAdjusterForTesting() {
        return mEdgeToEdgePadAdjuster;
    }
}
