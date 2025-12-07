// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.stylus_handwriting;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.graphics.Point;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.view.MotionEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;

import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.StylusWritingHandler;
import org.chromium.content_public.browser.StylusWritingImeCallback;
import org.chromium.content_public.browser.WebContents;

/**
 * Direct writing class that manages Input events, starting and stopping of recognition. Forwards
 * calls to DW service connection handler class {@link DirectWritingServiceBinder}. Also, sets the
 * {@link StylusWritingHandler} to receive messages about stylus writing events.
 */
@NullMarked
class DirectWritingTrigger implements StylusWritingHandler, StylusApiOption {
    private static final String TAG = "DwTrigger";

    private DirectWritingServiceBinder mBinder = new DirectWritingServiceBinder();
    private final DirectWritingServiceConfiguration mConfig =
            new DirectWritingServiceConfiguration();

    // Track whether DW recognition has been started.
    private boolean mRecognitionStarted;

    private final Handler mHandler = new Handler();

    // Token to determine if stylus writing can be continued without re-detection.
    private @Nullable Object mStopWritingCallbackToken;

    // Token to hide the DW toolbar as stylus wasn't used for a while.
    private @Nullable Object mHideDwToolbarCallbackToken;

    // Track whether DW service is enabled or not.
    private boolean mDwServiceEnabled;

    // Tracks whether handwriting hover icon is being shown or not.
    private boolean mIsHandwritingIconShowing;

    private @Nullable StylusWritingImeCallback mStylusWritingImeCallback;
    private @Nullable DirectWritingServiceCallback mCallback;

    private @Nullable MotionEvent mCurrentStylusDownEvent;
    private @Nullable MotionEvent mStylusUpEvent;
    private @Nullable Rect mEditableNodeBounds;
    private boolean mStylusWritingDetected;
    private boolean mNeedsFocusedNodeChangedAfterTouchUp;
    private boolean mWasButtonPressed;

    /**
     * Sets the stylus writing handler to current web contents when initialized to receive messages
     * via {@link StylusWritingHandler}
     *
     * @param context current {@link Context}
     * @param webContents current web contents
     */
    @Override
    public void onWebContentsChanged(Context context, WebContents webContents) {
        updateDwSettings(context);
        webContents.setStylusWritingHandler(this);
        mStylusWritingImeCallback = webContents.getStylusWritingImeCallback();
        assumeNonNull(mCallback);
        mCallback.setImeCallback(mStylusWritingImeCallback);
    }

    @Override
    public void onFocusedNodeChanged(
            Rect editableBoundsOnScreenDip,
            boolean isEditable,
            View currentView,
            float scaleFactor,
            int contentOffsetY) {
        if (!mDwServiceEnabled || !mBinder.isServiceConnected()) return;

        Rect bounds =
                new Rect(
                        Math.round(editableBoundsOnScreenDip.left * scaleFactor),
                        Math.round(editableBoundsOnScreenDip.top * scaleFactor),
                        Math.round(editableBoundsOnScreenDip.right * scaleFactor),
                        Math.round(editableBoundsOnScreenDip.bottom * scaleFactor));
        bounds.offset(0, contentOffsetY);

        if (isEditable) {
            if (!mStylusWritingDetected
                    && mNeedsFocusedNodeChangedAfterTouchUp
                    && mStylusUpEvent != null) {
                mBinder.updateEditableBounds(bounds, currentView, true);
                // Call onStopRecognition with editable bounds to show DW toolbar on Pen TAP in
                // input field.
                onStopRecognition(mStylusUpEvent, bounds, currentView);
                mNeedsFocusedNodeChangedAfterTouchUp = false;
            }
        } else {
            // Stop recognition and hide DW toolbar as focused node is not editable.
            hideDwToolbar();
            onStopRecognition(/* motionEvent= */ null, /* editableBounds= */ null, currentView);
        }

        mEditableNodeBounds = bounds;
        assumeNonNull(mCallback);
        mCallback.updateEditableBounds(bounds, /* cursorPosition= */ new Point());
    }

    @Override
    public boolean shouldInitiateStylusWriting() {
        if (!mDwServiceEnabled || !mBinder.isServiceConnected()) return false;
        mStylusWritingDetected = true;
        return true;
    }

