// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webid;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.WindowAndroid;

import java.lang.ref.WeakReference;

/**
 * Base class for digital credentials presentation and creation delegates. Manages intent
 * cancellation and window references.
 */
@NullMarked
public abstract class DigitalCredentialsDelegateBase {
    protected boolean mIsCanceled;
    // Held via WeakReference and cleared on every completion/cancellation path so an
    // async GMS Task or ResultReceiver never pins a destroyed Activity in memory.
    protected @Nullable WeakReference<WindowAndroid> mWindowAndroid;

    /**
     * Request code returned by {@link WindowAndroid#showCancelableIntent} used to cancel the
     * sub-activity via {@link android.app.Activity#finishActivity}. {@link
     * WindowAndroid#START_INTENT_FAILURE} (-1) indicates no active intent.
     */
    // Note: `showCancelableIntent` returns non-negative request codes (>= 0, starting at 0)
    // on success, so `START_INTENT_FAILURE` (-1) must be used as the inactive sentinel.
    protected int mRequestCode = WindowAndroid.START_INTENT_FAILURE;

    // Releases the WindowAndroid reference and resets the active request code sentinel.
    // Must be called on all exit paths (result delivery, intent completion, error, cancel).
    protected void resetWindowAndRequestCode() {
        mRequestCode = WindowAndroid.START_INTENT_FAILURE;
        mWindowAndroid = null;
    }

    /**
     * Cancels the in-flight intent and dismisses the platform Credential Manager UI. Prevents the
     * UI from lingering across navigation or when the request is destroyed (crbug.com/553271120).
     */
    public void cancel() {
        // Record cancellation even before `mRequestCode` is assigned (while GMS is still
        // preparing the PendingIntent) and to reject any late post-cancel ResultReceiver IPC.
        mIsCanceled = true;
        WindowAndroid windowAndroid = mWindowAndroid != null ? mWindowAndroid.get() : null;
        if (windowAndroid != null
                && !windowAndroid.isDestroyed()
                && mRequestCode != WindowAndroid.START_INTENT_FAILURE) {
            windowAndroid.cancelIntent(mRequestCode);
        }
        resetWindowAndRequestCode();
    }
}
