// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_universal_install;

import android.widget.TextView;

import org.chromium.components.webapps.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Binds a pwa-universal-install {@link PropertyModel} with a {@link
 * PwaUniversalInstallBottomSheetView}.
 */
class PwaUniversalInstallBottomSheetViewBinder {
    static void bind(
            PropertyModel model, PwaUniversalInstallBottomSheetView view, PropertyKey propertyKey) {
        if (propertyKey.equals(PwaUniversalInstallProperties.TITLE)) {
            ((TextView) view.getContentView().findViewById(R.id.title))
                    .setText(model.get(PwaUniversalInstallProperties.TITLE));
        }
    }
}
