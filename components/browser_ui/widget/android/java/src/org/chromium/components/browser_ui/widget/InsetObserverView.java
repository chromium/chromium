// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.browser_ui.widget;

import android.content.Context;
import android.graphics.Rect;
import android.os.Build;
import android.view.DisplayCutout;
import android.view.View;
import android.view.WindowInsets;

import androidx.annotation.RequiresApi;

import org.chromium.base.ObserverList;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;

/**
 * The purpose of this view is to store the system window insets (OSK, status bar) for
 * later use.
 */
public class InsetObserverView extends View {
    private final Rect mWindowInsets;
    protected final ObserverList<WindowInsetObserver> mObservers;
    private final BottomInsetObservableSupplier mBottomInsetSupplier;

    /**
     * Allows observing changes to the window insets from Android system UI.
     */
    public interface WindowInsetObserver {
        /**
         * Triggered when the window insets have changed.
         *
         * @param left The left inset.
         * @param top The top inset.
         * @param right The right inset (but it feels so wrong).
         * @param bottom The bottom inset.
         */
        void onInsetChanged(int left, int top, int right, int bottom);

        /** Called when a new Display Cutout safe area is applied. */
        void onSafeAreaChanged(Rect area);
    }

    private class BottomInsetObservableSupplier
            extends ObservableSupplierImpl<Integer> implements WindowInsetObserver {
        @Override
        public void onInsetChanged(int left, int top, int right, int bottom) {
            this.set(bottom);
        }

        @Override
        public void onSafeAreaChanged(Rect area) {}
    }

    /**
     * Constructs a new {@link InsetObserverView} for the appropriate Android version.
     * @param context The Context the view is running in, through which it can access the current
     *            theme, resources, etc.
     * @return an instance of a InsetObserverView.
     */
    public static InsetObserverView create(Context context) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            return new InsetObserverViewApi28(context);
        }
        return new InsetObserverView(context);
    }

    /**
     * Creates an instance of {@link InsetObserverView}.
     * @param context The Context to create this {@link InsetObserverView} in.
     */
    public InsetObserverView(Context context) {
        super(context);
        setVisibility(INVISIBLE);
        mWindowInsets = new Rect();
        mObservers = new ObserverList<>();
        mBottomInsetSupplier = new BottomInsetObservableSupplier();
        addObserver(mBottomInsetSupplier);
    }

    /**
     * Returns a supplier that observes this {@link InsetObserverView} and
     * provides changes to the bottom inset using the {@link
     * ObservableSupplier} interface.
     */
    public ObservableSupplier<Integer> getSupplierForBottomInset() {
        return mBottomInsetSupplier;
    }

    /**
     * Returns the left {@link WindowInsets} in pixels.
     */
    public int getSystemWindowInsetsLeft() {
        return mWindowInsets.left;
    }

    /**
     * Returns the top {@link WindowInsets} in pixels.
     */
    public int getSystemWindowInsetsTop() {
        return mWindowInsets.top;
    }

    /**
     * Returns the right {@link WindowInsets} in pixels.
     */
    public int getSystemWindowInsetsRight() {
        return mWindowInsets.right;
    }

    /**
     * Returns the bottom {@link WindowInsets} in pixels.
     */
    public int getSystemWindowInsetsBottom() {
        return mWindowInsets.bottom;
    }

    /**
     * Add an observer to be notified when the window insets have changed.
     */
    public void addObserver(WindowInsetObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Remove an observer of window inset changes.
     */
    public void removeObserver(WindowInsetObserver observer) {
        mObservers.removeObserver(observer);
    }

    @Override
    public WindowInsets onApplyWindowInsets(WindowInsets insets) {
        onInsetChanged(insets.getSystemWindowInsetLeft(), insets.getSystemWindowInsetTop(),
                insets.getSystemWindowInsetRight(), insets.getSystemWindowInsetBottom());
        return insets;
    }

    /**
     * Updates the window insets and notifies all observers if the values did indeed change.
     *
     * @param left The updated left inset.
     * @param top The updated right inset.
     * @param right The updated right inset.
     * @param bottom The updated bottom inset.
     */
    protected void onInsetChanged(int left, int top, int right, int bottom) {
        if (mWindowInsets.left == left && mWindowInsets.top == top && mWindowInsets.right == right
                && mWindowInsets.bottom == bottom) {
            return;
        }

        mWindowInsets.set(left, top, right, bottom);

        for (WindowInsetObserver observer : mObservers) {
            observer.onInsetChanged(left, top, right, bottom);
        }
    }

    @RequiresApi(Build.VERSION_CODES.P)
    private static class InsetObserverViewApi28 extends InsetObserverView {
        private Rect mCurrentSafeArea = new Rect();

        /**
         * Creates an instance of {@link InsetObserverView} for Android versions P and above.
         * @param context The Context to create this {@link InsetObserverView} in.
         */
        InsetObserverViewApi28(Context context) {
            super(context);
        }

        @Override
        public WindowInsets onApplyWindowInsets(WindowInsets insets) {
            setCurrentSafeAreaFromInsets(insets);
            return super.onApplyWindowInsets(insets);
        }

        /**
         * Get the safe area from the WindowInsets, store it and notify any observers.
         * @param insets The WindowInsets containing the safe area.
         */
        private void setCurrentSafeAreaFromInsets(WindowInsets insets) {
            DisplayCutout displayCutout = insets.getDisplayCutout();

            int left = 0;
            int top = 0;
            int right = 0;
            int bottom = 0;

            if (displayCutout != null) {
                left = displayCutout.getSafeInsetLeft();
                top = displayCutout.getSafeInsetTop();
                right = displayCutout.getSafeInsetRight();
                bottom = displayCutout.getSafeInsetBottom();
            }

            // If the safe area has not changed then we should stop now.
            if (mCurrentSafeArea.left == left && mCurrentSafeArea.top == top
                    && mCurrentSafeArea.right == right && mCurrentSafeArea.bottom == bottom) {
                return;
            }

            mCurrentSafeArea.set(left, top, right, bottom);

            for (WindowInsetObserver mObserver : mObservers) {
                mObserver.onSafeAreaChanged(mCurrentSafeArea);
            }
        }
    }
}
