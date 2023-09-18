// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_restore_ui;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Contains the properties that a pwa-restore {@link PropertyModel} can have.
 */
public class PwaRestoreProperties {
    static final PropertyModel.WritableObjectPropertyKey<String> DESCRIPTION =
            new PropertyModel.WritableObjectPropertyKey<>();
    static final PropertyModel.WritableObjectPropertyKey<String> TITLE =
            new PropertyModel.WritableObjectPropertyKey<>();
    static final PropertyModel.WritableObjectPropertyKey<String> BUTTON_LABEL =
            new PropertyModel.WritableObjectPropertyKey<>();
    static final PropertyModel.WritableBooleanPropertyKey CAN_SUBMIT =
            new PropertyModel.WritableBooleanPropertyKey();

    static final PropertyKey[] ALL_KEYS = {DESCRIPTION, TITLE, BUTTON_LABEL, CAN_SUBMIT};

    static PropertyModel createModel() {
        return new PropertyModel.Builder(ALL_KEYS).build();
    }
}
