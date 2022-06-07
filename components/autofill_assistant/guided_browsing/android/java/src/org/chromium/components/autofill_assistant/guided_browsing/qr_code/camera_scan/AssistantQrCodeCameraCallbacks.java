// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.guided_browsing.qr_code.camera_scan;

import android.content.Context;
import android.graphics.ImageFormat;
import android.hardware.Camera;
import android.util.SparseArray;

import com.google.android.gms.vision.Frame;
import com.google.android.gms.vision.barcode.Barcode;
import com.google.android.gms.vision.barcode.BarcodeDetector;

import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.components.autofill_assistant.guided_browsing.qr_code.AssistantQrCodeDelegate;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.nio.ByteBuffer;

/**
 * AssistantQrCodeCameraCallbacks provides the callbacks needed for camera preview.
 */
public class AssistantQrCodeCameraCallbacks
        implements Camera.PreviewCallback, Camera.ErrorCallback {
    private final Context mContext;
    private final AssistantQrCodeCameraScanModel mCameraScanModel;
    private final AssistantQrCodeCameraScanCoordinator.DialogCallbacks mDialogCallbacks;

    private BarcodeDetector mDetector;

    /**
     * The AssistantQrCodeCameraCallbacks constructor.
     */
    AssistantQrCodeCameraCallbacks(Context context, AssistantQrCodeCameraScanModel cameraScanModel,
            AssistantQrCodeCameraScanCoordinator.DialogCallbacks dialogCallbacks) {
        mContext = context;
        mCameraScanModel = cameraScanModel;
        mDialogCallbacks = dialogCallbacks;

        // Set detector to null until it gets initialized asynchronously.
        mDetector = null;
        initBarcodeDetectorAsync();
    }

    /**
     * Callback on successful camera preview. Inspects the frame for any QR Code. In case of no
     * QR code detection, the camera preview continues. In case of successful QR Code detection,
     * sends the output value using the |AssistantQrCodeDelegate| and dismisses the QR Code Camera
     * Scan dialog UI.
     */
    @Override
    public void onPreviewFrame(byte[] data, Camera camera) {
        if (mDetector == null) {
            return;
        }

        ByteBuffer buffer = ByteBuffer.allocate(data.length);
        buffer.put(data);
        Frame frame =
                new Frame.Builder()
                        .setImageData(buffer, camera.getParameters().getPreviewSize().width,
                                camera.getParameters().getPreviewSize().height, ImageFormat.NV21)
                        .build();
        SparseArray<Barcode> barcodes = mDetector.detect(frame);
        if (!mCameraScanModel.get(AssistantQrCodeCameraScanModel.IS_ON_FOREGROUND)) {
            return;
        }
        if (barcodes.size() == 0 || barcodes.valueAt(0).rawValue.isEmpty()) {
            camera.setOneShotPreviewCallback(this);
            return;
        }

        Barcode firstCode = barcodes.valueAt(0);
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                AssistantQrCodeDelegate delegate =
                        mCameraScanModel.get(AssistantQrCodeCameraScanModel.DELEGATE);
                if (delegate != null) {
                    delegate.onScanResult(firstCode.rawValue);
                }
            }
        });
        // Dismiss the QR Code scan UI dialog.
        mDialogCallbacks.dismiss();
    }

    /**
     * Callback on camera error. Sends back the error using the |AssistantQrCodeDelegate| and
     * dismisses the QR Code Camera Scan dialog UI.
     */
    @Override
    public void onError(int error, Camera camera) {
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                AssistantQrCodeDelegate delegate =
                        mCameraScanModel.get(AssistantQrCodeCameraScanModel.DELEGATE);
                if (delegate != null) {
                    delegate.onCameraError();
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
