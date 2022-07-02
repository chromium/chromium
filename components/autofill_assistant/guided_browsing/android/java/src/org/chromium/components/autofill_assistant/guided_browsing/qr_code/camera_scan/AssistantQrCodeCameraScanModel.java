// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.guided_browsing.qr_code.camera_scan;

import org.chromium.components.autofill_assistant.guided_browsing.qr_code.AssistantQrCodeDelegate;
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

    /** Does the application has permissions to start camera. */
    static final WritableBooleanPropertyKey HAS_CAMERA_PERMISSION =
            new WritableBooleanPropertyKey();

    /** Does the application has permissions to even prompt for camera permissions. */
    static final WritableBooleanPropertyKey CAN_PROMPT_FOR_CAMERA_PERMISSION =
            new WritableBooleanPropertyKey();

    /** Camera Scan Toolbar Title. */
    static final WritableObjectPropertyKey<String> TOOLBAR_TITLE =
            new WritableObjectPropertyKey<>();

    /** Camera Scan Permission Text. */
    static final WritableObjectPropertyKey<String> PERMISSION_TEXT =
            new WritableObjectPropertyKey<>();

    /** Camera Scan Permission Button Text. */
    static final WritableObjectPropertyKey<String> PERMISSION_BUTTON_TEXT =
            new WritableObjectPropertyKey<>();

    /** Camera Scan Open Settings Text. */
    static final WritableObjectPropertyKey<String> OPEN_SETTINGS_TEXT =
            new WritableObjectPropertyKey<>();

    /** Camera Scan Open Settings Button Text. */
    static final WritableObjectPropertyKey<String> OPEN_SETTINGS_BUTTON_TEXT =
            new WritableObjectPropertyKey<>();

    /** Camera Scan Preview Overlay Title. */
    static final WritableObjectPropertyKey<String> OVERLAY_TITLE =
            new WritableObjectPropertyKey<>();

    /**
     * The AssistantQrCodeCameraScanModel constructor.
     */
    public AssistantQrCodeCameraScanModel() {
        super(DELEGATE, IS_ON_FOREGROUND, HAS_CAMERA_PERMISSION, CAN_PROMPT_FOR_CAMERA_PERMISSION,
                TOOLBAR_TITLE, PERMISSION_TEXT, PERMISSION_BUTTON_TEXT, OPEN_SETTINGS_TEXT,
                OPEN_SETTINGS_BUTTON_TEXT, OVERLAY_TITLE);
    }

    public void setDelegate(AssistantQrCodeDelegate delegate) {
        set(DELEGATE, delegate);
    }

    public void setHasCameraPermission(boolean hasCameraPermission) {
        set(HAS_CAMERA_PERMISSION, hasCameraPermission);
    }

    public void setCanPromptForCameraPermission(boolean canPromptForCameraPermission) {
        set(CAN_PROMPT_FOR_CAMERA_PERMISSION, canPromptForCameraPermission);
    }

    public void setToolbarTitle(String text) {
        set(TOOLBAR_TITLE, text);
    }

    public void setPermissionText(String text) {
        set(PERMISSION_TEXT, text);
    }

    public void setPermissionButtonText(String text) {
        set(PERMISSION_BUTTON_TEXT, text);
    }

    public void setOpenSettingsText(String text) {
        set(OPEN_SETTINGS_TEXT, text);
    }

    public void setOpenSettingsButtonText(String text) {
        set(OPEN_SETTINGS_BUTTON_TEXT, text);
    }

    public void setOverlayTitle(String text) {
        set(OVERLAY_TITLE, text);
    }
}