// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.guided_browsing.qr_code.camera_scan;

import android.app.Dialog;
import android.app.DialogFragment;
import android.content.Context;
import android.os.Bundle;

import androidx.appcompat.app.AlertDialog;

import org.chromium.components.autofill_assistant.guided_browsing.R;
import org.chromium.ui.base.WindowAndroid;

/**
 * Main Dialog Fragment to trigger QR Code Camera Scan.
 */
public class AssistantQrCodeCameraScanDialog extends DialogFragment {
    private Context mContext;
    private WindowAndroid mWindowAndroid;
    private AssistantQrCodeCameraScanModel mCameraScanModel;
    private AssistantQrCodeCameraScanCoordinator mCameraScanCoordinator;

    /**
     * Create a new instance of {@link AssistantQrCodeCameraScanDialog}.
     */
    public static AssistantQrCodeCameraScanDialog newInstance(Context context,
            WindowAndroid windowAndroid, AssistantQrCodeCameraScanModel cameraScanModel) {
        AssistantQrCodeCameraScanDialog assistantQrCodeCameraScanDialog =
                new AssistantQrCodeCameraScanDialog();
        assistantQrCodeCameraScanDialog.setContext(context);
        assistantQrCodeCameraScanDialog.setWindowAndroid(windowAndroid);
        assistantQrCodeCameraScanDialog.setCameraScanModel(cameraScanModel);
        return assistantQrCodeCameraScanDialog;
    }

    public void setContext(Context context) {
        mContext = context;
    }

    public void setWindowAndroid(WindowAndroid windowAndroid) {
        mWindowAndroid = windowAndroid;
    }

    public void setCameraScanModel(AssistantQrCodeCameraScanModel cameraScanModel) {
        mCameraScanModel = cameraScanModel;
    }

    @Override
    public Dialog onCreateDialog(Bundle savedInstanceState) {
        mCameraScanCoordinator = new AssistantQrCodeCameraScanCoordinator(
                mContext, mWindowAndroid, mCameraScanModel, this::dismiss);
        AlertDialog.Builder builder =
                new AlertDialog.Builder(getActivity(), R.style.ThemeOverlay_BrowserUI_Fullscreen);
        builder.setView(mCameraScanCoordinator.getView());
        return builder.create();
    }

    @Override
    public void onResume() {
        super.onResume();
        mCameraScanCoordinator.resume();
    }

    @Override
    public void onPause() {
        super.onPause();
        mCameraScanCoordinator.pause();
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        mCameraScanCoordinator.destroy();
    }
}