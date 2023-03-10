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

import androidx.annotation.NonNull;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsAnimationCompat;
import androidx.core.view.WindowInsetsAnimationCompat.BoundsCompat;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.base.ObserverList;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;

import java.util.ArrayList;
import java.util.List;

/**
 * The purpose of this view is to store the system window insets (OSK, status bar) for
 * later use.
 */
public class InsetObserverView extends View {
    private final Rect mWindowInsets;
    protected final ObserverList<WindowInsetObserver> mObservers;
    private final BottomInsetObservableSupplier mBottomInsetSupplier;
    private final WindowInsetsAnimationCompat.Callback mWindowInsetsAnimationProxyCallback;
    private final List<WindowInsetsAnimationListener> mWindowInsetsAnimationListeners =
            new ArrayList<>();
    private List<WindowInsetsConsumer> mInsetsConsumers = new ArrayList<>();

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

    /**
     * Alias for {@link  androidx.core.view.OnApplyWindowInsetsListener} to emphasize that an
     * implementing class expects to consume insets, not just observe them. "Consuming" means "my
     * view/component will adjust its size to account for the space required by the inset." For
     * instance, the omnibox could "consume" the IME (keyboard) inset by adjusting the height of its
     * list of suggestions. By default the android framework handles which views consume insets by
     * applying them down the view hierarchy. You only need to add a consumer here if you want to
     * enable custom behavior, e.g. you want to shield a specific view from inset changes by
     * consuming them elsewhere.
     */
    public interface WindowInsetsConsumer extends androidx.core.view.OnApplyWindowInsetsListener {}

    /**
     * Interface equivalent of {@link  WindowInsetsAnimationCompat.Callback}. This allows
     * implementers to be notified of inset animation progress, enabling synchronization of browser
     * UI changes with system inset changes. This synchronization is potentially imperfect on API
     * level <30. Note that the interface version currently disallows modification of the insets
     * dispatched to the subtree. See {@link WindowInsetsAnimationCompat.Callback} for more.
     */
    public interface WindowInsetsAnimationListener {
        void onPrepare(@NonNull WindowInsetsAnimationCompat animation);

        void onStart(@NonNull WindowInsetsAnimationCompat animation,
                @NonNull WindowInsetsAnimationCompat.BoundsCompat bounds);

        void onProgress(@NonNull WindowInsetsCompat windowInsetsCompat,
                @NonNull List<WindowInsetsAnimationCompat> list);

        void onEnd(@NonNull WindowInsetsAnimationCompat animation);
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
        mWindowInsetsAnimationProxyCallback = new WindowInsetsAnimationCompat.Callback(
                WindowInsetsAnimationCompat.Callback.DISPATCH_MODE_STOP) {
            @Override
            public void onPrepare(@NonNull WindowInsetsAnimationCompat animation) {
                for (WindowInsetsAnimationListener listener : mWindowInsetsAnimationListeners) {
                    listener.onPrepare(animation);
                }
                super.onPrepare(animation);
            }

            @NonNull
            @Override
            public BoundsCompat onStart(
                    @NonNull WindowInsetsAnimationCompat animation, @NonNull BoundsCompat bounds) {
                for (WindowInsetsAnimationListener listener : mWindowInsetsAnimationListeners) {
                    listener.onStart(animation, bounds);
                }
                return super.onStart(animation, bounds);
            }

            @Override
            public void onEnd(@NonNull WindowInsetsAnimationCompat animation) {
                for (WindowInsetsAnimationListener listener : mWindowInsetsAnimationListeners) {
                    listener.onEnd(animation);
                }
                super.onEnd(animation);
            }

            @NonNull
            @Override
            public WindowInsetsCompat onProgress(@NonNull WindowInsetsCompat windowInsetsCompat,
                    @NonNull List<WindowInsetsAnimationCompat> list) {
                for (WindowInsetsAnimationListener listener : mWindowInsetsAnimationListeners) {
                    listener.onProgress(windowInsetsCompat, list);
                }
                return windowInsetsCompat;
            }
        };

        ViewCompat.setWindowInsetsAnimationCallback(this, mWindowInsetsAnimationProxyCallback);
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
     * Add a consumer of window insets. Consumers are given the opportunity to consume insets in
     * the order they're added.
     */
    public void addInsetsConsumer(@NonNull WindowInsetsConsumer insetConsumer) {
        mInsetsConsumers.add(insetConsumer);
    }

    /** Remove a consumer of window insets.*/
    public void removeInsetsConsumer(@NonNull WindowInsetsConsumer insetConsumer) {
        mInsetsConsumers.remove(insetConsumer);
    }

    /** Add a listener for inset animations. */
    public void addWindowInsetsAnimationListener(@NonNull WindowInsetsAnimationListener listener) {
        mWindowInsetsAnimationListeners.add(listener);
    }

    /** Remove a listener for inset animations. */
    public void removeWindowInsetsAnimationListener(
            @NonNull WindowInsetsAnimationListener listener) {
        mWindowInsetsAnimationListeners.remove(listener);
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

    @VisibleForTesting
    public WindowInsetsAnimationCompat.Callback getInsetAnimationProxyCallbackForTesting() {
        return mWindowInsetsAnimationProxyCallback;
    }

    @Override
    public WindowInsets onApplyWindowInsets(WindowInsets insets) {
        insets = forwardToInsetConsumers(insets);
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

    private WindowInsets forwardToInsetConsumers(WindowInsets insets) {
        WindowInsetsCompat windowInsetsCompat =
                WindowInsetsCompat.toWindowInsetsCompat(insets, this);
        for (WindowInsetsConsumer consumer : mInsetsConsumers) {
            windowInsetsCompat = consumer.onApplyWindowInsets(this, windowInsetsCompat);
        }
        return windowInsetsCompat.toWindowInsets();
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
