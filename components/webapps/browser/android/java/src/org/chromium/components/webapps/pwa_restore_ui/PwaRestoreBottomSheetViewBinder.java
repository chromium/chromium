// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_restore_ui;

import android.widget.Button;
import android.widget.TextView;

import org.chromium.components.webapps.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Binds a pwa-restore {@link PropertyModel} with a {@link PwaRestoreBottomSheetView}.
 */
class PwaRestoreBottomSheetViewBinder {
    static void bind(PropertyModel model, PwaRestoreBottomSheetView view, PropertyKey propertyKey) {
        if (propertyKey.equals(PwaRestoreProperties.BUTTON_LABEL)) {
            ((Button) view.getPreviewView().findViewById(R.id.button))
                    .setText(model.get(PwaRestoreProperties.BUTTON_LABEL));
        } else if (propertyKey.equals(PwaRestoreProperties.DESCRIPTION)) {
            ((TextView) view.getPreviewView().findViewById(R.id.description))
                    .setText(model.get(PwaRestoreProperties.DESCRIPTION));
        } else if (propertyKey.equals(PwaRestoreProperties.TITLE)) {
            ((TextView) view.getPreviewView().findViewById(R.id.title))
                    .setText(model.get(PwaRestoreProperties.TITLE));
        } else if (propertyKey.equals(PwaRestoreProperties.CAN_SUBMIT)) {
            ((Button) view.getPreviewView().findViewById(R.id.button))
                    .setEnabled(model.get(PwaRestoreProperties.CAN_SUBMIT));
        }
    }
}
