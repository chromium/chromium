// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.banners;

import static org.chromium.cc.mojom.RootScrollOffsetUpdateFrequency.ALL_UPDATES;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ObjectAnimator;
import android.content.Context;
import android.graphics.Region;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.animation.DecelerateInterpolator;
import android.view.animation.Interpolator;
import android.widget.FrameLayout;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.MathUtils;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.WebContents;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * View that slides up from the bottom of the page and slides away as the user scrolls the page.
 * Meant to be tacked onto the {@link org.chromium.content_public.browser.WebContents}'s view and
 * alerted when either the page scroll position or viewport size changes.
 *
 * GENERAL BEHAVIOR
 * This View is brought onto the screen by sliding upwards from the bottom of the screen.  Afterward
 * the View slides onto and off of the screen vertically as the user scrolls upwards or
 * downwards on the page.
 *
 * As the scroll offset or the viewport height are updated via a scroll or fling, the difference
 * from the initial value is used to determine the View's Y-translation.  If a gesture is stopped,
 * the View will be snapped back into the center of the screen or entirely off of the screen, based
 * on how much of the View is visible, or where the user is currently located on the page.
 */
public abstract class SwipableOverlayView extends FrameLayout {
    private static final float FULL_THRESHOLD = 0.5f;
    private static final float VERTICAL_FLING_SHOW_THRESHOLD = 0.2f;
    private static final float VERTICAL_FLING_HIDE_THRESHOLD = 0.9f;

    @IntDef({Gesture.NONE, Gesture.SCROLLING, Gesture.FLINGING})
    @Retention(RetentionPolicy.SOURCE)
    private @interface Gesture {
        int NONE = 0;
        int SCROLLING = 1;
        int FLINGING = 2;
    }

    private static final long ANIMATION_DURATION_MS = 250;

    /** Detects when the user is dragging the WebContents. */
    @Nullable protected final GestureStateListener mGestureStateListener;

    /** Listens for changes in the layout. */
    private final View.OnLayoutChangeListener mLayoutChangeListener;

    /** Interpolator used for the animation. */
    private final Interpolator mInterpolator;

    /** Tracks whether the user is scrolling or flinging. */
    private @Gesture int mGestureState;

    /** Animation currently being used to translate the View. */
    private Animator mCurrentAnimation;

    /** Used to determine when the layout has changed and the Viewport must be updated. */
    private int mParentHeight;

    /** Offset from the top of the page when the current gesture was first started. */
    private int mInitialOffsetY;

    /** How tall the View is, including its margins. */
    private int mTotalHeight;

    /** Whether or not the View ever been fully displayed. */
    private boolean mIsBeingDisplayedForFirstTime;

    /** The WebContents to which the overlay is added. */
    private WebContents mWebContents;

    /**
     * Creates a SwipableOverlayView.
     * @param context Context for acquiring resources.
     * @param attrs Attributes from the XML layout inflation.
     */
    public SwipableOverlayView(Context context, AttributeSet attrs) {
        this(context, attrs, true);
    }

    /**
     * Creates a SwipableOverlayView.
     * @param context Context for acquiring resources.
     * @param attrs Attributes from the XML layout inflation.
     * @param hideOnScroll Whether this view should observe user's gesture and then auto-hide when
     *                     page is scrolled down.
     */
    public SwipableOverlayView(Context context, AttributeSet attrs, boolean hideOnScroll) {
        super(context, attrs);
        mGestureStateListener = hideOnScroll ? createGestureStateListener() : null;
        mGestureState = Gesture.NONE;
        mLayoutChangeListener = createLayoutChangeListener();
        mInterpolator = new DecelerateInterpolator(1.0f);

        // We make this view 'draw' to provide a placeholder for its animations.
        setWillNotDraw(false);
    }

