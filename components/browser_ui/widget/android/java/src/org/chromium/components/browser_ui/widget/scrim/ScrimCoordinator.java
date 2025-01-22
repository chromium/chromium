// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.scrim;

import android.content.Context;
import android.view.MotionEvent;
import android.view.ViewGroup;

import androidx.annotation.ColorInt;
import androidx.core.content.ContextCompat;

import org.chromium.base.ObserverList;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.components.browser_ui.widget.R;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * The coordinator for the scrim widget used to bring focus to certain elements on screen.
 *
 * <p>To use the scrim, {@link #showScrim(PropertyModel)} must be called to set the params for how
 * the scrim will behave:
 *
 * <p>PropertyModel model = new PropertyModel.Builder(ScrimProperties.ALL_KEYS)...
 *
 * <p>After that, users can either allow the default animation to run or change the view's alpha
 * manually using {@link #setAlpha(float)}. Once the scrim is done being used, {@link
 * #hideScrim(boolean)} should be called.
 */
public class ScrimCoordinator {

    /** The duration for the scrim animation. */
    private static final int ANIM_DURATION_MS = 300;

    /** A mechanism for delegating motion events out to the mediator. */
    interface TouchEventDelegate {
        /**
         * @param event The event that occurred.
         * @return Whether the event was consumed.
         */
        boolean onTouchEvent(MotionEvent event);
    }

    public interface Observer {
        void scrimVisibilityChanged(boolean scrimVisible);
    }

    private final ObserverList<Observer> mScrimVisibilityObservers = new ObserverList<>();

    /**
     * A supplier of new {@link ScrimView}s to use when {@link #showScrim(PropertyModel)} is called.
     */
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
     * @param parent The {@link ViewGroup} the scrim should exist in.
     */
    public ScrimCoordinator(Context context, ViewGroup parent) {
        @ColorInt
        int defaultScrimColor = ContextCompat.getColor(context, R.color.default_scrim_color);
        mMediator =
                new ScrimMediator(
                        () -> {
                            if (mChangeProcessor != null) mChangeProcessor.destroy();
                            if (mView != null) UiUtils.removeViewFromParent(mView);
                            mView = null;
                            mChangeProcessor = null;
                        },
                        defaultScrimColor);
        mScrimViewBuilder =
                () -> {
                    ScrimView view = new ScrimView(context, parent);
                    return view;
                };
    }

    /**
     * Returns the current color being applied by the current scrim. If there is no active scrim,
     * then a fully transparent color will be returned. This is combination of the background color
     * and the currently fraction/alpha the scrim is being applied at.
     */
    public @ColorInt int getCurrentCompositeColor() {
        return mMediator.getCurrentCompositeColor();
    }

    public ObservableSupplier<Integer> getFullScrimColorSupplier() {
        return mMediator.getFullScrimColorSupplier();
    }

    public ObservableSupplier<Float> getStatusBarScrimFractionSupplier() {
        return mMediator.getStatusBarScrimFractionSupplier();
    }

    public ObservableSupplier<Float> getNavigationBarScrimFractionSupplier() {
        return mMediator.getNavigationBarScrimFractionSupplier();
    }

    /**
     * Show the scrim.
     *
     * @param model The property model of {@link ScrimProperties} that define the scrim behavior.
     */
    public void showScrim(PropertyModel model) {
        assert model != null : "Showing the scrim requires a model.";
        boolean isShowingScrim = isShowingScrim();

        // Ensure the previous scrim is hidden before showing the new one. This logic should be in
        // the mediator, but it depends on the old view and binder being available which are
        // replaced prior to mediator#showScrim being called.
        if (mMediator.isActive()) mMediator.hideScrim(false, ANIM_DURATION_MS);

        if (mChangeProcessor != null) mChangeProcessor.destroy();

        mView = mScrimViewBuilder.get();
        mChangeProcessor = PropertyModelChangeProcessor.create(model, mView, ScrimViewBinder::bind);
        mMediator.showScrim(model, ANIM_DURATION_MS);
        if (isShowingScrim != isShowingScrim()) {
            notifyVisibilityObservers();
        }
    }

    /**
     * Hide the scrim.
     *
     * @param animate Whether the scrim should animate and fade out.
     */
    public void hideScrim(boolean animate) {
        hideScrim(animate, ANIM_DURATION_MS);
    }

    /**
     * Hide the scrim.
     *
     * @param animate Whether the scrim should animate and fade out.
     * @param duration Duration for animation.
     */
    public void hideScrim(boolean animate, int duration) {
        boolean isShowingScrim = isShowingScrim();
        mMediator.hideScrim(animate, duration);
        if (isShowingScrim != isShowingScrim()) {
            notifyVisibilityObservers();
        }
    }

    /**
     * @return Whether the scrim is being shown.
     */
    public boolean isShowingScrim() {
        return mMediator.isActive();
    }

    public void addObserver(Observer observer) {
        mScrimVisibilityObservers.addObserver(observer);
    }

    public void removeObserver(Observer observer) {
        mScrimVisibilityObservers.removeObserver(observer);
    }

    private void notifyVisibilityObservers() {
        boolean isShowingScrim = isShowingScrim();
        for (Observer observer : mScrimVisibilityObservers) {
            observer.scrimVisibilityChanged(isShowingScrim);
        }
    }

    /** Forces the current scrim fade animation to complete if one is running. */
    public void forceAnimationToFinish() {
        mMediator.forceAnimationToFinish();
    }

    /**
     * Manually set the alpha for the scrim. This is exposed as part of the public API and should
     * not be called as part of animations as it cancels the currently running one.
     *
     * @param alpha The alpha in range [0, 1].
     * @param propertyModel The model used to show/identify the current scrim.
     */
    public void setAlpha(float alpha, PropertyModel propertyModel) {
        mMediator.setAlpha(alpha, propertyModel);
    }

    /**
     * Changes the target color of a full scrim, though the current color being applied will still
     * be subject to the current alpha. Safe to call this during animations.
     *
     * @param scrimColor The color to set the scrim to.
     * @param propertyModel The model used to show/identify the current scrim.
     */
    public void setScrimColor(@ColorInt int scrimColor, PropertyModel propertyModel) {
        mMediator.setScrimColor(scrimColor, propertyModel);
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

    /* package */ boolean areAnimationsRunningForTesting() {
        return mMediator.areAnimationsRunning();
    }
}
