// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.scrim;

import android.content.Context;
import android.view.MotionEvent;
import android.view.ViewGroup;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.Supplier;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * The coordinator for the scrim widget used to bring focus to certain elements on screen.
 *
 * To use the scrim, {@link #showScrim(PropertyModel)} must be called to set the params for
 * how the scrim will behave:
 *
 * PropertyModel model = new PropertyModel.Builder(ScrimProperties.ALL_KEYS)...
 *
 * After that, users can either allow the default animation to run or change the view's alpha
 * manually using {@link #setAlpha(float)}. Once the scrim is done being used,
 * {@link #hideScrim(boolean)} should be called.
 */
public class ScrimCoordinator {
    /** The duration for the scrim animation. */
    private static final int ANIM_DURATION_MS = 300;

    /** A delegate to expose functionality that changes the scrim over the system UI. */
    public interface SystemUiScrimDelegate {
        /**
         * Pass the current scrim color to the relevant system UI elements.
         * @param scrimColor The current base color of the scrim.
         */
        default void setScrimColor(@ColorInt int scrimColor) {
            // Default no-op, since we fallback to R.color.default_scrim_color if this isn't called.
        }

        /**
         * Set the amount of scrim over the status bar. The implementor may choose to not respect
         * the value provided to this method.
         * @param scrimFraction The scrim fraction over the status bar. 0 is completely hidden, 1 is
         *                      completely shown.
         */
        void setStatusBarScrimFraction(float scrimFraction);

        /**
         * Set the amount of scrim over the navigation bar. The implementor may choose to not
         * respect the value provided to this method.
         * @param scrimFraction The scrim fraction over the status bar. 0 is completely hidden, 1 is
         *                      completely shown.
         */
        void setNavigationBarScrimFraction(float scrimFraction);
    }

    /** A mechanism for delegating motion events out to the mediator. */
    interface TouchEventDelegate {
        /**
         * @param event The event that occurred.
         * @return Whether the event was consumed.
         */
        boolean onTouchEvent(MotionEvent event);
    }

    /** A supplier of new {@link ScrimView}s to use when {@link #showScrim(PropertyModel)} is called. */
    private final Supplier<ScrimView> mScrimViewBuilder;

    /** The component's mediator for handling animation and model management. */
    private final ScrimMediator mMediator;

    /**
     * A handle to the view to bind models to. This should otherwise remain untouched. Each time
     * {@link #showScrim(PropertyModel)} is called, this view is recreated, so all old state is
     * discarded.
     */
    private ScrimView mView;

    /** A handle to the object pushing updates from the model to the view. */
    private PropertyModelChangeProcessor mChangeProcessor;

    /**
     * @param context An Android {@link Context} for creating the view.
     * @param systemUiScrimDelegate A means of changing the scrim over the system UI.
     * @param parent The {@link ViewGroup} the scrim should exist in.
     * @param defaultColor The default color of the scrim.
     */
    public ScrimCoordinator(
            Context context,
            SystemUiScrimDelegate systemUiScrimDelegate,
            ViewGroup parent,
            @ColorInt int defaultColor) {
        mMediator =
                new ScrimMediator(
                        () -> {
                            if (mChangeProcessor != null) mChangeProcessor.destroy();
                            if (mView != null) UiUtils.removeViewFromParent(mView);
                            mView = null;
                            mChangeProcessor = null;
                        },
                        systemUiScrimDelegate);
        mScrimViewBuilder =
                () -> {
                    ScrimView view = new ScrimView(context, parent, defaultColor, mMediator);
                    return view;
                };
    }

    /**
     * Show the scrim.
     * @param model The property model of {@link ScrimProperties} that define the scrim behavior.
     */
    public void showScrim(PropertyModel model) {
        assert model != null : "Showing the scrim requires a model.";

        // Ensure the previous scrim is hidden before showing the new one. This logic should be in
        // the mediator, but it depends on the old view and binder being available which are
        // replaced prior to mediator#showScrim being called.
        if (mMediator.isActive()) mMediator.hideScrim(false, ANIM_DURATION_MS);

        if (mChangeProcessor != null) mChangeProcessor.destroy();

        mView = mScrimViewBuilder.get();
        mChangeProcessor = PropertyModelChangeProcessor.create(model, mView, ScrimViewBinder::bind);
        mMediator.showScrim(model, ANIM_DURATION_MS);
    }

    /**
     * Hide the scrim.
     * @param animate Whether the scrim should animate and fade out.
     */
    public void hideScrim(boolean animate) {
        hideScrim(animate, ANIM_DURATION_MS);
    }

    /**
     * Hide the scrim.
     * @param animate Whether the scrim should animate and fade out.
     * @param duration Duration for animation.
     */
    public void hideScrim(boolean animate, int duration) {
        mMediator.hideScrim(animate, duration);
    }

    /** @return Whether the scrim is being shown. */
    public boolean isShowingScrim() {
        return mMediator.isActive();
    }

    /** Forces the current scrim fade animation to complete if one is running. */
    public void forceAnimationToFinish() {
        mMediator.forceAnimationToFinish();
    }

    /**
     * Manually set the alpha for the scrim.
     * @param alpha The alpha in range [0, 1].
     */
    public void setAlpha(float alpha) {
        mMediator.setAlpha(alpha);
    }

    /** Clean up this coordinator. */
    public void destroy() {
        mMediator.destroy();
    }

    public void disableAnimationForTesting(boolean disable) {
        mMediator.disableAnimationForTesting(disable);
    }

    public ScrimView getViewForTesting() {
        return mView;
    }

    ScrimMediator getMediatorForTesting() {
        return mMediator;
    }

    @VisibleForTesting
    boolean areAnimationsRunning() {
        return mMediator.areAnimationsRunning();
    }
}
