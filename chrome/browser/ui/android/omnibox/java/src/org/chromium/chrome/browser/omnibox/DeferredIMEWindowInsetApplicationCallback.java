// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.app.Activity;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.core.graphics.Insets;
import androidx.core.view.WindowInsetsAnimationCompat;
import androidx.core.view.WindowInsetsAnimationCompat.BoundsCompat;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.ui.InsetObserver;
import org.chromium.ui.InsetObserver.WindowInsetsAnimationListener;
import org.chromium.ui.InsetObserver.WindowInsetsConsumer;
import org.chromium.ui.base.WindowAndroid;

import java.util.List;

/**
 * Class that, while attached, consumes all IME window insets and listens for insets animation
 * updates. This combination lets it selectively defer the application of IME insets until the
 * animation process is complete, avoiding premature layout at a shortened height. Since animation
 * isn't guaranteed to occur in practice, deferred application is only practiced when an animation
 * is known to be running.
 */
public class DeferredIMEWindowInsetApplicationCallback
        implements WindowInsetsConsumer, WindowInsetsAnimationListener {
    private static final int NO_DEFERRED_KEYBOARD_HEIGHT = -1;
    private int mDeferredKeyboardHeight = NO_DEFERRED_KEYBOARD_HEIGHT;
    private int mKeyboardHeight;
    private boolean mAnimationInProgress;
    private WindowInsetsAnimationCompat mCurrentAnimation;
    private InsetObserver mInsetObserver;
    private final Runnable mOnUpdateCallback;

    /**
     * Constructs a new DeferredIMEWindowInsetApplicationCallback.
     *
     * @param onUpdateCallback Callback to be invoked when the keyboard height changes.
     */
    public DeferredIMEWindowInsetApplicationCallback(@NonNull Runnable onUpdateCallback) {
        mOnUpdateCallback = onUpdateCallback;
    }

    /**
     * Attaches this callback to the root of the given window, activating interception of its IME
     * window insets and listening for IME animation updates. Attach will be skipped if the window's
     * activity that is already finishing.
     */
    public void attach(WindowAndroid windowAndroid) {
        // If the activity is finishing or the window is destroyed this attach is a no-op.
        if (windowAndroid.isDestroyed()) return;

        Activity activity = windowAndroid.getActivity().get();
        if (activity != null && activity.isFinishing()) return;

        InsetObserver insetObserver = windowAndroid.getInsetObserver();
        assert insetObserver != null
                : "DeferredIMEWindowInsetApplicationCallback can only be used in activities with an"
                        + " InsetObserverView";
        mInsetObserver = insetObserver;
        insetObserver.addInsetsConsumer(this);
        insetObserver.addWindowInsetsAnimationListener(this);
    }

    /** Detaches this callback from the root of the given window. */
    public void detach() {
        // Allow for a null inset observer here if the attach was a no-op.
        if (mInsetObserver != null) {
            mInsetObserver.removeInsetsConsumer(this);
            mInsetObserver.removeWindowInsetsAnimationListener(this);
        }
        mAnimationInProgress = false;
        mDeferredKeyboardHeight = NO_DEFERRED_KEYBOARD_HEIGHT;
        mKeyboardHeight = 0;
        mInsetObserver = null;
    }

    public int getCurrentKeyboardHeight() {
        return mKeyboardHeight;
    }

    @Override
    public void onPrepare(@NonNull WindowInsetsAnimationCompat animation) {
        if ((animation.getTypeMask() & WindowInsetsCompat.Type.ime()) == 0) return;
        mAnimationInProgress = true;
        mCurrentAnimation = animation;
        mDeferredKeyboardHeight = NO_DEFERRED_KEYBOARD_HEIGHT;
    }

    @Override
    public void onStart(
            @NonNull WindowInsetsAnimationCompat animation, @NonNull BoundsCompat bounds) {}

    @Override
    public void onProgress(
            @NonNull WindowInsetsCompat windowInsetsCompat,
            @NonNull List<WindowInsetsAnimationCompat> list) {}

    @Override
    public void onEnd(@NonNull WindowInsetsAnimationCompat animation) {
        if ((animation.getTypeMask() & WindowInsetsCompat.Type.ime()) == 0
                || animation != mCurrentAnimation) {
            return;
        }

        mAnimationInProgress = false;
        if (mDeferredKeyboardHeight != NO_DEFERRED_KEYBOARD_HEIGHT) {
            commitKeyboardHeight(mDeferredKeyboardHeight);
        }
    }

    @NonNull
    @Override
    public WindowInsetsCompat onApplyWindowInsets(
            @NonNull View view, @NonNull WindowInsetsCompat windowInsetsCompat) {
        int newKeyboardHeight = 0;
        Insets imeInsets = windowInsetsCompat.getInsets(WindowInsetsCompat.Type.ime());
        if (imeInsets.bottom > 0) {
            Insets systemBarInsets =
                    windowInsetsCompat.getInsets(WindowInsetsCompat.Type.systemBars());
            newKeyboardHeight = imeInsets.bottom - systemBarInsets.bottom;
        }
        // Keyboard going away or the change is not animated; apply immediately.
        if (newKeyboardHeight < mKeyboardHeight || !mAnimationInProgress) {
            commitKeyboardHeight(newKeyboardHeight);
        } else if (newKeyboardHeight > 0) {
            // Animated keyboard show - defer application.
            mDeferredKeyboardHeight = newKeyboardHeight;
        }

        // Zero out (consume) the ime insets; we're applying them ourselves so no one else needs
        // to consume them.
        return new WindowInsetsCompat.Builder(windowInsetsCompat)
                .setInsets(WindowInsetsCompat.Type.ime(), Insets.NONE)
                .build();
    }

    private void commitKeyboardHeight(int newKeyboardHeight) {
        mKeyboardHeight = newKeyboardHeight;
        mDeferredKeyboardHeight = NO_DEFERRED_KEYBOARD_HEIGHT;
        mOnUpdateCallback.run();
    }
}
