// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.guided_browsing.qr_code.image_picker;

import android.app.Dialog;
import android.app.DialogFragment;
import android.content.Context;
import android.content.DialogInterface;
import android.content.pm.ActivityInfo;
import android.os.Bundle;

import androidx.appcompat.app.AlertDialog;

import org.chromium.components.autofill_assistant.guided_browsing.R;
import org.chromium.components.autofill_assistant.guided_browsing.qr_code.AssistantQrCodeDelegate;
import org.chromium.ui.base.WindowAndroid;

/**
 * Main Dialog Fragment to trigger QR Code Scanning via Image Picker.
 */
public class AssistantQrCodeImagePickerDialog extends DialogFragment {
    private Context mContext;
    private WindowAndroid mWindowAndroid;
    private AssistantQrCodeImagePickerModel mImagePickerModel;
    private AssistantQrCodeImagePickerCoordinator mImagePickerCoordinator;

    /**
     * Create a new instance of {@link AssistantQrCodeImagePickerDialog}.
     */
    public static AssistantQrCodeImagePickerDialog newInstance(Context context,
            WindowAndroid windowAndroid, AssistantQrCodeImagePickerModel imagePickerModel) {
        AssistantQrCodeImagePickerDialog assistantQrCodeImagePickerDialog =
                new AssistantQrCodeImagePickerDialog();
        assistantQrCodeImagePickerDialog.setContext(context);
        assistantQrCodeImagePickerDialog.setWindowAndroid(windowAndroid);
        assistantQrCodeImagePickerDialog.setImagePickerModel(imagePickerModel);
        return assistantQrCodeImagePickerDialog;
    }

    public void setContext(Context context) {
        mContext = context;
    }

    public void setWindowAndroid(WindowAndroid windowAndroid) {
        mWindowAndroid = windowAndroid;
    }

    public void setImagePickerModel(AssistantQrCodeImagePickerModel imagePickerModel) {
        mImagePickerModel = imagePickerModel;
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        mImagePickerCoordinator = new AssistantQrCodeImagePickerCoordinator(
                mContext, mWindowAndroid, mImagePickerModel, this::dismiss);
        AlertDialog.Builder builder =
                new AlertDialog.Builder(getActivity(), R.style.ThemeOverlay_BrowserUI_Fullscreen);
        builder.setView(mImagePickerCoordinator.getView());
        return builder.create();
    }

    @Override
    @SuppressWarnings("SourceLockedOrientationActivity")
    public void onResume() {
        super.onResume();
        // Only portrait mode is supported for the dialog fragment.
        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        mImagePickerCoordinator.resume();
    }

    @Override
    public void onPause() {
        super.onPause();
        mImagePickerCoordinator.pause();
    }

    /**
     * Cancel QR Code Scanning via Image Picker and forward the event via delegate.
     */
    @Override
    public void onCancel(DialogInterface dialog) {
        super.onCancel(dialog);
        AssistantQrCodeDelegate delegate =
                mImagePickerModel.get(AssistantQrCodeImagePickerModel.DELEGATE);
        if (delegate != null) {
            delegate.onScanCancelled();
        }
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mImagePickerCoordinator.destroy();
        // Once the dialog fragment is destroyed, we should listen to screen orientation changes.
        getActivity().setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR);
    }
}
