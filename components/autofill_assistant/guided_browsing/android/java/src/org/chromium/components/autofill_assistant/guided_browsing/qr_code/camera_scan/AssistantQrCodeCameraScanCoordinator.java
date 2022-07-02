// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.guided_browsing.qr_code.camera_scan;

import android.content.Context;
import android.view.View;

import org.chromium.components.autofill_assistant.guided_browsing.qr_code.AssistantQrCodeDelegate;
import org.chromium.components.autofill_assistant.guided_browsing.qr_code.utils.AssistantQrCodePermissionUtils;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Creates and represents the QR Code camera scan UI.
 */
public class AssistantQrCodeCameraScanCoordinator {
    /** Callbacks to parent dialog. */
    public interface DialogCallbacks {
        /** Called when component UI is to be dismissed. */
        void dismiss();
    }

    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final AssistantQrCodeCameraScanModel mCameraScanModel;
    private final AssistantQrCodeCameraScanView mCameraScanView;
    private AssistantQrCodeCameraScanBinder.ViewHolder mViewHolder;

    /**
     * The AssistantQrCodeCameraScanCoordinator constructor.
     */
    public AssistantQrCodeCameraScanCoordinator(Context context, WindowAndroid windowAndroid,
            AssistantQrCodeCameraScanModel cameraScanModel,
            AssistantQrCodeCameraScanCoordinator.DialogCallbacks dialogCallbacks) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mCameraScanModel = cameraScanModel;

        AssistantQrCodeCameraCallbacks cameraCallbacks =
                new AssistantQrCodeCameraCallbacks(context, cameraScanModel, dialogCallbacks);
        mCameraScanView = new AssistantQrCodeCameraScanView(context,
                cameraCallbacks::onPreviewFrame, cameraCallbacks::onError,
                new AssistantQrCodeCameraScanView.Delegate() {
                    @Override
                    public void onScanCancelled() {
                        AssistantQrCodeDelegate delegate =
                                cameraScanModel.get(AssistantQrCodeCameraScanModel.DELEGATE);
                        if (delegate != null) {
                            delegate.onScanCancelled();
                        }
                        dialogCallbacks.dismiss();
                    }

                    @Override
                    public void promptForCameraPermission() {
                        AssistantQrCodePermissionUtils.promptForCameraPermission(
                                windowAndroid, cameraScanModel);
                    }
                });

        mViewHolder = new AssistantQrCodeCameraScanBinder.ViewHolder(mCameraScanView);
        PropertyModelChangeProcessor.create(
                cameraScanModel, mViewHolder, new AssistantQrCodeCameraScanBinder());

        updatePermissionSettings();
    }

    public View getView() {
        return mCameraScanView.getRootView();
    }

    public void resume() {
        updatePermissionSettings();
        mCameraScanModel.set(AssistantQrCodeCameraScanModel.IS_ON_FOREGROUND, true);
    }

    public void pause() {
        mCameraScanModel.set(AssistantQrCodeCameraScanModel.IS_ON_FOREGROUND, false);
    }

    public void destroy() {
        mCameraScanView.stopCamera();
        // Explicitly clean up view holder.
        mViewHolder = null;
    }

    /** Updates the permission settings with the latest values. */
    private void updatePermissionSettings() {
        mCameraScanModel.set(AssistantQrCodeCameraScanModel.HAS_CAMERA_PERMISSION,
                AssistantQrCodePermissionUtils.hasCameraPermission(mContext));
        mCameraScanModel.set(AssistantQrCodeCameraScanModel.CAN_PROMPT_FOR_CAMERA_PERMISSION,
                AssistantQrCodePermissionUtils.canPromptForCameraPermission(mWindowAndroid));
    }
}