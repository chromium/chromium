// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.content.Context;
import android.graphics.PixelFormat;
import android.os.Build;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.view.AttachedSurfaceControl;
import android.view.SurfaceControl;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.window.InputTransferToken;

import androidx.annotation.RequiresApi;
import androidx.recyclerview.widget.RecyclerView;

import org.jni_zero.JniUniquePtr;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionsOmtInputAnchor.NativeOmtInputReceiverState;
import org.chromium.ui.base.DeviceInput;

import java.util.concurrent.atomic.AtomicBoolean;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Transparent SurfaceView overlay attached above the {@link OmniboxSuggestionsDropdown} to
 * intercept touches off-main-thread. Only instantiated and used when the OMT prefetch feature is
 * enabled on Android 16+ (Baklava).
 */
@NullMarked
class SuggestionsOmtOverlayView extends SurfaceView implements SurfaceHolder.Callback {
    private final HandlerThread mInputThread;
    private final Handler mInputHandler;
    private final Handler mMainHandler = new Handler(Looper.getMainLooper());

    /** The companion {@link OmniboxSuggestionsDropdown} whose state dictates overlay visibility. */
    private final OmniboxSuggestionsDropdown mDropdown;

    /**
     * Tracks whether a touch gesture transfer from this overlay to the main window is currently in
     * progress.
     *
     * <p>Threading and Concurrency:
     *
     * <ul>
     *   <li>Written to {@code true} on the background input thread via {@link
     *       #onTouchTransferred()} when {@link WindowManager#transferTouchGesture} succeeds in
     *       native code.
     *   <li>Read on the UI thread in {@link
     *       OmniboxSuggestionsDropdown#dispatchWindowFocusChanged(boolean)} to suppress transient
     *       focus loss on dropdown items while the transfer completes.
     *   <li>Reset to {@code false} on the UI thread when the gesture ends or the surface/parent is
     *       detached.
     * </ul>
     */
    private final AtomicBoolean mTouchTransferActive = new AtomicBoolean(false);

    /**
     * Lifecycle generation counter for this overlay {@link SurfaceView}'s backing {@link
     * SurfaceHolder}.
     *
     * <p>Threading and Concurrency:
     *
     * <ul>
     *   <li>Incremented on the UI thread in {@link #surfaceCreated(SurfaceHolder)} and {@link
     *       #surfaceDestroyed(SurfaceHolder)}.
     *   <li>Read on the background {@link #mInputHandler} thread prior to invoking native {@code
     *       registerInputReceiver()} to verify the surface has not been destroyed while the task
     *       was queued.
     * </ul>
     */
    private final AtomicInteger mSurfaceGeneration = new AtomicInteger(0);

    private final View.OnLayoutChangeListener mLayoutChangeListener =
            (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) ->
                    updateOverlayVisualState();

    private final RecyclerView.OnScrollListener mScrollListener =
            new RecyclerView.OnScrollListener() {
                @Override
                public void onScrollStateChanged(RecyclerView recyclerView, int newState) {
                    updateOverlayVisualState();
                }
            };

    /**
     * Owned handle to the allocated {@code OmtInputReceiverState} struct holding the native {@code
     * AInputReceiver}, input transfer tokens, and touch transfer callback.
     *
     * <p>Initialized in {@link #surfaceCreated(SurfaceHolder)} via {@link
     * SuggestionsOmtInputAnchor.Natives#registerInputReceiver} and destroyed in {@link
     * #surfaceDestroyed(SurfaceHolder)} via {@link JniUniquePtr#destroy()}.
     *
     * <p>Threading and Concurrency: Written and cleared exclusively on the background input thread
     * ({@link #mInputHandler}) to serialize native receiver setup and teardown.
     */
    private @Nullable JniUniquePtr<NativeOmtInputReceiverState> mNativeReceiverState;

    /**
     * Constructs a new overlay view and starts its dedicated background input looper thread.
     *
     * @param context The Context the view is running in.
     * @param dropdown The companion {@link OmniboxSuggestionsDropdown}.
     */
    SuggestionsOmtOverlayView(Context context, OmniboxSuggestionsDropdown dropdown) {
        super(context);
        mDropdown = dropdown;
        mInputThread = new HandlerThread("OmniboxOmtInputThread");
        mInputThread.start();
        mInputHandler = new Handler(mInputThread.getLooper());
        // By default, a SurfaceView's SurfaceControl is placed behind its host window's surface.
        // Because SurfaceFlinger routes input based on surface Z-order, placing this surface on top
        // is required so initial ACTION_DOWN touches within the dropdown bounds hit the background
        // AInputReceiver before being transferred back to the main window. This is safe visually
        // and behaviorally because the surface uses PixelFormat.TRANSLUCENT (drawing nothing), is
        // sized strictly to mDropdown's bounds, is hidden whenever the dropdown is hidden or
        // scrolling or a precision pointer is connected, and immediately transfers any intercepted
        // touch gesture to the main window token on ACTION_DOWN.
        setZOrderOnTop(true);
        getHolder().setFormat(PixelFormat.TRANSLUCENT);
        getHolder().addCallback(this);
    }

    /**
     * Attaches the overlay view as a child to {@code parent}, initially hidden, and synchronizes
     * its visibility state with the companion dropdown.
     *
     * @param parent The parent ViewGroup (typically the container layout) to attach into.
     */
    void attachToParent(ViewGroup parent) {
        setVisibility(View.GONE);
        parent.addView(
                this, new ViewGroup.LayoutParams(mDropdown.getWidth(), mDropdown.getHeight()));
        mDropdown.addOnLayoutChangeListener(mLayoutChangeListener);
        mDropdown.addOnScrollListener(mScrollListener);
        updateOverlayVisualState();
    }