    /** Set the given WebContents for scrolling changes. */
    public void setWebContents(WebContents webContents) {
        if (mWebContents != null) {
            GestureListenerManager.fromWebContents(mWebContents)
                    .removeListener(mGestureStateListener);
        }

        mWebContents = webContents;
        // See comment in onLayout() as to why the listener is only attached if mTotalHeight is > 0.
        if (mWebContents != null && mTotalHeight > 0) {
            GestureListenerManager.fromWebContents(mWebContents)
                    .addListener(mGestureStateListener, ALL_UPDATES);
        }
    }

    public WebContents getWebContents() {
        return mWebContents;
    }

    protected int getTotalHeight() {
        return mTotalHeight;
    }

    protected void addToParentView(ViewGroup parentView) {
        if (parentView == null) return;
        if (getParent() == null) {
            parentView.addView(this, createLayoutParams());

            // Listen for the layout to know when to animate the View coming onto the screen.
            addOnLayoutChangeListener(mLayoutChangeListener);
        }
    }

    protected void addToParentViewAtIndex(ViewGroup parentView, int index) {
        if (parentView == null) return;
        if (getParent() == null) {
            parentView.addView(this, index, createLayoutParams());

            // Listen for the layout to know when to animate the View coming onto the screen.
            addOnLayoutChangeListener(mLayoutChangeListener);
        }
    }

    /**
     * Removes the SwipableOverlayView from its parent and stops monitoring the WebContents.
     * @return Whether the View was removed from its parent.
     */
    public boolean removeFromParentView() {
        if (getParent() == null) return false;

        ((ViewGroup) getParent()).removeView(this);
        removeOnLayoutChangeListener(mLayoutChangeListener);
        return true;
    }

    /**
     * Creates a set of LayoutParams that makes the View hug the bottom of the screen.  Override it
     * for other types of behavior.
     * @return LayoutParams for use when adding the View to its parent.
     */
    public ViewGroup.MarginLayoutParams createLayoutParams() {
        return new FrameLayout.LayoutParams(
                LayoutParams.MATCH_PARENT,
                LayoutParams.WRAP_CONTENT,
                Gravity.BOTTOM | Gravity.CENTER_HORIZONTAL);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        if (!isAllowedToAutoHide()) setTranslationY(0.0f);
    }

    @Override
    protected void onDetachedFromWindow() {
        super.onDetachedFromWindow();
        cancelCurrentAnimation();
    }

