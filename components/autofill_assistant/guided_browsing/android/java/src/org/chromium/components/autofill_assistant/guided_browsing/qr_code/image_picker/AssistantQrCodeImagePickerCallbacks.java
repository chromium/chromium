// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.guided_browsing.qr_code.image_picker;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.net.Uri;
import android.provider.MediaStore;
import android.util.SparseArray;

import com.google.android.gms.vision.Frame;
import com.google.android.gms.vision.barcode.Barcode;
import com.google.android.gms.vision.barcode.BarcodeDetector;

import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.components.autofill_assistant.guided_browsing.qr_code.AssistantQrCodeDelegate;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.base.WindowAndroid.IntentCallback;

import java.io.IOException;

/**
 * AssistantQrCodeImagePickerCallbacks provides the callbacks needed for QR scanning via image
 * picker.
 */
public class AssistantQrCodeImagePickerCallbacks implements IntentCallback {
    private final Context mContext;
    private final AssistantQrCodeImagePickerModel mImagePickerModel;
    private final AssistantQrCodeImagePickerCoordinator.DialogCallbacks mDialogCallbacks;

    private BarcodeDetector mDetector;

    /**
     * The AssistantQrCodeImagePickerCallbacks constructor.
     */
    AssistantQrCodeImagePickerCallbacks(Context context,
            AssistantQrCodeImagePickerModel imagePickerModel,
            AssistantQrCodeImagePickerCoordinator.DialogCallbacks dialogCallbacks) {
        mContext = context;
        mImagePickerModel = imagePickerModel;
        mDialogCallbacks = dialogCallbacks;

        // Set detector to null until it gets initialized asynchronously.
        mDetector = null;
        initBarcodeDetectorAsync();
    }

    /**
     * Callback when the image picker intent finishes. Inspects the image for any QR Code. In case
     * of successful QR Code detection, sends the output value using the |AssistantQrCodeDelegate|
     * and dismisses the QR Code Image Picker dialog UI.
     */
    @Override
    public void onIntentCompleted(int resultCode, Intent data) {
        // When the user presses back button, the resultCode will not be RESULT_OK.
        if (resultCode != Activity.RESULT_OK) {
            onQrCodeScanCancel();
            return;
        }
        if (data == null || mDetector == null) {
            onQrCodeScanFailure();
            return;
        }

        Uri imageUri = data.getData();
        try {
            Bitmap bitmap =
                    MediaStore.Images.Media.getBitmap(mContext.getContentResolver(), imageUri);
            Frame frame = new Frame.Builder().setBitmap(bitmap).build();
            SparseArray<Barcode> barcodes = mDetector.detect(frame);
            if (barcodes.size() == 0 || barcodes.valueAt(0).rawValue.isEmpty()) {
                onQrCodeScanFailure();
                return;
            }
            Barcode firstCode = barcodes.valueAt(0);
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
                @Override
                public void run() {
                    AssistantQrCodeDelegate delegate =
                            mImagePickerModel.get(AssistantQrCodeImagePickerModel.DELEGATE);
                    if (delegate != null) {
                        delegate.onScanResult(firstCode.rawValue);
                    }
                }
            });
            // Dismiss the QR Code scan UI dialog.
            mDialogCallbacks.dismiss();
        } catch (IOException e) {
            onQrCodeScanFailure();
            return;
        }
    }

    /**
     * Sends back the CANCEL response using the |AssistantQrCodeDelegate| and dismisses the QR Code
     * Image Picker dialog UI.
     */
    private void onQrCodeScanCancel() {
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                AssistantQrCodeDelegate delegate =
                        mImagePickerModel.get(AssistantQrCodeImagePickerModel.DELEGATE);
                if (delegate != null) {
                    delegate.onScanCancelled();
                }
            }
        });
        // Dismiss the QR Code scan UI dialog.
        mDialogCallbacks.dismiss();
    }

    /**
     * Sends back the FAILURE response using the |AssistantQrCodeDelegate| and dismisses the QR
     * Code Image Picker dialog UI.
     */
    private void onQrCodeScanFailure() {
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                AssistantQrCodeDelegate delegate =
                        mImagePickerModel.get(AssistantQrCodeImagePickerModel.DELEGATE);
                if (delegate != null) {
                    delegate.onScanFailure();
                }
            }
        });
        // Dismiss the QR Code scan UI dialog.
        mDialogCallbacks.dismiss();
    }

    private void initBarcodeDetectorAsync() {
        new AsyncTask<BarcodeDetector>() {
            @Override
            protected BarcodeDetector doInBackground() {
                return new BarcodeDetector.Builder(mContext).build();
            }

            @Override
            protected void onPostExecute(BarcodeDetector detector) {
                mDetector = detector;
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }
}
