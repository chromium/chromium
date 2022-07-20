// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.guided_browsing.qr_code.image_picker;

import android.content.Context;
import android.view.View;

import org.chromium.base.BuildInfo;
import org.chromium.components.autofill_assistant.guided_browsing.qr_code.AssistantQrCodeDelegate;
import org.chromium.components.autofill_assistant.guided_browsing.qr_code.permission.AssistantQrCodePermissionCallback;
import org.chromium.components.autofill_assistant.guided_browsing.qr_code.permission.AssistantQrCodePermissionCoordinator;
import org.chromium.components.autofill_assistant.guided_browsing.qr_code.permission.AssistantQrCodePermissionType;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Creates and represents the QR Code image picker UI.
 */
public class AssistantQrCodeImagePickerCoordinator {
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
    private final AssistantQrCodeImagePickerModel mImagePickerModel;
    private final AssistantQrCodeImagePickerView mImagePickerView;
    private AssistantQrCodeImagePickerBinder.ViewHolder mViewHolder;
    private final AssistantQrCodePermissionCoordinator mPermissionCoordinator;

    /**
     * The AssistantQrCodeImagePickerCoordinator constructor.
     */
    public AssistantQrCodeImagePickerCoordinator(Context context, WindowAndroid windowAndroid,
            AssistantQrCodeImagePickerModel imagePickerModel,
            AssistantQrCodeImagePickerCoordinator.DialogCallbacks dialogCallbacks) {
        mContext = context;
        mWindowAndroid = windowAndroid;
        mImagePickerModel = imagePickerModel;

        mPermissionCoordinator = new AssistantQrCodePermissionCoordinator(mContext, mWindowAndroid,
                mImagePickerModel.getReadImagesPermissionModel(),
                getPermissionTypeForReadingImages(), new AssistantQrCodePermissionCallback() {
                    @Override
                    public void onPermissionsChanged(boolean hasPermission) {
                        if (mImagePickerView != null) {
                            mImagePickerView.onPermissionsChanged(hasPermission);
                        }
                    }
                });

        AssistantQrCodeImagePickerCallbacks imagePickerCallbacks =
                new AssistantQrCodeImagePickerCallbacks(context, imagePickerModel, dialogCallbacks);
        mImagePickerView = new AssistantQrCodeImagePickerView(context, mWindowAndroid,
                mPermissionCoordinator.getView(), imagePickerCallbacks::onIntentCompleted,
                new AssistantQrCodeImagePickerView.Delegate() {
                    @Override
                    public void onScanCancelled() {
                        AssistantQrCodeDelegate delegate =
                                imagePickerModel.get(AssistantQrCodeImagePickerModel.DELEGATE);
                        if (delegate != null) {
                            delegate.onScanCancelled();
                        }
                        dialogCallbacks.dismiss();
                    }
                });

        mViewHolder = new AssistantQrCodeImagePickerBinder.ViewHolder(mImagePickerView);
        PropertyModelChangeProcessor.create(
                imagePickerModel, mViewHolder, new AssistantQrCodeImagePickerBinder());
        mPermissionCoordinator.updatePermissionSettings();
    }

    public View getView() {
        return mImagePickerView.getRootView();
    }

    public void resume() {
        mPermissionCoordinator.updatePermissionSettings();
        mImagePickerModel.set(AssistantQrCodeImagePickerModel.IS_ON_FOREGROUND, true);
    }

    public void pause() {
        mImagePickerModel.set(AssistantQrCodeImagePickerModel.IS_ON_FOREGROUND, false);
    }

    public void destroy() {
        // Explicitly clean up view holder.
        mViewHolder = null;
        mPermissionCoordinator.destroy();
    }

    /**
     * Returns permission type for reading images based on android version.
     */
    private AssistantQrCodePermissionType getPermissionTypeForReadingImages() {
        if (BuildInfo.isAtLeastT()) {
            // |READ_MEDIA_IMAGES| is to be used from API Version |T|
            // https://developer.android.com/reference/android/Manifest.permission#READ_MEDIA_IMAGES
            return AssistantQrCodePermissionType.READ_MEDIA_IMAGES;
        }
        return AssistantQrCodePermissionType.READ_EXTERNAL_STORAGE;
    }
}
