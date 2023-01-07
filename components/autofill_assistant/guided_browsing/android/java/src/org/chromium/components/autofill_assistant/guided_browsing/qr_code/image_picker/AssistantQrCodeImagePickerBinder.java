// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.guided_browsing.qr_code.image_picker;

import android.widget.TextView;

import org.chromium.components.autofill_assistant.guided_browsing.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor.ViewBinder;

/**
 * This class is responsible for pushing updates to the Autofill Assistant UI for QR Code Scanning
 * via Image Picker. These updates are pulled from the {@link AssistantQrCodeImagePickerModel} when
 * a notification of an update is received.
 */
class AssistantQrCodeImagePickerBinder implements ViewBinder<AssistantQrCodeImagePickerModel,
        AssistantQrCodeImagePickerBinder.ViewHolder, PropertyKey> {
    /**
     * A wrapper class that holds the different views of the QR Code Image Picker UI.
     */
    static class ViewHolder {
        private final AssistantQrCodeImagePickerView mImagePickerView;
        private final TextView mTitleView;

        public ViewHolder(AssistantQrCodeImagePickerView imagePickerView) {
            mImagePickerView = imagePickerView;
            mTitleView = imagePickerView.getRootView().findViewById(R.id.toolbar_title);
        }
    }

    @Override
    public void bind(
            AssistantQrCodeImagePickerModel model, ViewHolder viewHolder, PropertyKey propertyKey) {
        if (propertyKey == AssistantQrCodeImagePickerModel.DELEGATE) {
            // Do nothing. Subsequent notifications will be sent to the new delegate.
        } else if (propertyKey == AssistantQrCodeImagePickerModel.IS_ON_FOREGROUND) {
            viewHolder.mImagePickerView.onForegroundChanged(
                    model.get(AssistantQrCodeImagePickerModel.IS_ON_FOREGROUND));
        } else if (propertyKey == AssistantQrCodeImagePickerModel.TOOLBAR_TITLE) {
            viewHolder.mTitleView.setText(model.get(AssistantQrCodeImagePickerModel.TOOLBAR_TITLE));
        } else {
            assert false : "Unhandled property detected in AssistantQrCodeImagePickerBinder!";
        }
    }
}
