// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.webapps.bottomsheet;

import android.graphics.Bitmap;
import android.util.Pair;

import org.chromium.components.webapps.AddToHomescreenProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds an add-to-homescreen {@link PropertyModel} with a {@link PwaInstallBottomSheetContent}. */
class AddToHomescreenBottomSheetViewBinder {
    static void bind(PropertyModel model, PwaInstallBottomSheetView view, PropertyKey propertyKey) {
        if (propertyKey.equals(AddToHomescreenProperties.TITLE)) {
            view.setTitle(model.get(AddToHomescreenProperties.TITLE));
        } else if (propertyKey.equals(AddToHomescreenProperties.URL)) {
            view.setUrl(model.get(AddToHomescreenProperties.URL));
        } else if (propertyKey.equals(AddToHomescreenProperties.DESCRIPTION)) {
            view.setDescription(model.get(AddToHomescreenProperties.DESCRIPTION));
        } else if (propertyKey.equals(AddToHomescreenProperties.ICON)) {
            Pair<Bitmap, Boolean> iconPair = model.get(AddToHomescreenProperties.ICON);
            view.setIcon(iconPair.first, iconPair.second);
        } else if (propertyKey.equals(AddToHomescreenProperties.CAN_SUBMIT)) {
            view.setCanSubmit(model.get(AddToHomescreenProperties.CAN_SUBMIT));
        } else if (propertyKey.equals(AddToHomescreenProperties.CLICK_LISTENER)) {
            view.setOnClickListener(model.get(AddToHomescreenProperties.CLICK_LISTENER));
        }
    }
}
