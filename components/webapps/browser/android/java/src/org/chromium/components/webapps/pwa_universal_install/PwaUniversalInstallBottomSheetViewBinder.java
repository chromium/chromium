// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.pwa_universal_install;

import android.view.View.OnClickListener;
import android.widget.ImageView;
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
        } else if (propertyKey.equals(
                PwaUniversalInstallProperties.INSTALL_BUTTON_ON_CLICK_CALLBACK)) {
            OnClickListener listener =
                    model.get(PwaUniversalInstallProperties.INSTALL_BUTTON_ON_CLICK_CALLBACK);
            // Both the arrow and the underlying view should trigger the listener callback.
            ((ImageView) view.getContentView().findViewById(R.id.arrow_install))
                    .setOnClickListener(listener);
            view.getContentView().findViewById(R.id.option_install).setOnClickListener(listener);
        } else if (propertyKey.equals(
                PwaUniversalInstallProperties.ADD_SHORTCUT_BUTTON_ON_CLICK_CALLBACK)) {
            OnClickListener listener =
                    model.get(PwaUniversalInstallProperties.ADD_SHORTCUT_BUTTON_ON_CLICK_CALLBACK);
            // Both the arrow and the underlying view should trigger the listener callback.
            ((ImageView) view.getContentView().findViewById(R.id.arrow_shortcut))
                    .setOnClickListener(listener);
            view.getContentView().findViewById(R.id.option_shortcut).setOnClickListener(listener);
        }
    }
}