    private void startRecognition(Rect editableBound) {
        if (mCurrentStylusDownEvent == null || mStylusWritingImeCallback == null) return;

        View rootView = mStylusWritingImeCallback.getContainerView();
        if (!mBinder.startRecognition(editableBound, mCurrentStylusDownEvent, rootView)) return;
        mRecognitionStarted = true;
        // Dispatch stored action down before action move, when writing is not yet started.
        onDispatchEvent(mCurrentStylusDownEvent, rootView);
        mStylusWritingImeCallback.resetGestureDetection();
    }

    @Override
    public boolean canShowSoftKeyboard() {
        // We avoid showing soft keyboard during direct writing as there is widget toolbar provided
        // by the service that allows options like add space, backspace, show/hide keyboard, and to
        // perform editor actions like next, prev, search, go, etc. It can be noted that Platform
        // Edit Text also does not show keyboard during direct writing.
        return false;
    }

    private void updateDwServiceStatus(Context context) {
        mDwServiceEnabled = isDirectWritingServiceEnabled(context);
        Log.i(TAG, "updateDwServiceStatus() : isEnabled = " + mDwServiceEnabled);
    }

    /**
     * Updates whether the Direct writing service is enabled or not.
     *
     * @param context current context
     */
    @VisibleForTesting
    void updateDwSettings(Context context) {
        boolean wasDwEnabled = mDwServiceEnabled;
        updateDwServiceStatus(context);
        if (!wasDwEnabled && mDwServiceEnabled) {
            onDwServiceEnabled();
        }
    }

    private void onDwServiceEnabled() {
        // Create IDirectWritingServiceCallbackImpl instance when DW setting is changed to
        // enabled. Platform Crash occurs if it is created when DW setting is not enabled.
        if (mCallback != null) return;
        mCallback = new DirectWritingServiceCallback();
        mCallback.setTriggerCallback(
                new DirectWritingServiceCallback.TriggerCallback() {
                    @Override
                    public void updateEditableBoundsToService() {
                        if (mStylusWritingImeCallback == null) return;
                        mBinder.updateEditableBounds(
                                mEditableNodeBounds,
                                mStylusWritingImeCallback.getContainerView(),
                                true);
                    }

                    @Override
                    public boolean isHandwritingIconShowing() {
                        return mIsHandwritingIconShowing;
                    }
                });
    }

    @Override
    public void onFocusChanged(boolean hasFocus) {
        if (!hasFocus) {
            // Hide DW toolbar and Stop Recognition when View focus is lost.
            hideDwToolbar();
            onStopRecognition(/* motionEvent= */ null, /* editableBounds= */ null);
        }
    }

    @Override
    public void updateHandlerState(Context context, boolean hasWindowFocus) {
        if (hasWindowFocus) {
            updateDwSettings(context);
        } else {
            hideDwToolbar();
        }
        if (!mDwServiceEnabled) return;
        mBinder.handleWindowFocusChanged(context, hasWindowFocus);
    }

    /**
     * Notify the view is detached from window.
     *
     * @param context the current context
     */
    @Override
    public void onDetachedFromWindow(Context context) {
        // Unbind service when view is detached.
        if (!mDwServiceEnabled || !mBinder.isServiceConnected()) return;
        mBinder.unbindService(context);
    }

    @Override
    public void onImeAdapterDestroyed() {
        assumeNonNull(mCallback);
        mStylusWritingImeCallback = null;
        mCallback.setImeCallback(null);
    }

    /*
     * This API needs to be called before starting recognition to bind direct writing service.
     */
    private void bindDirectWritingService(View rootView) {
        mBinder.bindService(
                rootView.getContext(),
                new DirectWritingServiceBinder.DirectWritingTriggerCallback() {
                    @Override
                    public void updateConfiguration(Bundle bundle) {
                        mConfig.update(bundle);
                    }

                    @Override
                    public DirectWritingServiceCallback getServiceCallback() {
                        assumeNonNull(mCallback);
                        return mCallback;
                    }
                });
    }

    @VisibleForTesting
    @Nullable DirectWritingServiceCallback getServiceCallback() {
        return mCallback;
    }

    void setServiceCallbackForTest(DirectWritingServiceCallback serviceCallback) {
        mCallback = serviceCallback;
    }

    void setServiceBinderForTest(DirectWritingServiceBinder serviceBinder) {
        mBinder = serviceBinder;
    }

    @VisibleForTesting
    @Nullable StylusWritingImeCallback getStylusWritingImeCallbackForTest() {
        return mStylusWritingImeCallback;
    }

    @VisibleForTesting
    boolean stylusWritingDetected() {
        return mStylusWritingDetected;
    }

