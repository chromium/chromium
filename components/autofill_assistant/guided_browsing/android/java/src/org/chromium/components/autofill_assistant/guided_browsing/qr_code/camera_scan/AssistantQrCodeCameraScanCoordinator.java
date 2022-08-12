// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.guided_browsing.qr_code.camera_scan;

import android.content.Context;
import android.view.View;

import org.chromium.components.autofill_assistant.guided_browsing.qr_code.AssistantQrCodeDelegate;
import org.chromium.components.autofill_assistant.guided_browsing.qr_code.permission.AssistantQrCodePermissionCallback;
import org.chromium.components.autofill_assistant.guided_browsing.qr_code.permission.AssistantQrCodePermissionCoordinator;
import org.chromium.components.autofill_assistant.guided_browsing.qr_code.permission.AssistantQrCodePermissionType;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Creates and represents the QR Code camera scan UI.
 */
public class AssistantQrCodeCameraScanCoordinator {
    /**
     * Callbacks to parent dialog.
     */
    public interface DialogCallbacks {
        /**
         * Called when component UI is to be dismissed.
         */
        void dismiss();
    }

    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final AssistantQrCodeCameraScanModel mCameraScanModel;
    private final AssistantQrCodeCameraScanView mCameraScanView;
    private AssistantQrCodeCameraScanBinder.ViewHolder mViewHolder;
    private final AssistantQrCodePermissionCoordinator mPermissionCoordinator;

    /**
     * The AssistantQrCodeCameraScanCoordinator constructor.
     */
    public AssistantQrCodeCameraScanCoordinator(Context context, WindowAndroid windowAndroid,
            AssistantQrCodeCameraScanModel cameraScanModel,
            AssistantQrCodeCameraScanCoordinator.DialogCallbacks dialogCallbacks) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mCameraScanModel = cameraScanModel;

        mPermissionCoordinator = new AssistantQrCodePermissionCoordinator(mContext, mWindowAndroid,
                mCameraScanModel.getCameraPermissionModel(), AssistantQrCodePermissionType.CAMERA,
                new AssistantQrCodePermissionCallback() {
                    @Override
                    public void onPermissionsChanged(boolean hasPermission) {
                        if (mCameraScanView != null) {
                            mCameraScanView.onPermissionsChanged(hasPermission);
                        }
                    }
                });

        AssistantQrCodeCameraCallbacks cameraCallbacks =
                new AssistantQrCodeCameraCallbacks(context, cameraScanModel, dialogCallbacks);
        mCameraScanView = new AssistantQrCodeCameraScanView(context,
                mPermissionCoordinator.getView(), cameraCallbacks::onPreviewFrame,
                cameraCallbacks::onError, new AssistantQrCodeCameraScanView.Delegate() {
                    @Override
                    public void onScanCancelled() {
                        AssistantQrCodeDelegate delegate =
                                cameraScanModel.get(AssistantQrCodeCameraScanModel.DELEGATE);
                        if (delegate != null) {
                            delegate.onScanCancelled();
                        }
                        dialogCallbacks.dismiss();
                    }
                });

        mViewHolder = new AssistantQrCodeCameraScanBinder.ViewHolder(mCameraScanView);
        PropertyModelChangeProcessor.create(
                cameraScanModel, mViewHolder, new AssistantQrCodeCameraScanBinder());
        mPermissionCoordinator.updatePermissionSettings();
    }

    public View getView() {
        return mCameraScanView.getRootView();
    }

    public void resume() {
        mPermissionCoordinator.updatePermissionSettings();
        mPermissionCoordinator.maybePromptForPermissionOnce();
        mCameraScanModel.set(AssistantQrCodeCameraScanModel.IS_ON_FOREGROUND, true);
    }

    public void pause() {
        mCameraScanModel.set(AssistantQrCodeCameraScanModel.IS_ON_FOREGROUND, false);
    }

    public void destroy() {
        mCameraScanView.stopCamera();
        // Explicitly clean up view holder.
        mViewHolder = null;
        mPermissionCoordinator.destroy();
    }
}
