// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.navattach;

import android.text.TextUtils;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

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
    public static void bind(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == NavigationAttachmentItemProperties.THUMBNAIL) {
            ImageView imageView = view.findViewById(R.id.attachment_thumbnail);
            imageView.setImageDrawable(model.get(NavigationAttachmentItemProperties.THUMBNAIL));
        } else if (propertyKey == NavigationAttachmentItemProperties.TITLE
                || propertyKey == NavigationAttachmentItemProperties.DESCRIPTION) {
            applyTitleAndDescriptionIfPresent(model, view);
        } else if (propertyKey == NavigationAttachmentItemProperties.ON_REMOVE) {
            view.findViewById(R.id.attachment_remove_button)
                    .setOnClickListener(
                            v -> model.get(NavigationAttachmentItemProperties.ON_REMOVE).run());
        }
    }

    private static void applyTitleAndDescriptionIfPresent(PropertyModel model, View view) {
        CharSequence title = model.get(NavigationAttachmentItemProperties.TITLE);
        TextView titleView = view.findViewById(R.id.attachment_title);
        TextView descriptionView = view.findViewById(R.id.attachment_description);

        if (TextUtils.isEmpty(title)) {
            titleView.setVisibility(View.GONE);
            descriptionView.setVisibility(View.GONE);
        } else {
            titleView.setVisibility(View.VISIBLE);
            titleView.setText(title);
            descriptionView.setVisibility(View.VISIBLE);
            descriptionView.setText(model.get(NavigationAttachmentItemProperties.DESCRIPTION));
        }
    }
}
