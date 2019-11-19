// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ui.widget.highlight;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Canvas;
import android.graphics.ColorFilter;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.graphics.drawable.Animatable;
import android.graphics.drawable.Drawable;
import android.os.SystemClock;
import android.support.v4.view.animation.PathInterpolatorCompat;
import android.view.animation.Interpolator;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.browser.ui.widget.R;
import org.chromium.chrome.browser.ui.widget.animation.Interpolators;
import org.chromium.chrome.browser.util.MathUtils;

/**
 * A custom {@link Drawable} that will animate a pulse using the {@link PulseInterpolator}.  Meant
 * to be created with a {@link Painter} that does the actual drawing work based on the pulse
 * interpolation value.
 */
public class PulseDrawable extends Drawable implements Animatable {
    private static final long PULSE_DURATION_MS = 2500;
    private static final long FRAME_RATE = 60;

    /**
     * Informs the PulseDrawable about whether it can continue pulsing, and specifies a callback to
     * be run when the PulseDrawable is finished pulsing.
     */
    public interface PulseEndAuthority {
        /**
         * Called at the end of one pulse animation, to decide whether the PulseDrawable can pulse
         * again.
         *
         * @return True iff the PulseDrawable can continue pulsing.
         */
        boolean canPulseAgain();
    }

    /**
     * A PulseEndAuthority which allows the PulseDrawable to pulse forever.
     */
    private static class EndlessPulser implements PulseEndAuthority {
        // PulseEndAuthority implementation.

        @Override
        public boolean canPulseAgain() {
            return true;
        }
    }

    /**
     * An interface that does the actual drawing work for this {@link Drawable}.  Not meant to be
     * stateful, as this could be shared across multiple instances of this drawable if it gets
     * copied or mutated.
     */
    private interface Painter {
        /**
         * Called when this drawable updates it's pulse interpolation.  Should mutate the drawable
         * as necessary.  This is responsible for invalidating this {@link Drawable} if something
         * needs to be redrawn.
         *
         * @param drawable      The {@link PulseDrawable} that is updated.
         * @param interpolation The current progress of whatever is being pulsed.
         */
        void modifyDrawable(PulseDrawable drawable, float interpolation);

        /**
         * Called when this {@link PulseDrawable} needs to draw.  Should perform any draw operation
         * for the specific type of pulse.
         * @param drawable      The calling {@link PulseDrawable}.
         * @param paint         A {@link Paint} object to use.  This will automatically have the
         *                      color set.
         * @param canvas        The {@link Canvas} to draw to.
         * @param interpolation The current progress of whatever is being pulsed.
         */
        void draw(PulseDrawable drawable, Paint paint, Canvas canvas, float interpolation);
    }

    /**
     * Interface for calculating the max and min bounds in a pulsing circle.
     */
    public interface Bounds {
        /**
         * Calculates the maximum radius of a pulsing circle.
         * @param bounds the bounds of the canvas.
         * @return floating point maximum radius.
         */
        float getMaxRadiusPx(Rect bounds);

        /**
         * @param bounds the bounds of the canvas.
         * @return floating point minimum radius.
         */
        float getMinRadiusPx(Rect bounds);
    }

    private static Painter createCirclePainter(Bounds boundsFn) {
        return new Painter() {
            @Override
            public void modifyDrawable(PulseDrawable drawable, float interpolation) {
                drawable.invalidateSelf();
            }

            @Override
            public void draw(
                    PulseDrawable drawable, Paint paint, Canvas canvas, float interpolation) {
                Rect bounds = drawable.getBounds();

                float minRadiusPx = boundsFn.getMinRadiusPx(bounds);
                float maxRadiusPx = boundsFn.getMaxRadiusPx(bounds);
                float radius =
                        MathUtils.interpolate(minRadiusPx, maxRadiusPx, 1.0f - interpolation);

                canvas.drawCircle(bounds.exactCenterX(), bounds.exactCenterY(), radius, paint);
            }
        };
    }

    /**
     * Creates a {@link PulseDrawable} that will fill the bounds with a pulsing color.
     * @param context The {@link Context} under which the drawable is created.
     * @param pulseEndAuthority The {@link PulseEndAuthority} associated with this drawable.
     * @return A new {@link PulseDrawable} instance.
     */
    public static PulseDrawable createHighlight(
            Context context, PulseEndAuthority pulseEndAuthority) {
        Painter painter = new Painter() {
            @Override
            public void modifyDrawable(PulseDrawable drawable, float interpolation) {
                drawable.setAlpha((int) MathUtils.interpolate(12, 75, interpolation));
            }

            @Override
            public void draw(
                    PulseDrawable drawable, Paint paint, Canvas canvas, float interpolation) {
                canvas.drawRect(drawable.getBounds(), paint);
            }
        };

        return new PulseDrawable(
                context, Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR, painter, pulseEndAuthority);
    }