    /**
     * Handle hover events for Direct writing.
     *
     * @param event MotionEvent to be handled.
     * @param currentView the View which is receiving the events.
     */
    @RequiresApi(api = Build.VERSION_CODES.P)
    @Override
    public void handleHoverEvent(MotionEvent event, View currentView) {
        if (!mDwServiceEnabled) return;
        if (event.getToolType(0) != MotionEvent.TOOL_TYPE_STYLUS
                && event.getToolType(0) != MotionEvent.TOOL_TYPE_ERASER) {
            return;
        }
        // Try to connect and bind DW service if not connected already.
        if (!mBinder.isServiceConnected() && event.getAction() == MotionEvent.ACTION_HOVER_ENTER) {
            bindDirectWritingService(currentView);
        }
        handlePenEvent(event, currentView);
    }

    /**
     * Handle touch events if needed for Direct writing.
     *
     * @param event MotionEvent to be handled.
     * @param currentView the View which is receiving the events.
     * @return true if event is consumed by Direct writing system.
     */
    @RequiresApi(api = Build.VERSION_CODES.P)
    @Override
    public boolean handleTouchEvent(MotionEvent event, View currentView) {
        if (!mDwServiceEnabled) return false;
        if (handleButtonEvent(event)) {
            return false;
        }
        if (event.getToolType(0) == MotionEvent.TOOL_TYPE_STYLUS
                || event.getToolType(0) == MotionEvent.TOOL_TYPE_ERASER) {
            return handlePenEvent(event, currentView);
        } else {
            // Hide the DW toolbar when stylus is not being used.
            if (event.getActionMasked() == MotionEvent.ACTION_DOWN) {
                hideDwToolbar();
            }
        }
        return false;
    }

    private boolean handleButtonEvent(MotionEvent me) {
        if (me.isButtonPressed(MotionEvent.BUTTON_STYLUS_PRIMARY)) {
            if (me.getAction() == MotionEvent.ACTION_DOWN) {
                mWasButtonPressed = true;
            }
            return true;
        } else if (mWasButtonPressed) {
            if (me.getAction() == MotionEvent.ACTION_UP) {
                mWasButtonPressed = false;
            }
            return true;
        }
        return false;
    }

    @RequiresApi(api = Build.VERSION_CODES.P)
    private boolean handlePenEvent(MotionEvent me, View rootView) {
        int action = me.getAction();
        switch (action) {
            case MotionEvent.ACTION_DOWN:
                {
                    if (mHideDwToolbarCallbackToken != null) {
                        mHandler.removeCallbacksAndMessages(mHideDwToolbarCallbackToken);
                        mHideDwToolbarCallbackToken = null;
                    }

                    mCurrentStylusDownEvent = MotionEvent.obtain(me);
                    mNeedsFocusedNodeChangedAfterTouchUp = false;

                    if (mStopWritingCallbackToken != null) {
                        // We're still writing from last time.
                        mHandler.removeCallbacksAndMessages(mStopWritingCallbackToken);
                        mStopWritingCallbackToken = null;
                        onDispatchEvent(me, rootView);
                        return true;
                    }

                    // Reset cached stylus writing status when keep writing timer has expired to
                    // re-detect if writing is still over an input element.
                    mStylusWritingDetected = false;
                    mRecognitionStarted = false;
                    return false;
                }
            case MotionEvent.ACTION_MOVE:
                {
                    if (mRecognitionStarted) {
                        // Consume touch events once writing has started.
                        onDispatchEvent(me, rootView);
                        return true;
                    } else {
                        return false;
                    }
                }
            case MotionEvent.ACTION_UP:
                {
                    if (mRecognitionStarted) {
                        onDispatchEvent(me, rootView);
                        mStopWritingCallbackToken = new Object();
                        mHandler.postDelayed(
                                () -> {
                                    resetRecognition();
                                    mStopWritingCallbackToken = null;
                                },
                                mStopWritingCallbackToken,
                                mConfig.getKeepWritingDelayMs());
                        return true;
                    } else {
                        // Handle ACTION_UP in editable field, to show DW Toolbar.
                        if (mEditableNodeBounds != null
                                && !mEditableNodeBounds.isEmpty()
                                && mCurrentStylusDownEvent != null
                                && mEditableNodeBounds.contains(
                                        (int) mCurrentStylusDownEvent.getX(),
                                        (int) mCurrentStylusDownEvent.getY())) {
                            onStopRecognition(me, mEditableNodeBounds, rootView);
                        } else {
                            // It is possible that Pen TAP is done in an Input element without
                            // writing, so wait until element is focused to show DW toolbar.
                            mStylusUpEvent = MotionEvent.obtain(me);
                            mNeedsFocusedNodeChangedAfterTouchUp = true;
                        }
                        return false;
                    }
                }
            case MotionEvent.ACTION_HOVER_EXIT:
                {
                    // Hover exit is not forwarded to blink, so reset hover icon showing state.
                    mIsHandwritingIconShowing = false;

                    if (!mRecognitionStarted) break;
                    // Post task to stop recognition and hide DW toolbar as stylus is moved away.
                    mHideDwToolbarCallbackToken = new Object();
                    mHandler.postDelayed(
                            () -> {
                                onStopRecognition(
                                        /* motionEvent= */ null,
                                        /* editableBounds= */ null,
                                        rootView);
                                mHideDwToolbarCallbackToken = null;
                            },
                            mHideDwToolbarCallbackToken,
                            mConfig.getHideDwToolbarDelayMs());
                    break;
                }
            case MotionEvent.ACTION_HOVER_ENTER:
                {
                    if (mHideDwToolbarCallbackToken != null) {
                        mHandler.removeCallbacksAndMessages(mHideDwToolbarCallbackToken);
                        mHideDwToolbarCallbackToken = null;
                    }
                    break;
                }
            default:
                break;
        }
        return false;
    }

