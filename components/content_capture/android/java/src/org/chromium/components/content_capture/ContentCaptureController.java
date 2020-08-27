// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_capture;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * The abstract class to provide the allowlist and the runtime control of if ContentCapture should
 * start.
 */
@JNINamespace("content_capture")
public abstract class ContentCaptureController {
    /**
     * The singleton instance of ContentCaptureController, shall be set by subclass.
     */
    protected static ContentCaptureController sContentCaptureController;

    private long mNativeContentCaptureController;

    public static ContentCaptureController getInstance() {
        return sContentCaptureController;
    }

    protected ContentCaptureController() {
        mNativeContentCaptureController = ContentCaptureControllerJni.get().init(this);
    }

    /**
     * @return if ContentCapture should be started for this app at all.
     */
    public abstract boolean shouldStartCapture();

    /**
     * Clear all ContentCapture data associated with Chrome.
     */
    public void clearAllContentCaptureData() {}

    /**
     * Clear ContentCapture data for specific URLs.
     */
    public void clearContentCaptureDataForURLs(String[] urlsToDelete) {}

    /**
     * @param urls the urls need to check.
     * @return if the content of all urls should be captured.
     */
    public boolean shouldCapture(String[] urls) {
        return ContentCaptureControllerJni.get().shouldCapture(
                mNativeContentCaptureController, ContentCaptureController.this, urls);
    }

    /**
     * Invoked by native side to pull the allowlist, the subclass should implement this and set
     * the allowlist by call setAllowlist.
     */
    @CalledByNative
    protected abstract void pullAllowlist();

    /**
     * Invoked by subclass to set the allowlist to native side. No allowlist (allowlist == null)
     * indicates everything is allowed, empty allowlist (allowlist.length == 0) indicates
     * nothing is allowed.
     *
     * @param allowlist the array of allowlist, it could be the hostname or the regex.
     * @param isRegex to indicate that the corresponding allowlist is the regex or not.
     */
    protected void setAllowlist(String[] allowlist, boolean[] isRegex) {
        ContentCaptureControllerJni.get().setAllowlist(
                mNativeContentCaptureController, ContentCaptureController.this, allowlist, isRegex);
    }

    @NativeMethods
    interface Natives {
        long init(Object contentCaptureController);
        void setAllowlist(long nativeContentCaptureController, ContentCaptureController caller,
                String[] allowlist, boolean[] isRegex);
        boolean shouldCapture(long nativeContentCaptureController, ContentCaptureController caller,
                String[] urls);
    }
}
