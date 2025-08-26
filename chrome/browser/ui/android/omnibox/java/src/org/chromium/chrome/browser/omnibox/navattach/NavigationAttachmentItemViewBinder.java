// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.navattach;

import android.widget.ImageView;
import android.widget.TextView;

import androidx.constraintlayout.widget.ConstraintLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.omnibox.R;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** Binds the Navigation Attachment Item properties to the view. */
@NullMarked
class NavigationAttachmentItemViewBinder {
    /**
     * @see PropertyModelChangeProcessor.ViewBinder#bind(Object, Object, Object)
     */
    public static void bind(PropertyModel model, ConstraintLayout view, PropertyKey propertyKey) {
        if (propertyKey == NavigationAttachmentItemProperties.THUMBNAIL) {
            ImageView imageView = view.findViewById(R.id.attachment_thumbnail);
            imageView.setImageDrawable(model.get(NavigationAttachmentItemProperties.THUMBNAIL));
        } else if (propertyKey == NavigationAttachmentItemProperties.TITLE) {
            TextView textView = view.findViewById(R.id.attachment_title);
            textView.setText(model.get(NavigationAttachmentItemProperties.TITLE));
        } else if (propertyKey == NavigationAttachmentItemProperties.DESCRIPTION) {
            TextView textView = view.findViewById(R.id.attachment_description);
            textView.setText(model.get(NavigationAttachmentItemProperties.DESCRIPTION));
        }
    }
}