    /**
     * Dispatch event to Recognition View of Service after stylus writing is detected in edit rect.
     * Action Down is dispatched in startRecognition().
     */
    private void onDispatchEvent(MotionEvent me, View rootView) {
        mBinder.onDispatchEvent(me, rootView);
    }

    private boolean isDirectWritingServiceEnabled(Context context) {
        return DirectWritingSettingsHelper.isEnabled(context);
    }

    @Override
    public void updateInputState(@Nullable String text, int selectionStart, int selectionEnd) {
        if (!mDwServiceEnabled || !mBinder.isServiceConnected()) return;
        assumeNonNull(mCallback);
        mCallback.updateInputState(text, selectionStart, selectionEnd);
    }

    @Override
    public void onEditElementFocusedForStylusWriting(
            Rect focusedEditBounds,
            Point cursorPosition,
            float scaleFactor,
            int contentOffsetY,
            View view) {
        // Don't start recognition if focused edit bounds are empty as it means stylus writable
        // element was not focused or bounds could not be obtained.
        if (focusedEditBounds.isEmpty()) return;

        if (!mStylusWritingDetected || mStylusWritingImeCallback == null) return;

        focusedEditBounds.offset(0, contentOffsetY);
        StylusApiOption.recordStylusHandwritingTriggered(Api.DIRECT_WRITING);
        // Start recognition as stylus writable element is focused.
        startRecognition(focusedEditBounds);
        assumeNonNull(mCallback);
        mCallback.updateEditableBounds(focusedEditBounds, cursorPosition);
        mBinder.updateEditableBounds(focusedEditBounds, view, false);
    }

    @Override
    public void updateEditorInfo(EditorInfo editorInfo) {
        if (!mDwServiceEnabled || !mBinder.isServiceConnected()) return;
        assumeNonNull(mCallback);
        mCallback.updateEditorInfo(editorInfo);
        mBinder.updateEditorInfo(editorInfo);
    }

    @Override
    public int getStylusPointerIcon() {
        return DirectWritingConstants.STYLUS_WRITING_ICON_VALUE;
    }

    private void onStopRecognition(
            @Nullable MotionEvent motionEvent, @Nullable Rect editableBounds) {
        if (mStylusWritingImeCallback == null) return;
        onStopRecognition(
                motionEvent, editableBounds, mStylusWritingImeCallback.getContainerView());
    }

    private void onStopRecognition(
            @Nullable MotionEvent motionEvent, @Nullable Rect editableBounds, View currentView) {
        if (!mDwServiceEnabled) return;
        mBinder.onStopRecognition(motionEvent, editableBounds, currentView);
        resetRecognition();
    }

    private void resetRecognition() {
        mRecognitionStarted = false;
        mCurrentStylusDownEvent = null;
        mStylusUpEvent = null;
    }

    private void hideDwToolbar() {
        if (!mDwServiceEnabled) return;
        mBinder.hideDwToolbar();
    }
}
