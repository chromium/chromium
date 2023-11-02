// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.video_tutorials.list;

import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import org.chromium.chrome.browser.video_tutorials.R;
import org.chromium.components.browser_ui.widget.async_image.AsyncImageView;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * This class is responsible for building and binding the video tutorial card.
 */
class TutorialCardViewBinder {
    /** Builder method to create the card view. */
    static View buildView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(R.layout.video_tutorial_large_card, parent, false);
    }

    /** Binder method to bind the card view with the model properties. */
    static void bindView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == TutorialCardProperties.TITLE) {
            TextView title = view.findViewById((R.id.title));
            title.setText(model.get(TutorialCardProperties.TITLE));
        } else if (propertyKey == TutorialCardProperties.VIDEO_LENGTH) {
            TextView title = view.findViewById((R.id.video_length));
            title.setText(model.get(TutorialCardProperties.VIDEO_LENGTH));
        } else if (propertyKey == TutorialCardProperties.CLICK_CALLBACK) {
            view.setOnClickListener(
                    v -> { model.get(TutorialCardProperties.CLICK_CALLBACK).run(); });
        } else if (propertyKey == TutorialCardProperties.VISUALS_PROVIDER) {
            AsyncImageView thumbnailView = (AsyncImageView) view.findViewById(R.id.thumbnail);
            thumbnailView.setAsyncImageDrawable(
                    model.get(TutorialCardProperties.VISUALS_PROVIDER), null);
        } else {
            throw new IllegalArgumentException(
                    "Cannot update the view for propertyKey: " + propertyKey);
        }
    }
}
