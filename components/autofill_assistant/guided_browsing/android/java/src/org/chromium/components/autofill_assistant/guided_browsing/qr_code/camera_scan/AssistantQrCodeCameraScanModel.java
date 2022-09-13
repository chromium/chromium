// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.guided_browsing.qr_code.camera_scan;

import org.chromium.components.autofill_assistant.guided_browsing.qr_code.AssistantQrCodeDelegate;
import org.chromium.components.autofill_assistant.guided_browsing.qr_code.permission.AssistantQrCodePermissionModel;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

/**
 * State for the QR Code Camera Scan UI.
 */
public class AssistantQrCodeCameraScanModel extends PropertyModel {
    /** Assistant QR Code delegate. */
    static final WritableObjectPropertyKey<AssistantQrCodeDelegate> DELEGATE =
            new WritableObjectPropertyKey<>();

    /** Is the application in foreground. */
    static final WritableBooleanPropertyKey IS_ON_FOREGROUND = new WritableBooleanPropertyKey();

    /** Camera Scan Toolbar Title. */
    static final WritableObjectPropertyKey<String> TOOLBAR_TITLE =
            new WritableObjectPropertyKey<>();

    /** Camera Scan Preview Overlay Instruction Text. */
    static final WritableObjectPropertyKey<String> OVERLAY_INSTRUCTION_TEXT =
            new WritableObjectPropertyKey<>();

    /** Camera Scan Preview Overlay Security Text. */
    static final WritableObjectPropertyKey<String> OVERLAY_SECURITY_TEXT =
            new WritableObjectPropertyKey<>();

    private final AssistantQrCodePermissionModel mPermissionModel;

    /**
     * The AssistantQrCodeCameraScanModel constructor.
     */
    public AssistantQrCodeCameraScanModel() {
        super(DELEGATE, IS_ON_FOREGROUND, TOOLBAR_TITLE, OVERLAY_INSTRUCTION_TEXT,
                OVERLAY_SECURITY_TEXT);
        mPermissionModel = new AssistantQrCodePermissionModel();
    }

    public AssistantQrCodePermissionModel getCameraPermissionModel() {
        return mPermissionModel;
    }

    public void setDelegate(AssistantQrCodeDelegate delegate) {
        set(DELEGATE, delegate);
    }

    public void setToolbarTitle(String text) {
        set(TOOLBAR_TITLE, text);
    }

    public void setOverlayInstructionText(String text) {
        set(OVERLAY_INSTRUCTION_TEXT, text);
    }

    public void setOverlaySecurityText(String text) {
        set(OVERLAY_SECURITY_TEXT, text);
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