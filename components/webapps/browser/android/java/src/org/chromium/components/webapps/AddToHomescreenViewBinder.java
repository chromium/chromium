// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps;

import android.graphics.Bitmap;
import android.util.Pair;

import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds an add-to-homescreen {@link PropertyModel} with a {@link AddToHomescreenDialogView}. */
class AddToHomescreenViewBinder {
    static void bind(PropertyModel model, AddToHomescreenDialogView view, PropertyKey propertyKey) {
        if (propertyKey.equals(AddToHomescreenProperties.TITLE)) {
            view.setTitle(model.get(AddToHomescreenProperties.TITLE));
        } else if (propertyKey.equals(AddToHomescreenProperties.URL)) {
            view.setUrl(model.get(AddToHomescreenProperties.URL));
        } else if (propertyKey.equals(AddToHomescreenProperties.ICON)) {
            Pair<Bitmap, Boolean> iconPair = model.get(AddToHomescreenProperties.ICON);
            view.setIcon(iconPair.first, iconPair.second);
        } else if (propertyKey.equals(AddToHomescreenProperties.TYPE)) {
            view.setType(model.get(AddToHomescreenProperties.TYPE));
        } else if (propertyKey.equals(AddToHomescreenProperties.CAN_SUBMIT)) {
            view.setCanSubmit(model.get(AddToHomescreenProperties.CAN_SUBMIT));
        } else if (propertyKey.equals(AddToHomescreenProperties.NATIVE_INSTALL_BUTTON_TEXT)) {
            view.setNativeInstallButtonText(
                    model.get(AddToHomescreenProperties.NATIVE_INSTALL_BUTTON_TEXT));
        } else if (propertyKey.equals(AddToHomescreenProperties.NATIVE_APP_RATING)) {
            view.setNativeAppRating(model.get(AddToHomescreenProperties.NATIVE_APP_RATING));
        }
    }
}
