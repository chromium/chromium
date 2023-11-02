// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.guided_browsing.qr_code.image_picker;

import org.chromium.components.autofill_assistant.guided_browsing.qr_code.AssistantQrCodeDelegate;
import org.chromium.components.autofill_assistant.guided_browsing.qr_code.permission.AssistantQrCodePermissionModel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

/**
 * State for the QR Code Image Picker UI.
 */
public class AssistantQrCodeImagePickerModel extends PropertyModel {
    /** Assistant QR Code delegate. */
    static final WritableObjectPropertyKey<AssistantQrCodeDelegate> DELEGATE =
            new WritableObjectPropertyKey<>();

    /** Is the application in foreground. */
    static final WritableBooleanPropertyKey IS_ON_FOREGROUND = new WritableBooleanPropertyKey();

    /** Image Picker Toolbar Title. */
    static final WritableObjectPropertyKey<String> TOOLBAR_TITLE =
            new WritableObjectPropertyKey<>();

    private final AssistantQrCodePermissionModel mPermissionModel;

    /**
     * The AssistantQrCodeImagePickerModel constructor.
     */
    public AssistantQrCodeImagePickerModel() {
        super(DELEGATE, IS_ON_FOREGROUND, TOOLBAR_TITLE);
        mPermissionModel = new AssistantQrCodePermissionModel();
    }

    AssistantQrCodePermissionModel getReadImagesPermissionModel() {
        return mPermissionModel;
    }

    public void setDelegate(AssistantQrCodeDelegate delegate) {
        set(DELEGATE, delegate);
    }

    public void setToolbarTitle(String text) {
        set(TOOLBAR_TITLE, text);
    }

    public void setPermissionText(String text) {
        mPermissionModel.setPermissionText(text);
    }

    public void setPermissionButtonText(String text) {
        mPermissionModel.setPermissionButtonText(text);
    }

    public void setOpenSettingsText(String text) {
        mPermissionModel.setOpenSettingsText(text);
    }

    public void setOpenSettingsButtonText(String text) {
        mPermissionModel.setOpenSettingsButtonText(text);
    }
}
