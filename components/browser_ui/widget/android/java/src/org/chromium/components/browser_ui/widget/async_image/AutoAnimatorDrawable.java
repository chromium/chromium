// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget.async_image;

import android.graphics.drawable.Animatable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.DrawableWrapper;
import android.graphics.drawable.InsetDrawable;
import android.graphics.drawable.LayerDrawable;
import android.graphics.drawable.RotateDrawable;
import android.graphics.drawable.ScaleDrawable;

import androidx.appcompat.graphics.drawable.DrawableWrapperCompat;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.function.Consumer;

/**
 * A helper {@link Drawable} that wraps another {@link Drawable} and starts/stops any {@link
 * Animatable} {@link Drawable}s in the {@link Drawable} hierarchy when this {@link Drawable} is
 * shown or hidden.
 */
@NullMarked
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
    public static @Nullable Drawable wrap(@Nullable Drawable drawable) {
        if (drawable == null || !shouldWrapDrawable(drawable)) return drawable;
        return new AutoAnimatorDrawable(drawable);
    }

    private AutoAnimatorDrawable(Drawable drawable) {
        super(drawable);
    }

    // DrawableWrapperCompat implementation.
    @Override
    public boolean setVisible(boolean visible, boolean restart) {
        boolean changed = super.setVisible(visible, restart);
        Drawable wrapped = getDrawable();
        if (wrapped != null) {
            // DrawableWrapperCompat.setVisible short-circuits `||` when super.setVisible returns
            // true, leaving the wrapped drawable's visibility stale when hiding.
            changed |= wrapped.setVisible(visible, restart);
        }
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

    private static void animatedDrawableHelper(
            @Nullable Drawable drawable, Consumer<Animatable> consumer) {
        if (drawable == null) return;

        if (drawable instanceof Animatable animatable) {
            consumer.accept(animatable);

            // Assume Animatable drawables can handle animating their own internals/sub drawables.
            return;
        }

        if (drawable != drawable.getCurrent()) {
            // Check obvious cases where the current drawable isn't actually being shown.  This
            // should support all {@link DrawableContainer} instances.
            AutoAnimatorDrawable.animatedDrawableHelper(drawable.getCurrent(), consumer);
        }

        if (drawable instanceof DrawableWrapper wrapper) {
            // Support all modern versions of drawables that wrap other ones.  This won't cover old
            // versions of Android (see below for other if/else blocks).
            AutoAnimatorDrawable.animatedDrawableHelper(wrapper.getDrawable(), consumer);
        } else if (drawable instanceof DrawableWrapperCompat wrapperCompat) {
            // Support the AppCompat DrawableWrapperCompat.
            AutoAnimatorDrawable.animatedDrawableHelper(wrapperCompat.getDrawable(), consumer);
        } else if (drawable instanceof LayerDrawable layerDrawable) {
            // Support a LayerDrawable and try to animate all layers.
            for (int i = 0; i < layerDrawable.getNumberOfLayers(); i++) {
                AutoAnimatorDrawable.animatedDrawableHelper(layerDrawable.getDrawable(i), consumer);
            }
        } else if (drawable instanceof InsetDrawable insetDrawable) {
            // Support legacy versions of InsetDrawable.
            AutoAnimatorDrawable.animatedDrawableHelper(insetDrawable.getDrawable(), consumer);
        } else if (drawable instanceof RotateDrawable rotateDrawable) {
            // Support legacy versions of RotateDrawable.
            AutoAnimatorDrawable.animatedDrawableHelper(rotateDrawable.getDrawable(), consumer);
        } else if (drawable instanceof ScaleDrawable scaleDrawable) {
            // Support legacy versions of ScaleDrawable.
            AutoAnimatorDrawable.animatedDrawableHelper(scaleDrawable.getDrawable(), consumer);
        }
    }
}
