// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.autofill_assistant.guided_browsing.qr_code.permission;

import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableBooleanPropertyKey;

/**
 * State for the QR Code Permission UI.
 */
public class AssistantQrCodePermissionModel extends PropertyModel {
    /** Does the application has the required permission. */
    static final WritableBooleanPropertyKey HAS_PERMISSION = new WritableBooleanPropertyKey();

    /** Does the application has permissions to even prompt for the permission. */
    static final WritableBooleanPropertyKey CAN_PROMPT_FOR_PERMISSION =
            new WritableBooleanPropertyKey();

    /** Permission Text. */
    static final WritableObjectPropertyKey<String> PERMISSION_TEXT =
            new WritableObjectPropertyKey<>();

    /** Permission Button Text. */
    static final WritableObjectPropertyKey<String> PERMISSION_BUTTON_TEXT =
            new WritableObjectPropertyKey<>();

    /** Open Settings Text. */
    static final WritableObjectPropertyKey<String> OPEN_SETTINGS_TEXT =
            new WritableObjectPropertyKey<>();

    /** Open Settings Button Text. */
    static final WritableObjectPropertyKey<String> OPEN_SETTINGS_BUTTON_TEXT =
            new WritableObjectPropertyKey<>();

    /**
     * The AssistantQrCodePermissionModel constructor.
     */
    public AssistantQrCodePermissionModel() {
        super(HAS_PERMISSION, CAN_PROMPT_FOR_PERMISSION, PERMISSION_TEXT, PERMISSION_BUTTON_TEXT,
                OPEN_SETTINGS_TEXT, OPEN_SETTINGS_BUTTON_TEXT);
    }

    public void setHasPermission(boolean hasPermission) {
        set(HAS_PERMISSION, hasPermission);
    }

    public void setCanPromptForPermission(boolean canPromptForPermission) {
        set(CAN_PROMPT_FOR_PERMISSION, canPromptForPermission);
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
}