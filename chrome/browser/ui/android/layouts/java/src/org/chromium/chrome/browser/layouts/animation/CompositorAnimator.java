// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.layouts.animation;

import static org.chromium.base.ContextUtils.getApplicationContext;

import android.animation.Animator;
import android.animation.TimeInterpolator;
import android.animation.ValueAnimator;
import android.provider.Settings;
import android.util.FloatProperty;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.supplier.Supplier;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableFloatPropertyKey;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.ref.WeakReference;
import java.util.ArrayList;

/** An animator that can be used for animations in the Browser Compositor. */
public class CompositorAnimator extends Animator {
    private static final String TAG = "CompositorAnimator";

    /** The different states that this animator can be in. */
    @IntDef({
        AnimationState.STARTED,
        AnimationState.RUNNING,
        AnimationState.CANCELED,
        AnimationState.ENDED
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface AnimationState {
        int STARTED = 0;
        int RUNNING = 1;
        int CANCELED = 2;
        int ENDED = 3;
    }

    /**
     * The scale honoring Settings.Global.ANIMATOR_DURATION_SCALE. Use static to reduce updating.
     * See {@link ValueAnimator}.
     **/
    @VisibleForTesting public static float sDurationScale = 1;

    /** The {@link CompositorAnimationHandler} running the animation. */
    private final WeakReference<CompositorAnimationHandler> mHandler;

    /** The list of listeners for events through the life of an animation. */
    private final ObserverList<AnimatorListener> mListeners = new ObserverList<>();

    /** The list of frame update listeners for this animation. */
    private final ArrayList<AnimatorUpdateListener> mAnimatorUpdateListeners = new ArrayList<>();

    /**
     * A cached copy of the list of {@link AnimatorUpdateListener}s to prevent allocating a new list
     * every update.
     */
    private final ArrayList<AnimatorUpdateListener> mCachedList = new ArrayList<>();

    /** The time interpolator for the animator. */
    private TimeInterpolator mTimeInterpolator;

    /**
     * The amount of time in ms that has passed since the animation has started. This includes any
     * delay that was set.
     */
    private long mTimeSinceStartMs;

    /**
     * The fraction that the animation is complete. This number is in the range [0, 1] and accounts
     * for the set time interpolator.
     */
    private float mAnimatedFraction;

    /** The value that the animation should start with (ending at {@link #mEndValue}). */
    private Supplier<Float> mStartValue;

    /** The value that the animation will transition to (starting at {@link #mStartValue}). */
    private Supplier<Float> mEndValue;

    /** The duration of the animation in ms. */
    private long mDurationMs;

    /**
     * The animator's start delay in ms. Once {@link #start()} is called, updates are not sent until
     * this time has passed.
     */
    private long mStartDelayMs;

    /** The current state of the animation. */
    private @AnimationState int mAnimationState = AnimationState.ENDED;

    /**
     * Whether the animation ended because of frame updates. This is used to determine if any
     * listeners need to be updated one more time.
     */
    private boolean mDidUpdateToCompletion;

    /**
     * A utility for creating a basic animator.
     * @param handler The {@link CompositorAnimationHandler} responsible for running the animation.
     * @param startValue The starting animation value.
     * @param endValue The end animation value.
     * @param durationMs The duration of the animation in ms.
     * @param listener An update listener if specific actions need to be performed.
     * @return A {@link CompositorAnimator} for the property.
     */
    public static CompositorAnimator ofFloat(
            CompositorAnimationHandler handler,
            float startValue,
            float endValue,
            long durationMs,
            @Nullable AnimatorUpdateListener listener) {
        CompositorAnimator animator = new CompositorAnimator(handler);
        animator.setValues(startValue, endValue);
        if (listener != null) animator.addUpdateListener(listener);
        animator.setDuration(durationMs);
        return animator;
    }

    /**
     * A utility for creating a basic animator.
     * @param handler The {@link CompositorAnimationHandler} responsible for running the animation.
     * @param target The object to modify.
     * @param property The property of the object to modify.
     * @param startValue The starting animation value.
     * @param endValue The end animation value.
     * @param durationMs The duration of the animation in ms.
     * @param interpolator The time interpolator for the animation.
     * @return A {@link CompositorAnimator} for the property.
     */
    public static <T> CompositorAnimator ofFloatProperty(
            CompositorAnimationHandler handler,
            final T target,
            final FloatProperty<T> property,
            float startValue,
            float endValue,
            long durationMs,
            TimeInterpolator interpolator) {
        CompositorAnimator animator = new CompositorAnimator(handler);
        animator.setValues(startValue, endValue);
        animator.setDuration(durationMs);
        animator.addUpdateListener(
                (CompositorAnimator a) -> property.setValue(target, a.getAnimatedValue()));
        animator.setInterpolator(interpolator);
        return animator;
    }

    /**
     * A utility for creating a basic animator.
     * @param handler The {@link CompositorAnimationHandler} responsible for running the animation.
     * @param target The object to modify.
     * @param property The property of the object to modify.
     * @param startValue The {@link Supplier} of the starting animation value.
     * @param endValue The {@link Supplier} of the end animation value.
     * @param durationMs The duration of the animation in ms.
     * @param interpolator The time interpolator for the animation.
     * @return A {@link CompositorAnimator} for the property.
     */
    public static <T> CompositorAnimator ofFloatProperty(
            CompositorAnimationHandler handler,
            final T target,
            final FloatProperty<T> property,
            Supplier<Float> startValue,
            Supplier<Float> endValue,
            long durationMs,
            TimeInterpolator interpolator) {
        CompositorAnimator animator = new CompositorAnimator(handler);
        animator.setValues(startValue, endValue);
        animator.setDuration(durationMs);
        animator.addUpdateListener(
                (CompositorAnimator a) -> property.setValue(target, a.getAnimatedValue()));
        animator.setInterpolator(interpolator);
        return animator;
    }

    /**
     * A utility for creating a basic animator.
     * @param handler The {@link CompositorAnimationHandler} responsible for running the animation.
     * @param target The object to modify.
     * @param property The property of the object to modify.
     * @param startValue The starting animation value.
     * @param endValue The end animation value.
     * @param durationMs The duration of the animation in ms.
     * @return A {@link CompositorAnimator} for the property.
     */
    public static <T> CompositorAnimator ofFloatProperty(
            CompositorAnimationHandler handler,
            final T target,
            final FloatProperty<T> property,
            float startValue,
            float endValue,
            long durationMs) {
        return ofFloatProperty(
                handler,
                target,
                property,
                startValue,
                endValue,
                durationMs,
                Interpolators.DECELERATE_INTERPOLATOR);
    }

    /**
     * A utility for creating a basic animator.
     * @param handler The {@link CompositorAnimationHandler} responsible for running the animation.
     * @param target The object to modify.
     * @param property The property of the object to modify.
     * @param startValue The {@link Supplier} of the starting animation value.
     * @param endValue The {@link Supplier} of the end animation value.
     * @param durationMs The duration of the animation in ms.
     * @return A {@link CompositorAnimator} for the property.
     */
    public static <T> CompositorAnimator ofFloatProperty(
            CompositorAnimationHandler handler,
            final T target,
            final FloatProperty<T> property,
            Supplier<Float> startValue,
            Supplier<Float> endValue,
            long durationMs) {
        return ofFloatProperty(
                handler,
                target,
                property,
                startValue,
                endValue,
                durationMs,
                Interpolators.DECELERATE_INTERPOLATOR);
    }

    /**
     * Create a {@link CompositorAnimator} to animate the {@link WritableFloatPropertyKey}.
     * @param handler The {@link CompositorAnimationHandler} responsible for running the animation.
     * @param model The {@link PropertyModel} to modify.
     * @param key The {@link WritableFloatPropertyKey} in the model to update.
     * @param startValue The {@link Supplier} of the starting animation value.
     * @param endValue The {@link Supplier} of the end animation value.
     * @param durationMs The duration of the animation in ms.
     * @param interpolator The time interpolator for the animation.
     * @return {@link CompositorAnimator} for animating the {@link WritableFloatPropertyKey}.
     */
    public static CompositorAnimator ofWritableFloatPropertyKey(
            CompositorAnimationHandler handler,
            final PropertyModel model,
            WritableFloatPropertyKey key,
            Supplier<Float> startValue,
            Supplier<Float> endValue,
            long durationMs,
            TimeInterpolator interpolator) {
        CompositorAnimator animator = new CompositorAnimator(handler);
        animator.setValues(startValue, endValue);
        animator.setDuration(durationMs);
        animator.addUpdateListener((CompositorAnimator a) -> model.set(key, a.getAnimatedValue()));
        animator.setInterpolator(interpolator);
        return animator;
    }

    /**
     * Create a {@link CompositorAnimator} to animate the {@link WritableFloatPropertyKey}.
     * @param handler The {@link CompositorAnimationHandler} responsible for running the animation.
     * @param model The {@link PropertyModel} to modify.
     * @param key The {@link WritableFloatPropertyKey} in the model to update.
     * @param startValue The starting animation value.
     * @param endValue The end animation value.
     * @param durationMs The duration of the animation in ms.
     * @param interpolator The time interpolator for the animation.
     * @return {@link CompositorAnimator} for animating the {@link WritableFloatPropertyKey}.
     */
    public static CompositorAnimator ofWritableFloatPropertyKey(
            CompositorAnimationHandler handler,
            final PropertyModel model,
            WritableFloatPropertyKey key,
            float startValue,
            float endValue,
            long durationMs,
            TimeInterpolator interpolator) {
        return ofWritableFloatPropertyKey(
                handler, model, key, () -> startValue, () -> endValue, durationMs, interpolator);
    }

    /**
     * Create a {@link CompositorAnimator} to animate the {@link WritableFloatPropertyKey}.
     * @param handler The {@link CompositorAnimationHandler} responsible for running the animation.
     * @param model The {@link PropertyModel} to modify.
     * @param key The {@link WritableFloatPropertyKey} in the model to update.
     * @param startValue The starting animation value.
     * @param endValue The end animation value.
     * @param durationMs The duration of the animation in ms.
     * @return {@link CompositorAnimator} for animating the {@link WritableFloatPropertyKey}.
     */
    public static CompositorAnimator ofWritableFloatPropertyKey(
            CompositorAnimationHandler handler,
            final PropertyModel model,
            WritableFloatPropertyKey key,
            float startValue,
            float endValue,
            long durationMs) {
        return ofWritableFloatPropertyKey(
                handler,
                model,
                key,
                startValue,
                endValue,
                durationMs,
                Interpolators.DECELERATE_INTERPOLATOR);
    }

    /** An interface for listening for frames of an animation. */
    public interface AnimatorUpdateListener {
        /**
         * A notification of the occurrence of another frame of the animation.
         * @param animator The animator that was updated.
         */
        void onAnimationUpdate(CompositorAnimator animator);
    }

    /**
     * Create a new animator for the current context.
     * @param handler The {@link CompositorAnimationHandler} responsible for running the animation.
     */
    public CompositorAnimator(@NonNull CompositorAnimationHandler handler) {
        mHandler = new WeakReference<>(handler);

        // The default interpolator is decelerate; this mimics the existing ChromeAnimation
        // behavior.
        mTimeInterpolator = Interpolators.DECELERATE_INTERPOLATOR;

        // By default, animate for 0 to 1.
        setValues(0, 1);

        // Try to update from the system setting, but not too frequently.
        sDurationScale =
                Settings.Global.getFloat(
                        getApplicationContext().getContentResolver(),
                        Settings.Global.ANIMATOR_DURATION_SCALE,
                        sDurationScale);
        if (sDurationScale != 1) {
            Log.i(TAG, "Settings.Global.ANIMATOR_DURATION_SCALE = %f", sDurationScale);
        }
    }

    private long getScaledDuration() {
        return (long) (mDurationMs * sDurationScale);
    }

    /**
     * Push an update to the animation. This should be called while the start delay is active and
     * assumes that the animated object is at the starting position when {@link #start} is called.
     * @param deltaTimeMs The time since the previous frame.
     */
    public void doAnimationFrame(long deltaTimeMs) {
        mTimeSinceStartMs += deltaTimeMs;

        // Clamp to the animator's duration, taking into account the start delay.
        long finalTimeMs =
                Math.min(
                        (long) (mTimeSinceStartMs - mStartDelayMs * sDurationScale),
                        getScaledDuration());

        // Wait until the start delay has passed.
        if (finalTimeMs < 0) return;

        // In the case where duration is 0, the animation is complete.
        mAnimatedFraction = 1;
        if (getScaledDuration() > 0) {
            mAnimatedFraction =
                    mTimeInterpolator.getInterpolation(finalTimeMs / (float) getScaledDuration());
        }

        // Push update to listeners.
        mCachedList.addAll(mAnimatorUpdateListeners);
        for (int i = 0; i < mCachedList.size(); i++) mCachedList.get(i).onAnimationUpdate(this);
        mCachedList.clear();

        if (finalTimeMs == getScaledDuration()) {
            mDidUpdateToCompletion = true;
            end();
        }
    }

    /**
     * @return The animated fraction after being passed through the time interpolator, if set.
     */
    @VisibleForTesting
    float getAnimatedFraction() {
        return mAnimatedFraction;
    }

    /**
     * Add a listener for frame occurrences.
     * @param listener The listener to add.
     */
    public void addUpdateListener(AnimatorUpdateListener listener) {
        mAnimatorUpdateListeners.add(listener);
    }

    /**
     * @return Whether or not the animation has ended after being started. If the animation is
     *         started after ending, this value will be reset to true.
     */
    public boolean hasEnded() {
        return mAnimationState == AnimationState.ENDED;
    }

    /**
     * Set the values to animate between.
     * @param start The value to begin the animation with.
     * @param end The value to end the animation at.
     */
    void setValues(float start, float end) {
        setValues(() -> start, () -> end);
    }

    /**
     * Set the values to animate between.
     * @param start The value to begin the animation with.
     * @param end The value to end the animation at.
     */
    @VisibleForTesting
    void setValues(Supplier<Float> start, Supplier<Float> end) {
        mStartValue = start;
        mEndValue = end;
    }

    /**
     * @return The current value between the floats set by {@link #setValues(float, float)}.
     */
    public float getAnimatedValue() {
        return mStartValue.get() + (getAnimatedFraction() * (mEndValue.get() - mStartValue.get()));
    }

    @Override
    public void addListener(AnimatorListener listener) {
        mListeners.addObserver(listener);
    }

    @Override
    public void removeListener(AnimatorListener listener) {
        mListeners.removeObserver(listener);
    }

    @Override
    public void removeAllListeners() {
        mListeners.clear();
        mAnimatorUpdateListeners.clear();
    }

    @Override
    @SuppressWarnings("unchecked")
    public void start() {
        if (mAnimationState != AnimationState.ENDED) return;

        super.start();
        mAnimationState = AnimationState.RUNNING;
        mDidUpdateToCompletion = false;
        CompositorAnimationHandler handler = mHandler.get();
        if (handler != null) handler.registerAndStartAnimator(this);
        mTimeSinceStartMs = 0;

        for (AnimatorListener listener : mListeners) listener.onAnimationStart(this);
    }

    @Override
    @SuppressWarnings("unchecked")
    public void cancel() {
        if (mAnimationState == AnimationState.ENDED) return;

        mAnimationState = AnimationState.CANCELED;

        super.cancel();

        for (AnimatorListener listener : mListeners) listener.onAnimationCancel(this);

        end();
    }

    @Override
    @SuppressWarnings("unchecked")
    public void end() {
        if (mAnimationState == AnimationState.ENDED) return;

        super.end();
        boolean wasCanceled = mAnimationState == AnimationState.CANCELED;
        mAnimationState = AnimationState.ENDED;

        // If the animation was ended early but not canceled, push one last update to the listeners.
        if (!mDidUpdateToCompletion && !wasCanceled) {
            mAnimatedFraction = 1f;
            for (AnimatorUpdateListener listener : mAnimatorUpdateListeners) {
                listener.onAnimationUpdate(this);
            }
        }

        for (AnimatorListener listener : mListeners) listener.onAnimationEnd(this);
    }

    @Override
    public long getStartDelay() {
        return mStartDelayMs;
    }

    @Override
    public void setStartDelay(long startDelayMs) {
        if (startDelayMs < 0) startDelayMs = 0;
        mStartDelayMs = startDelayMs;
    }

    @Override
    public CompositorAnimator setDuration(long durationMs) {
        if (durationMs < 0) durationMs = 0;
        mDurationMs = durationMs;
        return this;
    }

    @Override
    public long getDuration() {
        return mDurationMs;
    }

    @Override
    public void setInterpolator(TimeInterpolator timeInterpolator) {
        assert timeInterpolator != null;
        mTimeInterpolator = timeInterpolator;
    }

    @Override
    public boolean isRunning() {
        return mAnimationState == AnimationState.RUNNING;
    }
}
