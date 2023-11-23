// Copyright 2022 The Chromium Authors
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
import android.os.DeadObjectException;
import android.os.IBinder;
import android.view.MotionEvent;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.widget.directwriting.IDirectWritingService;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.PackageUtils;

import java.util.List;

/**
 * Direct writing Service connection handler class. Takes care of calling DW Service APIs for
 * getting DW functionality.
 */
class DirectWritingServiceBinder {
    private static final String TAG = "DWServiceBinder";
    private IDirectWritingService mRemoteDwService;
    private String mPackageName;

    private final ServiceConnection mConnection =
            new ServiceConnection() {
                @Override
                public void onServiceConnected(ComponentName name, IBinder service) {
                    Log.d(
                            TAG,
                            "onServiceConnected for " + mPackageName + ", ComponentName=" + name);
                    mRemoteDwService = IDirectWritingService.Stub.asInterface(service);
                    registerCallback();
                    updateConfiguration();
                }

                @Override
                public void onServiceDisconnected(ComponentName name) {
                    Log.d(
                            TAG,
                            "onServiceDisconnected for "
                                    + mPackageName
                                    + ", ComponentName="
                                    + name);
                    // When service is disconnected for any reason, it is needed to unbind the
                    // service so that we can reconnect and start writing again. This also ensures
                    // service callback is registered again which would have been reset at service
                    // when this happened.
                    unbindService(mContext);
                }
            };

    private DirectWritingTriggerCallback mTriggerCallback;
    private Context mContext;

    /** Callback interface for DirectWritingTrigger class. */
    public interface DirectWritingTriggerCallback {
        /**
         * notify to update DW configuration.
         *
         * @param bundle the Bundle that contains configuration params from service.
         */
        void updateConfiguration(Bundle bundle);

        /** @return the object that implements DW service callback interface. */
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

        // Verify that connecting service package fingerprint matches with expected fingerprint of
        // Direct Writing service package. This is to prevent any attacker from spoofing the package
        // name and tricking Chrome into connecting to it.
        List<String> fingerprints =
                PackageUtils.getCertificateSHA256FingerprintForPackage(
                        DirectWritingConstants.SERVICE_PKG_NAME);
        if (fingerprints == null
                || fingerprints.size() > 1
                || !(fingerprints
                                .get(0)
                                .equals(
                                        DirectWritingConstants
                                                .SERVICE_PKG_SHA_256_FINGERPRINT_RELEASE)
                        || fingerprints
                                .get(0)
                                .equals(
                                        DirectWritingConstants
                                                .SERVICE_PKG_SHA_256_FINGERPRINT_DEBUG))) {
            Log.e(TAG, "Don't connect to service due to package fingerprint mismatch");
            return;
        }
        try {
            Intent intent = new Intent();
            intent.setComponent(
                    new ComponentName(
                            DirectWritingConstants.SERVICE_PKG_NAME,
                            DirectWritingConstants.SERVICE_CLS_NAME));
            context.bindService(intent, mConnection, BIND_AUTO_CREATE);

            mPackageName = context.getPackageName();
            mTriggerCallback = triggerCallback;
            mContext = context;
            Log.d(TAG, "bindService success");
        } catch (RuntimeException e) {
            Log.e(TAG, "bindService failed," + e);
        }
    }

    void setRemoteServiceForTest(IDirectWritingService remoteService) {
        mRemoteDwService = remoteService;
    }

    void setTriggerCallbackForTest(DirectWritingTriggerCallback callback) {
        mTriggerCallback = callback;
    }

