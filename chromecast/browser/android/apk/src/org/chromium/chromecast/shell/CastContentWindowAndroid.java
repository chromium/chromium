// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.content_public.browser.WebContents;

/**
 * The Java component of CastContentWindowAndroid. This class is responsible for starting, stopping
 * and monitoring CastWebContentsActivity.
 *
 * <p>See chromecast/browser/cast_content_window_android.* for the native half.
 */
@JNINamespace("chromecast")
public class CastContentWindowAndroid
        implements CastWebContentsComponent.OnComponentClosedHandler,
                CastWebContentsComponent.SurfaceEventHandler {
    private static final String TAG = "CastContentWindow";

    // Note: CastContentWindowAndroid may outlive the native object. The native
    // ref should be checked that it is not zero before it is used.
    private long mNativeCastContentWindowAndroid;
    private final String mSessionId;
    private final CastWebContentsComponent mComponent;

    private boolean mScreenAccess;
    private CastWebContentsComponent.StartParams mStartParams;

    private static int sInstanceId = 1;

    @SuppressWarnings("unused")
    @CalledByNative
    private static CastContentWindowAndroid create(
            long nativeCastContentWindowAndroid,
            boolean enableTouchInput,
            boolean turnOnScreen,
            boolean keepScreenOn,
            String sessionId) {
        return new CastContentWindowAndroid(
                nativeCastContentWindowAndroid,
                enableTouchInput,
                turnOnScreen,
                keepScreenOn,
                sessionId);
    }

    private CastContentWindowAndroid(
            long nativeCastContentWindowAndroid,
            boolean enableTouchInput,
            boolean turnOnScreen,
            boolean keepScreenOn,
            String sessionId) {
        mNativeCastContentWindowAndroid = nativeCastContentWindowAndroid;
        mSessionId = sessionId;
        Log.i(
                TAG,
                "Cast content window created: instanceId=%d, sessionId=%s",
                sInstanceId++,
                mSessionId);
        mComponent =
                new CastWebContentsComponent(
                        sessionId, this, this, enableTouchInput, turnOnScreen, keepScreenOn);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void createWindowForWebContents(
            WebContents webContents, String appId, boolean shouldRequestAudioFocus) {
        Log.d(
                TAG,
                "Creating window for WebContents: sessionId=%s, appId=%s, audioFocus=%b",
                mSessionId,
                appId,
                shouldRequestAudioFocus);
        mStartParams =
                new CastWebContentsComponent.StartParams(
                        webContents, appId, shouldRequestAudioFocus);
        maybeStartComponent();
    }

    private void maybeStartComponent() {
        if (mStartParams == null || !mScreenAccess) return;
        mComponent.start(mStartParams);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void grantScreenAccess() {
        Log.d(TAG, "Granting screen access: sessionId=" + mSessionId);
        mScreenAccess = true;
        maybeStartComponent();
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void revokeScreenAccess() {
        Log.d(TAG, "Revoking screen access: sessionId=" + mSessionId);
        mComponent.stop();
        mScreenAccess = false;
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void enableTouchInput(boolean enabled) {
        mComponent.enableTouchInput(enabled);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void setAllowPictureInPicture(boolean allowPictureInPicture) {
        mComponent.setAllowPictureInPicture(allowPictureInPicture);
    }

    @CalledByNative
    private void setMediaPlaying(boolean mediaPlaying) {
        mComponent.setMediaPlaying(mediaPlaying);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void onNativeDestroyed() {
        assert mNativeCastContentWindowAndroid != 0;
        mNativeCastContentWindowAndroid = 0;

        // Note: there is a potential race condition when this function is called after
        // createWindowForWebContents. If the stop intent is received after the start intent but
        // before onCreate, the activity won't shutdown.
        // TODO(derekjchow): Add a unittest to check this behaviour. Also consider using
        // Instrumentation.startActivitySync to guarentee onCreate is run.

        Log.d(TAG, "Native window destroyed: sessionId=" + mSessionId);
        mComponent.stop();
    }

    @Override
    public void onComponentClosed() {
        Log.d(TAG, "Component closed: sessionId=" + mSessionId);
        if (mNativeCastContentWindowAndroid != 0) {
            CastContentWindowAndroidJni.get()
                    .onActivityStopped(
                            mNativeCastContentWindowAndroid, CastContentWindowAndroid.this);
        }
    }

    @Override
    public void onVisibilityChange(int visibilityType) {
        Log.d(TAG, "Visiblity changed: sessionId=%s, visibility=%d", mSessionId, visibilityType);
        if (mNativeCastContentWindowAndroid != 0) {
            CastContentWindowAndroidJni.get()
                    .onVisibilityChange(
                            mNativeCastContentWindowAndroid,
                            CastContentWindowAndroid.this,
                            visibilityType);
        }
    }

    @NativeMethods
    interface Natives {
        void onActivityStopped(
                long nativeCastContentWindowAndroid, CastContentWindowAndroid caller);

        void onVisibilityChange(
                long nativeCastContentWindowAndroid,
                CastContentWindowAndroid caller,
                int visibilityType);
    }
}
