// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.stylus_handwriting;

import static android.content.Context.BIND_AUTO_CREATE;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.graphics.Rect;
import android.os.Bundle;
import android.os.IBinder;
import android.view.MotionEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;

import org.chromium.base.Log;

/**
 * Direct writing Service connection handler class. Takes care of calling DW Service APIs for
 * getting DW functionality.
 */
class DirectWritingServiceBinder {
    private static final String TAG = "DWServiceBinder";
    private boolean mCallbackRegistered;
    private String mPackageName;

    private final ServiceConnection mConnection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            Log.d(TAG, "onServiceConnected for " + mPackageName + ", ComponentName=" + name);
            registerCallback();
            updateConfiguration();
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            Log.d(TAG, "onServiceDisconnected for " + mPackageName + ", ComponentName=" + name);
        }
    };

    private DirectWritingTriggerCallback mTriggerCallback;

    /**
     * Callback interface for DirectWritingTrigger class.
     */
    public interface DirectWritingTriggerCallback {
        /**
         * notify to update DW configuration.
         *
         * @param bundle the Bundle that contains configuration params from service.
         */
        void updateConfiguration(Bundle bundle);

        /**
         * @return the object that implements DW service callback interface.
         */
        DirectWritingServiceCallback getServiceCallback();
    }

    void bindService(Context context, DirectWritingTriggerCallback triggerCallback) {
        if (isServiceConnected()) return;
        requestBindService(context, triggerCallback);
    }

    private void requestBindService(Context context, DirectWritingTriggerCallback triggerCallback) {
        if (context.getPackageName().equals(mPackageName)) {
            Log.d(TAG, "bindService already requested");
            return;
        }
        try {
            Intent intent = new Intent();
            // TODO(mahesh.ma): Check the signature of Direct writing service so that a non-Samsung
            // device cannot trick us into connecting to it.
            intent.setComponent(new ComponentName(DirectWritingConstants.SERVICE_PKG_NAME,
                    DirectWritingConstants.SERVICE_CLS_NAME));
            context.bindService(intent, mConnection, BIND_AUTO_CREATE);

            mPackageName = context.getPackageName();
            mTriggerCallback = triggerCallback;
            Log.d(TAG, "bindService success");
        } catch (RuntimeException e) {
            Log.e(TAG, "bindService failed," + e);
        }
    }

    private void handleWindowFocusLost(Context context) {
        if (!context.getPackageName().equals(mPackageName)) {
            return;
        }
        onWindowFocusLost(context.getPackageName());
    }

    void unbindService(Context context) {
        if (!isServiceConnected()) return;
        unregisterCallback();
        try {
            context.unbindService(mConnection);
            Log.d(TAG, "unbindService success");
        } catch (IllegalArgumentException e) {
            Log.e(TAG, "unbindService failed : " + e);
        }
        resetDwServiceConnection();
    }

    private void registerCallback() {
        if (mCallbackRegistered) return;
        assert mTriggerCallback != null;
        DirectWritingServiceCallback serviceCallback = mTriggerCallback.getServiceCallback();
    }

    private void unregisterCallback() {
        if (!mCallbackRegistered) return;
        assert mTriggerCallback != null;
        DirectWritingServiceCallback serviceCallback = mTriggerCallback.getServiceCallback();
    }

    private void resetDwServiceConnection() {
        mCallbackRegistered = false;
        mPackageName = "";
    }

    void onWindowFocusChanged(Context context, boolean hasWindowFocus) {
        if (hasWindowFocus && isServiceConnected()) {
            registerCallback();
        } else {
            handleWindowFocusLost(context);
            unregisterCallback();
        }
    }

    // TODO(mahesh.ma): Add implementations to below stub APIs when the direct writing service aidl
    // interface is added.
    void updateEditorInfo(EditorInfo editorInfo) {}

    void updateEditableBounds(Rect editableBounds, View rootView) {}

    boolean startRecognition(Rect editableBound, MotionEvent me, View rootView) {
        return false;
    }

    boolean isServiceConnected() {
        return false;
    }

    void onDispatchEvent(MotionEvent me, View rootView) {}

    void onStopRecognition(MotionEvent me, Rect editableBounds, View rootView) {}

    private void onWindowFocusLost(String packageName) {}

    private void updateConfiguration() {}

    void hideDWToolbar() {}
}