    /**
     * Creates a {@link PulseDrawable} that will fill the bounds with a pulsing color. The {@link
     * PulseDrawable} will continue pulsing forever (if this is not the desired behavior, please use
     * {@link PulseEndAuthority}).
     * @param context The {@link Context} under which the drawable is created.
     * @return A new {@link PulseDrawable} instance.
     */
    public static PulseDrawable createHighlight(Context context) {
        return createHighlight(context, new EndlessPulser());
    }

    /**
     * Creates a {@link PulseDrawable} that will draw a pulsing circle inside the bounds.
     * @param context The {@link Context} under which the drawable is created.
     * @param pulseEndAuthority The {@link PulseEndAuthority} associated with this drawable.
     * @return A new {@link PulseDrawable} instance.
     */
    public static PulseDrawable createCircle(Context context, PulseEndAuthority pulseEndAuthority) {
        final int startingPulseRadiusPx =
                context.getResources().getDimensionPixelSize(R.dimen.iph_pulse_baseline_radius);

        return createCustomCircle(context, new Bounds() {
            @Override
            public float getMaxRadiusPx(Rect bounds) {
                return Math.min(startingPulseRadiusPx * 1.2f,
                        Math.min(bounds.width(), bounds.height()) / 2.f);
            }
            @Override
            public float getMinRadiusPx(Rect bounds) {
                return Math.min(
                        startingPulseRadiusPx, Math.min(bounds.width(), bounds.height()) / 2.f);
            }
        }, pulseEndAuthority);
    }

    /**
     * Creates a {@link PulseDrawable} that will draw a pulsing circle inside the bounds. The {@link
     * PulseDrawable} will continue pulsing forever (if this is not the desired behavior, please use
     * {@link PulseEndAuthority}).
     * @param context The {@link Context} under which the drawable is created.
     * @return A new {@link PulseDrawable} instance.
     */
    public static PulseDrawable createCircle(Context context) {
        return createCircle(context, new EndlessPulser());
    }

    /**
     * Creates a {@link PulseDrawable} that will draw a pulsing circle as large as possible inside
     * the bounds.
     * @param context The {@link Context} under which the drawable is created.
     * @return A new {@link PulseDrawable} instance.
     */
    public static PulseDrawable createCustomCircle(
            Context context, Bounds boundsfn, PulseEndAuthority pulseEndAuthority) {
        Painter painter = createCirclePainter(boundsfn);

        PulseDrawable drawable = new PulseDrawable(context,
                PathInterpolatorCompat.create(.8f, 0.f, .6f, 1.f), painter, pulseEndAuthority);
        drawable.setAlpha(76);
        return drawable;
    }

    /**
     * Creates a {@link PulseDrawable} that will draw a pulsing circle as large as possible inside
     * the bounds.
     * @param context The {@link Context} under which the drawable is created.
     * @return A new {@link PulseDrawable} instance.
     */
    public static PulseDrawable createCustomCircle(Context context, Bounds boundsfn) {
        return createCustomCircle(context, boundsfn, new EndlessPulser());
    }

    private final Runnable mNextFrame = new Runnable() {
        @Override
        public void run() {
            stepPulse();
            if (mRunning) scheduleSelf(mNextFrame, SystemClock.uptimeMillis() + 1000 / FRAME_RATE);
        }
    };

    private final Paint mPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Rect mInset = new Rect();
    private final Rect mOriginalBounds = new Rect();
    private final Rect mInsetBounds = new Rect();

    private PulseState mState;
    private boolean mMutated;
    private boolean mRunning;
    private long mLastUpdateTime;

    private final PulseEndAuthority mPulseEndAuthority;

    /**
     * Creates a new {@link PulseDrawable} instance.
     * @param context The {@link Context} under which the drawable is created.
     * @param interpolator An {@link Interpolator} that defines how the pulse will fade in and out.
     * @param painter      The {@link Painter} that will be responsible for drawing the pulse.
     * @param pulseEndAuthority The {@link PulseEndAuthority} that is associated with this drawable.
     */
    private PulseDrawable(Context context, Interpolator interpolator, Painter painter,
            PulseEndAuthority pulseEndAuthority) {
        this(new PulseState(interpolator, painter), pulseEndAuthority);
        setUseLightPulseColor(context.getResources(), false);
    }

    private PulseDrawable(PulseState state, PulseEndAuthority pulseEndAuthority) {
        mState = state;
        mPulseEndAuthority = pulseEndAuthority;
    }

    private PulseDrawable(PulseState state) {
        this(state, new EndlessPulser());
    }

    /**
     * @param resources The {@link Resources} for accessing colors.
     * @param useLightPulseColor Whether or not to use a light or dark color for the pulse.
     * */
    public void setUseLightPulseColor(Resources resources, boolean useLightPulseColor) {
        @ColorInt
        int color = ApiCompatibilityUtils.getColor(resources,
                useLightPulseColor ? R.color.modern_blue_300 : R.color.default_icon_color_blue);
        if (mState.color == color) return;

        int alpha = getAlpha();
        mState.color = mState.drawColor = color;
        setAlpha(alpha);
        invalidateSelf();
    }