    @Override
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        super.onWindowFocusChanged(hasWindowFocus);
        if (!isAllowedToAutoHide()) setTranslationY(0.0f);
    }

    /** See {@link #android.view.ViewGroup.onLayout(boolean, int, int, int, int)}. */
    @Override
    protected void onLayout(boolean changed, int l, int t, int r, int b) {
        // Update the viewport height when the parent View's height changes (e.g. after rotation).
        int currentParentHeight = getParent() == null ? 0 : ((View) getParent()).getHeight();
        if (mParentHeight != currentParentHeight) {
            mParentHeight = currentParentHeight;
            mGestureState = Gesture.NONE;
            if (mCurrentAnimation != null) mCurrentAnimation.end();
        }

        // Update the known effective height of the View.
        MarginLayoutParams params = (MarginLayoutParams) getLayoutParams();
        mTotalHeight = getMeasuredHeight() + params.topMargin + params.bottomMargin;

        // Adding a listener to GestureListenerManager results in extra IPCs on every frame, which
        // is very costly. Only attach the listener if needed.
        if (mWebContents != null && mGestureStateListener != null) {
            if (mTotalHeight > 0) {
                GestureListenerManager.fromWebContents(mWebContents)
                        .addListener(mGestureStateListener, ALL_UPDATES);
            } else {
                GestureListenerManager.fromWebContents(mWebContents)
                        .removeListener(mGestureStateListener);
            }
        }

        super.onLayout(changed, l, t, r, b);
    }

    /**
     * Creates a listener than monitors the WebContents for scrolls and flings.
     * The listener updates the location of this View to account for the user's gestures.
     * @return GestureStateListener to send to the WebContents.
     */
    private GestureStateListener createGestureStateListener() {
        return new GestureStateListener() {
            /** Tracks the previous event's scroll offset to determine if a scroll is up or down. */
            private int mLastScrollOffsetY;

            /** Location of the View when the current gesture was first started. */
            private float mInitialTranslationY;

            /** The initial extent of the scroll when triggered. */
            private float mInitialExtentY;

            @Override
            public void onFlingStartGesture(
                    int scrollOffsetY, int scrollExtentY, boolean isDirectionUp) {
                if (!isAllowedToAutoHide() || !cancelCurrentAnimation()) return;
                resetInternalScrollState(scrollOffsetY, scrollExtentY);
                mGestureState = Gesture.FLINGING;
            }

            @Override
            public void onFlingEndGesture(int scrollOffsetY, int scrollExtentY) {
                if (mGestureState != Gesture.FLINGING) return;
                mGestureState = Gesture.NONE;

                updateTranslation(scrollOffsetY, scrollExtentY);

                boolean isScrollingDownward = scrollOffsetY > mLastScrollOffsetY;

                boolean isVisibleInitially = mInitialTranslationY < mTotalHeight;
                float percentageVisible = 1.0f - (getTranslationY() / mTotalHeight);
                float visibilityThreshold =
                        isVisibleInitially
                                ? VERTICAL_FLING_HIDE_THRESHOLD
                                : VERTICAL_FLING_SHOW_THRESHOLD;
                boolean isVisibleEnough = percentageVisible > visibilityThreshold;
                boolean isNearTopOfPage = scrollOffsetY < (mTotalHeight * FULL_THRESHOLD);

                boolean show = (!isScrollingDownward && isVisibleEnough) || isNearTopOfPage;

                runUpEventAnimation(show);
            }

            @Override
            public void onScrollStarted(
                    int scrollOffsetY, int scrollExtentY, boolean isDirectionUp) {
                if (!isAllowedToAutoHide() || !cancelCurrentAnimation()) return;
                resetInternalScrollState(scrollOffsetY, scrollExtentY);
                mLastScrollOffsetY = scrollOffsetY;
                mGestureState = Gesture.SCROLLING;
            }

            @Override
            public void onScrollEnded(int scrollOffsetY, int scrollExtentY) {
                if (mGestureState != Gesture.SCROLLING) return;
                mGestureState = Gesture.NONE;

                updateTranslation(scrollOffsetY, scrollExtentY);

                runUpEventAnimation(shouldSnapToVisibleState(scrollOffsetY));
            }

            @Override
            public void onScrollOffsetOrExtentChanged(int scrollOffsetY, int scrollExtentY) {
                mLastScrollOffsetY = scrollOffsetY;

                if (!shouldConsumeScroll(scrollOffsetY, scrollExtentY)) {
                    resetInternalScrollState(scrollOffsetY, scrollExtentY);
                    return;
                }

                // This function is called for both fling and scrolls.
                if (mGestureState == Gesture.NONE
                        || !cancelCurrentAnimation()
                        || isIndependentlyAnimating()) {
                    return;
                }

                updateTranslation(scrollOffsetY, scrollExtentY);
            }

            private void updateTranslation(int scrollOffsetY, int scrollExtentY) {
                float scrollDiff =
                        (scrollOffsetY - mInitialOffsetY) + (scrollExtentY - mInitialExtentY);
                float translation =
                        MathUtils.clamp(mInitialTranslationY + scrollDiff, mTotalHeight, 0);

                // If the container has reached the completely shown position, reset the initial
                // scroll so any movement will start hiding it again.
                if (translation <= 0f) resetInternalScrollState(scrollOffsetY, scrollExtentY);

                setTranslationY(translation);
            }

            /** Resets the internal values that a scroll or fling will base its calculations off of. */
            private void resetInternalScrollState(int scrollOffsetY, int scrollExtentY) {
                mInitialOffsetY = scrollOffsetY;
                mInitialExtentY = scrollExtentY;
                mInitialTranslationY = getTranslationY();
            }
        };
    }

    /**
     * @param scrollOffsetY The current scroll offset on the Y axis.
     * @param scrollExtentY The current scroll extent on the Y axis.
     * @return Whether or not the scroll should be consumed by the view.
     */
    protected boolean shouldConsumeScroll(int scrollOffsetY, int scrollExtentY) {
        return true;
    }

    /**
     * @param scrollOffsetY The current scroll offset on the Y axis.
     * @return Whether the view should snap to a visible state.
     */
    protected boolean shouldSnapToVisibleState(int scrollOffsetY) {
        boolean isNearTopOfPage = scrollOffsetY < (mTotalHeight * FULL_THRESHOLD);
        boolean isVisibleEnough = getTranslationY() < mTotalHeight * FULL_THRESHOLD;
        return isNearTopOfPage || isVisibleEnough;
    }

    /** @return Whether or not the view is animating independent of the user's scroll position. */
    protected boolean isIndependentlyAnimating() {
        return false;
    }

    /**
     * Creates a listener that is used only to animate the View coming onto the screen.
     * @return The SimpleOnGestureListener that will monitor the View.
     */
    private View.OnLayoutChangeListener createLayoutChangeListener() {
        return new View.OnLayoutChangeListener() {
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
                removeOnLayoutChangeListener(mLayoutChangeListener);

                // Animate the View coming in from the bottom of the screen.
                setTranslationY(mTotalHeight);
                mIsBeingDisplayedForFirstTime = true;
                runUpEventAnimation(true);
            }
        };
    }

    /**
     * Create an animation that snaps the View into position vertically.
     * @param visible If true, snaps the View to the bottom-center of the screen.  If false,
     *                translates the View below the bottom-center of the screen so that it is
     *                effectively invisible.
     * @return An animator with the snap animation.
     */
    protected Animator createVerticalSnapAnimation(boolean visible) {
        float targetTranslationY = visible ? 0.0f : mTotalHeight;
        float yDifference = Math.abs(targetTranslationY - getTranslationY()) / mTotalHeight;
        long duration = Math.max(0, (long) (ANIMATION_DURATION_MS * yDifference));

        Animator animator = ObjectAnimator.ofFloat(this, View.TRANSLATION_Y, targetTranslationY);
        animator.setDuration(duration);
        animator.setInterpolator(mInterpolator);

        return animator;
    }

    /**
     * Run an animation when a gesture has ended (an 'up' motion event).
     * @param visible Whether or not the view should be visible.
     */
    protected void runUpEventAnimation(boolean visible) {
        if (mCurrentAnimation != null) mCurrentAnimation.cancel();
        mCurrentAnimation = createVerticalSnapAnimation(visible);
        mCurrentAnimation.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mGestureState = Gesture.NONE;
                        mCurrentAnimation = null;
                        mIsBeingDisplayedForFirstTime = false;
                    }
                });
        mCurrentAnimation.start();
    }

    /**
     * Cancels the current animation, unless the View is coming onto the screen for the first time.
     * @return True if the animation was canceled or wasn't running, false otherwise.
     */
    private boolean cancelCurrentAnimation() {
        if (mIsBeingDisplayedForFirstTime) return false;
        if (mCurrentAnimation != null) mCurrentAnimation.cancel();
        return true;
    }

    /** @return Whether the SwipableOverlayView is allowed to hide itself on scroll. */
    protected boolean isAllowedToAutoHide() {
        return true;
    }

    /**
     * Override gatherTransparentRegion to make this view's layout a placeholder for its animations.
     * This is only called during layout, so it doesn't really make sense to apply post-layout
     * properties like it does by default. Together with setWillNotDraw(false), this ensures no
     * child animation within this view's layout will be clipped by a SurfaceView.
     */
    @Override
    // TODO(crbug.com/40779510): work out why this is causing a lint error
    @SuppressWarnings("Override")
    public boolean gatherTransparentRegion(Region region) {
        float translationY = getTranslationY();
        setTranslationY(0);
        boolean result = super.gatherTransparentRegion(region);
        // Restoring TranslationY invalidates this view unnecessarily. However, this function
        // is called as part of layout, which implies a full redraw is about to occur anyway.
        setTranslationY(translationY);
        return result;
    }
}
