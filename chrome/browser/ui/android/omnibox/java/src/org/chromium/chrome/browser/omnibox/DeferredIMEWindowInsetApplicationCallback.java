// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import android.view.View;
import android.view.WindowInsets;

import androidx.annotation.NonNull;
import androidx.core.graphics.Insets;
import androidx.core.view.OnApplyWindowInsetsListener;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsAnimationCompat;
import androidx.core.view.WindowInsetsCompat;

import org.chromium.ui.base.WindowAndroid;

import java.util.List;

/**
 * Class that, while attached, consumes all IME window insets and listens for insets animation
 * updates. This combination lets it selectively defer the application of IME insets until the
 * animation process is complete, avoiding premature layout at a shortened height. Since
 * animation isn't guaranteed to occur in practice, deferred application is only practiced when
 * an animation is known to be running.
 */
class DeferredIMEWindowInsetApplicationCallback
        extends WindowInsetsAnimationCompat.Callback implements OnApplyWindowInsetsListener {
    private static final int NO_DEFERRED_KEYBOARD_HEIGHT = -1;
    private int mDeferredKeyboardHeight = NO_DEFERRED_KEYBOARD_HEIGHT;
    private int mKeyboardHeight;
    private boolean mAnimationInProgress;
    private WindowInsetsAnimationCompat mCurrentAnimation;
    private View mView;
    private final Runnable mOnUpdateCallback;

    /**
     * Constructs a new DeferredIMEWindowInsetApplicationCallback.
     * @param onUpdateCallback Callback to be invoked when the keyboard height changes.
     */
    public DeferredIMEWindowInsetApplicationCallback(@NonNull Runnable onUpdateCallback) {
        super(WindowInsetsAnimationCompat.Callback.DISPATCH_MODE_CONTINUE_ON_SUBTREE);
        mOnUpdateCallback = onUpdateCallback;
    }

    /**
     * Attaches this callback to the root of the given window, activating interception of its
     * IME window insets and listening for IME animation updates.
     */
    public void attach(WindowAndroid windowAndroid) {
        mView = windowAndroid.getActivity().get().getWindow().getDecorView();
        ViewCompat.setWindowInsetsAnimationCallback(mView, this);
        ViewCompat.setOnApplyWindowInsetsListener(mView, this);
    }

    /** Detaches this callback from the root of the given window. */
    public void detach() {
        ViewCompat.setWindowInsetsAnimationCallback(mView, null);
        ViewCompat.setOnApplyWindowInsetsListener(mView, null);
        mView = null;
        mAnimationInProgress = false;
        mDeferredKeyboardHeight = NO_DEFERRED_KEYBOARD_HEIGHT;
        mKeyboardHeight = 0;
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

    @NonNull
    @Override
    public WindowInsetsCompat onProgress(@NonNull WindowInsetsCompat windowInsetsCompat,
            @NonNull List<WindowInsetsAnimationCompat> list) {
        return windowInsetsCompat;
    }

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
            Insets navigationBarInsets =
                    windowInsetsCompat.getInsets(WindowInsetsCompat.Type.navigationBars());
            newKeyboardHeight = imeInsets.bottom - navigationBarInsets.bottom;
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
        WindowInsets windowInsets = new WindowInsetsCompat.Builder(windowInsetsCompat)
                                            .setInsets(WindowInsetsCompat.Type.ime(), Insets.NONE)
                                            .build()
                                            .toWindowInsets();
        return WindowInsetsCompat.toWindowInsetsCompat(view.onApplyWindowInsets(windowInsets));
    }

    private void commitKeyboardHeight(int newKeyboardHeight) {
        mKeyboardHeight = newKeyboardHeight;
        mDeferredKeyboardHeight = NO_DEFERRED_KEYBOARD_HEIGHT;
        mOnUpdateCallback.run();
    }
}