    /** How much to inset the bounds of this {@link Drawable} by. */
    public void setInset(int left, int top, int right, int bottom) {
        mInset.set(left, top, right, bottom);
        if (!mOriginalBounds.isEmpty()) setBounds(mOriginalBounds);
    }

    // Animatable implementation.
    @Override
    public void start() {
        if (mRunning) {
            unscheduleSelf(mNextFrame);
            scheduleSelf(mNextFrame, SystemClock.uptimeMillis() + 1000 / FRAME_RATE);
        } else {
            mRunning = true;
            if (mState.startTime == 0) {
                mState.startTime = SystemClock.uptimeMillis();
                mLastUpdateTime = mState.startTime;
            }
            mNextFrame.run();
        }
    }

    @Override
    public void stop() {
        mRunning = false;
        mState.startTime = 0;
        unscheduleSelf(mNextFrame);
    }

    @Override
    public boolean isRunning() {
        return mRunning;
    }

    // Drawable implementation.
    // Overriding only this method because {@link Drawable#setBounds(Rect)} calls into this.
    @Override
    public void setBounds(int left, int top, int right, int bottom) {
        mOriginalBounds.set(left, top, right, bottom);
        mInsetBounds.set(
                left + mInset.left, top + mInset.top, right - mInset.right, bottom - mInset.bottom);
        super.setBounds(
                mInsetBounds.left, mInsetBounds.top, mInsetBounds.right, mInsetBounds.bottom);
    }

    @Override
    public void draw(@NonNull Canvas canvas) {
        mPaint.setColor(mState.drawColor);
        mState.painter.draw(this, mPaint, canvas, mState.progress);
    }

    @Override
    public void setAlpha(int alpha) {
        // Encode the alpha into the color.
        alpha += alpha >> 7; // make it 0..256
        final int baseAlpha = mState.color >>> 24;
        final int useAlpha = baseAlpha * alpha >> 8;
        final int useColor = (mState.color << 8 >>> 8) | (useAlpha << 24);
        if (mState.drawColor != useColor) {
            mState.drawColor = useColor;
            invalidateSelf();
        }
    }

    @Override
    public int getAlpha() {
        return mState.drawColor >>> 24;
    }

    @Override
    public void setColorFilter(ColorFilter colorFilter) {
        mPaint.setColorFilter(colorFilter);
    }

    @Override
    public int getOpacity() {
        return PixelFormat.TRANSLUCENT;
    }

    @Override
    public boolean setVisible(boolean visible, boolean restart) {
        final boolean changed = super.setVisible(visible, restart);
        if (visible) {
            if (changed || restart) start();
        } else {
            stop();
        }
        return changed;
    }

    @Override
    @NonNull
    public Drawable mutate() {
        if (!mMutated && super.mutate() == this) {
            mState = new PulseState(mState);
            mMutated = true;
        }
        return this;
    }

    @Override
    public ConstantState getConstantState() {
        return mState;
    }

    private void stepPulse() {
        long curTime = SystemClock.uptimeMillis();
        // If we are on a new pulse
        if ((mLastUpdateTime - mState.startTime) / PULSE_DURATION_MS
                != (curTime - mState.startTime) / PULSE_DURATION_MS) {
            if (!(mPulseEndAuthority.canPulseAgain())) {
                stop();
                return;
            }
        }
        long msIntoAnim = (curTime - mState.startTime) % PULSE_DURATION_MS;
        float timeProgress = ((float) msIntoAnim) / ((float) PULSE_DURATION_MS);
        mState.progress = mState.interpolator.getInterpolation(timeProgress);
        mState.painter.modifyDrawable(PulseDrawable.this, mState.progress);
        mLastUpdateTime = curTime;
    }

    /**
     * The {@link ConstantState} subclass for this {@link PulseDrawable}.
     */
    static final class PulseState extends ConstantState {
        // Current Paint State.
        /** The current color, including alpha, to draw. */
        public int drawColor;

        /** The original color to draw (will not include updates from calls to setAlpha()). */
        public int color;

        // Current Animation State
        /** The time from {@link SystemClock#uptimeMillis} that this animation started at. */
        public long startTime;

        /** The current progress from 0 to 1 of the pulse. */
        public float progress;

        /** The {@link Interpolator} that makes the pulse and generates the progress. */
        public Interpolator interpolator;

        /**
         * The {@link Painter} object that is responsible for modifying and drawing this
         * {@link PulseDrawable}.
         */
        public Painter painter;

        PulseState(Interpolator interpolator, Painter painter) {
            this.interpolator = new PulseInterpolator(interpolator);
            this.painter = painter;
        }

        PulseState(PulseState other) {
            drawColor = other.drawColor;
            color = other.color;

            startTime = other.startTime;

            interpolator = other.interpolator;
            painter = other.painter;
        }

        @Override
        public Drawable newDrawable() {
            return new PulseDrawable(this);
        }

        @Override
        public int getChangingConfigurations() {
            return 0;
        }
    }
}