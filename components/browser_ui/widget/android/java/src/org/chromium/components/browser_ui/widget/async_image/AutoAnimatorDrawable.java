// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.async_image;

import android.graphics.drawable.Animatable;
import android.graphics.drawable.Animatable2;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.InsetDrawable;
import android.graphics.drawable.LayerDrawable;
import android.graphics.drawable.RotateDrawable;
import android.graphics.drawable.ScaleDrawable;
import android.os.Handler;
import android.os.Looper;

import androidx.annotation.Nullable;
import androidx.appcompat.graphics.drawable.DrawableWrapperCompat;
import androidx.vectordrawable.graphics.drawable.Animatable2Compat;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * A helper {@link Drawable} that wraps another {@link Drawable} and starts/stops any
 * {@link Animatable} {@link Drawable}s in the {@link Drawable} hierarchy when this {@link Drawable}
 * is shown or hidden.
 */
public class AutoAnimatorDrawable extends DrawableWrapperCompat {
    // Since Drawables default visible to true by default, we might not get a change and start the
    // animation on the first visibility request.
    private boolean mGotVisibilityCall;

    /**
     * Wraps {@code drawable} and returns a new {@link Drawable} that will automatically start
     * animating all sub-drawables if possible when the {@link Drawable} is visible.  Stops
     * animating when the {@link Drawable} is no longer visible.
     * @param drawable The {@link Drawable} to wrap.
     * @return         A new {@link Drawable} that will automaticaly animate or {@code null} if
     *                 {@code drawable} is {@code null}.
     */
    public static Drawable wrap(@Nullable Drawable drawable) {
        if (drawable == null || !shouldWrapDrawable(drawable)) return drawable;
        return new AutoAnimatorDrawable(drawable);
    }

    private AutoAnimatorDrawable(Drawable drawable) {
        super(drawable);
        AutoAnimatorDrawable.attachRestartListeners(this);
    }

    // DrawableWrapperCompat implementation.
    @Override
    public boolean setVisible(boolean visible, boolean restart) {
        boolean changed = super.setVisible(visible, restart);
        if (visible) {
            if (changed || restart || !mGotVisibilityCall) {
                AutoAnimatorDrawable.startAnimatedDrawables(this);
            }
        } else {
            AutoAnimatorDrawable.stopAnimatedDrawables(this);
        }

        mGotVisibilityCall = true;
        return changed;
    }

    private static void startAnimatedDrawables(@Nullable Drawable drawable) {
        AutoAnimatorDrawable.animatedDrawableHelper(drawable, animatable -> animatable.start());
    }

    private static void stopAnimatedDrawables(@Nullable Drawable drawable) {
        AutoAnimatorDrawable.animatedDrawableHelper(drawable, animatable -> animatable.stop());
    }

    private static boolean shouldWrapDrawable(@Nullable Drawable drawable) {
        AtomicBoolean found = new AtomicBoolean();
        AutoAnimatorDrawable.animatedDrawableHelper(drawable, animatable -> found.set(true));
        return found.get();
    }

    private static void attachRestartListeners(@Nullable Drawable drawable) {
        AutoAnimatorDrawable.animatedDrawableHelper(
                drawable,
                animatable -> {
                    if (animatable instanceof Animatable2Compat) {
                        ((Animatable2Compat) animatable)
                                .registerAnimationCallback(LazyHolderCompat.INSTANCE);
                    } else if (animatable instanceof Animatable2) {
                        ((Animatable2) animatable).registerAnimationCallback(LazyHolder.INSTANCE);
                    }
                });
    }

    private static void animatedDrawableHelper(
            @Nullable Drawable drawable, org.chromium.base.Callback<Animatable> consumer) {
        if (drawable == null) return;

        if (drawable instanceof Animatable) {
            consumer.onResult((Animatable) drawable);

            // Assume Animatable drawables can handle animating their own internals/sub drawables.
            return;
        }

        if (drawable != drawable.getCurrent()) {
            // Check obvious cases where the current drawable isn't actually being shown.  This
            // should support all {@link DrawableContainer} instances.
            AutoAnimatorDrawable.animatedDrawableHelper(drawable.getCurrent(), consumer);
        }

        if (drawable instanceof android.graphics.drawable.DrawableWrapper) {
            // Support all modern versions of drawables that wrap other ones.  This won't cover old
            // versions of Android (see below for other if/else blocks).
            AutoAnimatorDrawable.animatedDrawableHelper(
                    ((android.graphics.drawable.DrawableWrapper) drawable).getDrawable(), consumer);
        } else if (drawable instanceof DrawableWrapperCompat) {
            // Support the AppCompat DrawableWrapperCompat.
            AutoAnimatorDrawable.animatedDrawableHelper(
                    ((DrawableWrapperCompat) drawable).getDrawable(), consumer);
        } else if (drawable instanceof LayerDrawable) {
            // Support a LayerDrawable and try to animate all layers.
            LayerDrawable layerDrawable = (LayerDrawable) drawable;
            for (int i = 0; i < layerDrawable.getNumberOfLayers(); i++) {
                AutoAnimatorDrawable.animatedDrawableHelper(layerDrawable.getDrawable(i), consumer);
            }
        } else if (drawable instanceof InsetDrawable) {
            // Support legacy versions of InsetDrawable.
            AutoAnimatorDrawable.animatedDrawableHelper(
                    ((InsetDrawable) drawable).getDrawable(), consumer);
        } else if (drawable instanceof RotateDrawable) {
            // Support legacy versions of RotateDrawable.
            AutoAnimatorDrawable.animatedDrawableHelper(
                    ((RotateDrawable) drawable).getDrawable(), consumer);
        } else if (drawable instanceof ScaleDrawable) {
            // Support legacy versions of ScaleDrawable.
            AutoAnimatorDrawable.animatedDrawableHelper(
                    ((ScaleDrawable) drawable).getDrawable(), consumer);
        }
    }

    private static final class LazyHolder {
        private static final AutoRestarter INSTANCE = new AutoRestarter();
    }

    private static final class LazyHolderCompat {
        private static final AutoRestarterCompat INSTANCE = new AutoRestarterCompat();
    }

    private static final class AutoRestarterCompat extends Animatable2Compat.AnimationCallback {
        private final Handler mHandler = new Handler(Looper.getMainLooper());

        // Animatable2Compat.AnimationCallback implementation.
        @Override
        public void onAnimationEnd(Drawable drawable) {
            if (!(drawable instanceof Animatable)) return;
            mHandler.post(
                    () -> {
                        if (drawable.isVisible()) ((Animatable) drawable).start();
                    });
        }
    }

    private static final class AutoRestarter extends Animatable2.AnimationCallback {
        // Animatable2.AnimationCallback implementation.
        @Override
        public void onAnimationEnd(Drawable drawable) {
            LazyHolderCompat.INSTANCE.onAnimationEnd(drawable);
        }
    }
}