    /** Detaches the overlay view from its parent and resets active touch transfer state. */
    private void detachFromParent() {
        mDropdown.removeOnLayoutChangeListener(mLayoutChangeListener);
        mDropdown.removeOnScrollListener(mScrollListener);
        resetTouchTransfer();
        ViewGroup parent = (ViewGroup) getParent();
        if (parent != null) {
            parent.removeView(this);
        }
    }

    /** Cleans up resources associated with the overlay view. */
    void destroy() {
        detachFromParent();
        mInputThread.quitSafely();
    }

    /**
     * Updates the overlay surface visibility and bounds based on live dropdown dimensions, scroll
     * state, connected pointing devices, and layout bounds.
     *
     * <p>Must be called on the UI thread.
     */
    void updateOverlayVisualState() {
        matchSuggestionsDropdownPlacement();

        // Require the host window to hold focus so focusable popups or dialogs displayed over part
        // of the dropdown are not blocked from receiving outside-touch dismissal events by the
        // overlay's Z-order-on-top surface. However, when ACTION_DOWN hits the overlay surface,
        // InputDispatcher shifts input focus to the overlay's surface token and dispatches
        // MSG_WINDOW_FOCUS_CHANGED(false) to ViewRootImpl, setting mAttachInfo.mHasWindowFocus to
        // false until transferTouchGesture returns focus to the main window. Treating an active
        // touch transfer as focused prevents layout callbacks during that transient window-focus
        // loss from hiding the SurfaceView and destroying its SurfaceControl/AInputReceiver
        // mid-gesture.
        boolean isVisible =
                mDropdown.isShown()
                        && mDropdown.getWindowVisibility() == View.VISIBLE
                        && (mDropdown.hasWindowFocus() || isTouchTransferActive());
        boolean isIdle = mDropdown.getScrollState() == RecyclerView.SCROLL_STATE_IDLE;
        boolean hasBounds = mDropdown.getWidth() > 0 && mDropdown.getHeight() > 0;
        // Enable overlay only when the dropdown is visible, stationary (IDLE),
        // no precision pointing devices are connected, and layout bounds are positive and
        // non-empty.
        boolean shouldBeVisible =
                isVisible && isIdle && !DeviceInput.supportsPrecisionPointer() && hasBounds;
        setVisibility(shouldBeVisible ? View.VISIBLE : View.GONE);
    }

    private void matchSuggestionsDropdownPlacement() {
        setTranslationX(mDropdown.getLeft());
        setTranslationY(mDropdown.getTop());
        ViewGroup.LayoutParams layoutParams = getLayoutParams();
        int targetWidth = mDropdown.getWidth();
        int targetHeight = mDropdown.getHeight();
        if (layoutParams != null
                && (layoutParams.width != targetWidth || layoutParams.height != targetHeight)) {
            layoutParams.width = targetWidth;
            layoutParams.height = targetHeight;
            // OnLayoutChangeListener fires synchronously inside mDropdown.layout() while the parent
            // ViewGroup is iterating through its children in onLayout(). Because this overlay was
            // added after mDropdown, it has already been measured with its old LayoutParams in the
            // current traversal and is about to have its own layout() called next in the parent's
            // loop. Calling setLayoutParams() synchronously during layout sets PFLAG_FORCE_LAYOUT /
            // PFLAG_LAYOUT_REQUIRED, which the subsequent layout() call immediately clears using
            // the stale measured dimensions, causing ViewRootImpl to drop the layout request.
            // Posting defers requestLayout() until after performLayout() finishes.
            if (isInLayout()) {
                mMainHandler.post(() -> setLayoutParams(layoutParams));
            } else {
                setLayoutParams(layoutParams);
            }
        }
    }

    void onTouchTransferred() {
        mTouchTransferActive.set(true);
    }

    boolean isTouchTransferActive() {
        return mTouchTransferActive.get();
    }

    void resetTouchTransfer() {
        mTouchTransferActive.set(false);
    }

    void onGestureEnded() {
        // WindowManager dispatches MSG_WINDOW_FOCUS_CHANGED asynchronously via the main
        // Looper queue. For fast taps or flings, ACTION_UP completes before the queued focus
        // messages are processed. Posting the reset places it at the tail of the FIFO queue
        // so in-flight focus transitions from the transfer are processed before clearing state.
        mMainHandler.post(
                () -> {
                    mTouchTransferActive.set(false);
                    updateOverlayVisualState();
                });
    }

    @Override
    @RequiresApi(Build.VERSION_CODES.BAKLAVA)
    public void surfaceCreated(SurfaceHolder holder) {
        AttachedSurfaceControl control = getRootSurfaceControl();
        if (control == null) {
            return;
        }
        InputTransferToken mainWindowToken = control.getInputTransferToken();
        if (mainWindowToken == null) {
            return;
        }
        final int generation = mSurfaceGeneration.incrementAndGet();
        SurfaceControl surfaceControl = getSurfaceControl();
        mInputHandler.post(
                () -> {
                    if (mSurfaceGeneration.get() != generation) {
                        return;
                    }
                    mNativeReceiverState =
                            SuggestionsOmtInputAnchorJni.get()
                                    .registerInputReceiver(
                                            surfaceControl,
                                            mainWindowToken,
                                            this::onTouchTransferred);
                });
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {}

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        mTouchTransferActive.set(false);
        mSurfaceGeneration.incrementAndGet();
        mInputHandler.post(
                () -> {
                    if (mNativeReceiverState != null) {
                        mNativeReceiverState.destroy();
                        mNativeReceiverState = null;
                    }
                });
    }

    HandlerThread getInputThreadForTesting() {
        return mInputThread;
    }

    Handler getInputHandlerForTesting() {
        return mInputHandler;
    }
}
