// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chromecast.base.Controller;
import org.chromium.content_public.browser.WebContents;

/**
 * The Java component of CastContentWindowAndroid. This class is responsible for
 * starting, stopping and monitoring CastWebContentsActivity.
 * <p>
 * See chromecast/browser/cast_content_window_android.* for the native half.
 */
@JNINamespace("chromecast")
public class CastContentWindowAndroid implements CastWebContentsComponent.OnComponentClosedHandler,
                                                 CastWebContentsComponent.SurfaceEventHandler {
    private static final String TAG = "CastContentWindow";
    private static final boolean DEBUG = true;

    // Note: CastContentWindowAndroid may outlive the native object. The native
    // ref should be checked that it is not zero before it is used.
    private long mNativeCastContentWindowAndroid;
    private Context mContext;
    private CastWebContentsComponent mComponent;

    private final Controller<Boolean> mScreenAccess = new Controller<>();
    private final Controller<CastWebContentsComponent.StartParams> mStartParams =
            new Controller<>();

    private static int sInstanceId = 1;

    @SuppressWarnings("unused")
    @CalledByNative
    private static CastContentWindowAndroid create(long nativeCastContentWindowAndroid,
            boolean enableTouchInput, boolean isRemoteControlMode, boolean turnOnScreen,
            boolean keepScreenOn, String sessionId) {
        return new CastContentWindowAndroid(nativeCastContentWindowAndroid,
                ContextUtils.getApplicationContext(), enableTouchInput, isRemoteControlMode,
                turnOnScreen, keepScreenOn, sessionId);
    }

    private CastContentWindowAndroid(long nativeCastContentWindowAndroid, final Context context,
            boolean enableTouchInput, boolean isRemoteControlMode, boolean turnOnScreen,
            boolean keepScreenOn, String sessionId) {
        mNativeCastContentWindowAndroid = nativeCastContentWindowAndroid;
        mContext = context;
        Log.i(TAG,
                "Creating new CastContentWindowAndroid(No. " + sInstanceId++
                        + ") Seesion ID: " + sessionId);
        mComponent = new CastWebContentsComponent(sessionId, this, this, enableTouchInput,
                isRemoteControlMode, turnOnScreen, keepScreenOn);
        mScreenAccess.subscribe(screenAccess -> mStartParams.subscribe(startParams -> {
            // If the app doesn't have screen access, start in headless mode, so that the web
            // content can still have a window attached. Since we have video overlay always
            // enabled, this can unblock video decoder.
            mComponent.start(startParams, !screenAccess);
            return () -> mComponent.stop(mContext);
        }));
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void createWindowForWebContents(WebContents webContents, String appId) {
        if (DEBUG) Log.d(TAG, "createWindowForWebContents");
        mStartParams.set(new CastWebContentsComponent.StartParams(mContext, webContents, appId));
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void grantScreenAccess() {
        if (DEBUG) Log.d(TAG, "grantScreenAccess");
        mScreenAccess.set(true);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void revokeScreenAccess() {
        if (DEBUG) Log.d(TAG, "revokeScreenAccess");
        mScreenAccess.set(false);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void enableTouchInput(boolean enabled) {
        if (DEBUG) Log.d(TAG, "enableTouchInput");
        mComponent.enableTouchInput(enabled);
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

        if (DEBUG) Log.d(TAG, "onNativeDestroyed");
        mComponent.stop(mContext);
    }

    @Override
    public void onComponentClosed() {
        if (DEBUG) Log.d(TAG, "onComponentClosed");
        if (mNativeCastContentWindowAndroid != 0) {
            CastContentWindowAndroidJni.get().onActivityStopped(
                    mNativeCastContentWindowAndroid, CastContentWindowAndroid.this);
        }
    }

    @Override
    public void onVisibilityChange(int visibilityType) {
        if (DEBUG) Log.d(TAG, "onVisibilityChange type=" + visibilityType);
        if (mNativeCastContentWindowAndroid != 0) {
            CastContentWindowAndroidJni.get().onVisibilityChange(
                    mNativeCastContentWindowAndroid, CastContentWindowAndroid.this, visibilityType);
        }
    }

    @NativeMethods
    interface Natives {
        void onActivityStopped(
                long nativeCastContentWindowAndroid, CastContentWindowAndroid caller);
        void onVisibilityChange(long nativeCastContentWindowAndroid,
                CastContentWindowAndroid caller, int visibilityType);
    }
}
