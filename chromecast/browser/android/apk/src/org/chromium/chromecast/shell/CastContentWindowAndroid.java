// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chromecast.shell;

import android.content.Context;
import android.hardware.display.DisplayManager;
import android.view.Display;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
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

    private boolean mScreenAccess;
    private CastWebContentsComponent.StartParams mStartParams;

    private static int sInstanceId = 1;

    @SuppressWarnings("unused")
    @CalledByNative
    private static CastContentWindowAndroid create(long nativeCastContentWindowAndroid,
            boolean enableTouchInput, boolean turnOnScreen, boolean keepScreenOn, String sessionId,
            int displayId) {
        return new CastContentWindowAndroid(nativeCastContentWindowAndroid,
                getContextWithDisplay(displayId), enableTouchInput, turnOnScreen, keepScreenOn,
                sessionId);
    }

    private static Context getContextWithDisplay(int displayId) {
        Context context = ContextUtils.getApplicationContext();
        DisplayManager displayManager = context.getSystemService(DisplayManager.class);
        Display display = displayManager.getDisplay(displayId);
        if (display != null) {
            return context.createDisplayContext(display);
        }
        Log.i(TAG,
                "Display with the given cast display id is not available, "
                        + "use the default display to create the web view.");
        return context;
    }

    private CastContentWindowAndroid(long nativeCastContentWindowAndroid, final Context context,
            boolean enableTouchInput, boolean turnOnScreen, boolean keepScreenOn,
            String sessionId) {
        mNativeCastContentWindowAndroid = nativeCastContentWindowAndroid;
        mContext = context;
        Log.i(TAG,
                "Creating new CastContentWindowAndroid(No. " + sInstanceId++
                        + ") Seesion ID: " + sessionId);
        mComponent = new CastWebContentsComponent(
                sessionId, this, this, enableTouchInput, turnOnScreen, keepScreenOn);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void createWindowForWebContents(
            WebContents webContents, String appId, boolean shouldRequestAudioFocus) {
        if (DEBUG) Log.d(TAG, "createWindowForWebContents");
        mStartParams = new CastWebContentsComponent.StartParams(
                mContext, webContents, appId, shouldRequestAudioFocus);
        maybeStartComponent();
    }

    private void maybeStartComponent() {
        if (mStartParams == null || !mScreenAccess) return;

        Log.d(TAG, "mComponent.start()");
        mComponent.start(mStartParams, !mScreenAccess /* isHeadless */);
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void grantScreenAccess() {
        if (DEBUG) Log.d(TAG, "grantScreenAccess");
        mScreenAccess = true;
        maybeStartComponent();
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void revokeScreenAccess() {
        if (DEBUG) Log.d(TAG, "revokeScreenAccess");
        mComponent.stop(mContext);
        mScreenAccess = false;
    }

    @SuppressWarnings("unused")
    @CalledByNative
    private void enableTouchInput(boolean enabled) {
        if (DEBUG) Log.d(TAG, "enableTouchInput");
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
