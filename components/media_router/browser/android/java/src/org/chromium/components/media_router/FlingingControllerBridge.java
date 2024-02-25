// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.media_router;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

/**
 * A wrapper around a FlingingController that allows the native code to use it
 * See flinging_controller_bridge.h for the corresponding native code.
 */
@JNINamespace("media_router")
public class FlingingControllerBridge implements MediaStatusObserver {
    private final FlingingController mFlingingController;
    private long mNativeFlingingControllerBridge;

    public FlingingControllerBridge(FlingingController flingingController) {
        mFlingingController = flingingController;
    }

    @CalledByNative
    public void play() {
        mFlingingController.getMediaController().play();
    }

    @CalledByNative
    public void pause() {
        mFlingingController.getMediaController().pause();
    }

    @CalledByNative
    public void setMute(boolean mute) {
        mFlingingController.getMediaController().setMute(mute);
    }

    @CalledByNative
    public void setVolume(float volume) {
        mFlingingController.getMediaController().setVolume(volume);
    }

    @CalledByNative
    public void seek(long positionInMs) {
        mFlingingController.getMediaController().seek(positionInMs);
    }

    @CalledByNative
    public long getApproximateCurrentTime() {
        return mFlingingController.getApproximateCurrentTime();
    }

    // MediaStatusObserver implementation.
    @Override
    public void onMediaStatusUpdate(MediaStatusBridge status) {
        if (mNativeFlingingControllerBridge != 0) {
            FlingingControllerBridgeJni.get()
                    .onMediaStatusUpdated(
                            mNativeFlingingControllerBridge, FlingingControllerBridge.this, status);
        }
    }

    @CalledByNative
    public void addNativeFlingingController(long nativeFlingingControllerBridge) {
        mNativeFlingingControllerBridge = nativeFlingingControllerBridge;
        mFlingingController.setMediaStatusObserver(this);
    }

    @CalledByNative
    public void clearNativeFlingingController() {
        mFlingingController.clearMediaStatusObserver();
        mNativeFlingingControllerBridge = 0;
    }

    @NativeMethods
    interface Natives {
        void onMediaStatusUpdated(
                long nativeFlingingControllerBridge,
                FlingingControllerBridge caller,
                MediaStatusBridge status);
    }
}