    @VisibleForTesting
    void handleWindowFocusLost(Context context) {
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

    @VisibleForTesting
    void registerCallback() {
        // It would be nice to extract the pattern of "do something with a service, surround it in
        // a try catch" into a method, unfortunately that would increase the binary size too much,
        // see:
        // https://ci.chromium.org/ui/p/chromium/builders/try/android-binary-size/1175796/overview
        if (!isServiceConnected()) return;
        assert mTriggerCallback != null;
        DirectWritingServiceCallback serviceCallback = mTriggerCallback.getServiceCallback();
        try {
            String callbackPackage =
                    (mPackageName + IDirectWritingService.VALUE_SERVICE_HOST_SOURCE_WEBVIEW);
            mRemoteDwService.registerCallback(serviceCallback, callbackPackage);
            Log.d(TAG, "Service callback registered");
        } catch (DeadObjectException e) {
            Log.e(TAG, "registerCallback failed due to DeadObjectException.", e);
            resetDwServiceConnection();
        } catch (Exception e) {
            Log.e(TAG, "registerCallback failed.", e);
        }
    }

    @VisibleForTesting
    void unregisterCallback() {
        if (!isServiceConnected()) return;
        assert mTriggerCallback != null;
        DirectWritingServiceCallback serviceCallback = mTriggerCallback.getServiceCallback();
        try {
            mRemoteDwService.unregisterCallback(serviceCallback);
            Log.d(TAG, "Service callback unregistered");
        } catch (DeadObjectException e) {
            Log.e(TAG, "unregisterCallback failed due to DeadObjectException.", e);
            resetDwServiceConnection();
        } catch (Exception e) {
            Log.e(TAG, "unregisterCallback failed.", e);
        }
    }

    private void resetDwServiceConnection() {
        mRemoteDwService = null;
        mPackageName = "";
    }

    void onWindowFocusChanged(Context context, boolean hasWindowFocus) {
        if (hasWindowFocus) {
            // Need to register DW service callback object again when window gets focus, so that
            // commit happens in the intended Chrome instance and web input. This is required even
            // though we haven't unregistered the callback. This is a limitation in DW service side.
            // It is also intentional to not unregister this callback when window loses focus, as it
            // affects the DW done in Chrome's omnibox (url bar).
            registerCallback();
        } else {
            handleWindowFocusLost(context);
        }
    }

    private void updateConfiguration() {
        if (!isServiceConnected()) return;
        try {
            Bundle bundle = new Bundle();
            mRemoteDwService.getConfiguration(bundle);
            assert mTriggerCallback != null;
            mTriggerCallback.updateConfiguration(bundle);
        } catch (DeadObjectException e) {
            Log.e(TAG, "updateConfiguration failed due to DeadObjectException.", e);
            resetDwServiceConnection();
        } catch (Exception e) {
            Log.e(TAG, "updateConfiguration failed.", e);
        }
    }

    boolean isServiceConnected() {
        return mRemoteDwService != null;
    }

    boolean startRecognition(Rect editableBound, MotionEvent me, View rootView) {
        if (!isServiceConnected()) {
            Log.e(TAG, "startRecognition failed, not bounded");
            return false;
        }
        try {
            mRemoteDwService.onStartRecognition(
                    DirectWritingBundleUtil.buildBundle(me, editableBound, rootView));
            return true;
        } catch (DeadObjectException e) {
            Log.e(TAG, "startRecognition failed due to DeadObjectException.", e);
            resetDwServiceConnection();
            return false;
        } catch (Exception e) {
            Log.e(TAG, "startRecognition failed with exception.", e);
            return false;
        }
    }

    void onStopRecognition(MotionEvent me, Rect editableBounds, View rootView) {
        if (!isServiceConnected()) return;
        try {
            Bundle bundle = DirectWritingBundleUtil.buildBundle(me, editableBounds, rootView);
            mRemoteDwService.onStopRecognition(bundle);
        } catch (DeadObjectException e) {
            Log.e(TAG, "onStopRecognition failed due to DeadObjectException.", e);
            resetDwServiceConnection();
        } catch (Exception e) {
            Log.e(TAG, "onStopRecognition failed.", e);
        }
    }

    void updateEditorInfo(EditorInfo editorInfo) {
        if (!isServiceConnected()) return;
        try {
            mRemoteDwService.onUpdateImeOptions(editorInfo.imeOptions);
        } catch (DeadObjectException e) {
            Log.e(TAG, "updateEditorInfo failed due to DeadObjectException.", e);
            resetDwServiceConnection();
        } catch (Exception e) {
            Log.e(TAG, "updateEditorInfo failed.", e);
        }
    }

    void updateEditableBounds(Rect editableBounds, View rootView, boolean isOnlyRectChanged) {
        if (!isServiceConnected()) return;
        try {
            mRemoteDwService.onBoundedEditTextChanged(
                    DirectWritingBundleUtil.buildBundle(
                            editableBounds, rootView, isOnlyRectChanged));
        } catch (DeadObjectException e) {
            Log.e(TAG, "updateEditableBounds failed due to DeadObjectException.", e);
            resetDwServiceConnection();
        } catch (Exception e) {
            Log.e(TAG, "updateEditableBounds failed.", e);
        }
    }

    void onDispatchEvent(MotionEvent me, View rootView) {
        if (!isServiceConnected()) return;
        try {
            mRemoteDwService.onDispatchEvent(DirectWritingBundleUtil.buildBundle(me, rootView));
        } catch (DeadObjectException e) {
            Log.e(TAG, "onDispatchEvent failed due to DeadObjectException.", e);
            resetDwServiceConnection();
        } catch (Exception e) {
            Log.e(TAG, "onDispatchEvent failed.", e);
        }
    }

    private void onWindowFocusLost(String packageName) {
        if (!isServiceConnected()) return;
        try {
            mRemoteDwService.onWindowFocusLost(packageName);
        } catch (DeadObjectException e) {
            Log.e(TAG, "onWindowFocusLost failed due to DeadObjectException.", e);
            resetDwServiceConnection();
        } catch (Exception e) {
            Log.e(TAG, "onWindowFocusLost failed.", e);
        }
    }

    void hideDWToolbar() {
        if (!isServiceConnected()) return;
        try {
            Bundle bundle = DirectWritingBundleUtil.buildBundle();
            mRemoteDwService.onEditTextActionModeStarted(bundle);
        } catch (DeadObjectException e) {
            Log.e(TAG, "hideDWToolbar failed due to DeadObjectException.", e);
            resetDwServiceConnection();
        } catch (Exception e) {
            Log.e(TAG, "hideDWToolbar failed.", e);
        }
    }
}
