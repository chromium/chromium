// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.navattach;

import android.view.View;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.ButtonCompat;

/** View binder for tabs shown in the attachments popup */
@NullMarked
public class TabAttachmentPopupChoiceViewBinder {

    public static void bind(PropertyModel propertyModel, View view, PropertyKey propertyKey) {
        ButtonCompat buttonCompat = (ButtonCompat) view;
        if (propertyKey == TabAttachmentPopupChoiceProperties.THUMBNAIL) {
            buttonCompat.setCompoundDrawablesRelativeWithIntrinsicBounds(
                    propertyModel.get(TabAttachmentPopupChoiceProperties.THUMBNAIL),
                    null,
                    null,
                    null);
        } else if (propertyKey == TabAttachmentPopupChoiceProperties.TITLE) {
            buttonCompat.setText(propertyModel.get(TabAttachmentPopupChoiceProperties.TITLE));
        }
    }
}
